/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#include "utils/trace_base.h"

#include <vector>
#include <string>
#include <utility>
#include <algorithm>

#include "ir/graph_utils.h"

namespace mindspore {
namespace trace {
namespace {
std::vector<DebugInfoPtr> GetSourceCodeDebugInfoVec(DebugInfoPtr debug_info, bool is_debug = false) {
  std::vector<DebugInfoPtr> debug_with_loc_vec;
  HashSet<DebugInfoPtr> visited;
  while (debug_info != nullptr) {
    if (visited.find(debug_info) != visited.end()) {
      int i = 0;
      for (const auto &info : debug_with_loc_vec) {
        auto loc = info->location();
        MS_LOG(ERROR) << "[" << std::to_string(i) << "]:" << info.get()
                      << ", loc:" << (loc == nullptr ? "null" : loc->ToString());
        ++i;
      }
      auto loc = debug_info->location();
      MS_LOG(INTERNAL_EXCEPTION) << "Find loop debug info: " << debug_info.get()
                                 << ", loc:" << (loc == nullptr ? "null" : loc->ToString()) << ".\n"
                                 << "Please set 'compile_config.ENABLE_FIX_CODE_LINE=0' to avoid this problem.";
    }
    auto loc = debug_info->location();
    MS_LOG(DEBUG) << "Visited Insert debug info: " << debug_info.get()
                  << ", loc:" << (loc == nullptr ? "null" : loc->ToString());
    (void)visited.insert(debug_info);
    if (is_debug || debug_info->location() != nullptr) {
      debug_with_loc_vec.push_back(debug_info);
      MS_LOG(DEBUG) << "debug loc: " << debug_info->location()->DebugString();
    }
    if (debug_info->trace_info() != nullptr) {
      MS_LOG(DEBUG) << "trace: " << debug_info->trace_info()->name();
      debug_info = debug_info->trace_info()->debug_info();
    } else {
      break;
    }
  }
  return debug_with_loc_vec;
}

void ReplaceLinefeed(std::string *txt) {
  MS_EXCEPTION_IF_NULL(txt);
  std::vector<int> erases;
  constexpr char cr = '\r';
  constexpr char lf = '\n';
  constexpr char slash = '/';
  for (size_t i = 0; i < txt->length(); i++) {
    if ((*txt)[i] == lf) {
      (*txt)[i] = slash;
    }
    if ((*txt)[i] == cr) {
      (*txt)[i] = slash;
      if (i + 1 < txt->length() && (*txt)[i + 1] == lf) {
        erases.emplace_back(i + 1);
        i++;
      }
    }
  }
  constexpr auto n = 1;
  std::reverse(erases.begin(), erases.end());
  for (const auto &index : erases) {
    txt->erase(index, n);
  }
}
}  // namespace

DebugInfoPtr GetSourceCodeDebugInfo(const DebugInfoPtr &info) {
  auto debug_with_loc_vec = GetSourceCodeDebugInfoVec(info);
  if (!debug_with_loc_vec.empty()) {
    return debug_with_loc_vec[0];
  } else {
    return info;
  }
}

std::string GetDebugInfoStr(const DebugInfoPtr &info, const std::string &prefix, SourceLineTip tip) {
  if (info == nullptr) {
    return "";
  }
  const auto &src_info = GetSourceCodeDebugInfo(info);
  if (src_info->location() == nullptr) {
    return "";
  }
  auto line_str = src_info->location()->ToString(tip);
  if (tip == kSourceLineTipDiscard) {
    ReplaceLinefeed(&line_str);
  }
  std::stringstream ss;
  ss << prefix << line_str;
  return ss.str();
}

std::string DumpSourceLines(const AnfNodePtr &node, bool has_title) {
  auto vec_source = GetSourceLineList(node);
  if (vec_source.empty()) {
    return "";
  }
  std::ostringstream oss;
  for (auto &src_info : vec_source) {
    oss << src_info;
  }
  if (oss.str().empty()) {
    return "";
  }
  const std::string prefix = has_title ? "#dmsg#The Function Call Stack:#dmsg#" : "\n";
  return prefix + oss.str();
}

std::string DumpSourceLines(AnfNode *node, bool has_title) {
  if (node == nullptr) {
    MS_LOG(WARNING) << "Node is null";
    return "";
  }
  AnfNodePtr ptr = std::static_pointer_cast<AnfNode>(node->shared_from_this());
  return DumpSourceLines(ptr, has_title);
}

void GetSourceLineFromDebugInfo(const DebugInfoPtr &debug_info, std::vector<std::string> *result,
                                const std::string &prefix = "") {
  MS_EXCEPTION_IF_NULL(result);
  auto info_vec = GetSourceCodeDebugInfoVec(debug_info);
  const std::string spaces(prefix.size(), ' ');
  bool first_line = true;
  HashSet<std::string> exist_locations;
  for (const auto &info : info_vec) {
    MS_EXCEPTION_IF_NULL(info);
    auto loc = info->location();
    if (loc == nullptr) {
      continue;
    }
    auto loc_str = loc->ToString(kSourceLineTipDiscard);
    if (exist_locations.find(loc_str) != exist_locations.cend()) {
      continue;
    }
    (void)exist_locations.insert(loc_str);
    ReplaceLinefeed(&loc_str);
    if (first_line) {
      result->push_back(std::string(prefix).append(loc_str).append("\n"));
      first_line = false;
    } else {
      result->push_back(std::string(spaces).append(loc_str).append("\n"));
    }
  }
}

void GetFusedDebugInfos(const NodeDebugInfoSet &fused_debug_infos, std::vector<std::string> *result) {
  MS_EXCEPTION_IF_NULL(result);
  (void)result->emplace_back("Corresponding code candidate:\n");
  // Flag to mark whether fused_debug_infos has valid print.
  bool is_empty = true;
  for (const auto &debug_info : fused_debug_infos) {
    std::vector<std::string> debug_info_vec_str;
    GetSourceLineFromDebugInfo(debug_info, &debug_info_vec_str, kSectionPrefix);
    if (!debug_info_vec_str.empty()) {
      (void)result->insert(result->cend(), debug_info_vec_str.cbegin(), debug_info_vec_str.cend());
      is_empty = false;
    }
  }

  if (is_empty) {
    result->pop_back();
  }
}

void GetPrimalDebugInfos(const CNodePtr &cnode, std::vector<std::string> *result) {
  MS_EXCEPTION_IF_NULL(cnode);
  MS_EXCEPTION_IF_NULL(result);
  auto primal_debug_infos = cnode->primal_debug_infos();
  if (!primal_debug_infos.empty()) {
    (void)result->emplace_back("Corresponding forward node candidate:\n");
    for (const auto &primal_debug_info : primal_debug_infos) {
      std::vector<std::string> debug_info_vec_str;
      GetSourceLineFromDebugInfo(primal_debug_info, &debug_info_vec_str, kSectionPrefix);
      if (!debug_info_vec_str.empty()) {
        (void)result->insert(result->cend(), debug_info_vec_str.cbegin(), debug_info_vec_str.cend());
      }
    }
  }
}
std::vector<std::string> GetSourceLineList(const DebugInfoPtr &debug_info) {
  std::vector<std::string> result;
  GetSourceLineFromDebugInfo(debug_info, &result);
  return result;
}

std::vector<std::string> GetSourceLineList(const AnfNodePtr &node) {
  std::vector<std::string> result;
  if (node == nullptr) {
    MS_LOG(WARNING) << "Node is null";
    return result;
  }
  if (!node->isa<CNode>()) {
    GetSourceLineFromDebugInfo(node->debug_info(), &result);
    return result;
  }
  auto cnode = node->cast<CNodePtr>();
  auto fused_debug_infos = cnode->fused_debug_infos();
  if (fused_debug_infos.empty()) {
    GetSourceLineFromDebugInfo(node->debug_info(), &result);
  } else {
    GetFusedDebugInfos(fused_debug_infos, &result);
  }
  GetPrimalDebugInfos(cnode, &result);
  return result;
}

std::vector<LocationPtr> GetSourceLocationList(const AnfNodePtr &node) {
  std::vector<LocationPtr> result;
  if (node == nullptr) {
    MS_LOG(WARNING) << "Node is null";
    return result;
  }
  auto info_vec = GetSourceCodeDebugInfoVec(node->debug_info());
  for (const auto &info : info_vec) {
    MS_EXCEPTION_IF_NULL(info);
    if (info->location() != nullptr) {
      result.emplace_back(info->location());
    }
  }
  return result;
}

std::string GetTracedDebugInfoStr(const DebugInfoPtr &debug_info, bool is_debug) {
  if (debug_info == nullptr) {
    MS_LOG(WARNING) << "debug_info is null";
    return "";
  }
  auto info_vec = GetSourceCodeDebugInfoVec(debug_info, is_debug);
  std::ostringstream oss;
  for (auto iter = info_vec.crbegin(); iter != info_vec.crend(); ++iter) {
    const auto &info = *iter;
    MS_EXCEPTION_IF_NULL(info);
    auto loc = info->location();
    if (loc == nullptr) {
      oss << "Location miss\n";
      continue;
    }
    oss << "# " << loc->ToString() << "\n";
  }
  return oss.str();
}
}  // namespace trace
}  // namespace mindspore
