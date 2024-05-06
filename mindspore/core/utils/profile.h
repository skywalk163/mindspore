/**
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CORE_UTILS_PROFILE_H_
#define MINDSPORE_CORE_UTILS_PROFILE_H_

#include <utility>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "utils/log_adapter.h"
#include "utils/compile_config.h"

namespace mindspore {
inline bool *EnabledProfilePtr() {
  static auto enabled_profile = common::GetCompileConfig("COMPILE_PROFILE") == "1";
  return &enabled_profile;
}

inline void SetEnabledProfile(bool enabled) {
  if (EnabledProfilePtr() != nullptr) {
    *EnabledProfilePtr() = enabled;
  }
}

inline bool EnabledProfile() {
#ifdef ENABLE_PROFILE
  return true;
#else
  return *EnabledProfilePtr();
#endif
}

struct TimeInfo;
using TimeInfoMap = std::map<std::string, const TimeInfo *>;

MS_CORE_API double GetTime();

class ProfileBase;

struct TimeInfo {
  explicit TimeInfo(double time = -1.0) : time_(time), dict_(nullptr), actionNum_(0) {}
  TimeInfo(const TimeInfo &) = delete;
  TimeInfo &operator=(const TimeInfo &) = delete;
  ~TimeInfo();

  double time_;
  TimeInfoMap *dict_;
  size_t actionNum_;
};

// Utility class for Profile.
class MS_CORE_API ProfContext {
  friend class Profile;
  friend class ProfileBase;
  friend class ProfTransaction;

 public:
  ProfContext(const std::string &name, ProfileBase *const prof);
  ~ProfContext();

  ProfContext(const ProfContext &) = delete;
  ProfContext &operator=(const ProfContext &) = delete;

  void SetTime(double time) noexcept;
  void Insert(const std::string &name, const TimeInfo *time) noexcept;
  bool IsTopContext() const noexcept;

  // Used for Execute break.
  void set_start_time(double start_time) { start_time_ = start_time; }
  double start_time() { return start_time_; }

 private:
  std::string name_;
  ProfileBase *prof_;
  ProfContext *parent_;
  TimeInfo *time_info_;
  double start_time_{-1.0};  // Used for Execute break.
};

class MS_CORE_API ProfileBase {
  friend class ProfContext;
  friend class ProfTransaction;

 public:
  ProfileBase();
  virtual ~ProfileBase();

  virtual void Print() {}
  virtual ProfContext *Step(const std::string &) { return nullptr; }
  virtual ProfContext *Lap(int) { return nullptr; }
  virtual void Pop() {}

  // top level profile context
  ProfContext context_;
  // profile context pointer, act as a stack pointer
  ProfContext *ctx_ptr_ = nullptr;
};

class MS_CORE_API Profile : public ProfileBase {
 public:
  Profile() = default;
  ~Profile() override = default;
  Profile(const Profile &) = delete;
  Profile &operator=(const Profile &) = delete;

  void Print() override;
  ProfContext *Step(const std::string &name) override;
  ProfContext *Lap(int count) override;
  void Pop() noexcept override;
};

class MS_CORE_API ProfTransaction {
 public:
  explicit ProfTransaction(const ProfileBase *prof);
  explicit ProfTransaction(ProfContext *const ctx);
  ProfTransaction(const ProfTransaction &) = delete;
  ProfTransaction &operator=(const ProfTransaction &) = delete;
  ~ProfTransaction();

  template <class Function>
  void Execute(const Function &func) {
    if (ctx_ == nullptr) {
      func();
      return;
    }

    double start_time = GetTime();
    // Set for Execute break.
    ctx_->set_start_time(start_time);
    func();
    double end_time = GetTime();
    ctx_->SetTime(end_time - start_time);
  }

 private:
  ProfContext *ctx_ = nullptr;
};

class NoProfTransaction {
 public:
  explicit NoProfTransaction(ProfileBase *) {}
  explicit NoProfTransaction(ProfContext *) {}
  ~NoProfTransaction() = default;

  template <class Function>
  void Execute(const Function &func) const {
    func();
  }
};

class MS_CORE_API DumpTime {
 public:
  DumpTime(const DumpTime &) = delete;
  DumpTime &operator=(const DumpTime &) = delete;
  ~DumpTime();
  static DumpTime &GetInstance();
  void set_file_path(const std::string &save_path) { file_path_ = save_path; }
  void Record(const std::string &step_name, const double time, const bool is_start);
  void Save();

 private:
  DumpTime() = default;
  std::stringstream file_ss_;
  std::ofstream file_out_;
  std::string file_path_ = "./timeline.json";
};

struct TimeStat {
  TimeStat() : time_(0.0), count_(0) {}
  ~TimeStat() = default;

  void operator+=(double t) {
    time_ += t;
    count_ += 1;
  }

  TimeStat operator+(double t) {
    TimeStat ts = *this;
    ts += t;
    return ts;
  }

  double time_;
  int count_;
};

class MS_CORE_API MsProfile {
 public:
  ~MsProfile();

  static void Reset();
  static ProfileBase *GetProfile();
  static void StatTime(const std::string &id, double time);
  static void Print();

 private:
  MsProfile() = default;

  static MsProfile &GetSingleton();

  void Clear();

  std::map<std::string, TimeStat> time_stat_;  // record time and count info from some activity
  ProfileBase *profile_ = nullptr;             // record hierarchical profile info
};

template <typename Function>
void ProfileExecute(ProfileBase *profile, const Function &func) {
  if (EnabledProfile()) {
    ProfTransaction(profile).Execute(func);
  } else {
    NoProfTransaction(profile).Execute(func);
  }
}

inline void ProfileExecuteBreak(ProfileBase *profile) {
  if (!EnabledProfile()) {
    return;
  }

  auto ctx = profile->ctx_ptr_;
  if (ctx != nullptr && ctx->start_time() != -1.0) {
    double end_time = GetTime();
    ctx->SetTime(end_time - ctx->start_time());
  }
}

template <typename Function>
void ProfileExecute(ProfContext *profile_ctx, const Function &func) {
  if (EnabledProfile()) {
    ProfTransaction(profile_ctx).Execute(func);
  } else {
    NoProfTransaction(profile_ctx).Execute(func);
  }
}
class MsProfileStatGuard {
 public:
  explicit MsProfileStatGuard(std::string &&state_name) {
    if (!EnabledProfile()) {
      return;
    }
    start_ = GetTime();
    state_name_ = std::move(state_name);
  }

  ~MsProfileStatGuard() {
    if (!EnabledProfile()) {
      return;
    }
    if (interrupted_) {
      return;
    }
    auto end = GetTime();
    MsProfile::StatTime(state_name_, end - start_);
  }

  void Interrupt() { interrupted_ = true; }

 private:
  std::string state_name_;
  double start_;
  bool interrupted_{false};
};

struct MemoryInfo {
  std::string name{""};
  int64_t start_memory{-1};
  int64_t end_memory{-1};
  size_t depth{0};
};

class MS_CORE_API ProcessStatus {
 public:
  ~ProcessStatus() = default;
  static ProcessStatus &GetInstance();
  // Get current process status by a key. Only useful for Linux.
  int64_t GetMemoryCost(const std::string &key) const;
  // Start to record memory increase info. It must be used with RecordEnd().
  // If previous record not end, the next record will have indent when printed.
  void RecordStart(const std::string &step_name);
  // End to record memory increase info. It must be used with RecordStart().
  void RecordEnd();
  // Print memory increase info which are recorded.
  void Print();
  // Clear all records.
  void Clear();

 private:
  ProcessStatus() = default;
  std::vector<MemoryInfo> stack_;
  std::vector<MemoryInfo> memory_used_;
};

}  // namespace mindspore
#endif  // MINDSPORE_CORE_UTILS_PROFILE_H_
