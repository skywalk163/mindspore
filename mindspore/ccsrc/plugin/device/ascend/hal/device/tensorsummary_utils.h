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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_HAL_DEVICE_TENSORSUMMARY_UTILS_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_HAL_DEVICE_TENSORSUMMARY_UTILS_H_

#include <vector>
#include <string>
#include <utility>
#include "plugin/device/ascend/hal/device/mbuf_receive_manager.h"

namespace mindspore::device::ascend {
const std::vector<std::pair<string, string>> summary_mappings{{"ms_tensor_summary", "TensorSummary"},
                                                              {"ms_image_summary", "ImageSummary"},
                                                              {"ms_scalar_summary", "ScalarSummary"},
                                                              {"ms_histogram_summary", "HistogramSummary"}};

void SummaryReceiveData(const ScopeAclTdtDataset &dataset, const string &channel_name);
};  // namespace mindspore::device::ascend

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_HAL_DEVICE_TENSORSUMMARY_UTILS_H_
