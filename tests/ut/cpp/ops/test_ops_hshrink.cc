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
#include "ir/dtype/type.h"
#include "ir/primitive.h"
#include "utils/tensor_construct_utils.h"
#include "abstract/dshape.h"
#include "abstract/abstract_value.h"
#include "ops/test_ops.h"
#include "ops/ops_func_impl/hshrink.h"
#include "ops/test_value_utils.h"

namespace mindspore {
namespace ops {

struct HShrinkShape {
  ShapeVector input_x_shape;
  ValuePtr lambd;
  ShapeVector out_shape;
};

struct HShrinkDtype {
  TypePtr input_x_type;
  TypePtr out_type;
};

class TestHShrink : public TestOps, public testing::WithParamInterface<std::tuple<HShrinkShape, HShrinkDtype>> {};

TEST_P(TestHShrink, hshrink_input_xn_shape) {
  const auto &shape_param = std::get<0>(GetParam());
  const auto &dtype_param = std::get<1>(GetParam());

  HShrinkFuncImpl hshrink_func_impl;
  auto prim = std::make_shared<Primitive>("HShrink");

  auto input_x = std::make_shared<abstract::AbstractTensor>(dtype_param.input_x_type, shape_param.input_x_shape);
  auto lambd = shape_param.lambd->ToAbstract();

  auto expect_shape = std::make_shared<abstract::TensorShape>(shape_param.out_shape);
  auto expect_dtype = std::make_shared<TensorType>(dtype_param.out_type);

  auto out_shape = hshrink_func_impl.InferShape(prim, {input_x, lambd});
  ASSERT_TRUE(*out_shape == *expect_shape);
  auto out_dtype = hshrink_func_impl.InferType(prim, {input_x, lambd});
  ASSERT_TRUE(*out_dtype == *expect_dtype);
}

auto HShrinkOpShapeTestCases = testing::ValuesIn({
  /* static */
  HShrinkShape{{2, 3, 4}, CreateScalar(0.5), {2, 3, 4}},
  /* dynamic shape */
  HShrinkShape{{-1}, CreateScalar(0.3), {-1}},
  HShrinkShape{{-1, 2, 4}, CreateScalar(0.5), {-1, 2, 4}},
  HShrinkShape{{5, 3, -1, 2, 1}, CreateScalar(0.1), {5, 3, -1, 2, 1}},
  HShrinkShape{{5, 3, -1, 2, 1, 4, 7, 4}, CreateScalar(-0.4), {5, 3, -1, 2, 1, 4, 7, 4}},
  /* dynamic rank */
  HShrinkShape{{-2}, CreateScalar(0.5), {-2}},
});

auto HShrinkOpTypeTestCases = testing::ValuesIn({
  HShrinkDtype{kFloat16, kFloat16},
  HShrinkDtype{kFloat32, kFloat32},
});

INSTANTIATE_TEST_CASE_P(TestHShrink, TestHShrink, testing::Combine(HShrinkOpShapeTestCases, HShrinkOpTypeTestCases));
}  // namespace ops
}  // namespace mindspore
