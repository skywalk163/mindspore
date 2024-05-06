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

#include "debug/data_dump/data_dumper.h"
#include <utility>

namespace mindspore {
namespace datadump {

DataDumperRegister &DataDumperRegister::Instance() {
  static DataDumperRegister dumper_register;
  return dumper_register;
}

void DataDumperRegister::RegistDumper(device::DeviceType backend, const std::shared_ptr<DataDumper> &dumper_ptr) {
  registered_dumpers_.emplace(std::pair<device::DeviceType, std::shared_ptr<DataDumper>>{backend, dumper_ptr});
}

std::shared_ptr<DataDumper> DataDumperRegister::GetDumperForBackend(device::DeviceType backend) {
  auto iter = registered_dumpers_.find(backend);
  if (iter == registered_dumpers_.end()) {
    return nullptr;
  }
  return iter->second;
}

class CpuDumpRegister {
 public:
  CpuDumpRegister() {
    MS_LOG(INFO) << "Register DataDumper for cpu backend\n";
    DataDumperRegister::Instance().RegistDumper(device::DeviceType::kCPU, std::make_shared<DataDumper>());
  }

  ~CpuDumpRegister() = default;
} g_cpu_dump_register;

}  // namespace datadump
}  // namespace mindspore
