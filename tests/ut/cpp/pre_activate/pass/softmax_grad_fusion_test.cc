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
#include "common/backend_common_test.h"
#include "common/py_func_graph_fetcher.h"
#include "include/backend/optimizer/optimizer.h"
#include "plugin/device/cpu/optimizer/softmax_grad_fusion.h"
#include "include/common/debug/anf_ir_dump.h"

namespace mindspore {
namespace opt {
class TestSoftmaxGradFusionCpu : public BackendCommon {
 public:
  TestSoftmaxGradFusionCpu() : get_py_fun_("gtest_input.pre_activate.softmax_grad_fusion_cpu", true) {
    auto context = MsContext::GetInstance();
    MS_EXCEPTION_IF_NULL(context);
    orig_deivce_ = context->get_param<std::string>(MS_CTX_DEVICE_TARGET);
    context->set_param<std::string>(MS_CTX_DEVICE_TARGET, kCPUDevice);
  }
  ~TestSoftmaxGradFusionCpu() override {
    auto context = MsContext::GetInstance();
    MS_EXCEPTION_IF_NULL(context);
    context->set_param<std::string>(MS_CTX_DEVICE_TARGET, orig_deivce_);
  }

  std::string orig_deivce_;
  UT::PyFuncGraphFetcher get_py_fun_;
};

/// Feature: Test SoftmaxGradFusionCpu pass
/// Description: Test SoftmaxGradFusionCpu pass
/// Expectation: The graph after fusion is as expected when it meets the pattern of the pass.
TEST_F(TestSoftmaxGradFusionCpu, test_softmax_grad_fusion_cpu) {
  FuncGraphPtr g = get_py_fun_.CallAndParseRet("test_softmax_grad_fusion_cpu", "before");
  EXPECT_NE(g, nullptr);
  std::vector<int64_t> shp{1, 1, 1, 1};
  auto x_abstract = std::make_shared<abstract::AbstractTensor>(kFloat32, shp);
  AbstractBasePtrList args_spec_list;
  for (size_t i = 0; i < 2; ++i) {
    args_spec_list.push_back(x_abstract);
  }
  auto fg = GetKernelGraph(g, args_spec_list);

  auto optimizer = std::make_shared<opt::GraphOptimizer>();
  auto pm = std::make_shared<opt::PassManager>();
  pm->AddPass(std::make_shared<opt::SoftmaxGradFusionCpu>());
  optimizer->AddPassManager(pm);
  FuncGraphPtr new_graph = optimizer->Optimize(fg);

  FuncGraphPtr g_after = get_py_fun_.CallAndParseRet("test_softmax_grad_fusion_cpu", "after");

  EXPECT_TRUE(CheckEqualGraph(g_after, new_graph));
}
}  // namespace opt
}  // namespace mindspore
