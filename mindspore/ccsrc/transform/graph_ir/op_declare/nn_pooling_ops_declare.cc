/**
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
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

#include "transform/graph_ir/op_declare/nn_pooling_ops_declare.h"
#include <string>
#include <vector>
#include "ops/conv_pool_ops.h"
#include "ops/nn_ops.h"

namespace mindspore::transform {
// MaxPool
INPUT_MAP(MaxPool) = {{1, INPUT_DESC(x)}};
ATTR_MAP(MaxPool) = {{"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                     {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                     {"pad_mode", ATTR_DESC(padding, AnyTraits<std::string>())},
                     {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
OUTPUT_MAP(MaxPool) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPool, kNameMaxPool, ADPT_DESC(MaxPool))

// MaxPool3D
INPUT_MAP(MaxPool3D) = {{1, INPUT_DESC(x)}};
ATTR_MAP(MaxPool3D) = {{"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"pad_mode", ATTR_DESC(padding, AnyTraits<std::string>())},
                       {"pad_list", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"dilation", ATTR_DESC(dilation, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<int64_t>())},
                       {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
OUTPUT_MAP(MaxPool3D) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPool3D, kNameMaxPool3D, ADPT_DESC(MaxPool3D))

// MaxPool3DWithArgmax
CUST_INPUT_MAP(MaxPool3DWithArgmax) = {{1, INPUT_DESC(x)}};
CUST_ATTR_MAP(MaxPool3DWithArgmax) = {
  {"ksize", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"pads", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"dilation", ATTR_DESC(dilation, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<bool>())},
  {"format", ATTR_DESC(data_format, AnyTraits<std::string>())},
  {"argmax_type", ATTR_DESC(argmax_type, AnyTraits<std::string>())}};
CUST_OUTPUT_MAP(MaxPool3DWithArgmax) = {{0, OUTPUT_DESC(y)}, {1, OUTPUT_DESC(argmax)}};
REG_ADPT_DESC(MaxPool3DWithArgmax, prim::kPrimMaxPool3DWithArgmax->name(), CUST_ADPT_DESC(MaxPool3DWithArgmax))

// MaxPool3DGrad
INPUT_MAP(MaxPool3DGrad) = {{1, INPUT_DESC(orig_x)}, {2, INPUT_DESC(orig_y)}, {3, INPUT_DESC(grads)}};
ATTR_MAP(MaxPool3DGrad) = {{"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                           {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                           {"pad_list", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                           {"format", ATTR_DESC(data_format, AnyTraits<std::string>())},
                           {"pad_mode", ATTR_DESC(padding, AnyTraits<std::string>())}};
OUTPUT_MAP(MaxPool3DGrad) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPool3DGrad, kNameMaxPool3DGrad, ADPT_DESC(MaxPool3DGrad))

// MaxPool3DGradGrad
INPUT_MAP(MaxPool3DGradGrad) = {{1, INPUT_DESC(orig_x)}, {2, INPUT_DESC(orig_y)}, {3, INPUT_DESC(grads)}};
ATTR_MAP(MaxPool3DGradGrad) = {
  {"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"pad_list", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
OUTPUT_MAP(MaxPool3DGradGrad) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPool3DGradGrad, kNameMaxPool3DGradGrad, ADPT_DESC(MaxPool3DGradGrad))

// AvgPool
INPUT_MAP(AvgPool) = {{kIndex1, INPUT_DESC(x)}};
ATTR_MAP(AvgPool) = EMPTY_ATTR_MAP;
INPUT_ATTR_MAP(AvgPool) = {{kIndex2, ATTR_DESC(ksize, AnyTraits<std::vector<int64_t>>(), 4L, 1L)},
                           {kIndex3, ATTR_DESC(strides, AnyTraits<std::vector<int64_t>>(), 4L, 1L)},
                           {kIndex4, ATTR_DESC(padding, AnyTraits<GEPadMod>())},
                           {kIndex5, ATTR_DESC(data_format, AnyTraits<GEDataFormat>())}};
OUTPUT_MAP(AvgPool) = {{kIndex0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(AvgPool, kNameAvgPool, ADPT_DESC(AvgPool))

// AvgPool3D
INPUT_MAP(AvgPool3D) = {{1, INPUT_DESC(x)}};
ATTR_MAP(AvgPool3D) = {{"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"pad_list", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"format", ATTR_DESC(data_format, AnyTraits<std::string>())},
                       {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<bool>())},
                       {"count_include_pad", ATTR_DESC(count_include_pad, AnyTraits<bool>())},
                       {"divisor_override", ATTR_DESC(divisor_override, AnyTraits<int64_t>())}};
OUTPUT_MAP(AvgPool3D) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(AvgPool3D, kAvgPool3DOpName, ADPT_DESC(AvgPool3D))

// AvgPool3DD
INPUT_MAP(AvgPool3DD) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(filter)}, {3, INPUT_DESC(multiplier)}};
ATTR_MAP(AvgPool3DD) = {{"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                        {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                        {"pad", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                        {"format", ATTR_DESC(data_format, AnyTraits<std::string>())},
                        {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<bool>())},
                        {"count_include_pad", ATTR_DESC(count_include_pad, AnyTraits<bool>())},
                        {"divisor_override", ATTR_DESC(divisor_override, AnyTraits<int64_t>())}};
OUTPUT_MAP(AvgPool3DD) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(AvgPool3DD, kAvgPool3DDOpName, ADPT_DESC(AvgPool3DD))

// MaxPoolGrad
INPUT_MAP(MaxPoolGrad) = {{1, INPUT_DESC(x1)}, {2, INPUT_DESC(x2)}, {3, INPUT_DESC(grad)}};
ATTR_MAP(MaxPoolGrad) = {{"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                         {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                         {"pad_mode", ATTR_DESC(padding, AnyTraits<std::string>())},
                         {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
OUTPUT_MAP(MaxPoolGrad) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPoolGrad, kNameMaxPoolGrad, ADPT_DESC(MaxPoolGrad))

// MaxPoolGradGrad
INPUT_MAP(MaxPoolGradGrad) = {{1, INPUT_DESC(x1)}, {2, INPUT_DESC(x2)}, {3, INPUT_DESC(grad)}};
ATTR_MAP(MaxPoolGradGrad) = {{"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                             {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                             {"pad_mode", ATTR_DESC(padding, AnyTraits<std::string>())},
                             {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
OUTPUT_MAP(MaxPoolGradGrad) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPoolGradGrad, kNameMaxPoolGradGrad, ADPT_DESC(MaxPoolGradGrad))

// avgpoolgrad
INPUT_MAP(AvgPoolGrad) = {{kIndex1, INPUT_DESC(orig_input_shape)}, {kIndex2, INPUT_DESC(input_grad)}};
ATTR_MAP(AvgPoolGrad) = EMPTY_ATTR_MAP;
INPUT_ATTR_MAP(AvgPoolGrad) = {{kIndex3, ATTR_DESC(ksize, AnyTraits<std::vector<int64_t>>(), 4L, 1L)},
                               {kIndex4, ATTR_DESC(strides, AnyTraits<std::vector<int64_t>>(), 4L, 1L)},
                               {kIndex5, ATTR_DESC(padding, AnyTraits<GEPadMod>())},
                               {kIndex6, ATTR_DESC(data_format, AnyTraits<GEDataFormat>())}};
OUTPUT_MAP(AvgPoolGrad) = {{kIndex0, OUTPUT_DESC(out_grad)}};
REG_ADPT_DESC(AvgPoolGrad, kNameAvgPoolGrad, ADPT_DESC(AvgPoolGrad))
REG_ADPT_DESC(AvgPoolGradGe, kNameAvgPoolGradGe, ADPT_DESC(AvgPoolGrad))

// MaxPoolWithArgmax
INPUT_MAP(MaxPoolWithArgmax) = {{1, INPUT_DESC(x)}};
ATTR_MAP(MaxPoolWithArgmax) = {
  {"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"pad_mode", ATTR_DESC(padding, AnyTraits<std::string>())}};
OUTPUT_MAP(MaxPoolWithArgmax) = {{0, OUTPUT_DESC(y)}, {1, OUTPUT_DESC(argmax)}};
REG_ADPT_DESC(MaxPoolWithArgmax, kNameMaxPoolWithArgmax, ADPT_DESC(MaxPoolWithArgmax))

// MaxPoolWithArgmaxV2
INPUT_MAP(MaxPoolWithArgmaxV2) = {{1, INPUT_DESC(x)}};
ATTR_MAP(MaxPoolWithArgmaxV2) = {
  {"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"pads", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"dilation", ATTR_DESC(dilation, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<bool>())},
  {"argmax_type", ATTR_DESC(dtype, AnyTraits<int64_t>())}};
OUTPUT_MAP(MaxPoolWithArgmaxV2) = {{0, OUTPUT_DESC(y)}, {1, OUTPUT_DESC(argmax)}};
REG_ADPT_DESC(MaxPoolWithArgmaxV2, kNameMaxPoolWithArgmaxV2, ADPT_DESC(MaxPoolWithArgmaxV2))

// MaxPoolGradWithArgmax
INPUT_MAP(MaxPoolGradWithArgmax) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(grad)}, {3, INPUT_DESC(argmax)}};
ATTR_MAP(MaxPoolGradWithArgmax) = {
  {"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"pad_mode", ATTR_DESC(padding, AnyTraits<std::string>())}};
OUTPUT_MAP(MaxPoolGradWithArgmax) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPoolGradWithArgmax, kNameMaxPoolGradWithArgmax, ADPT_DESC(MaxPoolGradWithArgmax))

// MaxPoolGradWithArgmaxV2
INPUT_MAP(MaxPoolGradWithArgmaxV2) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(grad)}, {3, INPUT_DESC(argmax)}};
ATTR_MAP(MaxPoolGradWithArgmaxV2) = {
  {"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"pads", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"dilation", ATTR_DESC(dilation, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<bool>())},
  {"argmax_type", ATTR_DESC(dtype, AnyTraits<int64_t>())}};
OUTPUT_MAP(MaxPoolGradWithArgmaxV2) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPoolGradWithArgmaxV2, kNameMaxPoolGradWithArgmaxV2, ADPT_DESC(MaxPoolGradWithArgmaxV2))

// MaxPoolGradGradWithArgmax
INPUT_MAP(MaxPoolGradGradWithArgmax) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(grad)}, {3, INPUT_DESC(argmax)}};
ATTR_MAP(MaxPoolGradGradWithArgmax) = {
  {"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"pad_mode", ATTR_DESC(padding, AnyTraits<std::string>())}};
OUTPUT_MAP(MaxPoolGradGradWithArgmax) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPoolGradGradWithArgmax, kNameMaxPoolGradGradWithArgmax, ADPT_DESC(MaxPoolGradGradWithArgmax))

// Pooling
INPUT_MAP(Pooling) = {{1, INPUT_DESC(x)}};
ATTR_MAP(Pooling) = {{"mode", ATTR_DESC(mode, AnyTraits<int64_t>())},
                     {"global", ATTR_DESC(global_pooling, AnyTraits<bool>())},
                     {"kernel_size", ATTR_DESC(window, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                     {"strides", ATTR_DESC(stride, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                     {"pad", ATTR_DESC(pad, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                     {"dilation", ATTR_DESC(dilation, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                     {"round_mode", ATTR_DESC(ceil_mode, AnyTraits<int64_t>())},
                     {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
OUTPUT_MAP(Pooling) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(Pooling, kNamePooling, ADPT_DESC(Pooling))

// MaxPoolV3
INPUT_MAP(MaxPoolV3) = {{1, INPUT_DESC(x)}};
ATTR_MAP(MaxPoolV3) = {{"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"padding_mode", ATTR_DESC(padding_mode, AnyTraits<std::string>())},
                       {"pad", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"format", ATTR_DESC(data_format, AnyTraits<std::string>())},
                       {"global", ATTR_DESC(global_pooling, AnyTraits<bool>())},
                       {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<bool>())}};
OUTPUT_MAP(MaxPoolV3) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPoolV3, kNameMaxPoolV3, ADPT_DESC(MaxPoolV3))

// AvgPoolV2
INPUT_MAP(AvgPoolV2) = {{1, INPUT_DESC(x)}};
ATTR_MAP(AvgPoolV2) = {{"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"padding_mode", ATTR_DESC(padding_mode, AnyTraits<std::string>())},
                       {"pad", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                       {"format", ATTR_DESC(data_format, AnyTraits<std::string>())},
                       {"global", ATTR_DESC(global_pooling, AnyTraits<bool>())},
                       {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<bool>())},
                       {"exclusive", ATTR_DESC(exclusive, AnyTraits<bool>())},
                       {"divisor_override", ATTR_DESC(divisor_override, AnyTraits<int>())}};
OUTPUT_MAP(AvgPoolV2) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(AvgPoolV2, kNameAvgPoolV2, ADPT_DESC(AvgPoolV2))

// GlobalAveragePool
INPUT_MAP(GlobalAveragePool) = {{1, INPUT_DESC(x)}};
ATTR_MAP(GlobalAveragePool) = EMPTY_ATTR_MAP;
OUTPUT_MAP(GlobalAveragePool) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(GlobalAveragePool, kNameGlobalAvgPool, ADPT_DESC(GlobalAveragePool))

// Upsample
INPUT_MAP(Upsample) = {{1, INPUT_DESC(x)}};
ATTR_MAP(Upsample) = {{"scale", ATTR_DESC(scale, AnyTraits<float>())},
                      {"stride_h", ATTR_DESC(stride_h, AnyTraits<int64_t>())},
                      {"stride_w", ATTR_DESC(stride_w, AnyTraits<int64_t>())}};
OUTPUT_MAP(Upsample) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(Upsample, kNameUpsample, ADPT_DESC(Upsample))

// AdaptiveMaxPool2d
INPUT_MAP(AdaptiveMaxPool2d) = {{1, INPUT_DESC(x)}};
ATTR_MAP(AdaptiveMaxPool2d) = {{"output_size", ATTR_DESC(output_size, AnyTraits<std::vector<int64_t>>())}};
OUTPUT_MAP(AdaptiveMaxPool2d) = {{0, OUTPUT_DESC(y)}, {1, OUTPUT_DESC(argmax)}};
REG_ADPT_DESC(AdaptiveMaxPool2d, kAdaptiveMaxPool2dOpName, ADPT_DESC(AdaptiveMaxPool2d))
REG_ADPT_DESC(AdaptiveMaxPool2D, kNameAdaptiveMaxPool2D, ADPT_DESC(AdaptiveMaxPool2d))

// AvgPool3DGrad
INPUT_MAP(AvgPool3DGrad) = {{kIndex1, INPUT_DESC(orig_input_shape)}, {kIndex2, INPUT_DESC(grads)}};
ATTR_MAP(AvgPool3DGrad) = {{"kernel_size", ATTR_DESC(ksize, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                           {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                           {"pad_list", ATTR_DESC(pads, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
                           {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<bool>())},
                           {"count_include_pad", ATTR_DESC(count_include_pad, AnyTraits<bool>())},
                           {"divisor_override", ATTR_DESC(divisor_override, AnyTraits<int64_t>())},
                           {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
ATTR_INPUT_MAP(AvgPool3DGrad) = {{"origin_input_shape", "orig_input_shape"}};
OUTPUT_MAP(AvgPool3DGrad) = {{kIndex0, OUTPUT_DESC(output)}};
REG_ADPT_DESC(AvgPool3DGrad, kAvgPool3DGradOpName, ADPT_DESC(AvgPool3DGrad))

// Dilation2DBackpropFilter
INPUT_MAP(Dilation2DBackpropFilter) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(filter)}, {3, INPUT_DESC(out_backprop)}};
ATTR_MAP(Dilation2DBackpropFilter) = {{"strides", ATTR_DESC(strides, AnyTraits<std::vector<int64_t>>())},
                                      {"rates", ATTR_DESC(rates, AnyTraits<std::vector<int64_t>>())},
                                      {"padding_mode", ATTR_DESC(padding_mode, AnyTraits<std::string>())},
                                      {"pads", ATTR_DESC(pads, AnyTraits<std::vector<int64_t>>())},
                                      {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<bool>())},
                                      {"data_format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
OUTPUT_MAP(Dilation2DBackpropFilter) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(Dilation2DBackpropFilter, prim::kPrimDilation2DBackpropFilter->name(),
              ADPT_DESC(Dilation2DBackpropFilter))

// Dilation2DBackpropInput
INPUT_MAP(Dilation2DBackpropInput) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(filter)}, {3, INPUT_DESC(out_backprop)}};
ATTR_MAP(Dilation2DBackpropInput) = {
  {"strides", ATTR_DESC(strides, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"rates", ATTR_DESC(rates, AnyTraits<int64_t>(), AnyTraits<std::vector<int64_t>>())},
  {"padding_mode", ATTR_DESC(padding_mode, AnyTraits<std::string>())},
  {"pads", ATTR_DESC(pads, AnyTraits<std::vector<int64_t>>())},
  {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<int64_t>())},
  {"data_format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
OUTPUT_MAP(Dilation2DBackpropInput) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(Dilation2DBackpropInput, kNameDilation2DBackpropInput, ADPT_DESC(Dilation2DBackpropInput))

// AdaptiveAvgPool3dGrad
CUST_INPUT_MAP(AdaptiveAvgPool3dGrad) = {{1, INPUT_DESC(input_grad)}, {2, INPUT_DESC(orig_input_shape)}};
CUST_ATTR_MAP(AdaptiveAvgPool3dGrad) = EMPTY_ATTR_MAP;
CUST_OUTPUT_MAP(AdaptiveAvgPool3dGrad) = {{0, OUTPUT_DESC(output_grad)}};
REG_ADPT_DESC(AdaptiveAvgPool3dGrad, prim::kPrimAdaptiveAvgPool3DGrad->name(), CUST_ADPT_DESC(AdaptiveAvgPool3dGrad));

// AdaptiveMaxPool2dGrad
CUST_INPUT_MAP(AdaptiveMaxPool2dGrad) = {{1, INPUT_DESC(y_grad)}, {2, INPUT_DESC(x)}, {3, INPUT_DESC(argmax)}};
CUST_ATTR_MAP(AdaptiveMaxPool2dGrad) = EMPTY_ATTR_MAP;
CUST_OUTPUT_MAP(AdaptiveMaxPool2dGrad) = {{0, OUTPUT_DESC(x_grad)}};
REG_ADPT_DESC(AdaptiveMaxPool2dGrad, prim::kPrimAdaptiveMaxPool2DGrad->name(), CUST_ADPT_DESC(AdaptiveMaxPool2dGrad));

// hotfix only for lite
// AdaptiveAvgPool2D
INPUT_MAP(AdaptiveAvgPool2d) = {{1, INPUT_DESC(x)}};
ATTR_MAP(AdaptiveAvgPool2d) = {{"output_size", ATTR_DESC(output_size, AnyTraits<std::vector<int64_t>>())}};
OUTPUT_MAP(AdaptiveAvgPool2d) = {{0, OUTPUT_DESC(y)}};
// AdaptiveAvgPool2D
CUST_INPUT_MAP(AdaptiveAvgPool2D) = {{1, INPUT_DESC(x)}};
CUST_ATTR_MAP(AdaptiveAvgPool2D) = {{"output_size", ATTR_DESC(output_size, AnyTraits<std::vector<int64_t>>())}};
CUST_OUTPUT_MAP(AdaptiveAvgPool2D) = {{0, OUTPUT_DESC(y)}};
#ifdef BUILD_LITE
REG_ADPT_DESC(AdaptiveAvgPool2d, kAdaptiveAvgPool2DOpName, ADPT_DESC(AdaptiveAvgPool2d))
#else
REG_ADPT_DESC(AdaptiveAvgPool2D, prim::kPrimAdaptiveAvgPool2D->name(), CUST_ADPT_DESC(AdaptiveAvgPool2D));
#endif

// AdaptiveAvgPool3D
CUST_INPUT_MAP(AdaptiveAvgPool3d) = {{1, INPUT_DESC(x)}};
CUST_ATTR_MAP(AdaptiveAvgPool3d) = {{"output_size", ATTR_DESC(output_size, AnyTraits<std::vector<int64_t>>())}};
CUST_OUTPUT_MAP(AdaptiveAvgPool3d) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(AdaptiveAvgPool3d, prim::kPrimAdaptiveAvgPool3D->name(), CUST_ADPT_DESC(AdaptiveAvgPool3d));

// AdaptiveAvgPool2DGrad
CUST_INPUT_MAP(AdaptiveAvgPool2DGrad) = {{1, INPUT_DESC(input_grad)}, {2, INPUT_DESC(orig_input_shape)}};
CUST_ATTR_MAP(AdaptiveAvgPool2DGrad) = EMPTY_ATTR_MAP;
CUST_OUTPUT_MAP(AdaptiveAvgPool2DGrad) = {{0, OUTPUT_DESC(output_grad)}};
REG_ADPT_DESC(AdaptiveAvgPool2DGrad, prim::kPrimAdaptiveAvgPool2DGrad->name(), CUST_ADPT_DESC(AdaptiveAvgPool2DGrad));

// DataFormatVecPermute
INPUT_MAP(DataFormatVecPermute) = {{1, INPUT_DESC(x)}};
ATTR_MAP(DataFormatVecPermute) = {{"src_format", ATTR_DESC(src_format, AnyTraits<std::string>())},
                                  {"dst_format", ATTR_DESC(dst_format, AnyTraits<std::string>())}};
OUTPUT_MAP(DataFormatVecPermute) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(DataFormatVecPermute, prim::kPrimDataFormatVecPermute->name(), ADPT_DESC(DataFormatVecPermute));

// MaxPool3DGradWithArgmax
CUST_INPUT_MAP(MaxPool3DGradWithArgmax) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(grads)}, {3, INPUT_DESC(argmax)}};
CUST_ATTR_MAP(MaxPool3DGradWithArgmax) = {{"ksize", ATTR_DESC(ksize, AnyTraits<std::vector<int64_t>>())},
                                          {"strides", ATTR_DESC(strides, AnyTraits<std::vector<int64_t>>())},
                                          {"pads", ATTR_DESC(pads, AnyTraits<std::vector<int64_t>>())},
                                          {"dilation", ATTR_DESC(dilation, AnyTraits<std::vector<int64_t>>())},
                                          {"ceil_mode", ATTR_DESC(ceil_mode, AnyTraits<bool>())},
                                          {"format", ATTR_DESC(data_format, AnyTraits<std::string>())},
                                          {"argmax_type", ATTR_DESC(argmax_type, AnyTraits<std::string>())}};
CUST_OUTPUT_MAP(MaxPool3DGradWithArgmax) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxPool3DGradWithArgmax, prim::kPrimMaxPool3DGradWithArgmax->name(),
              CUST_ADPT_DESC(MaxPool3DGradWithArgmax));

// FractionalAvgPoolGrad
INPUT_MAP(FractionalAvgPoolGrad) = {{1, INPUT_DESC(orig_input_tensor_shape)},
                                    {2, INPUT_DESC(out_backprop)},
                                    {3, INPUT_DESC(row_pooling_sequence)},
                                    {4, INPUT_DESC(col_pooling_sequence)}};
ATTR_MAP(FractionalAvgPoolGrad) = {{"overlapping", ATTR_DESC(overlapping, AnyTraits<bool>())}};
OUTPUT_MAP(FractionalAvgPoolGrad) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(FractionalAvgPoolGrad, prim::kPrimFractionalAvgPoolGrad->name(), ADPT_DESC(FractionalAvgPoolGrad));

// FractionalMaxPool
INPUT_MAP(FractionalMaxPool) = {{1, INPUT_DESC(x)}};
ATTR_MAP(FractionalMaxPool) = {{"pooling_ratio", ATTR_DESC(pooling_ratio, AnyTraits<std::vector<float>>())},
                               {"pseudo_random", ATTR_DESC(pseudo_random, AnyTraits<bool>())},
                               {"overlapping", ATTR_DESC(overlapping, AnyTraits<bool>())},
                               {"deterministic", ATTR_DESC(deterministic, AnyTraits<bool>())},
                               {"seed", ATTR_DESC(seed, AnyTraits<int64_t>())},
                               {"seed2", ATTR_DESC(seed2, AnyTraits<int64_t>())}};
OUTPUT_MAP(FractionalMaxPool) = {
  {0, OUTPUT_DESC(y)}, {1, OUTPUT_DESC(row_pooling_sequence)}, {2, OUTPUT_DESC(col_pooling_sequence)}};
REG_ADPT_DESC(FractionalMaxPool, prim::kPrimFractionalMaxPool->name(), ADPT_DESC(FractionalMaxPool));

// FractionalMaxPoolGrad
INPUT_MAP(FractionalMaxPoolGrad) = {{1, INPUT_DESC(orig_input)},
                                    {2, INPUT_DESC(orig_output)},
                                    {3, INPUT_DESC(out_backprop)},
                                    {4, INPUT_DESC(row_pooling_sequence)},
                                    {5, INPUT_DESC(col_pooling_sequence)}};
ATTR_MAP(FractionalMaxPoolGrad) = {{"overlapping", ATTR_DESC(overlapping, AnyTraits<bool>())}};
OUTPUT_MAP(FractionalMaxPoolGrad) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(FractionalMaxPoolGrad, prim::kPrimFractionalMaxPoolGrad->name(), ADPT_DESC(FractionalMaxPoolGrad));

// FractionalAvgPool
INPUT_MAP(FractionalAvgPool) = {{1, INPUT_DESC(x)}};
ATTR_MAP(FractionalAvgPool) = {{"pooling_ratio", ATTR_DESC(pooling_ratio, AnyTraits<std::vector<float>>())},
                               {"pseudo_random", ATTR_DESC(pseudo_random, AnyTraits<bool>())},
                               {"overlapping", ATTR_DESC(overlapping, AnyTraits<bool>())},
                               {"deterministic", ATTR_DESC(deterministic, AnyTraits<bool>())},
                               {"seed", ATTR_DESC(seed, AnyTraits<int64_t>())},
                               {"seed2", ATTR_DESC(seed2, AnyTraits<int64_t>())}};
OUTPUT_MAP(FractionalAvgPool) = {
  {0, OUTPUT_DESC(y)}, {1, OUTPUT_DESC(row_pooling_sequence)}, {2, OUTPUT_DESC(col_pooling_sequence)}};
REG_ADPT_DESC(FractionalAvgPool, prim::kPrimFractionalAvgPool->name(), ADPT_DESC(FractionalAvgPool));

// NthElement
INPUT_MAP(NthElement) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(n)}};
ATTR_MAP(NthElement) = {{"reverse", ATTR_DESC(reverse, AnyTraits<bool>())}};
OUTPUT_MAP(NthElement) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(NthElement, prim::kPrimNthElement->name(), ADPT_DESC(NthElement));

// AdaptiveAvgPool
INPUT_MAP(AdaptiveAvgPool) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(output_size)}};
ATTR_MAP(AdaptiveAvgPool) = EMPTY_ATTR_MAP;
OUTPUT_MAP(AdaptiveAvgPool) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(AdaptiveAvgPool, kNameAdaptiveAvgPool, ADPT_DESC(AdaptiveAvgPool));

// MaxUnpool2D
CUST_INPUT_MAP(MaxUnpool2D) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(argmax)}};
CUST_ATTR_MAP(MaxUnpool2D) = {{"ksize", ATTR_DESC(ksize, AnyTraits<std::vector<int64_t>>())},
                              {"strides", ATTR_DESC(strides, AnyTraits<std::vector<int64_t>>())},
                              {"pads", ATTR_DESC(pads, AnyTraits<std::vector<int64_t>>())},
                              {"output_shape", ATTR_DESC(output_shape, AnyTraits<std::vector<int64_t>>())},
                              {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
CUST_OUTPUT_MAP(MaxUnpool2D) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxUnpool2D, prim::kPrimMaxUnpool2D->name(), CUST_ADPT_DESC(MaxUnpool2D));

// MaxUnpool3D
CUST_INPUT_MAP(MaxUnpool3D) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(argmax)}};
CUST_ATTR_MAP(MaxUnpool3D) = {{"ksize", ATTR_DESC(ksize, AnyTraits<std::vector<int64_t>>())},
                              {"strides", ATTR_DESC(strides, AnyTraits<std::vector<int64_t>>())},
                              {"pads", ATTR_DESC(pads, AnyTraits<std::vector<int64_t>>())},
                              {"output_shape", ATTR_DESC(output_shape, AnyTraits<std::vector<int64_t>>())},
                              {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
CUST_OUTPUT_MAP(MaxUnpool3D) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxUnpool3D, prim::kPrimMaxUnpool3D->name(), CUST_ADPT_DESC(MaxUnpool3D));

// MaxUnpool3DGrad
CUST_INPUT_MAP(MaxUnpool3DGrad) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(grads)}, {3, INPUT_DESC(argmax)}};
CUST_ATTR_MAP(MaxUnpool3DGrad) = {{"ksize", ATTR_DESC(ksize, AnyTraits<std::vector<int64_t>>())},
                                  {"strides", ATTR_DESC(strides, AnyTraits<std::vector<int64_t>>())},
                                  {"pads", ATTR_DESC(pads, AnyTraits<std::vector<int64_t>>())},
                                  {"output_shape", ATTR_DESC(output_shape, AnyTraits<std::vector<int64_t>>())},
                                  {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
CUST_OUTPUT_MAP(MaxUnpool3DGrad) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxUnpool3DGrad, prim::kPrimMaxUnpool3DGrad->name(), CUST_ADPT_DESC(MaxUnpool3DGrad));

// MaxUnpool2DGrad
CUST_INPUT_MAP(MaxUnpool2DGrad) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(grads)}, {3, INPUT_DESC(argmax)}};
CUST_ATTR_MAP(MaxUnpool2DGrad) = {{"ksize", ATTR_DESC(ksize, AnyTraits<std::vector<int64_t>>())},
                                  {"strides", ATTR_DESC(strides, AnyTraits<std::vector<int64_t>>())},
                                  {"pads", ATTR_DESC(pads, AnyTraits<std::vector<int64_t>>())},
                                  {"output_shape", ATTR_DESC(output_shape, AnyTraits<std::vector<int64_t>>())},
                                  {"format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
CUST_OUTPUT_MAP(MaxUnpool2DGrad) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(MaxUnpool2DGrad, prim::kPrimMaxUnpool2DGrad->name(), CUST_ADPT_DESC(MaxUnpool2DGrad));

// FractionalMaxPool3DWithFixedKsize
CUST_INPUT_MAP(FractionalMaxPool3DWithFixedKsize) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(random_samples)}};
CUST_ATTR_MAP(FractionalMaxPool3DWithFixedKsize) = {
  {"ksize", ATTR_DESC(ksize, AnyTraits<std::vector<int64_t>>())},
  {"output_shape", ATTR_DESC(output_shape, AnyTraits<std::vector<int64_t>>())},
  {"data_format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
CUST_OUTPUT_MAP(FractionalMaxPool3DWithFixedKsize) = {{0, OUTPUT_DESC(y)}, {1, OUTPUT_DESC(argmax)}};
REG_ADPT_DESC(FractionalMaxPool3DWithFixedKsize, prim::kPrimFractionalMaxPool3DWithFixedKsize->name(),
              CUST_ADPT_DESC(FractionalMaxPool3DWithFixedKsize));

// FractionalMaxPool3DGradWithFixedKsize
CUST_INPUT_MAP(FractionalMaxPool3DGradWithFixedKsize) = {
  {1, INPUT_DESC(origin_input)}, {2, INPUT_DESC(out_backprop)}, {3, INPUT_DESC(argmax)}};
CUST_ATTR_MAP(FractionalMaxPool3DGradWithFixedKsize) = {
  {"data_format", ATTR_DESC(data_format, AnyTraits<std::string>())}};
CUST_OUTPUT_MAP(FractionalMaxPool3DGradWithFixedKsize) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(FractionalMaxPool3DGradWithFixedKsize, prim::kPrimFractionalMaxPool3DGradWithFixedKsize->name(),
              CUST_ADPT_DESC(FractionalMaxPool3DGradWithFixedKsize));
}  // namespace mindspore::transform
