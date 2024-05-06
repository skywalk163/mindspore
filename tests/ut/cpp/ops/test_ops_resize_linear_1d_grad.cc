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
#include "ops/ops_func_impl/resize_linear_1d_grad.h"
#include "ops/auto_generate/gen_ops_name.h"
#include "ir/dtype/type.h"
#include "abstract/dshape.h"
#include "utils/tensor_construct_utils.h"
#include "abstract/abstract_value.h"
#include "ops/test_ops.h"
#include "ops/test_value_utils.h"
#include "ops/test_ops_cmp_utils.h"

namespace mindspore {
namespace ops {
struct ResizeLinear1DGradParams {
  ShapeVector grads_shape;
  TypePtr grads_type;
  ShapeVector x_shape;
  TypePtr x_type;
  ValuePtr coordinate_transformation_mode;  // bool
  ShapeVector out_shape;
  TypePtr out_type;
};

class TestResizeLinear1DGrad : public TestOps, public testing::WithParamInterface<ResizeLinear1DGradParams> {};

TEST_P(TestResizeLinear1DGrad, dyn_shape) {
  const auto &param = GetParam();
  auto grads = std::make_shared<abstract::AbstractTensor>(param.grads_type, param.grads_shape);
  ASSERT_NE(grads, nullptr);
  auto x = std::make_shared<abstract::AbstractTensor>(param.x_type, param.x_shape);
  ASSERT_NE(x, nullptr);
  auto coordinate_transformation_mode = param.coordinate_transformation_mode->ToAbstract();
  ASSERT_NE(coordinate_transformation_mode, nullptr);

  auto expect_shape = std::make_shared<abstract::Shape>(param.out_shape);
  auto expect_type = std::make_shared<TensorType>(param.out_type);
  DoFuncImplInferAndCompare<ResizeLinear1DGradFuncImpl>(
    kNameResizeLinear1DGrad, {grads, x, coordinate_transformation_mode}, expect_shape, expect_type);
}

INSTANTIATE_TEST_CASE_P(
  TestResizeLinear1DGradGroup, TestResizeLinear1DGrad,
  testing::Values(
    ResizeLinear1DGradParams{{1, 3, 4}, kFloat32, {1, 3, 8}, kFloat32, CreateScalar(true), {1, 3, 8}, kFloat32},
    ResizeLinear1DGradParams{{1, 3, -1}, kFloat32, {1, 3, 8}, kFloat32, CreateScalar(true), {1, 3, 8}, kFloat32},
    ResizeLinear1DGradParams{
      {-1, -1, -1}, kFloat32, {-1, -1, -1}, kFloat32, CreateScalar(true), {-1, -1, -1}, kFloat32},
    ResizeLinear1DGradParams{{-2}, kFloat32, {-2}, kFloat32, CreateScalar(true), {-2}, kFloat32}));
}  // namespace ops
}  // namespace mindspore
