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
#include <vector>
#include <memory>
#include "common/common_test.h"
#include "ops/ops_func_impl/bias_add.h"
#include "ir/dtype/type.h"
#include "abstract/dshape.h"
#include "utils/tensor_construct_utils.h"
#include "ir/primitive.h"
#include "abstract/abstract_value.h"
#include "ops/test_ops.h"
#include "ops/auto_generate/gen_ops_name.h"
#include "ops/test_ops_cmp_utils.h"
#include "ops/test_value_utils.h"

namespace mindspore {
namespace ops {
struct TestBiasAddParams {
  ShapeVector input_x_shape;
  TypePtr input_x_type;
  ShapeVector bias_shape;
  TypePtr bias_type;
  std::string data_format;
  ShapeVector out_shape;
  TypePtr out_type;
};

class TestBiasAdd : public TestOps, public testing::WithParamInterface<TestBiasAddParams> {};

TEST_P(TestBiasAdd, bias_add_dyn_shape) {
  const auto &param = GetParam();
  auto input_x = std::make_shared<abstract::AbstractTensor>(param.input_x_type, param.input_x_shape);
  auto bias = std::make_shared<abstract::AbstractTensor>(param.bias_type, param.bias_shape);
  ASSERT_NE(input_x, nullptr);
  ASSERT_NE(bias, nullptr);
  AbstractBasePtr format;
  if (param.data_format == "kValueAny") {
    format = std::make_shared<abstract::AbstractScalar>(kValueAny, kInt64);
  } else {
    auto format_value = MakeValue<int64_t>(FormatStringToEnum(param.data_format));
    format = std::make_shared<abstract::AbstractScalar>(format_value, kInt64);
  }
  ASSERT_NE(format, nullptr);
  auto expect_shape = std::make_shared<abstract::Shape>(param.out_shape);
  auto expect_type = std::make_shared<TensorType>(param.out_type);
  DoFuncImplInferAndCompare<BiasAddFuncImpl>(kNameBiasAdd, {input_x, bias, format}, expect_shape, expect_type);
}

INSTANTIATE_TEST_CASE_P(TestBiasAdd, TestBiasAdd,
    testing::Values(TestBiasAddParams{{-1, -1, -1, 5}, kFloat32, {3}, kFloat32, "NCHW", {-1, 3, -1, 5}, kFloat32},
                    TestBiasAddParams{{2, -1, 4}, kFloat32, {3}, kFloat32, "NCHW", {2, 3, 4}, kFloat32},
                    TestBiasAddParams{{-1, -1, -1, -1 ,-1}, kFloat32, {3}, kFloat32, "NCDHW", {-1, 3, -1, -1, -1}, kFloat32},
                    TestBiasAddParams{{-1, -1, -1}, kFloat32, {3}, kFloat32, "kValueAny", {-1, -1, -1}, kFloat32},
                    TestBiasAddParams{{-1, -1, -1}, kFloat32, {-1}, kFloat32, "NHWC", {-1, -1, -1}, kFloat32},
                    TestBiasAddParams{{-2}, kFloat32, {-2}, kFloat32, "kValueAny", {-2}, kFloat32}));
}  // namespace ops
}  // namespace mindspore
