/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#include "tools/converter/adapter/acl/mapper/quant_dtype_cast_mapper.h"
#include <memory>
#include "tools/converter/adapter/acl/mapper/primitive_mapper_register.h"
#include "tools/converter/adapter/acl/mapper/tbe_op_def.h"
#include "tools/converter/quantizer/quantize_util.h"
#include "tools/optimizer/common/gllo_utils.h"
#include "src/common/log_util.h"
#include "ops/op_name.h"
#include "ops/quant_dtype_cast.h"
#include "nnacl/op_base.h"

namespace mindspore {
namespace lite {
namespace {
constexpr auto kQuantInputNum = 2;
constexpr auto kDequantInputNum = 3;
}  // namespace

STATUS QuantDTypeCastMapper::Mapper(const CNodePtr &cnode) {
  ValueNodePtr value_node = nullptr;
  PrimitivePtr src_prim = nullptr;
  if (GetValueNodeAndPrimFromCnode(cnode, &value_node, &src_prim) != lite::RET_OK) {
    MS_LOG(ERROR) << "Get primitive from cnode failed.";
    return lite::RET_ERROR;
  }

  PrimitivePtr dst_prim = nullptr;
  std::vector<schema::QuantParamT> quant_param;
  if (cnode->size() == kQuantInputNum) {
    // map to Quant.
    // quant param in QuantParamHolder
    if (src_prim->HasAttr("quant_params")) {
      MS_LOG(INFO) << "Get quant param from QuantParamHolder, cnode name: " << cnode->fullname_with_scope();
      auto quant_params_holder_attr = src_prim->GetAttr("quant_params");
      CHECK_NULL_RETURN(quant_params_holder_attr);
      auto quant_params_holder = quant_params_holder_attr->cast<QuantParamHolderPtr>();
      CHECK_NULL_RETURN(quant_params_holder);
      MS_CHECK_TRUE_RET(!quant_params_holder->get_output_quant_params().empty(), RET_ERROR);
      quant_param = quant_params_holder->get_output_quant_params().front();
    }
    // quant param in QuantizationParam
    if (src_prim->HasAttr(quant::kQuantParam)) {
      MS_LOG(INFO) << "Get quant param from QuantizationParam, cnode name : " << cnode->fullname_with_scope();
      auto quantization_param_value = src_prim->GetAttr(quant::kQuantParam);
      auto quantization_param_list = GetValue<std::vector<QuantizationParamPtr>>(quantization_param_value);
      if (quantization_param_list.empty()) {
        MS_LOG(ERROR) << cnode->fullname_with_scope() << " quantization_param_list is empty.";
        return lite::RET_ERROR;
      }
      quant_param = quant::ConvertQuantizationParamToQuantParamT(quantization_param_list.front());
    }
    MS_CHECK_TRUE_RET(!quant_param.empty(), RET_ERROR);
    dst_prim = std::make_shared<acl::Quant>();
    CHECK_NULL_RETURN(dst_prim);
    dst_prim->AddAttr("scale", MakeValue(static_cast<float>(quant_param.front().scale)));
    dst_prim->AddAttr("offset", MakeValue(static_cast<float>(quant_param.front().zeroPoint)));
    dst_prim->AddAttr(ops::kDstType, kInt8);
    MS_LOG(INFO) << cnode->fullname_with_scope() << " scale:" << quant_param.front().scale;
    MS_LOG(INFO) << cnode->fullname_with_scope() << " offset:" << quant_param.front().zeroPoint;
    if (quant_param.front().scale < 1.0) {
      MS_LOG(WARNING) << cnode->fullname_with_scope()
                      << " scale less than 1.0, scale value:" << quant_param.front().scale;
    }
  } else if (cnode->size() == kDequantInputNum) {
    // map to Dequant.
    dst_prim = std::make_shared<acl::Dequant>();
    auto dst_type = src_prim->GetAttr(mindspore::ops::kDstT);
    if (dst_type != nullptr) {
      auto origin_type = static_cast<TypeId>(opt::CastToInt(dst_type).front());
      dst_prim->AddAttr("dtype", TypeIdToType(origin_type));
    }
    CHECK_NULL_RETURN(dst_prim);
  } else {
    MS_LOG(ERROR) << "Invalid input size: " << cnode->size();
    return lite::RET_ERROR;
  }

  value_node->set_value(dst_prim);
  return RET_OK;
}

REGISTER_PRIMITIVE_MAPPER(kNameQuantDTypeCast, QuantDTypeCastMapper)
}  // namespace lite
}  // namespace mindspore
