/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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
#include <x86intrin.h>
#include "nnacl/fp32/matmul_avx512_fp32.h"

// nnacl gemm in x86 avx512 asm code
void nnacl_gemm_avx512_4x96_kernel_nhwc_fp32(float *dst, const float *src, const float *weight, const float *bias,
                                             const size_t act_flag, const size_t row_block, const size_t col_block,
                                             const size_t depth, const size_t src_stride, const size_t dst_stride,
                                             const size_t inc_flag) {
  const float *dst_3 = dst + 3 * dst_stride;
  size_t dst_stride_t = dst_stride << 2;
  asm volatile(
    // inc in depth
    "movq %[inc_flag], %%rax\n"
    "and $0x1, %%rax\n"
    "je 0f\n"
    "vmovups 0(%[dst_0]), %%zmm0\n"
    "vmovups 64(%[dst_0]), %%zmm1\n"
    "vmovups 128(%[dst_0]), %%zmm2\n"
    "vmovups 192(%[dst_0]), %%zmm3\n"
    "vmovups 256(%[dst_0]), %%zmm4\n"
    "vmovups 320(%[dst_0]), %%zmm5\n"
    "vmovups 0(%[dst_0], %[dst_stride], 1), %%zmm6\n"
    "vmovups 64(%[dst_0], %[dst_stride], 1), %%zmm7\n"
    "vmovups 128(%[dst_0], %[dst_stride], 1), %%zmm8\n"
    "vmovups 192(%[dst_0], %[dst_stride], 1), %%zmm9\n"
    "vmovups 256(%[dst_0], %[dst_stride], 1), %%zmm10\n"
    "vmovups 320(%[dst_0], %[dst_stride], 1), %%zmm11\n"
    "vmovups 0(%[dst_0], %[dst_stride], 2), %%zmm12\n"
    "vmovups 64(%[dst_0], %[dst_stride], 2), %%zmm13\n"
    "vmovups 128(%[dst_0], %[dst_stride], 2), %%zmm14\n"
    "vmovups 192(%[dst_0], %[dst_stride], 2), %%zmm15\n"
    "vmovups 256(%[dst_0], %[dst_stride], 2), %%zmm16\n"
    "vmovups 320(%[dst_0], %[dst_stride], 2), %%zmm17\n"
    "vmovups 0(%[dst_3]), %%zmm18\n"
    "vmovups 64(%[dst_3]), %%zmm19\n"
    "vmovups 128(%[dst_3]), %%zmm20\n"
    "vmovups 192(%[dst_3]), %%zmm21\n"
    "vmovups 256(%[dst_3]), %%zmm22\n"
    "vmovups 320(%[dst_3]), %%zmm23\n"
    "jmp 2f\n"
    ".align 16\n"
    "0:\n"
    "cmpq $0, %[bias]\n"
    "je 1f\n"
    "vmovups 0(%[bias]), %%zmm0\n"
    "vmovups 64(%[bias]), %%zmm1\n"
    "vmovups 128(%[bias]), %%zmm2\n"
    "vmovups 192(%[bias]), %%zmm3\n"
    "vmovups 256(%[bias]), %%zmm4\n"
    "vmovups 320(%[bias]), %%zmm5\n"
    "vmovups 0(%[bias]), %%zmm6\n"
    "vmovups 64(%[bias]), %%zmm7\n"
    "vmovups 128(%[bias]), %%zmm8\n"
    "vmovups 192(%[bias]), %%zmm9\n"
    "vmovups 256(%[bias]), %%zmm10\n"
    "vmovups 320(%[bias]), %%zmm11\n"
    "vmovups 0(%[bias]), %%zmm12\n"
    "vmovups 64(%[bias]), %%zmm13\n"
    "vmovups 128(%[bias]), %%zmm14\n"
    "vmovups 192(%[bias]), %%zmm15\n"
    "vmovups 256(%[bias]), %%zmm16\n"
    "vmovups 320(%[bias]), %%zmm17\n"
    "vmovups 0(%[bias]), %%zmm18\n"
    "vmovups 64(%[bias]), %%zmm19\n"
    "vmovups 128(%[bias]), %%zmm20\n"
    "vmovups 192(%[bias]), %%zmm21\n"
    "vmovups 256(%[bias]), %%zmm22\n"
    "vmovups 320(%[bias]), %%zmm23\n"
    "jmp 2f\n"
    ".align 16\n"
    "1:\n"
    "vxorps %%zmm0, %%zmm0, %%zmm0\n"
    "vxorps %%zmm1, %%zmm1, %%zmm1\n"
    "vxorps %%zmm2, %%zmm2, %%zmm2\n"
    "vxorps %%zmm3, %%zmm3, %%zmm3\n"
    "vxorps %%zmm4, %%zmm4, %%zmm4\n"
    "vxorps %%zmm5, %%zmm5, %%zmm5\n"
    "vxorps %%zmm6, %%zmm6, %%zmm6\n"
    "vxorps %%zmm7, %%zmm7, %%zmm7\n"
    "vxorps %%zmm8, %%zmm8, %%zmm8\n"
    "vxorps %%zmm9, %%zmm9, %%zmm9\n"
    "vxorps %%zmm10, %%zmm10, %%zmm10\n"
    "vxorps %%zmm11, %%zmm11, %%zmm11\n"
    "vxorps %%zmm12, %%zmm12, %%zmm12\n"
    "vxorps %%zmm13, %%zmm13, %%zmm13\n"
    "vxorps %%zmm14, %%zmm14, %%zmm14\n"
    "vxorps %%zmm15, %%zmm15, %%zmm15\n"
    "vxorps %%zmm16, %%zmm16, %%zmm16\n"
    "vxorps %%zmm17, %%zmm17, %%zmm17\n"
    "vxorps %%zmm18, %%zmm18, %%zmm18\n"
    "vxorps %%zmm19, %%zmm19, %%zmm19\n"
    "vxorps %%zmm20, %%zmm20, %%zmm20\n"
    "vxorps %%zmm21, %%zmm21, %%zmm21\n"
    "vxorps %%zmm22, %%zmm22, %%zmm22\n"
    "vxorps %%zmm23, %%zmm23, %%zmm23\n"
    ".align 16\n"
    "2:\n"
    :
    : [ dst_0 ] "r"(dst), [ bias ] "r"(bias), [ dst_stride ] "r"(dst_stride_t), [ inc_flag ] "r"(inc_flag),
      [ dst_3 ] "r"(dst_3)
    : "%zmm0", "%zmm1", "%zmm2", "%zmm3", "%zmm4", "%zmm5", "%zmm6", "%zmm7", "%zmm8", "%zmm9", "%zmm10", "%zmm11",
      "%zmm12", "%zmm13", "%zmm14", "%zmm15", "%zmm16", "%zmm17", "%zmm18", "%zmm19", "%zmm20", "%zmm21", "%zmm22",
      "%zmm23");
  const float *src_3 = src + 3 * src_stride;
  size_t src_stride_t = src_stride << 2;
  asm volatile(
    "cmp $16, %[depth]\n"
    "jb 1f\n"
    ".align 16\n"
    "0:\n"
    // block 0
    "vmovups 0(%[weight]), %%zmm31\n"
    "vmovups 64(%[weight]), %%zmm30\n"
    "vmovups 128(%[weight]), %%zmm29\n"
    "vmovups 192(%[weight]), %%zmm28\n"
    "vmovups 256(%[weight]), %%zmm27\n"
    "vmovups 320(%[weight]), %%zmm26\n"
    "vbroadcastss 0(%[src_0]), %%zmm25\n"
    "vbroadcastss 0(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 0(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 0(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 1
    "vmovups 384(%[weight]), %%zmm31\n"
    "vmovups 448(%[weight]), %%zmm30\n"
    "vmovups 512(%[weight]), %%zmm29\n"
    "vmovups 576(%[weight]), %%zmm28\n"
    "vmovups 640(%[weight]), %%zmm27\n"
    "vmovups 704(%[weight]), %%zmm26\n"
    "vbroadcastss 4(%[src_0]), %%zmm25\n"
    "vbroadcastss 4(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 4(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 4(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 2
    "vmovups 768(%[weight]), %%zmm31\n"
    "vmovups 832(%[weight]), %%zmm30\n"
    "vmovups 896(%[weight]), %%zmm29\n"
    "vmovups 960(%[weight]), %%zmm28\n"
    "vmovups 1024(%[weight]), %%zmm27\n"
    "vmovups 1088(%[weight]), %%zmm26\n"
    "vbroadcastss 8(%[src_0]), %%zmm25\n"
    "vbroadcastss 8(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 8(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 8(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 3
    "vmovups 1152(%[weight]), %%zmm31\n"
    "vmovups 1216(%[weight]), %%zmm30\n"
    "vmovups 1280(%[weight]), %%zmm29\n"
    "vmovups 1344(%[weight]), %%zmm28\n"
    "vmovups 1408(%[weight]), %%zmm27\n"
    "vmovups 1472(%[weight]), %%zmm26\n"
    "vbroadcastss 12(%[src_0]), %%zmm25\n"
    "vbroadcastss 12(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 12(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 12(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 4
    "vmovups 1536(%[weight]), %%zmm31\n"
    "vmovups 1600(%[weight]), %%zmm30\n"
    "vmovups 1664(%[weight]), %%zmm29\n"
    "vmovups 1728(%[weight]), %%zmm28\n"
    "vmovups 1792(%[weight]), %%zmm27\n"
    "vmovups 1856(%[weight]), %%zmm26\n"
    "vbroadcastss 16(%[src_0]), %%zmm25\n"
    "vbroadcastss 16(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 16(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 16(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 5
    "vmovups 1920(%[weight]), %%zmm31\n"
    "vmovups 1984(%[weight]), %%zmm30\n"
    "vmovups 2048(%[weight]), %%zmm29\n"
    "vmovups 2112(%[weight]), %%zmm28\n"
    "vmovups 2176(%[weight]), %%zmm27\n"
    "vmovups 2240(%[weight]), %%zmm26\n"
    "vbroadcastss 20(%[src_0]), %%zmm25\n"
    "vbroadcastss 20(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 20(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 20(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 6
    "vmovups 2304(%[weight]), %%zmm31\n"
    "vmovups 2368(%[weight]), %%zmm30\n"
    "vmovups 2432(%[weight]), %%zmm29\n"
    "vmovups 2496(%[weight]), %%zmm28\n"
    "vmovups 2560(%[weight]), %%zmm27\n"
    "vmovups 2624(%[weight]), %%zmm26\n"
    "vbroadcastss 24(%[src_0]), %%zmm25\n"
    "vbroadcastss 24(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 24(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 24(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 7
    "vmovups 2688(%[weight]), %%zmm31\n"
    "vmovups 2752(%[weight]), %%zmm30\n"
    "vmovups 2816(%[weight]), %%zmm29\n"
    "vmovups 2880(%[weight]), %%zmm28\n"
    "vmovups 2944(%[weight]), %%zmm27\n"
    "vmovups 3008(%[weight]), %%zmm26\n"
    "vbroadcastss 28(%[src_0]), %%zmm25\n"
    "vbroadcastss 28(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 28(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 28(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 8
    "vmovups 3072(%[weight]), %%zmm31\n"
    "vmovups 3136(%[weight]), %%zmm30\n"
    "vmovups 3200(%[weight]), %%zmm29\n"
    "vmovups 3264(%[weight]), %%zmm28\n"
    "vmovups 3328(%[weight]), %%zmm27\n"
    "vmovups 3392(%[weight]), %%zmm26\n"
    "vbroadcastss 32(%[src_0]), %%zmm25\n"
    "vbroadcastss 32(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 32(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 32(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 9
    "vmovups 3456(%[weight]), %%zmm31\n"
    "vmovups 3520(%[weight]), %%zmm30\n"
    "vmovups 3584(%[weight]), %%zmm29\n"
    "vmovups 3648(%[weight]), %%zmm28\n"
    "vmovups 3712(%[weight]), %%zmm27\n"
    "vmovups 3776(%[weight]), %%zmm26\n"
    "vbroadcastss 36(%[src_0]), %%zmm25\n"
    "vbroadcastss 36(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 36(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 36(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 10
    "vmovups 3840(%[weight]), %%zmm31\n"
    "vmovups 3904(%[weight]), %%zmm30\n"
    "vmovups 3968(%[weight]), %%zmm29\n"
    "vmovups 4032(%[weight]), %%zmm28\n"
    "vmovups 4096(%[weight]), %%zmm27\n"
    "vmovups 4160(%[weight]), %%zmm26\n"
    "vbroadcastss 40(%[src_0]), %%zmm25\n"
    "vbroadcastss 40(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 40(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 40(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 11
    "vmovups 4224(%[weight]), %%zmm31\n"
    "vmovups 4288(%[weight]), %%zmm30\n"
    "vmovups 4352(%[weight]), %%zmm29\n"
    "vmovups 4416(%[weight]), %%zmm28\n"
    "vmovups 4480(%[weight]), %%zmm27\n"
    "vmovups 4544(%[weight]), %%zmm26\n"
    "vbroadcastss 44(%[src_0]), %%zmm25\n"
    "vbroadcastss 44(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 44(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 44(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 12
    "vmovups 4608(%[weight]), %%zmm31\n"
    "vmovups 4672(%[weight]), %%zmm30\n"
    "vmovups 4736(%[weight]), %%zmm29\n"
    "vmovups 4800(%[weight]), %%zmm28\n"
    "vmovups 4864(%[weight]), %%zmm27\n"
    "vmovups 4928(%[weight]), %%zmm26\n"
    "vbroadcastss 48(%[src_0]), %%zmm25\n"
    "vbroadcastss 48(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 48(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 48(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 13
    "vmovups 4992(%[weight]), %%zmm31\n"
    "vmovups 5056(%[weight]), %%zmm30\n"
    "vmovups 5120(%[weight]), %%zmm29\n"
    "vmovups 5184(%[weight]), %%zmm28\n"
    "vmovups 5248(%[weight]), %%zmm27\n"
    "vmovups 5312(%[weight]), %%zmm26\n"
    "vbroadcastss 52(%[src_0]), %%zmm25\n"
    "vbroadcastss 52(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 52(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 52(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 14
    "vmovups 5376(%[weight]), %%zmm31\n"
    "vmovups 5440(%[weight]), %%zmm30\n"
    "vmovups 5504(%[weight]), %%zmm29\n"
    "vmovups 5568(%[weight]), %%zmm28\n"
    "vmovups 5632(%[weight]), %%zmm27\n"
    "vmovups 5696(%[weight]), %%zmm26\n"
    "vbroadcastss 56(%[src_0]), %%zmm25\n"
    "vbroadcastss 56(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 56(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 56(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    // block 15
    "vmovups 5760(%[weight]), %%zmm31\n"
    "vmovups 5824(%[weight]), %%zmm30\n"
    "vmovups 5888(%[weight]), %%zmm29\n"
    "vmovups 5952(%[weight]), %%zmm28\n"
    "vmovups 6016(%[weight]), %%zmm27\n"
    "vmovups 6080(%[weight]), %%zmm26\n"
    "vbroadcastss 60(%[src_0]), %%zmm25\n"
    "vbroadcastss 60(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 60(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 60(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    "add $6144, %[weight]\n"
    "add $64, %[src_0]\n"
    "add $64, %[src_3]\n"
    "sub $16, %[depth]\n"
    "cmp $16, %[depth]\n"
    "jge 0b\n"
    "cmp $0, %[depth]\n"
    "je 2f\n"
    ".align 16\n"
    "1:\n"
    // block 0
    "vmovups 0(%[weight]), %%zmm31\n"
    "vmovups 64(%[weight]), %%zmm30\n"
    "vmovups 128(%[weight]), %%zmm29\n"
    "vmovups 192(%[weight]), %%zmm28\n"
    "vmovups 256(%[weight]), %%zmm27\n"
    "vmovups 320(%[weight]), %%zmm26\n"
    "vbroadcastss 0(%[src_0]), %%zmm25\n"
    "vbroadcastss 0(%[src_0], %[src_stride], 1), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm4\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm6\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm7\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm8\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm9\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm11\n"
    "vbroadcastss 0(%[src_0], %[src_stride], 2), %%zmm25\n"
    "vbroadcastss 0(%[src_3]), %%zmm24\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm12\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm13\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm14\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm15\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm16\n"
    "vfmadd231ps %%zmm26, %%zmm25, %%zmm17\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm18\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm19\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm20\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm21\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm22\n"
    "vfmadd231ps %%zmm26, %%zmm24, %%zmm23\n"
    "add $384, %[weight]\n"
    "add $4, %[src_0]\n"
    "add $4, %[src_3]\n"
    "dec %[depth]\n"
    "jg 1b\n"
    ".align 16\n"
    "2:\n"
    "and $0x2, %[inc_flag]\n"
    "je 3f\n"
    "and $0x3, %[act_flag]\n"
    "je 3f\n"
    // relu
    "vxorps %%zmm31, %%zmm31, %%zmm31\n"
    "vmaxps %%zmm0, %%zmm31, %%zmm0\n"
    "vmaxps %%zmm1, %%zmm31, %%zmm1\n"
    "vmaxps %%zmm2, %%zmm31, %%zmm2\n"
    "vmaxps %%zmm3, %%zmm31, %%zmm3\n"
    "vmaxps %%zmm4, %%zmm31, %%zmm4\n"
    "vmaxps %%zmm5, %%zmm31, %%zmm5\n"
    "vmaxps %%zmm6, %%zmm31, %%zmm6\n"
    "vmaxps %%zmm7, %%zmm31, %%zmm7\n"
    "vmaxps %%zmm8, %%zmm31, %%zmm8\n"
    "vmaxps %%zmm9, %%zmm31, %%zmm9\n"
    "vmaxps %%zmm10, %%zmm31, %%zmm10\n"
    "vmaxps %%zmm11, %%zmm31, %%zmm11\n"
    "vmaxps %%zmm12, %%zmm31, %%zmm12\n"
    "vmaxps %%zmm13, %%zmm31, %%zmm13\n"
    "vmaxps %%zmm14, %%zmm31, %%zmm14\n"
    "vmaxps %%zmm15, %%zmm31, %%zmm15\n"
    "vmaxps %%zmm16, %%zmm31, %%zmm16\n"
    "vmaxps %%zmm17, %%zmm31, %%zmm17\n"
    "vmaxps %%zmm18, %%zmm31, %%zmm18\n"
    "vmaxps %%zmm19, %%zmm31, %%zmm19\n"
    "vmaxps %%zmm20, %%zmm31, %%zmm20\n"
    "vmaxps %%zmm21, %%zmm31, %%zmm21\n"
    "vmaxps %%zmm22, %%zmm31, %%zmm22\n"
    "vmaxps %%zmm23, %%zmm31, %%zmm23\n"
    "and $0x1, %[act_flag]\n"
    "je 3f\n"
    // relu6
    "mov $0x40C00000, %%eax\n"
    "vmovd %%eax, %%xmm30\n"
    "vbroadcastss %%xmm30, %%zmm30\n"
    "vminps %%zmm0, %%zmm30, %%zmm0\n"
    "vminps %%zmm1, %%zmm30, %%zmm1\n"
    "vminps %%zmm2, %%zmm30, %%zmm2\n"
    "vminps %%zmm3, %%zmm30, %%zmm3\n"
    "vminps %%zmm4, %%zmm30, %%zmm4\n"
    "vminps %%zmm5, %%zmm30, %%zmm5\n"
    "vminps %%zmm6, %%zmm30, %%zmm6\n"
    "vminps %%zmm7, %%zmm30, %%zmm7\n"
    "vminps %%zmm8, %%zmm30, %%zmm8\n"
    "vminps %%zmm9, %%zmm30, %%zmm9\n"
    "vminps %%zmm10, %%zmm30, %%zmm10\n"
    "vminps %%zmm11, %%zmm30, %%zmm11\n"
    "vminps %%zmm12, %%zmm30, %%zmm12\n"
    "vminps %%zmm13, %%zmm30, %%zmm13\n"
    "vminps %%zmm14, %%zmm30, %%zmm14\n"
    "vminps %%zmm15, %%zmm30, %%zmm15\n"
    "vminps %%zmm16, %%zmm30, %%zmm16\n"
    "vminps %%zmm17, %%zmm30, %%zmm17\n"
    "vminps %%zmm18, %%zmm30, %%zmm18\n"
    "vminps %%zmm19, %%zmm30, %%zmm19\n"
    "vminps %%zmm20, %%zmm30, %%zmm20\n"
    "vminps %%zmm21, %%zmm30, %%zmm21\n"
    "vminps %%zmm22, %%zmm30, %%zmm22\n"
    "vminps %%zmm23, %%zmm30, %%zmm23\n"
    ".align 16\n"
    "3:\n"
    "vmovups %%zmm0, 0(%[dst_0])\n"
    "vmovups %%zmm1, 64(%[dst_0])\n"
    "vmovups %%zmm2, 128(%[dst_0])\n"
    "vmovups %%zmm3, 192(%[dst_0])\n"
    "vmovups %%zmm4, 256(%[dst_0])\n"
    "vmovups %%zmm5, 320(%[dst_0])\n"
    "vmovups %%zmm6, 0(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm7, 64(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm8, 128(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm9, 192(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm10, 256(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm11, 320(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm12, 0(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm13, 64(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm14, 128(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm15, 192(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm16, 256(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm17, 320(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm18, 0(%[dst_3])\n"
    "vmovups %%zmm19, 64(%[dst_3])\n"
    "vmovups %%zmm20, 128(%[dst_3])\n"
    "vmovups %%zmm21, 192(%[dst_3])\n"
    "vmovups %%zmm22, 256(%[dst_3])\n"
    "vmovups %%zmm23, 320(%[dst_3])\n"
    :
    : [ src_0 ] "r"(src), [ src_stride ] "r"(src_stride_t), [ weight ] "r"(weight), [ depth ] "r"(depth),
      [ inc_flag ] "r"(inc_flag), [ act_flag ] "r"(act_flag), [ dst_0 ] "r"(dst), [ dst_stride ] "r"(dst_stride_t),
      [ dst_3 ] "r"(dst_3), [ src_3 ] "r"(src_3)
    : "%rax", "%zmm0", "%zmm1", "%zmm2", "%zmm3", "%zmm4", "%zmm5", "%zmm6", "%zmm7", "%zmm8", "%zmm9", "%zmm10",
      "%zmm11", "%zmm12", "%zmm13", "%zmm14", "%zmm15", "%zmm16", "%zmm17", "%zmm18", "%zmm19", "%zmm20", "%zmm21",
      "%zmm22", "%zmm23", "%zmm24", "%zmm25", "%zmm26", "%zmm27", "%zmm28", "%zmm29", "%zmm30", "%zmm31");
}
