/**
 * Copyright 2022-2024 Huawei Technologies Co., Ltd
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

#include "plugin/device/ascend/hal/hardware/ascend_deprecated_interface.h"

namespace mindspore {
namespace device {
namespace ascend {
void AscendDeprecatedInterface::DoExecNonInputGraph(const std::string &) {}

void AscendDeprecatedInterface::ExportDFGraph(const std::string &, const std::string &, const pybind11::object &,
                                              char *) {}
FuncGraphPtr AscendDeprecatedInterface::BuildDFGraph(const FuncGraphPtr &, const pybind11::dict &) { return nullptr; }
void AscendDeprecatedInterface::RunInitGraph(const FuncGraphPtr &anf_graph, const pybind11::dict &init_params) {}
void AscendDeprecatedInterface::ClearGraphWrapper() {}
void AscendDeprecatedInterface::ClearOpAdapterMap() {}
void AscendDeprecatedInterface::DumpProfileParallelStrategy(const FuncGraphPtr &) {}

bool AscendDeprecatedInterface::OpenTsd(const std::shared_ptr<MsContext> &) { return true; }
bool AscendDeprecatedInterface::CloseTsd(const std::shared_ptr<MsContext> &, bool) { return true; }
bool AscendDeprecatedInterface::IsTsdOpened(const std::shared_ptr<MsContext> &) { return true; }
bool AscendDeprecatedInterface::CheckIsAscend910Soc() { return true; }
void AscendDeprecatedInterface::UnregisterExternalAllocator() {}
}  // namespace ascend
}  // namespace device
}  // namespace mindspore
