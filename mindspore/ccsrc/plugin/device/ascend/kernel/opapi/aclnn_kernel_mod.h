/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_ACLNN_KERNEL_MOD_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_ACLNN_KERNEL_MOD_H_
#include <vector>
#include <memory>
#include <map>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include "ops/base_operator.h"
#include "ops/op_def.h"
#include "kernel/kernel.h"
#include "plugin/factory/ms_factory.h"
#include "include/common/utils/utils.h"
#include "runtime/pynative/op_runtime_info.h"
#include "transform/acl_ir/acl_convert.h"
#include "transform/acl_ir/op_api_exec.h"
#include "transform/acl_ir/op_api_util.h"

namespace mindspore {
namespace kernel {
using aclTensor = transform::aclTensor;
using aclOpExecutor = transform::aclOpExecutor;
using CallBackFunc = std::function<void()>;
using OpApiUtil = transform::OpApiUtil;
using AclUtil = transform::AclUtil;

#define DEFINE_GET_WORKSPACE_FOR_RESIZE()                                                                        \
  template <typename... Args>                                                                                    \
  void GetWorkspaceForResize(const Args &... args) {                                                             \
    hash_id_ = transform::CalcOpApiHash(op_type_, args...);                                                      \
    if (cache_hash_.count(hash_id_) == 0) {                                                                      \
      const bool use_huge_pages = false;                                                                         \
      auto return_value = GEN_EXECUTOR_CUST(op_type_, use_huge_pages, args...);                                  \
      UpdateWorkspace(return_value);                                                                             \
    } else {                                                                                                     \
      auto return_value = GEN_EXECUTOR_BOOST(op_type_, hash_id_, args...);                                       \
      UpdateWorkspace(return_value);                                                                             \
    }                                                                                                            \
  }                                                                                                              \
                                                                                                                 \
  void RunOp(void *stream_ptr, const std::vector<KernelTensor *> &workspace) {                                   \
    if (workspace_size_list_.empty()) {                                                                          \
      RUN_OP_API_ASYNC(op_type_, nullptr, 0, executor_, stream_ptr, release_func_);                              \
    } else {                                                                                                     \
      if (workspace.empty()) {                                                                                   \
        MS_LOG(EXCEPTION) << "Failed to allocate workspace tensor!";                                             \
      }                                                                                                          \
      auto workspace_tensor = workspace[0];                                                                      \
      if (workspace_tensor->size() != workspace_size_list_[0]) {                                                 \
        MS_LOG(EXCEPTION) << "Please check 'GetWorkSpaceInfo' and 'Launch' func. Expected workspace size is"     \
                          << workspace_size_list_[0] << ", but get " << workspace_tensor->size();                \
      }                                                                                                          \
      RUN_OP_API_ASYNC(op_type_, workspace_tensor->device_ptr(), workspace_size_list_[0], executor_, stream_ptr, \
                       release_func_);                                                                           \
    }                                                                                                            \
  }                                                                                                              \
                                                                                                                 \
  void RunOpSync(void *stream_ptr, const std::vector<KernelTensor *> &workspace) {                               \
    if (workspace_size_list_.empty()) {                                                                          \
      RUN_OP_API_SYNC(op_type_, nullptr, 0, executor_, stream_ptr);                                              \
    } else {                                                                                                     \
      if (workspace.empty()) {                                                                                   \
        MS_LOG(EXCEPTION) << "Failed to allocate workspace tensor!";                                             \
      }                                                                                                          \
      const auto &workspace_tensor = workspace[0];                                                               \
      if (workspace_tensor->size() != workspace_size_list_[0]) {                                                 \
        MS_LOG(EXCEPTION) << "Please check 'GetWorkSpaceInfo' and 'Launch' func. Expected workspace size is"     \
                          << workspace_size_list_[0] << ", but get " << workspace_tensor->size();                \
      }                                                                                                          \
      RUN_OP_API_SYNC(op_type_, workspace_tensor->device_ptr(), workspace_size_list_[0], executor_, stream_ptr); \
    }                                                                                                            \
  }

class EmptyKernelTensor {
 public:
  EmptyKernelTensor() { tensor_ = new KernelTensor(); }
  EmptyKernelTensor(TypeId type_id, TypeId dtype_id) {
    if (type_id == kObjectTypeTensorType) {
      tensor_ = new KernelTensor();
      auto tensor_shape = std::make_shared<abstract::TensorShape>();
      tensor_shape->SetShapeVector({0});
      tensor_->SetType(std::make_shared<TensorType>(TypeIdToType(dtype_id)));
      tensor_->SetShape(tensor_shape);
    }
  }
  ~EmptyKernelTensor() { delete tensor_; }
  KernelTensor *get() const { return tensor_; }

 private:
  KernelTensor *tensor_;
};

class AclnnKernelMod : public KernelMod {
 public:
  explicit AclnnKernelMod(std::string &&op_type) : op_type_(std::move(op_type)) {}
  ~AclnnKernelMod() = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);

  virtual void GetWorkSpaceInfo(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  }
  virtual bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                      const std::vector<KernelTensor *> &outputs, void *stream_ptr);

  void ResetDeivceAddress(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {}

  bool IsNeedUpdateOutputShapeAndSize() override { return false; }
  std::vector<KernelAttr> GetOpSupport() override { MS_LOG(EXCEPTION) << "This interface is not support in aclnn."; }

  template <typename... Args>
  void UpdateWorkspace(const std::tuple<Args...> &args) {
    auto real_workspace_size = static_cast<size_t>(std::get<0>(args));
    if (real_workspace_size != 0) {
      std::vector<size_t> workspace_size_list = {real_workspace_size};
      SetWorkspaceSizeList(workspace_size_list);
    }

    constexpr size_t kBoostGeneratorSize = 5;
    if constexpr (std::tuple_size_v<std::tuple<Args...>> == kBoostGeneratorSize) {
      constexpr size_t kHashIdIndex = 3;
      hash_id_ = std::get<kHashIdIndex>(args);
    }
  }

  template <typename... Args>
  void ParseGenExecutor(const std::tuple<Args...> &args) {
    executor_ = std::get<1>(args);
    if (executor_ == nullptr) {
      MS_LOG(INTERNAL_EXCEPTION) << "Please check op api's generate!";
    }
    release_func_ = std::get<2>(args);

    constexpr size_t kBoostGeneratorSize = 5;
    if constexpr (std::tuple_size_v<std::tuple<Args...>> == kBoostGeneratorSize) {
      constexpr size_t kHashIdIndex = 3;
      hash_id_ = std::get<kHashIdIndex>(args);
      if (cache_hash_.count(hash_id_) != 0) {
        return;
      }
      constexpr size_t kHitIndex = 4;
      if (std::get<kHitIndex>(args)) {
        cache_hash_.insert(hash_id_);
      }
    }
  }

 protected:
  template <size_t N, std::size_t... Is>
  auto GetTupleFrontImpl(const std::vector<KernelTensor *> &vecs, std::index_sequence<Is...>) {
    return std::make_tuple(vecs[Is]...);
  }

  template <size_t N>
  auto GetTupleFront(const std::vector<KernelTensor *> &vecs) {
    return GetTupleFrontImpl<N>(vecs, std::make_index_sequence<N>());
  }

  template <typename T, typename... Vecs>
  std::vector<T> ConcatVecs(const std::vector<T> &vec, const Vecs &... vecs) {
    std::vector<T> result = vec;
    (result.insert(result.end(), vecs.begin(), vecs.end()), ...);
    return result;
  }

  template <typename T, typename... Vecs>
  std::vector<T> ConcatVecs(const Vecs &... vecs) {
    static_assert((std::is_same_v<T, typename Vecs::value_type> && ...), "All vectors must have the same type!");
    std::vector<T> result;
    (result.insert(result.end(), vecs.begin(), vecs.end()), ...);
    return result;
  }

  template <size_t N, typename... Ts>
  auto GetKernelTuple(const std::vector<Ts> &... vecs) {
    const auto &new_vec = ConcatVecs(vecs...);
    if (new_vec.size() != N) {
      MS_LOG(EXCEPTION) << op_type_ << "'s config op input and output's size must be same, but get " << N << " with "
                        << new_vec.size();
    }
    const auto &result = GetTupleFront<N>(new_vec);
    return result;
  }

  aclOpExecutor *executor_{nullptr};
  CallBackFunc release_func_{nullptr};
  std::string op_type_;
  uint64_t hash_id_{0};
  std::unordered_set<uint64_t> cache_hash_;
};

using AclnnKernelModPtr = std::shared_ptr<AclnnKernelMod>;
using AclnnKernelModPtrList = std::vector<AclnnKernelModPtr>;

#define REGISTER_ACLNN_CLASS(TYPE)                                                                                 \
  template <size_t N>                                                                                              \
  class Aclnn##TYPE##KernelMod : public AclnnKernelMod {                                                           \
   public:                                                                                                         \
    explicit Aclnn##TYPE##KernelMod(std::string &&op_type) : AclnnKernelMod(std::move(op_type)) {}                 \
    ~Aclnn##TYPE##KernelMod() = default;                                                                           \
    void GetWorkSpaceInfo(const std::vector<KernelTensor *> &inputs,                                               \
                          const std::vector<KernelTensor *> &outputs) override {                                   \
      const auto &res_tuple = this->GetKernelTuple<N>(inputs, outputs);                                            \
      std::apply(                                                                                                  \
        [this](const auto &... args) {                                                                             \
          hash_id_ = transform::CalcOpApiHash(op_type_, args...);                                                  \
          if (cache_hash_.count(hash_id_) == 0) {                                                                  \
            const bool use_huge_pages = false;                                                                     \
            auto return_value = GEN_EXECUTOR_CUST(op_type_, use_huge_pages, args...);                              \
            UpdateWorkspace(return_value);                                                                         \
          } else {                                                                                                 \
            auto return_value = GEN_EXECUTOR_BOOST(op_type_, hash_id_, args...);                                   \
            UpdateWorkspace(return_value);                                                                         \
          }                                                                                                        \
        },                                                                                                         \
        res_tuple);                                                                                                \
    }                                                                                                              \
    bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,           \
                const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {                           \
      this->ParseGenExecutor(GenExecutor(inputs, outputs));                                                        \
      RunOp(stream_ptr, workspace);                                                                                \
      return true;                                                                                                 \
    }                                                                                                              \
                                                                                                                   \
   private:                                                                                                        \
    template <typename... Ts>                                                                                      \
    auto GenExecutor(const std::vector<Ts> &... vecs) {                                                            \
      const auto &op_type = this->op_type_;                                                                        \
      const auto &hash_id = this->hash_id_;                                                                        \
      const auto &res_tuple = this->GetKernelTuple<N>(vecs...);                                                    \
      auto executor_info = std::apply(                                                                             \
        [&op_type, &hash_id](const auto &... args) { return GEN_EXECUTOR_BOOST(op_type, hash_id, args...); },      \
        res_tuple);                                                                                                \
      return executor_info;                                                                                        \
    }                                                                                                              \
                                                                                                                   \
    void RunOp(void *stream_ptr, const std::vector<KernelTensor *> &workspace) {                                   \
      if (workspace_size_list_.empty()) {                                                                          \
        RUN_OP_API_ASYNC(op_type_, nullptr, 0, executor_, stream_ptr, release_func_);                              \
      } else {                                                                                                     \
        if (workspace.empty()) {                                                                                   \
          MS_LOG(EXCEPTION) << "Failed to allocate workspace tensor!";                                             \
        }                                                                                                          \
        auto workspace_tensor = workspace[0];                                                                      \
        if (workspace_tensor->size() != workspace_size_list_[0]) {                                                 \
          MS_LOG(EXCEPTION) << "Please check 'GetWorkSpaceInfo' and 'Launch' func. Expected workspace size is"     \
                            << workspace_size_list_[0] << ", but get " << workspace_tensor->size();                \
        }                                                                                                          \
        RUN_OP_API_ASYNC(op_type_, workspace_tensor->device_ptr(), workspace_size_list_[0], executor_, stream_ptr, \
                         release_func_);                                                                           \
      }                                                                                                            \
    }                                                                                                              \
  };

#define MS_ACLNN_KERNEL_FACTORY_REG(NAME, DERIVE_CLASS) MS_KERNEL_FACTORY_REG(AclnnKernelMod, NAME, DERIVE_CLASS)
#define MS_ACLNN_COMMON_KERNEL_FACTORY_REG(NAME, TYPE, N)                     \
  REGISTER_ACLNN_CLASS(NAME)                                                  \
  static const KernelRegistrar<AclnnKernelMod> g_##NAME##_AclnnKernelMod_reg( \
    #NAME, []() { return std::make_shared<Aclnn##NAME##KernelMod<N>>(#TYPE); });
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_ACLNN_KERNEL_MOD_H_
