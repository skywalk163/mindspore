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
#ifndef TESTS_C_UT_COMMON_COMMON_TEST_H_
#define TESTS_C_UT_COMMON_COMMON_TEST_H_

#include <cmath>
#include <fstream>
#include <iostream>
#include "gtest/gtest.h"
#include "include/api/context.h"

namespace UT {
class CApiCommon : public testing::Test {
 public:
  // TestCase only enter for once
  static void SetUpTestCase();
  static void TearDownTestCase();

  // every TEST_F macro will enter for once
  virtual void SetUp();
  virtual void TearDown();

 private:
  std::string org_policy_ = "vm";
};
}  // namespace UT
#endif  // TESTS_C_UT_COMMON_COMMON_TEST_H_
