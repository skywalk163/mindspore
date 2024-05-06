/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_HAL_PROFILER_ASCEND_PROFILING_H
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_HAL_PROFILER_ASCEND_PROFILING_H
#include <string>
#include <memory>
#include <map>
#include "include/backend/debug/profiler/profiling.h"
#include "acl/acl_prof.h"
#include "include/backend/kernel_graph.h"
#include "kernel/kernel.h"
#include "plugin/device/ascend/hal/profiler/profiling_data_dumper.h"

namespace mindspore {
namespace profiler {
namespace ascend {
class AscendProfiler : public Profiler {
 public:
  static std::shared_ptr<AscendProfiler> GetInstance();

  AscendProfiler() {}
  ~AscendProfiler() = default;
  AscendProfiler(const AscendProfiler &) = delete;
  AscendProfiler &operator=(const AscendProfiler &) = delete;
  void Init(const std::string &profiling_path, uint32_t device_id, const std::string &profiling_options) override;
  void Finalize() override;
  void Start() override;
  void Stop() override;
  void StepStart(uint64_t step_id, void *stream) override;
  void StepStop() override;
  void StepProfilingEnable(const bool enable_flag) override;
  void OpDataProducerEnd() override { return; }
  uint64_t GetOptionsMask(aclprofAicoreMetrics aic_metrics) const;
  void MsprofInitProfiler() const;
  void MsprofStopProfiler() const;
  aclprofAicoreMetrics GetAicMetrics() const;
  std::map<std::thread::id, uint32_t> last_tid_;
  std::map<std::thread::id, uint32_t> last_streamid_;

 protected:
  void SaveProfileData() override { return; }
  void ClearInst() override { return; }

 private:
  uint32_t device_id_ = 0;
  uint32_t max_op_taskid_limit_ = 65536;
  aclprofConfig *acl_config_{nullptr};
  aclprofStepInfo *acl_prof_step_info_{nullptr};
  aclrtStream acl_stream_{nullptr};
};
}  // namespace ascend
}  // namespace profiler
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_HAL_PROFILER_ASCEND_PROFILING_H
