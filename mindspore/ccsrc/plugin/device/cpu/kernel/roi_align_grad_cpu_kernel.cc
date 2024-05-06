/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#include <algorithm>
#include <utility>
#include <memory>
#include <map>
#include "plugin/device/cpu/kernel/atomic_add.h"
#include "plugin/device/cpu/kernel/roi_align_grad_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
template <typename T>
void bilinear_interpolate(const int height, const int width, T y, T x, int *x_low, int *y_low, int *x_high, int *y_high,
                          T *w1, T *w2, T *w3, T *w4) {
  constexpr float kEps = 0.00007;
  const T kZero = T(0.0);
  const T kOne = T(1.0);
  const T kMinusOne = static_cast<T>(-1.0);
  if (y < kMinusOne || y > static_cast<T>(height) || x < kMinusOne || x > static_cast<T>(width)) {
    *w1 = *w2 = *w3 = *w4 = kZero;
    *x_low = *x_high = *y_low = *y_high = -1;
    return;
  }

  // low bounder is at least zero
  y = y <= kZero ? kZero : y;
  x = x <= kZero ? kZero : x;

  // top left point
  *y_low = (y <= static_cast<T>(kEps) ? 0 : static_cast<int>(floor(y)));
  *x_low = (x <= static_cast<T>(kEps) ? 0 : static_cast<int>(floor(x)));

  // bottom right point
  if (*y_low >= height - 1) {
    *y_high = *y_low = height - 1;
    y = static_cast<T>(*y_low);
  } else {
    *y_high = *y_low + 1;
  }

  if (*x_low >= width - 1) {
    *x_high = *x_low = width - 1;
    x = static_cast<T>(*x_low);
  } else {
    *x_high = *x_low + 1;
  }

  // distance to nearest points
  T lx;
  T ly;
  T hx;
  T hy;
  ly = y - static_cast<T>(*y_low), lx = x - static_cast<T>(*x_low);
  hy = kOne - ly, hx = kOne - lx;

  // weight is evaluated by the distance to point away.
  //   the closer to point home, the more weight, the farther to point away.
  *w1 = hy * hx, *w2 = hy * lx, *w3 = ly * hx, *w4 = ly * lx;
  return;
}

template <typename T>
void bin_box(int thread_idx, const T *roi_boxes, int roi_cols, const T spatial_scale, const int sample_num,
             int roi_end_mode, const int channels, const int height, const int width, const int pooled_height,
             const int pooled_width, int *offset, int *n, int *c, int *ph, int *pw, int *roi_bin_grid_h,
             int *roi_bin_grid_w, T *bin_size_h, T *bin_size_w, T *roi_start_h, T *roi_start_w) {
  constexpr float kEps = 0.00007;
  constexpr int kStartW = 0;
  constexpr int kStartH = 1;
  constexpr int kEndW = 2;
  constexpr int kEndH = 3;
  constexpr size_t kRoisCols = 5;
  // (n, c, ph, pw) is the base param of pooled map
  *pw = thread_idx % pooled_width;
  *ph = (thread_idx / pooled_width) % pooled_height;
  *c = (thread_idx / pooled_width / pooled_height) % channels;
  *n = thread_idx / pooled_width / pooled_height / channels;

  // Roi has
  //   1. 4 points, or
  //   2. indicator + 4 points (1 + 4)
  const T *roi_box = roi_boxes + (*n) * roi_cols;
  int roi_batch_ind = 0;
  if (roi_cols == kRoisCols) {
    roi_batch_ind = FloatToInt(rintf(static_cast<float>(roi_box[0]) + kEps));
    roi_box++;
  }

  // Scale and shift ROI
  *roi_start_w = roi_box[kStartW] * spatial_scale;
  *roi_start_h = roi_box[kStartH] * spatial_scale;
  T roi_end_w = (roi_box[kEndW] + static_cast<T>(roi_end_mode)) * spatial_scale;
  T roi_end_h = (roi_box[kEndH] + static_cast<T>(roi_end_mode)) * spatial_scale;

  // New ROI height/width
  T roi_width = roi_end_w - (*roi_start_w);
  T roi_height = roi_end_h - (*roi_start_h);

  if (roi_end_mode == 0) {  // backward compatibility
    // Force malformed ROIs to be 1x1
    roi_width = roi_width > static_cast<T>(1.0) ? roi_width : static_cast<T>(1.0);
    roi_height = roi_height > static_cast<T>(1.0) ? roi_height : static_cast<T>(1.0);
  }

  // ratio of roi / pooled
  *bin_size_h = static_cast<T>(roi_height) / static_cast<T>(pooled_height);
  *bin_size_w = static_cast<T>(roi_width) / static_cast<T>(pooled_width);

  *offset = (roi_batch_ind * channels + (*c)) * height * width;

  // grid (int) by Sample ratio if defined, otherwise by pooled H/W
  *roi_bin_grid_h = (sample_num > 0) ? sample_num : static_cast<int>(floor(roi_height / static_cast<T>(pooled_height)));
  *roi_bin_grid_w = (sample_num > 0) ? sample_num : static_cast<int>(floor(roi_width / static_cast<T>(pooled_width)));
  return;
}
}  // namespace

bool ROIAlignGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  //  Get the number of the input args
  constexpr size_t kInputSize = 3;
  constexpr size_t kOutputSize = 1;
  if (inputs.size() != kInputSize) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of inputs must be 3, but got " << inputs.size()
                      << ".";
  }
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kOutputSize, kernel_name_);
  if (!MatchKernelFunc(kernel_name_, inputs, outputs)) {
    return false;
  }
  // Get primitive args
  pooled_height_ = LongToInt(GetValue<int64_t>(primitive_->GetAttr(ops::kPooledHeight)));
  pooled_width_ = LongToInt(GetValue<int64_t>(primitive_->GetAttr(ops::kPooledWidth)));
  spatial_scale_ = GetValue<float>(primitive_->GetAttr(ops::kSpatialScale));
  sample_num_ = LongToInt(GetValue<int64_t>(primitive_->GetAttr(ops::kSampleNum)));
  roi_end_mode_ = 1;
  return true;
}

int ROIAlignGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  std::vector<int64_t> xdiff_shape = inputs[kIndex2]->GetValueWithCheck<std::vector<int64_t>>();

  //  Get the input shapes
  auto dy_shape = inputs[kIndex0]->GetShapeVector();
  auto rois_shape = inputs[kIndex1]->GetShapeVector();
  constexpr size_t kDiffDims = 4;
  constexpr size_t kRoisDims = 2;
  if (dy_shape.size() != kDiffDims) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'dy' must be 4, but got " << dy_shape.size()
                  << ".";
    return KRET_RESIZE_FAILED;
  }
  if (rois_shape.size() != kRoisDims) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'rois' must be 2, but got " << rois_shape.size()
                  << ".";
    return KRET_RESIZE_FAILED;
  }
  if (xdiff_shape.size() > kDiffDims) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the length of xdiff_shape cannot be greater than 4, but got "
                  << xdiff_shape.size() << ".";
    return KRET_RESIZE_FAILED;
  }
  // Calculate the sizes of inputs and output
  auto dy_type_size = abstract::TypeIdSize(inputs[kIndex0]->dtype_id());
  dy_size_ = LongToSize(std::accumulate(dy_shape.begin(), dy_shape.end(), 1, std::multiplies{})) * dy_type_size;

  auto rois_type_size = abstract::TypeIdSize(inputs[kIndex1]->dtype_id());
  rois_size_ = LongToSize(std::accumulate(rois_shape.begin(), rois_shape.end(), 1, std::multiplies{})) * rois_type_size;
  roi_rows_ = LongToInt(rois_shape[kIndex0]);
  roi_cols_ = LongToInt(rois_shape[kIndex1]);

  output_size_ =
    LongToSize(std::accumulate(xdiff_shape.begin(), xdiff_shape.end(), 1, std::multiplies{})) * dy_type_size;
  batch_ = LongToInt(xdiff_shape[kIndex0]);
  channels_ = LongToInt(xdiff_shape[kIndex1]);
  height_ = LongToInt(xdiff_shape[kIndex2]);
  width_ = LongToInt(xdiff_shape[kIndex3]);

  ResetResource();
  InitSizeLists();
  return KRET_OK;
}

const ROIAlignGradCpuKernelMod::FuncList &ROIAlignGradCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, ROIAlignGradCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
     &ROIAlignGradCpuKernelMod::LaunchKernel<float>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
     &ROIAlignGradCpuKernelMod::LaunchKernel<float16>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat32),
     &ROIAlignGradCpuKernelMod::LaunchKernel<float>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat16),
     &ROIAlignGradCpuKernelMod::LaunchKernel<float16>},
  };
  return func_list;
}

template <typename T>
bool ROIAlignGradCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &workspace,
                                            const std::vector<KernelTensor *> &outputs) {
  const T *dy = static_cast<T *>(inputs[0]->device_ptr());
  const T *rois = static_cast<T *>(inputs[1]->device_ptr());
  T *dx = static_cast<T *>(outputs[0]->device_ptr());

  int size_init = batch_ * channels_ * height_ * width_;
  auto task1 = [this, &dx](size_t start, size_t end) {
    const T kZero = T(0.0);
    for (size_t thread_idx = start; thread_idx < end; thread_idx++) {
      dx[thread_idx] = kZero;
    }
  };
  ParallelLaunchAutoSearch(task1, IntToSize(size_init), this, &parallel_search_info_);

  int elem_num = roi_rows_ * channels_ * pooled_height_ * pooled_width_;
  auto task2 = [this, &dy, &rois, &dx](size_t start, size_t end) {
    const T kOffset = T(0.001);
    for (size_t thread_idx = start; thread_idx < end; thread_idx++) {
      int n = SizeToInt(thread_idx) / pooled_width_ / pooled_height_ / channels_;
      const T *roi_box = rois + n * roi_cols_;
      const T spatial_scale = static_cast<T>(spatial_scale_);
      if (roi_box[1] < kOffset && roi_box[3] < kOffset && roi_box[1] > -kOffset && roi_box[3] > -kOffset) {
        continue;
      }
      int offset = -1;
      int c;
      int ph;
      int pw;
      int roi_bin_grid_h;
      int roi_bin_grid_w;
      T bin_size_h;
      T bin_size_w;
      T roi_start_h;
      T roi_start_w;

      bin_box(SizeToInt(thread_idx), rois, roi_cols_, spatial_scale, sample_num_, roi_end_mode_, channels_, height_,
              width_, pooled_height_, pooled_width_, &offset, &n, &c, &ph, &pw, &roi_bin_grid_h, &roi_bin_grid_w,
              &bin_size_h, &bin_size_w, &roi_start_h, &roi_start_w);

      // (n, c, ph, pw) is the base param of pooled map
      const T count_points_in_grid_cell = static_cast<T>(roi_bin_grid_h) * static_cast<T>(roi_bin_grid_w);

      int top_offset = (n * channels_ + c) * pooled_height_ * pooled_width_;
      const T *offset_top_diff = dy + top_offset;
      const T top_diff_this_bin = offset_top_diff[ph * pooled_width_ + pw];

      for (int iy = 0; iy < roi_bin_grid_h; iy++) {
        // Shift half point RIGHT for y / x,  while previous scaled roi shift half point LEFT
        const T y = roi_start_h + static_cast<T>(ph) * bin_size_h +
                    static_cast<T>(iy + .5f) * bin_size_h / static_cast<T>(roi_bin_grid_h);
        for (int ix = 0; ix < roi_bin_grid_w; ix++) {
          const T x = roi_start_w + static_cast<T>(pw) * bin_size_w +
                      static_cast<T>(ix + .5f) * bin_size_w / static_cast<T>(roi_bin_grid_w);
          // bilinear interpolate by shifted y / x
          // calculate bilinear interpolation
          int x_low = 0;
          int y_low = 0;
          int x_high = 0;
          int y_high = 0;
          T w1;
          T w2;
          T w3;
          T w4;
          bilinear_interpolate(height_, width_, y, x, &x_low, &y_low, &x_high, &y_high, &w1, &w2, &w3, &w4);
          if (x_low >= 0 && x_high >= 0 && y_low >= 0 && y_high >= 0 && y_low < height_ && y_high < height_ &&
              x_low < width_ && x_high < width_) {
            T g1 = top_diff_this_bin * w1 / count_points_in_grid_cell;
            T g2 = top_diff_this_bin * w2 / count_points_in_grid_cell;
            T g3 = top_diff_this_bin * w3 / count_points_in_grid_cell;
            T g4 = top_diff_this_bin * w4 / count_points_in_grid_cell;

            T *dx_1 = dx + offset + y_low * width_ + x_low;
            T *dx_2 = dx + offset + y_low * width_ + x_high;
            T *dx_3 = dx + offset + y_high * width_ + x_low;
            T *dx_4 = dx + offset + y_high * width_ + x_high;

            AtomicAdd(dx_1, g1);
            AtomicAdd(dx_2, g2);
            AtomicAdd(dx_3, g3);
            AtomicAdd(dx_4, g4);
          }
        }
      }
    }
  };
  ParallelLaunchAutoSearch(task2, IntToSize(elem_num), this, &parallel_search_info_);
  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ROIAlignGrad, ROIAlignGradCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
