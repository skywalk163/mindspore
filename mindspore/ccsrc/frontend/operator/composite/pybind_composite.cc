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
#include "frontend/operator/composite/composite.h"
#include "include/common/pybind_api/api_register.h"
#include "frontend/operator/composite/list_operation.h"
#include "frontend/operator/composite/dict_operation.h"
#include "frontend/operator/composite/map.h"
#include "frontend/operator/composite/unpack_call.h"
#include "frontend/operator/composite/vmap.h"
#include "frontend/operator/composite/multitype_funcgraph.h"
#include "frontend/operator/composite/zip_operation.h"
#include "frontend/operator/composite/tensor_index.h"
#include "frontend/operator/composite/starred_operation.h"
namespace mindspore {
namespace prim {
void RegCompositeOpsGroup(const py::module *m) {
  //  Reg HyperMap
  (void)py::class_<HyperMapPy, MetaFuncGraph, std::shared_ptr<HyperMapPy>>(*m, "HyperMap_")
    .def(py::init<bool, py::object>(), py::arg("reverse"), py::arg("ops"))
    .def(py::init<bool>(), py::arg("reverse"));

  // Reg Tail
  (void)py::class_<Tail, MetaFuncGraph, std::shared_ptr<Tail>>(*m, "Tail_").def(py::init<std::string &>());

  // Reg GradOperation
  (void)py::class_<GradOperation, MetaFuncGraph, std::shared_ptr<GradOperation>>(*m, "GradOperation_")
    .def(py::init<std::string &>(), py::arg("fn"))
    .def(py::init<std::string &, bool, bool, bool, bool, bool, bool, bool, bool>(), py::arg("fn"), py::arg("get_all"),
         py::arg("get_by_list"), py::arg("sens_param"), py::arg("get_by_position"), py::arg("has_aux"),
         py::arg("get_value"), py::arg("return_ids"), py::arg("merge_forward"));

  // Reg VmapOperation
  (void)py::class_<VmapOperation, MetaFuncGraph, std::shared_ptr<VmapOperation>>(*m, "VmapOperation_")
    .def(py::init<const std::string &>(), py::arg("fn"));

  // Reg VmapGeneralRulePyAdapter
  (void)py::class_<VmapGeneralRulePyAdapter, MetaFuncGraph, std::shared_ptr<VmapGeneralRulePyAdapter>>(
    *m, "VmapGeneralRulePyAdapter_")
    .def(py::init<const std::string &, const PrimitivePyAdapterPtr &, int64_t>(), py::arg("fn"), py::arg("prim"),
         py::arg("axis_size"));

  // Reg TaylorOperation
  (void)py::class_<TaylorOperation, MetaFuncGraph, std::shared_ptr<TaylorOperation>>(*m, "TaylorOperation_")
    .def(py::init<const std::string &>(), py::arg("fn"));

  // Reg TupleAdd
  (void)py::class_<TupleAdd, MetaFuncGraph, std::shared_ptr<TupleAdd>>(*m, "TupleAdd_").def(py::init<std::string &>());

  // Reg ListAdd
  (void)py::class_<ListAdd, MetaFuncGraph, std::shared_ptr<ListAdd>>(*m, "ListAdd_").def(py::init<std::string &>());

  // Reg TupleGetItemTensor
  (void)py::class_<TupleGetItemTensor, MetaFuncGraph, std::shared_ptr<TupleGetItemTensor>>(*m, "TupleGetItemTensor_")
    .def(py::init<std::string &>());

  // Reg ListSliceSetItem
  (void)py::class_<ListSliceSetItem, MetaFuncGraph, std::shared_ptr<ListSliceSetItem>>(*m, "ListSliceSetItem_")
    .def(py::init<const std::string &>());

  // Reg SequenceSliceGetItem
  (void)py::class_<SequenceSliceGetItem, MetaFuncGraph, std::shared_ptr<SequenceSliceGetItem>>(*m,
                                                                                               "SequenceSliceGetItem_")
    .def(py::init<std::string &, std::string &, std::string &>());

  // Reg ZerosLike
  (void)py::class_<ZerosLike, MetaFuncGraph, std::shared_ptr<ZerosLike>>(*m, "ZerosLike_")
    .def(py::init<const std::string &, std::shared_ptr<MultitypeFuncGraph>>());

  // Reg Shard
  (void)py::class_<Shard, MetaFuncGraph, std::shared_ptr<Shard>>(*m, "Shard_")
    .def(py::init<const std::string &>(), py::arg("fn"));

  // Reg ListAppend
  (void)py::class_<ListAppend, MetaFuncGraph, std::shared_ptr<ListAppend>>(*m, "ListAppend_")
    .def(py::init<std::string &>());

  // Reg ListInsert
  (void)py::class_<ListInsert, MetaFuncGraph, std::shared_ptr<ListInsert>>(*m, "ListInsert_")
    .def(py::init<const std::string &>());

  // Reg ListPop
  (void)py::class_<ListPop, MetaFuncGraph, std::shared_ptr<ListPop>>(*m, "ListPop_")
    .def(py::init<const std::string &>());

  // Reg ListClear
  (void)py::class_<ListClear, MetaFuncGraph, std::shared_ptr<ListClear>>(*m, "ListClear_")
    .def(py::init<const std::string &>());

  // Reg ListReverse
  (void)py::class_<ListReverse, MetaFuncGraph, std::shared_ptr<ListReverse>>(*m, "ListReverse_")
    .def(py::init<const std::string &>());

  // Reg ListExtend
  (void)py::class_<ListExtend, MetaFuncGraph, std::shared_ptr<ListExtend>>(*m, "ListExtend_")
    .def(py::init<const std::string &>());

  // Reg DictSetItem
  (void)py::class_<DictSetItem, MetaFuncGraph, std::shared_ptr<DictSetItem>>(*m, "DictSetItem_")
    .def(py::init<const std::string &>());

  // Reg DictClear
  (void)py::class_<DictClear, MetaFuncGraph, std::shared_ptr<DictClear>>(*m, "DictClear_")
    .def(py::init<const std::string &>());

  // Reg DictHasKey
  (void)py::class_<DictHasKey, MetaFuncGraph, std::shared_ptr<DictHasKey>>(*m, "DictHasKey_")
    .def(py::init<const std::string &>());

  // Reg DictUpdate
  (void)py::class_<DictUpdate, MetaFuncGraph, std::shared_ptr<DictUpdate>>(*m, "DictUpdate_")
    .def(py::init<const std::string &>());

  // Reg DictFromKeys
  (void)py::class_<DictFromKeys, MetaFuncGraph, std::shared_ptr<DictFromKeys>>(*m, "DictFromKeys_")
    .def(py::init<const std::string &>());

  // Reg MapPy
  (void)py::class_<MapPy, MetaFuncGraph, std::shared_ptr<MapPy>>(*m, "Map_")
    .def(py::init<bool, std::shared_ptr<MultitypeFuncGraph>>(), py::arg("reverse"), py::arg("ops"))
    .def(py::init<bool>(), py::arg("reverse"));

  // Reg MultitypeFuncGraph
  (void)py::class_<MultitypeFuncGraph, MetaFuncGraph, std::shared_ptr<MultitypeFuncGraph>>(*m, "MultitypeFuncGraph_")
    .def(py::init<const std::string &>())
    .def("register_fn", &MultitypeFuncGraph::PyRegister)
    .def("set_doc_url_", &MultitypeFuncGraph::set_doc_url)
    .def("set_need_raise_", &MultitypeFuncGraph::set_need_raise);

  // Reg UnpackCall
  (void)py::class_<UnpackCall, MetaFuncGraph, std::shared_ptr<UnpackCall>>(*m, "UnpackCall_")
    .def(py::init<std::string &>());

  // Reg ZipOperation
  (void)py::class_<ZipOperation, MetaFuncGraph, std::shared_ptr<ZipOperation>>(*m, "ZipOperation_")
    .def(py::init<std::string &>());

  // Reg StarredUnpack
  (void)py::class_<StarredUnpack, MetaFuncGraph, std::shared_ptr<StarredUnpack>>(*m, "StarredUnpack_")
    .def(py::init<std::string &>());

  // Reg StarredGetItem
  (void)py::class_<StarredGetItem, MetaFuncGraph, std::shared_ptr<StarredGetItem>>(*m, "StarredGetItem_")
    .def(py::init<std::string &>());

  // Reg StarredUnpackMerge
  (void)py::class_<StarredUnpackMerge, MetaFuncGraph, std::shared_ptr<StarredUnpackMerge>>(*m, "StarredUnpackMerge_")
    .def(py::init<std::string &>());

  // Reg IterConverter
  (void)py::class_<IterConverter, MetaFuncGraph, std::shared_ptr<IterConverter>>(*m, "IterConverter_")
    .def(py::init<std::string &>());

  // Reg HasNext
  (void)py::class_<HasNext, MetaFuncGraph, std::shared_ptr<HasNext>>(*m, "HasNext_").def(py::init<std::string &>());

  // Reg Next
  (void)py::class_<Next, MetaFuncGraph, std::shared_ptr<Next>>(*m, "Next_").def(py::init<std::string &>());

  // Reg VmapGeneralPreprocess
  (void)py::class_<VmapGeneralPreprocess, MetaFuncGraph, std::shared_ptr<VmapGeneralPreprocess>>(
    *m, "VmapGeneralPreprocess_")
    .def(py::init<std::string &>(), py::arg("fn"));

  // Reg TensorIndexGetitem
  (void)py::class_<TensorIndexGetitem, MetaFuncGraph, std::shared_ptr<TensorIndexGetitem>>(*m, "TensorIndexGetitem_")
    .def(py::init<std::string &>());

  // Reg TensorIndexSetitem
  (void)py::class_<TensorIndexSetitem, MetaFuncGraph, std::shared_ptr<TensorIndexSetitem>>(*m, "TensorIndexSetitem_")
    .def(py::init<std::string &>());

  // Reg HandleBoolTensor
  (void)py::class_<HandleBoolTensor, MetaFuncGraph, std::shared_ptr<HandleBoolTensor>>(*m, "HandleBoolTensor_")
    .def(py::init<std::string &>());

  // Reg PreSetitemByTuple_
  (void)py::class_<PreSetitemByTuple, MetaFuncGraph, std::shared_ptr<PreSetitemByTuple>>(*m, "PreSetitemByTuple_")
    .def(py::init<std::string &>());
}
}  // namespace prim
}  // namespace mindspore
