/**
 * This is the C++ adaptation and derivative work of Myia (https://github.com/mila-iqia/myia/).
 *
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_PIPELINE_JIT_PARSE_DATA_CONVERTER_H_
#define MINDSPORE_CCSRC_PIPELINE_JIT_PARSE_DATA_CONVERTER_H_

#include <deque>
#include <memory>
#include <utility>
#include <vector>
#include <string>
#include "utils/ordered_map.h"
#include "utils/hash_map.h"
#include "pipeline/jit/ps/parse/parse_base.h"
#include "include/common/utils/python_adapter.h"
#include "utils/log_adapter.h"
#include "ops/op_def.h"

namespace mindspore {
namespace parse {
// data convert for parse
namespace data_converter {
void CacheObjectValue(const std::string &obj_key, const ValuePtr &data);
bool GetObjectValue(const std::string &obj_key, ValuePtr *const data);

void SetObjGraphValue(const std::string &obj_key, const FuncGraphPtr &data);

const mindspore::OrderedMap<std::string, std::vector<FuncGraphPtr>> &GetObjGraphs();

std::vector<std::string> GetObjKey(const py::object &obj);
ResolveType GetObjType(const py::object &obj);
ClassInstanceType GetClassInstanceType(const py::object &obj);

bool IsCellInstance(const py::object &obj);
bool IsNumpyArrayInstance(const py::object &obj);
bool IsMsClassInstance(const py::object &obj);
bool IsJITForbiddenAPI(const py::object &obj);
bool IsClassType(const py::object &obj);
py::object CreatePythonObject(const py::object &type, const py::tuple &args_kwargs);
py::object CallPythonScript(const py::object &script, const py::tuple &args_kwargs);
py::set GetPythonScriptIdAttrs(const py::object &script);
void MakeProperNameToFuncGraph(const FuncGraphPtr &func_graph, std::string name);
ValuePtr PyDataToValue(const py::object &obj);
ValuePtr PyDataToStubNode(const py::object &obj);
void ClearObjectCache();
}  // namespace data_converter

class DataConverter {
 public:
  DataConverter(ValuePtrList args_value_list, bool use_signature)
      : args_value_list_(std::move(args_value_list)),
        use_signature_(use_signature),
        dtype_(nullptr),
        forbid_reuse_(false) {}

  virtual ~DataConverter() = default;

  ValuePtr ConvertData(const py::object &obj);

 private:
  ValuePtrList args_value_list_;
  bool use_signature_;
  TypePtr dtype_;
  bool forbid_reuse_;
};

FuncGraphPtr ConvertToBpropCut(const py::object &obj);
constexpr int32_t kTypeShiftBits = 16;
constexpr auto kDstMask = (1 << kTypeShiftBits) - 1;
inline int32_t CombineTypesForTypeCast(const mindspore::ops::OP_DTYPE &src, const mindspore::ops::OP_DTYPE &dst) {
  return (static_cast<int32_t>(src) << kTypeShiftBits) | static_cast<int32_t>(dst);
}
// using OpDefConvertFunc = std::function<ValuePtr(const py::object &obj)>;
typedef ValuePtr (*OpDefConvertFunc)(const py::object &);
OpDefConvertFunc GetConverterByType(int32_t dtype);
ValuePtr ConvertTensor(const py::object &obj);
template <typename TS, typename TD, OpDefConvertFunc func>
ValuePtr ConvertSequence(const py::object &obj);
tensor::TensorPtr ConvertTensorValue(const py::object &obj);
}  // namespace parse
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PIPELINE_JIT_PARSE_DATA_CONVERTER_H_
