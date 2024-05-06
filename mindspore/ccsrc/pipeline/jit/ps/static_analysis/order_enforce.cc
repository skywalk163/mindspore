/**
 * Copyright 2021-2024 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pipeline/jit/ps/static_analysis/order_enforce.h"
#include <algorithm>
#include <map>
#include <set>
#include <queue>
#include <vector>
#include <utility>
#include <string>
#include <memory>
#include "mindspore/core/ops/sequence_ops.h"
#include "mindspore/core/ops/nn_ops.h"
#include "mindspore/core/ops/array_ops.h"
#include "mindspore/core/ops/framework_ops.h"
#include "utils/hash_map.h"
#include "utils/hash_set.h"
#include "utils/compact_set.h"
#include "include/common/utils/utils.h"

namespace mindspore::pipeline {
namespace {
class OrderEnforcer {
 public:
  explicit OrderEnforcer(const FuncGraphPtr &func_graph) : func_graph_(func_graph), manager_(func_graph->manager()) {
    MS_EXCEPTION_IF_NULL(func_graph_);
    MS_EXCEPTION_IF_NULL(manager_);
  }
  ~OrderEnforcer() = default;

  void Run() {
    auto nodes = MakeTopoSortMap();
    for (auto &node : nodes) {
      if (IsPrimitiveCNode(node, prim::kPrimUpdateState)) {
        HandleUpdateState(node);
      } else if (IsPrimitiveCNode(node, prim::kPrimMakeTuple)) {
        // op(MakeTuple(Load, ...)) sometimes do not attach update_state,
        // So need special treatment in order to ensure the exec_order of MakeTuple users.
        HandleMakeTupleUsers(node);
      }
    }
    static const bool no_insert_tensormove = common::GetEnv("MS_DEV_SIDE_EFFECT_LOAD_ELIM") == "3";
    // Do not insert TensorMove for all Load nodes
    if (no_insert_tensormove) {
      MS_LOG(WARNING) << "Do not insert TensorMove for all Load nodes, the memory footprint is minimal, "
                         "but there may be accuracy issues with the results.";
      return;
    }
    // After ensuring the correct control edge relationship, then insert the TensorMove operator.
    // In order to store current value of parameter, insert TensorMove for Load:
    // whose refkey appears more than once,
    // or the load is input of call or partial,
    // or the first input of load is call or partial.
    mindspore::HashSet<CNodePtr> need_insert_loads = GetNeedInsertLoads();
    for (auto &node : need_insert_loads) {
      InsertTensorMoveForLoad(node->cast<CNodePtr>());
    }
  }

 private:
  AnfNodePtrList MakeTopoSortMap() {
    auto nodes = TopoSort(func_graph_->get_return());
    for (size_t i = 0; i < nodes.size(); ++i) {
      (void)topo_sort_map_.emplace(nodes[i], i);
    }
    return nodes;
  }

  void HandleUpdateState(const AnfNodePtr &node) {
    auto update_state = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(update_state);
    const size_t update_state_inputs_size = 3;
    if (update_state->size() < update_state_inputs_size) {
      MS_LOG(ERROR) << "UpdateState inputs size is less than 3, node is:" << update_state->DebugString();
    }
    if (!HasAbstractUMonad(update_state->input(1))) {
      // Skip UpdateStates for IO.
      return;
    }
    const size_t attach_index = 2;
    auto &attach = update_state->input(attach_index);
    if (IsPrimitiveCNode(attach, prim::kPrimLoad) && IsPrimitiveCNode(attach, prim::kPrimMakeTuple)) {
      // Skip UpdateState for Loads.
      return;
    }
    // Check previous update_state.
    auto &prev_u = update_state->input(1);
    if (!IsPrimitiveCNode(prev_u, prim::kPrimUpdateState)) {
      // Skip if previous is not UpdateState (maybe a U).
      return;
    }
    // Search side effect cnodes that use previous update_state as input.
    auto side_effect_nodes = FindNodeUsers(prev_u, [&update_state](const AnfNodePtr &user_node) {
      return (user_node != update_state) && !IsPrimitiveCNode(user_node, prim::kPrimLoad);
    });
    // For such side effect cnodes, try enfore order for them.
    for (auto &side_effect_node : side_effect_nodes) {
      HandleSideEffectNode(side_effect_node->cast<CNodePtr>(), prev_u->cast<CNodePtr>());
    }
  }

  bool HasLoadInput(const CNodePtr &cnode) const {
    auto &weak_inputs = cnode->weak_inputs();
    return std::any_of(weak_inputs.cbegin() + 1, weak_inputs.cend(), [](const auto &weak_input) {
      const auto &input = weak_input.lock();
      MS_EXCEPTION_IF_NULL(input);
      return IsPrimitiveCNode(input, prim::kPrimLoad);
    });
  }

  std::vector<AnfNodePtr> FindUpdateStateUsers(const AnfNodePtr &node) {
    auto &node_users = manager_->node_users();
    auto iter = node_users.find(node);
    if (iter == node_users.end()) {
      return {};
    }
    std::vector<AnfNodePtr> update_states;
    auto &users = iter->second;
    for (auto &user : users) {
      auto &user_node = user.first;
      if (IsPrimitiveCNode(user_node, prim::kPrimUpdateState)) {
        (void)update_states.emplace_back(user_node);
        continue;
      }
      if (IsPrimitiveCNode(user_node, prim::kPrimMakeTuple)) {
        auto make_tuple_users = FindUpdateStateUsers(user_node);
        (void)update_states.insert(update_states.end(), make_tuple_users.begin(), make_tuple_users.end());
      }
    }
    return update_states;
  }

  AnfNodePtr FindLastUpdateState(const CNodePtr &cnode) {
    // Find all update_state nodes from the user of input load nodes.
    std::vector<AnfNodePtr> all_update_states;
    for (size_t index = 1; index < cnode->size(); index++) {
      auto &input = cnode->input(index);
      if (IsPrimitiveCNode(input, prim::kPrimLoad)) {
        std::vector<AnfNodePtr> update_states = FindUpdateStateUsers(input);
        (void)all_update_states.insert(all_update_states.end(), update_states.begin(), update_states.end());
      }
    }
    // Find the last update_state by topo sort order.
    auto last_update_state =
      std::max_element(all_update_states.begin(), all_update_states.end(),
                       [this](const AnfNodePtr &a, const AnfNodePtr &b) { return IsBefore(a, b); });
    if (last_update_state == all_update_states.end()) {
      return nullptr;
    }
    return *last_update_state;
  }

  // Convert:
  // load1 = Load(para1, u1)
  // load2 = Load(para2, u2)
  // maketuple1 = MakeTuple(inputs, load1, load2) # the make_tuple we should handle.
  // addn = AddN(maketupe1) # or other-op, user of the make_tuple
  // maketuple2 = MakeTuple(load1, load2)  # load user
  // u3 = UpdateState(u', maketuple2)  # the last update_state for load users.
  // assign = Assign(para2, inputs, u3)
  // To:
  // load1 = Load(para1, u1)
  // load2 = Load(para2, u2)
  // maketuple1 = MakeTuple(inputs, load1, load2)
  // addn = AddN(maketupe1)
  // maketuple2 = MakeTuple(load1, load2)
  // u3 = UpdateState(u', maketuple2, addn) # need put addn or other-op into u3 inputs
  // assign = Assign(para2, inputs, u3)
  void HandleMakeTupleUsers(const AnfNodePtr &node) {
    auto maketuple = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(maketuple);
    if (!HasLoadInput(maketuple)) {
      // MakeTuple without Load input.
      return;
    }
    // Find the last update_state node from users of input Loads.
    auto update_state = FindLastUpdateState(maketuple);
    if (update_state == nullptr) {
      return;
    }
    // Users of the make_tuple.
    auto maketuple_users = FindNodeUsers(maketuple, [](const AnfNodePtr &user_node) {
      // Push and Pull at the end of the execution order,
      // In order to ensure push and pull operator cut into the same graph,
      // we do not put push operator into updatestate.
      return !IsPrimitiveCNode(user_node, prim::kPrimPush);
    });
    // Attach make_tuple users to the update_state.
    auto update_state_cnode = update_state->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(update_state_cnode);
    AddInputEdges(update_state_cnode, maketuple_users);
  }

  bool IsRef(const AnfNodePtr &node) const {
    MS_EXCEPTION_IF_NULL(node);
    auto &abs = node->abstract();
    return abs != nullptr && abs->isa<abstract::AbstractRefTensor>();
  }

  bool IsSpecialPrimitive(const AnfNodePtr &node) const {
    return IsPrimitiveCNode(node, prim::kPrimExpandDims) || IsPrimitiveCNode(node, prim::kPrimBatchNormGrad) ||
           IsPrimitiveCNode(node, prim::kPrimReshape);
  }

  bool IsSpecialParallelPrimitive(const AnfNodePtr &node) const {
    MS_EXCEPTION_IF_NULL(node);
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    auto prim = GetCNodePrimitiveWithoutDoSignature(cnode);
    if (prim == nullptr) {
      return false;
    }
    if (prim->HasAttr(GRAPH_FLAG_ORDER_ENFORCE_SKIP)) {
      return true;
    }
    return false;
  }

  void HandleSideEffectNode(const CNodePtr &cnode, const CNodePtr &update_state) {
    MS_EXCEPTION_IF_NULL(cnode);
    // Find refs from the cnode inputs.
    for (size_t i = 1; i < cnode->size(); ++i) {
      auto &input = cnode->input(i);
      // Skip non-ref input and update_state.
      if (!IsRef(input) || input == update_state) {
        continue;
      }
      // The input is a ref (of parameter), find load nodes for it.
      auto loads = FindLoadNodes(input);
      for (auto &load : loads) {
        // Find user nodes of the Load.
        auto load_users = FindLoadUsers(load);
        mindspore::CompactSet<AnfNodePtr> real_users;
        for (auto &load_user : load_users) {
          // Check the special operator, only one level of user is considered for now.
          if (IsSpecialPrimitive(load_user)) {
            auto special_real_users = FindNodeUsers(load_user);
            (void)real_users.insert(special_real_users.begin(), special_real_users.end());
          } else if (IsSpecialParallelPrimitive(load_user)) {
            auto parallel__users = FindParallelNodeUsers(load_user);
            (void)real_users.insert(parallel__users.begin(), parallel__users.end());
          } else {
            (void)real_users.insert(load_user);
          }
        }
        AddInputEdges(update_state, real_users);
      }
    }
  }

  bool IsInUpdateState(const AnfNodePtr &load_user, const CNodePtr &update_state) const {
    MS_EXCEPTION_IF_NULL(update_state);
    const size_t attach_index = 2;
    const size_t input_size = update_state->size();
    for (size_t index = attach_index; index < input_size; index++) {
      auto &attach = update_state->input(index);
      if (attach == load_user) {
        return true;
      }
      if (IsPrimitiveCNode(attach, prim::kPrimMakeTuple)) {
        auto attach_cnode = attach->cast<CNodePtr>();
        auto &weak_inputs = attach_cnode->weak_inputs();
        auto iter = std::find_if(weak_inputs.begin() + 1, weak_inputs.end(), [&load_user](const auto &weak_input) {
          return weak_input.lock() != nullptr && weak_input.lock() == load_user;
        });
        if (iter != weak_inputs.end()) {
          return true;
        }
      }
    }
    return false;
  }

  // Add load users as input edges of the update_state node.
  void AddInputEdges(const CNodePtr &update_state, const mindspore::CompactSet<AnfNodePtr> &load_users) {
    auto sorted_load_users = SortLoadUsers(load_users);
    for (auto &load_user : sorted_load_users) {
      if (IsPrimitiveCNode(load_user, prim::kPrimMakeTuple) || IsPrimitiveCNode(load_user, prim::kPrimUpdateState)) {
        continue;
      }
      if (IsDependOn(load_user, update_state)) {
        continue;
      }
      (void)processed_nodes_.insert(load_user);
      if (!IsInUpdateState(load_user, update_state)) {
        manager_->AddEdge(update_state, load_user);
      }
    }
  }

  // Sort load users by their topo sort order.
  std::vector<AnfNodePtr> SortLoadUsers(const mindspore::CompactSet<AnfNodePtr> &load_users) {
    std::vector<AnfNodePtr> vec{load_users.begin(), load_users.end()};
    std::sort(vec.begin(), vec.end(), [this](const AnfNodePtr &a, const AnfNodePtr &b) { return IsBefore(a, b); });
    return vec;
  }

  // Check if the load user node depend on the given UpdateState node.
  bool IsDependOn(const AnfNodePtr &load_user, const AnfNodePtr &update_state) {
    size_t update_state_order = topo_sort_map_[update_state];
    if (topo_sort_map_[load_user] < update_state_order) {
      return false;
    }
    auto user_cnode = dyn_cast<CNode>(load_user);
    if (user_cnode == nullptr) {
      return false;
    }
    auto seen = NewSeenGeneration();
    std::queue<CNodePtr> q;
    user_cnode->seen_ = seen;
    q.push(user_cnode);
    while (!q.empty()) {
      auto cnode = q.front();
      MS_EXCEPTION_IF_NULL(cnode);
      q.pop();
      for (auto &weak_input : cnode->weak_inputs()) {
        auto input = weak_input.lock();
        MS_EXCEPTION_IF_NULL(input);
        if (input == update_state) {
          // Dependency found.
          return true;
        }
        if (input->seen_ == seen) {
          // Skip visited nodes.
          continue;
        }
        if (topo_sort_map_[input] < update_state_order) {
          // Skip input nodes that before the UpdateState node.
          continue;
        }
        auto input_cnode = dyn_cast<CNode>(input);
        if (input_cnode != nullptr) {
          input_cnode->seen_ = seen;
          q.push(input_cnode);
        }
      }
    }
    return false;
  }

  bool IsBefore(const AnfNodePtr &node1, const AnfNodePtr &node2) {
    return topo_sort_map_[node1] < topo_sort_map_[node2];
  }

  using PredFunc = std::function<bool(const AnfNodePtr &)>;

  // Find user nodes for the given node.
  mindspore::CompactSet<AnfNodePtr> FindNodeUsers(const AnfNodePtr &node, const PredFunc &pred = nullptr) {
    auto &node_users = manager_->node_users();
    auto iter = node_users.find(node);
    if (iter == node_users.end()) {
      return {};
    }
    mindspore::CompactSet<AnfNodePtr> users;
    for (auto &user : iter->second) {
      auto &user_node = user.first;
      if (pred == nullptr || pred(user_node)) {
        users.insert(user_node);
      }
    }
    return users;
  }

  // Find real user nodes for the given parallel nodes.
  mindspore::CompactSet<AnfNodePtr> FindParallelNodeUsers(const AnfNodePtr &node) {
    auto &node_users = manager_->node_users();
    auto iter = node_users.find(node);
    if (iter == node_users.end()) {
      return {};
    }
    mindspore::CompactSet<AnfNodePtr> users;
    for (auto &user : iter->second) {
      auto &user_node = user.first;
      if (!IsSpecialParallelPrimitive(user_node)) {
        (void)users.insert(user_node);
      } else {
        mindspore::CompactSet<AnfNodePtr> real_users;
        real_users = FindParallelNodeUsers(user_node);
        (void)users.insert(real_users.begin(), real_users.end());
      }
    }
    return users;
  }

  // Find Load or parameter users as the candidate nodes to enforce order of execution.
  mindspore::CompactSet<AnfNodePtr> FindLoadUsers(const AnfNodePtr &load_or_param) {
    return FindNodeUsers(load_or_param, [this](const AnfNodePtr &user_node) {
      // Skip processed nodes.
      return processed_nodes_.find(user_node) == processed_nodes_.end();
    });
  }

  // Find Load nodes for a parameter.
  mindspore::CompactSet<AnfNodePtr> FindLoadNodes(const AnfNodePtr &param) {
    return FindNodeUsers(param, [this](const AnfNodePtr &user_node) {
      // Search for Load nodes only.
      return IsPrimitiveCNode(user_node, prim::kPrimLoad);
    });
  }

  std::string GetRefKey(const AnfNodePtr &node) const {
    MS_EXCEPTION_IF_NULL(node);
    auto abs = node->abstract();
    if (abs == nullptr) {
      if (IsPrimitiveCNode(node, prim::kPrimDepend)) {
        return GetRefKey(node->cast<CNodePtr>()->input(1));
      }
      return "";
    }
    auto abs_ref = abs->cast<abstract::AbstractRefPtr>();
    if (abs_ref == nullptr) {
      return "";
    }
    MS_EXCEPTION_IF_NULL(abs_ref->ref_key_value());
    auto ref_key = abs_ref->ref_key_value()->cast<StringImmPtr>();
    if (ref_key == nullptr) {
      return "";
    }
    return ref_key->value();
  }

  mindspore::HashSet<CNodePtr> GetAllLoads(const AnfNodePtrList &check_nodes) const {
    mindspore::HashSet<CNodePtr> need_insert_loads;
    for (auto &node : check_nodes) {
      if (IsPrimitiveCNode(node, prim::kPrimLoad)) {
        auto load = node->cast<CNodePtr>();
        (void)need_insert_loads.insert(load);
      }
    }
    return need_insert_loads;
  }

  using RefLoads = std::map<std::string, std::vector<CNodePtr>>;

  void AppendLoads(const RefLoads &loads_map, mindspore::HashSet<CNodePtr> *need_insert_loads) const {
    for (auto &refkey_load_special : loads_map) {
      auto &loads = refkey_load_special.second;
      for (auto load : loads) {
        (void)need_insert_loads->insert(load);
      }
    }
  }

  bool HasRefKeyInput(const AnfNodePtr &node, const std::string &ref_key) const {
    auto key = GetRefKey(node);
    if (key == ref_key) {
      return true;
    }
    if (!node->isa<CNode>()) {
      return false;
    }
    const auto &inner_weak_inputs = node->cast<CNodePtr>()->weak_inputs();
    return std::any_of(inner_weak_inputs.cbegin(), inner_weak_inputs.cend(), [&](const auto &inner_weak_input) {
      const auto &inner_input = inner_weak_input.lock();
      MS_EXCEPTION_IF_NULL(inner_input);
      return !HasAbstractMonad(inner_input) && HasRefKeyInput(inner_input, ref_key);
    });
  }

  // If two loads at different times are used as inputs to the same node, need to insert TensorMove
  // load1 = Load(param, u1)
  // load2 = Load(param, u2)
  // load3 = Load(param, u3)
  // tuple1 = MakeTuple(load1, load2)
  // a1 = AddN(tuple1)
  // tuple2 = MakeTuple(a1, load3)
  // a2 = AddN(tuple2)
  // ---> after insert TensorMove
  // load1 = Load(param, u1)
  // load2 = Load(param, u2)
  // load3 = Load(param, u3)
  // t1 = TensorMove(load1)
  // t2 = TensorMove(load2)
  // tuple1 = MakeTuple(t1, t2)
  // a1 = AddN(tuple1)
  // t3 = TensorMove(load3)
  // tuple2 = MakeTuple(a1, t3)
  // a2 = AddN(tuple2)
  bool ReCheckUsersOfLoad(const AnfNodePtr &load, const std::string &ref_key) const {
    auto &node_users = manager_->node_users();
    auto iter = node_users.find(load);
    if (iter == node_users.end()) {
      return false;
    }
    auto &users = iter->second;
    for (auto &user : users) {
      auto &user_node = user.first;
      auto user_cnode = user_node->cast<CNodePtr>();
      if (IsPrimitiveCNode(user_cnode, prim::kPrimUpdateState)) {
        continue;
      }
      const auto &weak_inputs = user_cnode->weak_inputs();
      size_t ref_key_times = 0;
      for (auto &weak_input : weak_inputs) {
        const auto &input = weak_input.lock();
        MS_EXCEPTION_IF_NULL(input);
        if (IsPrimitiveCNode(input, prim::kPrimLoad)) {
          auto key = GetRefKey(input->cast<CNodePtr>()->input(1));
          if (key == ref_key) {
            ref_key_times++;
            if (ref_key_times > 1) {
              return true;
            }
          }
        }

        if (!input->isa<CNode>()) {
          continue;
        }
        const auto &inner_weak_inputs = input->cast<CNodePtr>()->weak_inputs();
        for (auto inner_weak_input : inner_weak_inputs) {
          const auto &inner_input = inner_weak_input.lock();
          MS_EXCEPTION_IF_NULL(inner_input);
          if (IsPrimitiveCNode(inner_input, prim::kPrimUpdateState) || !HasRefKeyInput(inner_input, ref_key)) {
            continue;
          }
          ref_key_times++;
          if (ref_key_times > 1) {
            return true;
          }
        }
      }
    }
    return false;
  }
  mindspore::HashSet<CNodePtr> GetSpecialLoads(const RefLoads &loads_map1, const RefLoads &loads_map2,
                                               const RefLoads &loads_map3, const RefLoads &loads_map4,
                                               const std::set<CNodePtr> &call_nodes) const {
    mindspore::HashSet<CNodePtr> need_insert_loads;
    static const bool strict_insert_tensormove = common::GetEnv("MS_DEV_SIDE_EFFECT_LOAD_ELIM") == "2";
    static const bool insert_tensormove_default =
      common::GetEnv("MS_DEV_SIDE_EFFECT_LOAD_ELIM") == "" || common::GetEnv("MS_DEV_SIDE_EFFECT_LOAD_ELIM") == "1";
    for (auto &refkey_load : loads_map1) {
      auto &loads = refkey_load.second;
      if (loads.size() > 1) {
        // Insert TensorMove strictly, need check the insert conditions for TensorMove.
        if (strict_insert_tensormove) {
          auto ref_key = refkey_load.first;
          bool need_insert = std::any_of(loads.begin(), loads.end(),
                                         [&](const AnfNodePtr &load) { return ReCheckUsersOfLoad(load, ref_key); });
          if (need_insert) {
            for (const auto &load : loads) {
              (void)need_insert_loads.insert(load);
            }
          }
        } else if (insert_tensormove_default) {
          // If not set MS_DEV_SIDE_EFFECT_LOAD_ELIM or set to 1, TensorMove is inserted by default.
          for (const auto &load : loads) {
            (void)need_insert_loads.insert(load);
          }
        }
      }
    }
    AppendLoads(loads_map2, &need_insert_loads);
    AppendLoads(loads_map3, &need_insert_loads);
    AppendLoads(loads_map4, &need_insert_loads);
    // Add call node will output is a AbstractRefTensor and ref_key is kValueAny.
    for (const auto &call_node : call_nodes) {
      if (std::find(need_insert_loads.begin(), need_insert_loads.end(), call_node) == need_insert_loads.end()) {
        (void)need_insert_loads.insert(call_node);
      }
    }
    return need_insert_loads;
  }

  bool CheckLoadInput(const AnfNodePtr &input) const {
    return IsPrimitiveCNode(input, prim::kPrimCall) || IsPrimitiveCNode(input, prim::kPrimPartial) ||
           (input->isa<CNode>() && (IsValueNode<FuncGraph>(input->cast<CNodePtr>()->input(0)) ||
                                    IsPrimitiveCNode(input->cast<CNodePtr>()->input(0), prim::kPrimSwitch) ||
                                    IsPrimitiveCNode(input->cast<CNodePtr>()->input(0), prim::kPrimSwitchLayer)));
  }

  void ProcessReturnLoad(const AnfNodePtr &node, const RefLoads &refkey_loads, RefLoads *refkey_loads_return_is_load) {
    MS_EXCEPTION_IF_NULL(node->cast<CNodePtr>());
    auto return_input = node->cast<CNodePtr>()->input(1);
    while (IsPrimitiveCNode(return_input, prim::kPrimDepend)) {
      return_input = return_input->cast<CNodePtr>()->input(1);
    }
    auto check_load = [this, &refkey_loads, &refkey_loads_return_is_load](const AnfNodePtr &inp_node) {
      auto load = inp_node->cast<CNodePtr>();
      auto refkey = GetRefKey(load->input(1));
      if (refkey == "") {
        MS_LOG(INFO) << "Load without ref key:" << load->DebugString();
        return;
      }
      (void)(*refkey_loads_return_is_load)[refkey].emplace_back(load);
    };
    if (IsPrimitiveCNode(return_input, prim::kPrimMakeTuple)) {
      const auto &make_tuple = return_input->cast<CNodePtr>();
      if (make_tuple->size() <= 1) {
        return;
      }
      for (size_t i = 1; i < make_tuple->size(); ++i) {
        if (IsPrimitiveCNode(make_tuple->input(i), prim::kPrimLoad)) {
          check_load(make_tuple->input(i));
        }
      }
    } else if (IsPrimitiveCNode(return_input, prim::kPrimLoad)) {
      check_load(return_input);
    }
  }

  mindspore::HashSet<CNodePtr> GetNeedInsertLoads() {
    auto check_nodes = TopoSort(func_graph_->get_return());
    static const bool enable_all_load = common::GetEnv("MS_DEV_SIDE_EFFECT_LOAD_ELIM") == "0";
    // Insert TensorMove for all Load nodes
    if (enable_all_load) {
      return GetAllLoads(check_nodes);
    }
    RefLoads refkey_loads;
    RefLoads refkey_loads_in_call_or_partial;
    RefLoads refkey_loads_input_is_call_or_partial;
    RefLoads refkey_loads_return_is_load;
    std::set<CNodePtr> ref_call_nodes;
    for (auto &node : check_nodes) {
      // Record load refkey
      if (IsPrimitiveCNode(node, prim::kPrimLoad)) {
        auto load = node->cast<CNodePtr>();
        auto input = load->input(1);
        if (CheckLoadInput(input)) {
          (void)ref_call_nodes.insert(load);
        }
        auto refkey = GetRefKey(input);
        if (refkey == "") {
          MS_LOG(INFO) << "Load without ref key:" << load->DebugString();
          continue;
        }
        (void)refkey_loads[refkey].emplace_back(load);
        while (IsPrimitiveCNode(input, prim::kPrimDepend)) {
          input = input->cast<CNodePtr>()->input(1);
        }
        // If Load(call/partial, monad), we should insert TensorMove for the load node.
        if (CheckLoadInput(input)) {
          (void)refkey_loads_input_is_call_or_partial[refkey].emplace_back(load);
        }
      }

      // Check if the return node is a load.
      if (IsPrimitiveCNode(node, prim::kPrimReturn)) {
        ProcessReturnLoad(node, refkey_loads, &refkey_loads_return_is_load);
      }

      // Find special load which is in call or partial
      if (!IsPrimitiveCNode(node, prim::kPrimCall) && !IsPrimitiveCNode(node, prim::kPrimPartial) &&
          !(node->isa<CNode>() && IsValueNode<FuncGraph>(node->cast<CNodePtr>()->input(0)))) {
        continue;
      }
      auto cnode = node->cast<CNodePtr>();
      for (size_t index = 1; index < cnode->size(); ++index) {
        auto input = cnode->input(index);
        if (IsPrimitiveCNode(input, prim::kPrimLoad)) {
          auto load = input->cast<CNodePtr>();
          auto refkey = GetRefKey(load->input(1));
          if (refkey == "") {
            MS_LOG(INFO) << "Load without ref key:" << load->DebugString();
            continue;
          }
          (void)refkey_loads_in_call_or_partial[refkey].emplace_back(load);
        }
      }
    }
    return GetSpecialLoads(refkey_loads, refkey_loads_in_call_or_partial, refkey_loads_input_is_call_or_partial,
                           refkey_loads_return_is_load, ref_call_nodes);
  }

  void InsertTensorMoveForLoad(const CNodePtr &node) {
    if (!IsPrimitiveCNode(node, prim::kPrimLoad)) {
      return;
    }
    auto prim = std::make_shared<Primitive>(kTensorMoveOpName);
    std::vector<AnfNodePtr> new_inputs{NewValueNode(prim)};
    (void)new_inputs.emplace_back(node);
    auto real_load = func_graph_->NewCNode(new_inputs);
    auto load_abs = node->abstract();
    auto abs_ref = dyn_cast_ptr<abstract::AbstractRefTensor>(load_abs);
    if (abs_ref != nullptr) {
      real_load->set_abstract(abs_ref->CloneAsTensor());
    } else {
      real_load->set_abstract(load_abs);
    }
    MS_LOG(DEBUG) << "Insert TensorMove " << real_load->DebugString() << " for load " << node->DebugString();
    (void)manager_->Replace(node, real_load);
  }

  const FuncGraphPtr &func_graph_;
  FuncGraphManagerPtr manager_;
  mindspore::HashMap<AnfNodePtr, size_t> topo_sort_map_;
  // As of now it's no requirement for insertion order, so use the unordered set.
  mindspore::HashSet<AnfNodePtr> processed_nodes_;
};
}  // namespace

// Enforce order of execution for Load users node.
void OrderEnforce(const FuncGraphPtr &func_graph) {
  MS_EXCEPTION_IF_NULL(func_graph);
  OrderEnforcer enforcer(func_graph);
  enforcer.Run();
  auto fg_used_total = func_graph->func_graphs_used_total();
  for (const auto &fg : fg_used_total) {
    OrderEnforcer fg_enforcer(fg);
    fg_enforcer.Run();
  }
}
}  // namespace mindspore::pipeline
