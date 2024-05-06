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

#ifndef NNACL_KERNEL_DEFAULT_KERNEL_BASE_H_
#define NNACL_KERNEL_DEFAULT_KERNEL_BASE_H_

#include "nnacl/op_base.h"
#include "nnacl/tensor_c.h"
#include "nnacl/kernel.h"

int DefaultPrepare3In2Out(KernelBase *self);
int DefaultPrepare1In1Out(KernelBase *self);
int DefaultPrepare2In1Out(KernelBase *self);
int DefaultPrepare1In2Out(KernelBase *self);
int DefaultPrepare3In1Out(KernelBase *self);
int DefaultResize(KernelBase *self);
int DefaultRelease(KernelBase *self);

#endif  // NNACL_KERNEL_DEFAULT_KERNEL_BASE_H_
