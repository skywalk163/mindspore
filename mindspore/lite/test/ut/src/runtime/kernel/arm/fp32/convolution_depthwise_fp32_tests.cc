/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#include <iostream>
#include <memory>
#include "src/common/log_adapter.h"
#include "common/common_test.h"
#include "src/common/file_utils.h"
#include "mindspore/lite/src/litert/kernel/cpu/base/convolution_base.h"
#include "nnacl/nnacl_manager.h"

namespace mindspore {
class TestConvolutionDwFp32 : public mindspore::CommonTest {
 public:
  TestConvolutionDwFp32() {}
};

void InitConvDwParam(ConvParameter *conv_param) {
  conv_param->input_batch_ = 1;
  conv_param->input_h_ = 288;
  conv_param->input_w_ = 288;
  conv_param->input_channel_ = 25;

  conv_param->output_batch_ = 1;
  conv_param->output_h_ = 288;
  conv_param->output_w_ = 288;
  conv_param->output_channel_ = 25;

  conv_param->group_ = 25;

  conv_param->kernel_h_ = 3;
  conv_param->kernel_w_ = 3;

  conv_param->stride_h_ = 1;
  conv_param->stride_w_ = 1;

  conv_param->dilation_h_ = 1;
  conv_param->dilation_w_ = 1;

  conv_param->pad_u_ = 1;
  conv_param->pad_l_ = 1;
}

void InitConvDwCreator(std::vector<lite::Tensor *> *inputs, std::vector<lite::Tensor *> *outputs,
                       const ConvParameter *conv_param, const InnerContext *ctx) {
  // prepare input, format NHWC
  size_t input_size;
  std::string input_path = "./test_data/convDw/convDwfp32_input.bin";
  auto input_data = reinterpret_cast<float *>(lite::ReadFile(input_path.c_str(), &input_size, ctx->allocator));

  auto *input = new lite::Tensor;
  input->set_data_type(kNumberTypeFloat32);
  input->set_format(mindspore::NHWC);
  input->set_shape({conv_param->input_batch_, conv_param->input_h_, conv_param->input_w_, conv_param->input_channel_});
  input->MallocData();
  memcpy(input->MutableData(), input_data, input_size);

  // prepare weight, format co kh kw ci, ci = 1
  size_t weight_size;
  std::string weight_path = "./test_data/convDw/convDwfp32_weight.bin";
  auto weight_data = reinterpret_cast<float *>(lite::ReadFile(weight_path.c_str(), &weight_size, ctx->allocator));

  auto *weight = new lite::Tensor;
  weight->set_data_type(kNumberTypeFloat32);
  weight->set_shape({conv_param->output_channel_, conv_param->kernel_h_, conv_param->kernel_w_, 1});
  weight->MallocData();
  memcpy(weight->MutableData(), weight_data, weight_size);

  // prepare bias
  auto *bias = new lite::Tensor;
  bias->set_data_type(kNumberTypeFloat32);
  bias->set_shape({conv_param->output_channel_});
  bias->MallocData();
  memset(bias->MutableData(), 0, bias->ElementsNum() * sizeof(float));

  inputs->push_back(input);
  inputs->push_back(weight);
  inputs->push_back(bias);

  auto *output = new lite::Tensor;
  output->set_data_type(kNumberTypeFloat32);
  output->set_shape(
    {conv_param->output_batch_, conv_param->output_h_, conv_param->output_w_, conv_param->output_channel_});
  output->set_format(mindspore::NHWC);
  output->MallocData();
  memset(output->MutableData(), 0, output->ElementsNum() * sizeof(float));
  outputs->push_back(output);
}

TEST_F(TestConvolutionDwFp32, ConvDwFp32Accuracy) {
  // prepare stage
  int thread_num = 1;
  auto conv_param = new ConvParameter();
  conv_param->op_parameter_.type_ = PrimType_Conv2DFusion;
  InitConvDwParam(conv_param);

  // init ctx
  auto ctx = new InnerContext();
  ctx->thread_num_ = thread_num;
  conv_param->op_parameter_.thread_num_ = thread_num;
  ASSERT_EQ(lite::RET_OK, ctx->Init());

  // init tensor
  std::vector<lite::Tensor *> inputs;
  std::vector<lite::Tensor *> outputs;
  InitConvDwCreator(&inputs, &outputs, conv_param, ctx);

  // register op
  kernel::KernelKey desc = {kernel::KERNEL_ARCH::kCPU, kNumberTypeFloat32, NHWC, schema::PrimitiveType_Conv2DFusion};

  auto kernel = nnacl::NNACLKernelRegistry(&conv_param->op_parameter_, inputs, outputs, ctx, desc);
  ASSERT_NE(kernel, nullptr);
  // op run
  auto ret = kernel->Prepare();
  EXPECT_EQ(0, ret);
  ret = kernel->Run();
  EXPECT_EQ(0, ret);

  std::cout << "==================output data=================" << std::endl;
  auto output_ptr = reinterpret_cast<float *>(outputs[0]->MutableData());
  for (int i = 0; i < 20; i++) {
    std::cout << output_ptr[i] << ", ";
  }
  std::cout << std::endl;

  // read output data, format NHWC
  size_t output_size;
  std::string output_path = "./test_data/convDw/convDwfp32_output.bin";
  auto correct_data = reinterpret_cast<float *>(mindspore::lite::ReadFile(output_path.c_str(), &output_size));

  // compare
  ASSERT_EQ(0, CompareOutputData(output_ptr, correct_data, outputs[0]->ElementsNum(), 0.0001));

  for (auto &input : inputs) {
    delete input;
  }
  for (auto &output : outputs) {
    delete output;
  }
  delete kernel;
  delete correct_data;
  delete ctx;
  MS_LOG(INFO) << "TestConvolutionDwFp32 accuracy passed";
}
}  // namespace mindspore
