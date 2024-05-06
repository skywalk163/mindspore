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

#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/resize_linear_1d.cuh"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/util.cuh"

namespace {
struct AlignCornersFunc {
  __device__ void operator()(const double &new_x, const int &old_length, const int &new_length, double *old_x) const {
    *old_x = new_length != 1 ? new_x * (old_length - 1) / (new_length - 1) : 0;
  }
};

struct HalfPixelFunc {
  __device__ void operator()(const double &new_x, const int &old_length, const int &new_length, double *old_x) const {
    *old_x = new_length > 1 ? (new_x + 0.5) * old_length / new_length - 0.5 : 0;
  }
};

struct CachedInterpolation {
  size_t lower;
  size_t upper;
  double lerp;
};

template <typename TransformationT>
__device__ void InterpolationCal(const int64_t out_i, const int64_t in_width, const int64_t out_width,
                                 const TransformationT func, CachedInterpolation *ret) {
  double in_i;
  func(out_i, in_width, out_width, &in_i);
  const double in_floor = std::floor(in_i);
  const double in_ceil = std::ceil(in_i);
  ret->lower = static_cast<size_t>(in_floor > 0 ? in_floor : 0);
  ret->upper = static_cast<size_t>(in_ceil < static_cast<double>(in_width - 1) ? in_ceil : in_width - 1);
  ret->lerp = in_i - in_floor;
}
}  // namespace

template <typename TransformationT>
__global__ void ResizeLinear1DKernel(const int64_t output_size, const int64_t in_width, const int64_t out_width,
                                     const half *input, half *output, const TransformationT func) {
  for (int index = blockIdx.x * blockDim.x + threadIdx.x; index < output_size; index += blockDim.x * gridDim.x) {
    int64_t out_index = index / out_width;
    int64_t out_i = index % out_width;
    CachedInterpolation interp;
    InterpolationCal(out_i, in_width, out_width, func, &interp);
    size_t in_lower = interp.lower;
    size_t in_upper = interp.upper;
    double lerp = interp.lerp;

    const half left(*(input + out_index * in_width + in_lower));
    const half right(*(input + out_index * in_width + in_upper));
    *(output + out_index * out_width + out_i) = static_cast<half>(left + (right - left) * __float2half(lerp));
  }

  return;
}

template <typename T, typename TransformationT>
__global__ void ResizeLinear1DKernel(const int64_t output_size, const int64_t in_width, const int64_t out_width,
                                     const T *input, T *output, const TransformationT func) {
  for (int index = blockIdx.x * blockDim.x + threadIdx.x; index < output_size; index += blockDim.x * gridDim.x) {
    int64_t out_index = index / out_width;
    int64_t out_i = index % out_width;
    CachedInterpolation interp;
    InterpolationCal(out_i, in_width, out_width, func, &interp);
    size_t in_lower = interp.lower;
    size_t in_upper = interp.upper;
    double lerp = interp.lerp;

    const T left(static_cast<T>(*(input + out_index * in_width + in_lower)));
    const T right(static_cast<T>(*(input + out_index * in_width + in_upper)));
    *(output + out_index * out_width + out_i) = static_cast<T>(left + (right - left) * lerp);
  }

  return;
}

template <typename T>
cudaError_t ResizeLinear1D(const enum ResizeLinearCoordinateTransformationMode mode, const int64_t output_size,
                           const int64_t in_width, const int64_t out_width, const T *input, T *output,
                           const uint32_t device_id, cudaStream_t stream) {
  switch (mode) {
    case ALIGN_CORNERS:
      ResizeLinear1DKernel<T><<<CUDA_BLOCKS(device_id, output_size), CUDA_THREADS(device_id), 0, stream>>>(
        output_size, in_width, out_width, input, output, AlignCornersFunc());
      break;
    case HALF_PIXEL:
      ResizeLinear1DKernel<T><<<CUDA_BLOCKS(device_id, output_size), CUDA_THREADS(device_id), 0, stream>>>(
        output_size, in_width, out_width, input, output, HalfPixelFunc());
      break;
    default:
      break;
  }
  return GetCudaStatus();
}

template <>
cudaError_t ResizeLinear1D(const enum ResizeLinearCoordinateTransformationMode mode, const int64_t output_size,
                           const int64_t in_width, const int64_t out_width, const half *input, half *output,
                           const uint32_t device_id, cudaStream_t stream) {
  switch (mode) {
    case ALIGN_CORNERS:
      ResizeLinear1DKernel<<<CUDA_BLOCKS(device_id, output_size), CUDA_THREADS(device_id), 0, stream>>>(
        output_size, in_width, out_width, input, output, AlignCornersFunc());
      break;
    case HALF_PIXEL:
      ResizeLinear1DKernel<<<CUDA_BLOCKS(device_id, output_size), CUDA_THREADS(device_id), 0, stream>>>(
        output_size, in_width, out_width, input, output, HalfPixelFunc());
      break;
    default:
      break;
  }
  return GetCudaStatus();
}

__global__ void AssignToOutput(const int64_t output_size, float *grad_work, half *grad_input) {
  for (int i = blockIdx.x * blockDim.x + threadIdx.x; i < output_size; i += blockDim.x * gridDim.x) {
    grad_input[i] = __float2half(grad_work[i]);
  }
}

template <typename TransformationT>
__global__ void ResizeLinear1DGradKernel(const int64_t batch, const int64_t channel, const int64_t in_width,
                                         const int64_t out_width, const half *grad_output, float *grad_work,
                                         const TransformationT func) {
  for (int index = blockIdx.x * blockDim.x + threadIdx.x; index < out_width; index += blockDim.x * gridDim.x) {
    int64_t out_i = index % out_width;
    CachedInterpolation interp;
    InterpolationCal(out_i, in_width, out_width, func, &interp);
    size_t in_lower = interp.lower;
    size_t in_upper = interp.upper;
    double lerp = interp.lerp;
    for (int64_t n = 0; n < batch; ++n) {
      for (int64_t c = 0; c < channel; ++c) {
        (void)MsAtomicAdd<float>(
          grad_work + (n * channel + c) * in_width + in_lower,
          (__half2float(*(grad_output + (n * channel + c) * out_width + out_i)) * static_cast<float>(1 - lerp)));
        (void)MsAtomicAdd<float>(
          grad_work + (n * channel + c) * in_width + in_upper,
          (__half2float(*(grad_output + (n * channel + c) * out_width + out_i)) * static_cast<float>(lerp)));
      }
    }
  }
  return;
}

template <typename T, typename S, typename TransformationT>
__global__ void ResizeLinear1DGradKernel(const int64_t batch, const int64_t channel, const int64_t in_width,
                                         const int64_t out_width, const T *grad_output, S *grad_input,
                                         const TransformationT func) {
  int64_t output_size = batch * channel * out_width;
  for (int index = blockIdx.x * blockDim.x + threadIdx.x; index < output_size; index += blockDim.x * gridDim.x) {
    int64_t out_index = index / out_width;
    int64_t out_i = index % out_width;
    CachedInterpolation interp;
    InterpolationCal(out_i, in_width, out_width, func, &interp);
    size_t in_lower = interp.lower;
    size_t in_upper = interp.upper;
    double lerp = interp.lerp;
    (void)MsAtomicAdd<T>(grad_input + out_index * in_width + in_lower,
                         static_cast<T>((*(grad_output + out_index * out_width + out_i)) * (1 - lerp)));

    (void)MsAtomicAdd<T>(grad_input + out_index * in_width + in_upper,
                         static_cast<T>((*(grad_output + out_index * out_width + out_i)) * lerp));
  }
  return;
}

template <typename T>
cudaError_t ResizeLinear1DGrad(const enum ResizeLinearCoordinateTransformationMode mode, const int64_t batch,
                               const int64_t channel, const int64_t in_width, const int64_t out_width,
                               const T *grad_output, T *grad_input, float *grad_work, const uint32_t device_id,
                               cudaStream_t stream) {
  int64_t output_size = batch * channel * out_width;
  switch (mode) {
    case ALIGN_CORNERS:
      ResizeLinear1DGradKernel<T, T><<<CUDA_BLOCKS(device_id, output_size), CUDA_THREADS(device_id), 0, stream>>>(
        batch, channel, in_width, out_width, grad_output, grad_input, AlignCornersFunc());
      break;
    case HALF_PIXEL:
      ResizeLinear1DGradKernel<T, T><<<CUDA_BLOCKS(device_id, output_size), CUDA_THREADS(device_id), 0, stream>>>(
        batch, channel, in_width, out_width, grad_output, grad_input, HalfPixelFunc());
      break;
    default:
      break;
  }
  return GetCudaStatus();
}

template <>
cudaError_t ResizeLinear1DGrad(const enum ResizeLinearCoordinateTransformationMode mode, const int64_t batch,
                               const int64_t channel, const int64_t in_width, const int64_t out_width,
                               const half *grad_output, half *grad_input, float *grad_work, const uint32_t device_id,
                               cudaStream_t stream) {
  switch (mode) {
    case ALIGN_CORNERS:
      ResizeLinear1DGradKernel<<<CUDA_BLOCKS(device_id, out_width), CUDA_THREADS(device_id), 0, stream>>>(
        batch, channel, in_width, out_width, grad_output, grad_work, AlignCornersFunc());
      break;
    case HALF_PIXEL:
      ResizeLinear1DGradKernel<<<CUDA_BLOCKS(device_id, out_width), CUDA_THREADS(device_id), 0, stream>>>(
        batch, channel, in_width, out_width, grad_output, grad_work, HalfPixelFunc());
      break;
    default:
      break;
  }
  int64_t output_size = batch * channel * in_width;
  AssignToOutput<<<CUDA_BLOCKS(device_id, output_size), CUDA_THREADS(device_id), 0, stream>>>(output_size, grad_work,
                                                                                              grad_input);
  return GetCudaStatus();
}

#define RESIZE_LINEAR_1D_FUNC(T)                                                                                 \
  template CUDA_LIB_EXPORT cudaError_t ResizeLinear1D(                                                           \
    const enum ResizeLinearCoordinateTransformationMode mode, const int64_t output_size, const int64_t in_width, \
    const int64_t out_width, const T *input, T *output, const uint32_t device_id, cudaStream_t stream);

RESIZE_LINEAR_1D_FUNC(half)
RESIZE_LINEAR_1D_FUNC(float)
RESIZE_LINEAR_1D_FUNC(double)

template CUDA_LIB_EXPORT cudaError_t ResizeLinear1DGrad(const enum ResizeLinearCoordinateTransformationMode mode,
                                                        const int64_t batch, const int64_t channel,
                                                        const int64_t in_width, const int64_t out_width,
                                                        const half *grad_output, half *grad_input, float *grad_work,
                                                        const uint32_t device_id, cudaStream_t stream);

template CUDA_LIB_EXPORT cudaError_t ResizeLinear1DGrad(const enum ResizeLinearCoordinateTransformationMode mode,
                                                        const int64_t batch, const int64_t channel,
                                                        const int64_t in_width, const int64_t out_width,
                                                        const float *grad_output, float *grad_input, float *grad_work,
                                                        const uint32_t device_id, cudaStream_t stream);

template CUDA_LIB_EXPORT cudaError_t ResizeLinear1DGrad(const enum ResizeLinearCoordinateTransformationMode mode,
                                                        const int64_t batch, const int64_t channel,
                                                        const int64_t in_width, const int64_t out_width,
                                                        const double *grad_output, double *grad_input, float *grad_work,
                                                        const uint32_t device_id, cudaStream_t stream);
