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

#ifndef NNACL_KERNEL_STACK_H_
#define NNACL_KERNEL_STACK_H_

#include "nnacl/op_base.h"
#include "nnacl/tensor_c.h"
#include "nnacl/kernel.h"

#define NNACL_STACK_STEP 64

typedef struct StackStruct {
  KernelBase base_;
  TypeIdC data_type_;
  int axis_;
  int outer_size_;
  size_t copy_size_;
  void **buffers_;
} StackStruct;

KernelBase *CreateStack(OpParameter *param, int data_type);
int StackRun(void *cdata, int task_id, float l, float r);
int StackRelease(KernelBase *self);
int StackPrepare(KernelBase *self);
int StackResize(KernelBase *self);

#endif  // NNACL_KERNEL_STACK_H_
