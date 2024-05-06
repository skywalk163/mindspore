/**
 * Copyright 2021-2024 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUSTOM_CUSTOM_AOT_GPU_KERNEL_H
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUSTOM_CUSTOM_AOT_GPU_KERNEL_H

#ifdef _MSC_VER
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <functional>
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "kernel/common_utils.h"
#include "utils/custom_aot_extra.h"
#include "utils/file_utils.h"

namespace mindspore {
namespace kernel {
class CustomAOTGpuKernelMod : public NativeGpuKernelMod {
 public:
  CustomAOTGpuKernelMod() : handle_(nullptr), aot_func_(nullptr) {}
  ~CustomAOTGpuKernelMod() override {
    attrs_.DestructKernelData();

    if (handle_ != nullptr) {
#ifdef _MSC_VER
      FreeLibrary(handle_);
#else
      dlclose(handle_);
#endif
    }
  }

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    std::vector<void *> params;

    for (size_t i = 0; i < inputs.size(); i++) {
      params.push_back(static_cast<void *>(inputs[i]->device_ptr()));
    }
    for (size_t i = 0; i < outputs.size(); i++) {
      params.push_back(static_cast<void *>(outputs[i]->device_ptr()));
    }
    for (size_t i = 0; i < workspace.size(); i++) {
      params.push_back(static_cast<void *>(workspace[i]->device_ptr()));
    }
    if (!handle_) {
#ifdef _MSC_VER
      MS_LOG(ERROR) << "For '" << kernel_name_ << "' on GPU, dlopen file '" << file_path_
                    << "' should be successful, but error occurs! ";
#else
      MS_LOG(ERROR) << "For '" << kernel_name_ << "' on GPU, dlopen file '" << file_path_
                    << "' should be successful, but error occurs! Error message is: " << dlerror();
#endif
      return false;
    }

    if (!aot_func_) {
#ifdef _MSC_VER
      aot_func_ =
        reinterpret_cast<std::add_pointer<int(int, void **, int *, int64_t **, const char **, void *, void *)>::type>(
          GetProcAddress(handle_, func_name_.c_str()));
      if (aot_func_ == nullptr) {
        MS_LOG(ERROR) << "For '" << kernel_name_ << "' on GPU, error occurs when fetching function '" << func_name_;
        return false;
      }
#else
      aot_func_ =
        reinterpret_cast<std::add_pointer<int(int, void **, int *, int64_t **, const char **, void *, void *)>::type>(
          dlsym(handle_, func_name_.c_str()));
      if (auto error_info = dlerror(); error_info != nullptr) {
        MS_LOG(ERROR) << "For '" << kernel_name_ << "' on GPU, error occurs when fetching function '" << func_name_
                      << "'. Error info: " << error_info;
        return false;
      }
#endif
    }

    int nparam = SizeToInt(params.size());
    int ret = 0;
    try {
      if (nparam == 0) {
        ret = aot_func_(0, nullptr, nullptr, nullptr, nullptr, stream_ptr, nullptr);
      } else {
        ret = aot_func_(nparam, &params[0], &ndims_[0], &shapes_[0], &type_pointer_list_[0], stream_ptr,
                        reinterpret_cast<void *>(&attrs_));
      }
    } catch (const std::exception &e) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "' on GPU, operator failed when executing user defined file "
                    << file_path_ << "! "
                    << "Error message is " << e.what();
      return false;
    }

    if (ret != 0) {
      MS_LOG(EXCEPTION) << "Return value from GPU AOT kernel(" << file_path_ << ")'s function(" << func_name_ << ") is "
                        << ret << ". "
                        << "Any return value not equal to 0 will be treated as user defined error code and we will "
                           "terminate execution. If termination is not your purpose, please set return value to 0.";
    }

    return true;
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override {
    const auto &exec_info = GetValue<std::string>(primitive_->GetAttr("func_name"));
    if (auto pos = exec_info.find(":"); pos != std::string::npos) {
      auto path = exec_info.substr(0, pos);
      if (primitive_->HasAttr("path_from_env") && GetValue<bool>(primitive_->GetAttr("path_from_env"))) {
        const char *path_in_env = std::getenv(path.c_str());
        if (path_in_env == nullptr) {
          MS_LOG(WARNING) << "For '" << kernel_name_ << "' on GPU, the attr path_from_env is set but the env var ["
                          << path << "] is empty. Use [" << path << "] as the path to the library instead.";

        } else {
          path = std::string(path_in_env);
        }
      }
      auto real_path = FileUtils::GetRealPath(path.c_str());
      if (!real_path.has_value()) {
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "' on GPU, couldn't find the AOT binary file: " << path;
      }
      file_path_ = real_path.value();
      func_name_ = exec_info.substr(pos + 1);
      PathChecking();
    } else {
      MS_LOG(EXCEPTION)
        << "For '" << kernel_name_ << "' on GPU, user defined function path '" << exec_info
        << "' is illegal. Proper function path should follow the format of 'dir_path/file_name:func_name'";
    }

    for (size_t i = 0; i < inputs.size(); i++) {
      auto in_shape = inputs[i]->GetShapeVector();
      auto dtype = inputs[i]->dtype_id();
      (void)type_list_.emplace_back(TypeIdToString(dtype, true));
      ndims_.push_back(SizeToInt(in_shape.size()));
      (void)shape_list_.emplace_back(in_shape);
    }

    for (size_t i = 0; i < outputs.size(); i++) {
      auto out_shape = outputs[i]->GetShapeVector();
      auto dtype = outputs[i]->dtype_id();
      (void)shape_list_.emplace_back(out_shape);
      ndims_.push_back(SizeToInt(out_shape.size()));
      (void)type_list_.emplace_back(TypeIdToString(dtype, true));
    }

    std::transform(std::begin(shape_list_), std::end(shape_list_), std::back_inserter(shapes_),
                   [](auto &v) { return &v[0]; });
    std::transform(std::begin(type_list_), std::end(type_list_), std::back_inserter(type_pointer_list_),
                   [](auto &str) { return str.c_str(); });

    attrs_.SetKernelPrim(primitive_);

    if (!handle_) {
#ifdef _MSC_VER
      handle_ = LoadLibrary(file_path_.c_str());
      if (!handle_) {
        MS_LOG(ERROR) << "For '" << kernel_name_ << "' on GPU, dlopen file '" << file_path_
                      << "' should be successful, but error occurs! ";
        return false;
      }
#else
      handle_ = dlopen(file_path_.c_str(), RTLD_LAZY | RTLD_LOCAL);
      if (!handle_) {
        MS_LOG(ERROR) << "For '" << kernel_name_ << "' on GPU, dlopen file '" << file_path_
                      << "' should be successful, but error occurs! Error message is: " << dlerror();
        return false;
      }
#endif
    }
#ifdef _MSC_VER
    init_func_ = reinterpret_cast<std::add_pointer<int(int *, int64_t **, const char **, AotExtra *)>::type>(
      GetProcAddress(handle_, (func_name_ + "Init").c_str()));
#else
    init_func_ = reinterpret_cast<std::add_pointer<int(int *, int64_t **, const char **, AotExtra *)>::type>(
      dlsym(handle_, (func_name_ + "Init").c_str()));
#endif
    if (init_func_ != nullptr) {
      // Init func exist in the custom aot file
      // Call this init func to set custom op attrs
      int ret = 0;
      try {
        ret = init_func_(&ndims_[0], &shapes_[0], &type_pointer_list_[0], (&attrs_));
      } catch (const std::exception &e) {
        MS_LOG(ERROR) << "For '" << kernel_name_ << "' on GPU, operator failed when executing user defined file "
                      << file_path_ << "! "
                      << "Error message is " << e.what();
        return false;
      }

      if (ret != 0) {
        MS_LOG(EXCEPTION) << "Return value from GPU AOT kernel(" << file_path_ << ")'s function(" << func_name_
                          << ") is " << ret << ". "
                          << "Any return value not equal to 0 will be treated as user defined error code and we will "
                             "terminate execution. If termination is not your purpose, please set return value to 0.";
      }
    }
    return true;
  }

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override {
    int ret = KernelMod::Resize(inputs, outputs);
    if (ret != 0) {
      return ret;
    }

    shapes_.clear();
    shape_list_.clear();
    ndims_.clear();

    for (size_t i = 0; i < inputs.size(); i++) {
      auto in_shape = inputs[i]->GetShapeVector();
      (void)shape_list_.emplace_back(in_shape);
      ndims_.push_back(SizeToInt(in_shape.size()));
    }

    for (size_t i = 0; i < outputs.size(); i++) {
      auto out_shape = outputs[i]->GetShapeVector();
      (void)shape_list_.emplace_back(out_shape);
      ndims_.push_back(SizeToInt(out_shape.size()));
    }

    (void)std::transform(std::begin(shape_list_), std::end(shape_list_), std::back_inserter(shapes_),
                         [](auto &v) { return &v[0]; });

    workspace_size_list_ = attrs_.WorkSpace();
    return static_cast<int>(KRET_OK);
  }

 private:
  std::string file_path_;
  std::string func_name_;
#ifdef _MSC_VER
  HMODULE handle_{nullptr};
#else
  void *handle_{nullptr};
#endif
  int (*init_func_)(int *, int64_t **, const char **, AotExtra *);
  int (*aot_func_)(int, void **, int *, int64_t **, const char **, void *, void *);

  std::vector<std::vector<int64_t>> shape_list_;
  std::vector<int> ndims_;
  std::vector<std::string> type_list_;

  std::vector<int64_t *> shapes_;
  std::vector<const char *> type_pointer_list_;

  AotExtraImpl attrs_;

  void PathChecking() {
    constexpr auto kWhiteList = "MS_CUSTOM_AOT_WHITE_LIST";
    const char *value = std::getenv(kWhiteList);
    if (value == nullptr) {
      static bool print_gpu_warning_once = true;
      if (print_gpu_warning_once) {
        MS_LOG(INFO) << "For '" << kernel_name_ << "' on GPU, no white list is set and it might cause problems. "
                     << "Set the legal path of the file in MS_CUSTOM_AOT_WHITE_LIST.";
        print_gpu_warning_once = false;
      }
    } else {
      auto white_list = FileUtils::GetRealPath(value);
      if (!white_list.has_value()) {
        MS_LOG(EXCEPTION) << "Illegal white list path in MS_CUSTOM_AOT_WHITE_LIST: " << std::string(value);
      }
      constexpr auto kKernelMeta = "akg_kernel_meta";
      if (file_path_.find(white_list.value()) == std::string::npos &&
          file_path_.find(kKernelMeta) == std::string::npos) {
        MS_LOG(EXCEPTION)
          << "For '" << kernel_name_
          << "' on GPU, the file is not place in the legal path file defined by MS_CUSTOM_AOT_WHITE_LIST: "
          << white_list.value() << ". The file path is: " << file_path_;
      }
    }
  }
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUSTOM_CUSTOM_AOT_GPU_KERNEL_H
