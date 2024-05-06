/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/cast_cpu_kernel.h"

#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <algorithm>
#include <complex>

#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
template <typename S, typename T>
class CastCpuKernelFunc : public CpuKernelFunc {
 public:
  CastCpuKernelFunc() = default;
  ~CastCpuKernelFunc() override = default;

  bool RunFunc(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
               const std::vector<KernelTensor *> &outputs) override;

 private:
  TypeId source_dtype_{kTypeUnknown};
  TypeId target_dtype_{kTypeUnknown};
};

template <typename S, typename T>
void Cast(CastCpuKernelFunc<S, T> *content, const S *in, T *out, size_t size) {
  auto task = [&in, &out](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      if constexpr (std::is_same_v<S, T>) {
        out[i] = static_cast<T>(in[i]);
      } else if constexpr (std::is_same_v<S, bool> && std::is_same_v<T, std::complex<float>>) {
        out[i] = std::complex<float>(in[i] ? 1.0f : 0.0f, 0.0f);
      } else if constexpr (std::is_same_v<S, bool> && std::is_same_v<T, std::complex<double>>) {
        out[i] = std::complex<double>(in[i] ? 1.0 : 0.0, 0.0);
      } else if constexpr ((std::is_same_v<S, std::complex<float>>) || (std::is_same_v<S, std::complex<double>>)) {
        out[i] = static_cast<T>(std::real(in[i]));
      } else if constexpr ((std::is_same_v<T, std::complex<float>>) || (std::is_same_v<T, std::complex<double>>)) {
        double realValue = static_cast<double>(in[i]);
        std::complex<double> complexValue(realValue, 0.0);
        out[i] = (std::is_same_v<T, std::complex<float>>) ? static_cast<T>(complexValue) : complexValue;
      } else {
        out[i] = static_cast<T>(in[i]);
      }
    }
  };
  ParallelLaunchAutoSearch(task, size, content, &content->parallel_search_info_);
}

template <typename S, typename T>
bool CastCpuKernelFunc<S, T>::RunFunc(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                                      const std::vector<KernelTensor *> &outputs) {
  const auto *input = reinterpret_cast<S *>(inputs[0]->device_ptr());
  auto *output = reinterpret_cast<T *>(outputs[0]->device_ptr());
  MS_LOG(DEBUG) << "Type source: " << typeid(S).name() << "; target: " << typeid(T).name();
  Cast<S, T>(this, input, output, outputs[0]->size() / sizeof(T));
  return true;
}

template <typename S, typename T>
std::shared_ptr<CpuKernelFunc> CreateCastFunc() {
  return std::make_shared<CastCpuKernelFunc<S, T>>();
}
using CastCpuKernelFuncCreator = std::function<std::shared_ptr<CpuKernelFunc>()>;

static std::vector<std::pair<KernelAttr, CastCpuKernelFuncCreator>> kernel_attr_lists = {
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<uint8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<uint8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<uint8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<uint8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<uint8_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<uint8_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<uint8_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<uint8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<uint8_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<uint8_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<uint8_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<uint8_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint8_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint8_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeBool), CreateCastFunc<uint8_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<uint16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<uint16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<uint16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<uint16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<uint16_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<uint16_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<uint16_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<uint16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<uint16_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<uint16_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<uint16_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<uint16_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeBool), CreateCastFunc<uint16_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint16_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint16_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<uint32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<uint32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<uint32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<uint32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<uint32_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<uint32_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<uint32_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<uint32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<uint32_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<uint32_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<uint32_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<uint32_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeBool), CreateCastFunc<uint32_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint32_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint32_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<uint64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<uint64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<uint64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<uint64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<uint64_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<uint64_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<uint64_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<uint64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<uint64_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<uint64_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<uint64_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<uint64_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeBool), CreateCastFunc<uint64_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint64_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint64_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<int8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<int8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<int8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<int8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<int8_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<int8_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<int8_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<int8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<int8_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<int8_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<int8_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<int8_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeBool), CreateCastFunc<int8_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int8_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int8_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<int16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<int16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<int16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<int16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<int16_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<int16_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<int16_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<int16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<int16_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<int16_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<int16_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<int16_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeBool), CreateCastFunc<int16_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int16_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int16_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<int32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<int32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<int32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<int32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<int32_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<int32_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<int32_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<int32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<int32_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<int32_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<int32_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<int32_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeBool), CreateCastFunc<int32_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int32_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int32_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<int64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<int64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<int64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<int64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<int64_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<int64_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<int64_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<int64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<int64_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<int64_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<int64_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<int64_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool), CreateCastFunc<int64_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int64_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int64_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<float16, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<float16, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<float16, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<float16, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<float16, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<float16, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<float16, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<float16, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<float16, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<float16, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<float16, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<float16, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeBool), CreateCastFunc<float16, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<float16, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<float16, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<float, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<float, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<float, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<float, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<float, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<float, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<float, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<float, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<float, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<float, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<float, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<float, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeBool), CreateCastFunc<float, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<float, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<float, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<double, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<double, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<double, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<double, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<double, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<double, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<double, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<double, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<double, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<double, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<double, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<double, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeBool), CreateCastFunc<double, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<double, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<double, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<bfloat16, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<bfloat16, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<bfloat16, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<bfloat16, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<bfloat16, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<bfloat16, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<bfloat16, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<bfloat16, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<bfloat16, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<bfloat16, float>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<bfloat16, double>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<bfloat16, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeBool), CreateCastFunc<bfloat16, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<bfloat16, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<bfloat16, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt8), CreateCastFunc<bool, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt16), CreateCastFunc<bool, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt32), CreateCastFunc<bool, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt64), CreateCastFunc<bool, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt8), CreateCastFunc<bool, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt16), CreateCastFunc<bool, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt32), CreateCastFunc<bool, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt64), CreateCastFunc<bool, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeFloat16), CreateCastFunc<bool, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeFloat32), CreateCastFunc<bool, float>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeFloat64), CreateCastFunc<bool, double>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeBFloat16), CreateCastFunc<bool, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeBool), CreateCastFunc<bool, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<bool, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<bool, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<std::complex<float>, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<std::complex<float>, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<std::complex<float>, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<std::complex<float>, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<std::complex<float>, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<std::complex<float>, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<std::complex<float>, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<std::complex<float>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<std::complex<float>, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<std::complex<float>, float>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<std::complex<float>, double>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<std::complex<float>, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<std::complex<float>, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<std::complex<float>, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<std::complex<double>, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<std::complex<double>, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<std::complex<double>, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<std::complex<double>, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<std::complex<double>, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<std::complex<double>, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<std::complex<double>, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<std::complex<double>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<std::complex<double>, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<std::complex<double>, float>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<std::complex<double>, double>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<std::complex<double>, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<std::complex<double>, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<std::complex<double>, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<std::complex<double>, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint8_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint8_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint8_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint8_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint8_t, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint8_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint8_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint8_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint8_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint8_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint16_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint16_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint16_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint16_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint16_t, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint16_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint16_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<uint16_t, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint16_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint16_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint16_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint32_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint32_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint32_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint32_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint32_t, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint32_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint32_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<uint32_t, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint32_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint32_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint32_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint64_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint64_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint64_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint64_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint64_t, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint64_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint64_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<uint64_t, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint64_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint64_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint64_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int8_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int8_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int8_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int8_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int8_t, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int8_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int8_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int8_t, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int8_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int8_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int8_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int16_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int16_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int16_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int16_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int16_t, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int16_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int16_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int16_t, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int16_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int16_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int16_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int32_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int32_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int32_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int32_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int32_t, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int32_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int32_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int32_t, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int32_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int32_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int32_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int64_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int64_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int64_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int64_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int64_t, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int64_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int64_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int64_t, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int64_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int64_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int64_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<float16, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<float16, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<float16, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<float16, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<float16, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<float16, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<float16, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<float16, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<float16, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<float16, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<float16, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<float16, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<float16, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<float16, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<float16, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<float, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<float, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<float, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<float, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<float, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<float, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<float, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<float, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<float, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<float, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<float, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<float, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<float, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<float, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<float, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<double, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<double, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<double, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<double, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<double, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<double, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<double, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<double, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<double, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<double, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<double, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<double, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<double, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<double, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<double, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<bfloat16, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<bfloat16, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<bfloat16, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<bfloat16, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<bfloat16, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<bfloat16, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<bfloat16, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<bfloat16, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<bfloat16, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<bfloat16, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<bfloat16, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<bfloat16, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<bfloat16, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<bfloat16, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<bfloat16, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<bool, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<bool, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<bool, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<bool, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<bool, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<bool, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<bool, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<bool, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<bool, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<bool, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<bool, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<bool, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<bool, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<bool, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<bool, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<std::complex<float>, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<std::complex<float>, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<std::complex<float>, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<std::complex<float>, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<std::complex<float>, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<std::complex<float>, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<std::complex<float>, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<std::complex<float>, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<std::complex<float>, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<std::complex<float>, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<std::complex<float>, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<std::complex<float>, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<std::complex<float>, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<std::complex<float>, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<std::complex<float>, std::complex<double>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<std::complex<double>, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<std::complex<double>, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<std::complex<double>, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<std::complex<double>, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<std::complex<double>, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<std::complex<double>, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<std::complex<double>, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<std::complex<double>, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<std::complex<double>, float16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<std::complex<double>, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<std::complex<double>, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<std::complex<double>, bfloat16>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<std::complex<double>, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<std::complex<double>, std::complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<std::complex<double>, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint8_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint8_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint8_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint8_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint8_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint8_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<uint8_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint8_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint8_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint8_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint16_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint16_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint16_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint16_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint16_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint16_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<uint16_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint16_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint16_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint16_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint32_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint32_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint32_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint32_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint32_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint32_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<uint32_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint32_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint32_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint32_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint64_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint64_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint64_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint64_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint64_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint64_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<uint64_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint64_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint64_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint64_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int8_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int8_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int8_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int8_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int8_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int8_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int8_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int8_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int8_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int8_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int16_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int16_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int16_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int16_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int16_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int16_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int16_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int16_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int16_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int16_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int32_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int32_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int32_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int32_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int32_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int32_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int32_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int32_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int32_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int32_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int64_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int64_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int64_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int64_t, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int64_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int64_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int64_t, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int64_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int64_t, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int64_t, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<float16, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<float16, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<float16, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<float16, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<float16, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<float16, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<float16, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<float16, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<float16, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<float16, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<float16, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<float16, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<float16, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<float16, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<float16, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<float, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<float, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<float, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<float, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<float, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<float, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<float, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<float, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<float, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<float, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<float, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<float, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<float, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<float, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<float, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<double, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<double, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<double, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<double, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<double, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<double, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<double, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<double, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<double, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<double, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<double, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<double, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<double, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<double, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<double, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<bfloat16, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<bfloat16, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<bfloat16, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<bfloat16, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<bfloat16, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<bfloat16, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<bfloat16, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<bfloat16, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<bfloat16, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<bfloat16, float>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<bfloat16, double>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<bfloat16, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<bfloat16, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<bfloat16, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeBFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<bfloat16, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<bool, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<bool, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<bool, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<bool, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<bool, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<bool, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<bool, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<bool, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<bool, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<bool, float>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<bool, double>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<bool, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<bool, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<bool, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<bool, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<std::complex<float>, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<std::complex<float>, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<std::complex<float>, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<std::complex<float>, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<std::complex<float>, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<std::complex<float>, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<std::complex<float>, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<std::complex<float>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<std::complex<float>, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<std::complex<float>, float>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<std::complex<float>, double>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<std::complex<float>, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<std::complex<float>, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<std::complex<float>, std::complex<double>>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<std::complex<double>, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<std::complex<double>, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<std::complex<double>, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<std::complex<double>, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<std::complex<double>, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<std::complex<double>, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<std::complex<double>, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<std::complex<double>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<std::complex<double>, float16>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<std::complex<double>, float>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<std::complex<double>, double>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<std::complex<double>, bfloat16>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<std::complex<double>, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<std::complex<double>, std::complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<std::complex<double>, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint8_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint8_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint8_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint8_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint8_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint8_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint8_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint8_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint8_t, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint8_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint8_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint8_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint8_t, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint8_t, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint16_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint16_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint16_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint16_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint16_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint16_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint16_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint16_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint16_t, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint16_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint16_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<uint16_t, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint16_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint16_t, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint16_t, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint32_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint32_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint32_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint32_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint32_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint32_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint32_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint32_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint32_t, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint32_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint32_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<uint32_t, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint32_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint32_t, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint32_t, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<uint64_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<uint64_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<uint64_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<uint64_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<uint64_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<uint64_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<uint64_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<uint64_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<uint64_t, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<uint64_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<uint64_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<uint64_t, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<uint64_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<uint64_t, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<uint64_t, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int8_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int8_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int8_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int8_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int8_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int8_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int8_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int8_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int8_t, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int8_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int8_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int8_t, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int8_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int8_t, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int8_t, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int16_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int16_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int16_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int16_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int16_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int16_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int16_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int16_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int16_t, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int16_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int16_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int16_t, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int16_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int16_t, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int16_t, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int32_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int32_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int32_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int32_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int32_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int32_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int32_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int32_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int32_t, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int32_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int32_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int32_t, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int32_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int32_t, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int32_t, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<int64_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<int64_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<int64_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<int64_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<int64_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<int64_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<int64_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<int64_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<int64_t, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<int64_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<int64_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<int64_t, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<int64_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<int64_t, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<int64_t, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<float16, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<float16, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<float16, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<float16, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<float16, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<float16, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<float16, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<float16, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<float16, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<float16, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<float16, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<float16, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<float16, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<float16, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<float16, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<float, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<float, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<float, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<float, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<float, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<float, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<float, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<float, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<float, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<float, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<float, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<float, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<float, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<float, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<float, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<double, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<double, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<double, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<double, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<double, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<double, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<double, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<double, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<double, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<double, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<double, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<double, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<double, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<double, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<double, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<bfloat16, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<bfloat16, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<bfloat16, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<bfloat16, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<bfloat16, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<bfloat16, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<bfloat16, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<bfloat16, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<bfloat16, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<bfloat16, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<bfloat16, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<bfloat16, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<bfloat16, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<bfloat16, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<bfloat16, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<bool, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<bool, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<bool, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<bool, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<bool, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<bool, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<bool, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<bool, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<bool, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<bool, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<bool, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<bool, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<bool, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<bool, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<bool, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<std::complex<float>, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<std::complex<float>, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<std::complex<float>, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<std::complex<float>, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<std::complex<float>, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<std::complex<float>, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<std::complex<float>, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<std::complex<float>, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<std::complex<float>, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<std::complex<float>, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<std::complex<float>, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<std::complex<float>, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<std::complex<float>, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<std::complex<float>, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<std::complex<float>, std::complex<double>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   CreateCastFunc<std::complex<double>, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   CreateCastFunc<std::complex<double>, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   CreateCastFunc<std::complex<double>, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   CreateCastFunc<std::complex<double>, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   CreateCastFunc<std::complex<double>, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   CreateCastFunc<std::complex<double>, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   CreateCastFunc<std::complex<double>, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateCastFunc<std::complex<double>, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   CreateCastFunc<std::complex<double>, float16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateCastFunc<std::complex<double>, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   CreateCastFunc<std::complex<double>, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBFloat16),
   CreateCastFunc<std::complex<double>, bfloat16>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   CreateCastFunc<std::complex<double>, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   CreateCastFunc<std::complex<double>, std::complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   CreateCastFunc<std::complex<double>, std::complex<double>>}};
}  // namespace

bool CastCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  source_dtype_ = inputs[kIndex0]->dtype_id();
  target_dtype_ = outputs[kIndex0]->dtype_id();

  ResetKernelFunc(inputs, outputs);
  return true;
}

std::vector<KernelAttr> CastCpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list;
  if (support_list.empty()) {
    (void)std::transform(kernel_attr_lists.begin(), kernel_attr_lists.end(), std::back_inserter(support_list),
                         [](const std::pair<KernelAttr, CastCpuKernelFuncCreator> &pair) { return pair.first; });
  }
  return support_list;
}

int CastCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  MS_LOG(DEBUG) << "Cast resize info :input : " << TypeIdToType(inputs[0]->dtype_id())->ToString()
                << ", out : " << TypeIdToType(outputs[0]->dtype_id())->ToString();
  ResetKernelFunc(inputs, outputs);
  return KernelMod::Resize(inputs, outputs);
}

void CastCpuKernelMod::ResetKernelFunc(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  std::vector<KernelAttr> support_list;
  (void)std::transform(kernel_attr_lists.begin(), kernel_attr_lists.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, CastCpuKernelFuncCreator> &pair) { return pair.first; });
  auto [is_match, index] = MatchKernelAttr(kernel_attr, support_list);
  if (!is_match) {
    MS_LOG(EXCEPTION) << "Cast does not support this kernel data type: " << kernel_attr;
  }

  kernel_func_ = kernel_attr_lists[index].second();
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Cast, CastCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
