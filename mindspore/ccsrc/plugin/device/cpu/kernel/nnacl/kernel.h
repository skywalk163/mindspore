/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#ifndef NNACL_KERNEL_H_
#define NNACL_KERNEL_H_

#include "nnacl/op_base.h"
#include "nnacl/infer/common_infer.h"

typedef struct ExecEnv {
  void *allocator_;
  void *thread_pool_;
  void *(*Alloc)(void *allocator, size_t sz);
  void (*Free)(void *allocator, void *ptr);
  int (*ParallelLaunch)(void *thread_pool, void *task, void *param, int task_num);
} ExecEnv;

typedef struct KernelBase {
  int (*Release)(struct KernelBase *self);
  int (*Prepare)(struct KernelBase *self);
  int (*Compute)(struct KernelBase *self);
  int (*Resize)(struct KernelBase *self);
  int (*InferShape)(struct KernelBase *self);
  int (*UpdateThread)(int32_t type, int64_t load, int64_t store, int64_t unit, int thread);
  OpParameter *param_;
  int thread_nr_;
  ExecEnv *env_;
  TensorC **in_;
  size_t in_size_;
  TensorC **out_;
  size_t out_size_;
  bool train_session_;
  void *workspace_; /* only used in train */
  int work_size_;   /* only used in train */
} KernelBase;

#ifdef _MSC_VER
#define REG_KERNEL_CREATOR(op, data_type, func)
#else
#define REG_KERNEL_CREATOR(op, data_type, func) \
  __attribute__((constructor(102))) void Reg##op##data_type##Creator() { RegKernelCreator(op, data_type, func); }
#endif

#define REGIST_DT(DataType) (DataType - kNumberTypeBegin - 1)
typedef KernelBase *(*KernelCreator)(OpParameter *param, int data_type);
void RegKernelCreator(int opType, int dataType, KernelCreator func);

#ifdef __cplusplus
extern "C" {
#endif
KernelBase *CreateKernel(OpParameter *param, TensorC **ins, size_t in_size, TensorC **outs, size_t out_size,
                         int data_type, ExecEnv *env);
bool SupportKernelC(int opType, int dataType);
#ifdef __cplusplus
}
#endif
#endif
