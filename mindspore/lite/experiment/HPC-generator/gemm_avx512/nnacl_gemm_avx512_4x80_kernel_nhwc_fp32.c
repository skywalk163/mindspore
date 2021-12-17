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

// nnacl gemm in x86 avx512 asm code
void nnacl_gemm_avx512_4x80_kernel_nhwc_fp32(float *dst, const float *src, const float *weight, const float *bias,
                                             const size_t act_flag, const size_t row_block, const size_t col_block,
                                             const size_t deep, const size_t src_stride, const size_t dst_stride,
                                             const size_t inc_flag) {
  const float *dst_3 = dst + 3 * dst_stride;
  size_t deep_t = deep >> 3;
  size_t dst_stride_t = dst_stride << 2;
  asm volatile(
    // inc in deep
    "and $0x1, %[inc_flag]\n"
    "je 0f\n"
    "vmovups 0(%[dst_0]), %%zmm0\n"
    "vmovups 64(%[dst_0]), %%zmm1\n"
    "vmovups 128(%[dst_0]), %%zmm2\n"
    "vmovups 192(%[dst_0]), %%zmm3\n"
    "vmovups 256(%[dst_0]), %%zmm4\n"
    "vmovups 0(%[dst_0], %[dst_stride], 1), %%zmm5\n"
    "vmovups 64(%[dst_0], %[dst_stride], 1), %%zmm6\n"
    "vmovups 128(%[dst_0], %[dst_stride], 1), %%zmm7\n"
    "vmovups 192(%[dst_0], %[dst_stride], 1), %%zmm8\n"
    "vmovups 256(%[dst_0], %[dst_stride], 1), %%zmm9\n"
    "vmovups 0(%[dst_0], %[dst_stride], 2), %%zmm10\n"
    "vmovups 64(%[dst_0], %[dst_stride], 2), %%zmm11\n"
    "vmovups 128(%[dst_0], %[dst_stride], 2), %%zmm12\n"
    "vmovups 192(%[dst_0], %[dst_stride], 2), %%zmm13\n"
    "vmovups 256(%[dst_0], %[dst_stride], 2), %%zmm14\n"
    "vmovups 0(%[dst_3]), %%zmm15\n"
    "vmovups 64(%[dst_3]), %%zmm16\n"
    "vmovups 128(%[dst_3]), %%zmm17\n"
    "vmovups 192(%[dst_3]), %%zmm18\n"
    "vmovups 256(%[dst_3]), %%zmm19\n"
    "jmp 2f\n"
    "0:\n"
    "cmpq $0, %[bias]\n"
    "je 1f\n"
    "vmovaps 0(%[bias]), %%zmm0\n"
    "vmovaps 64(%[bias]), %%zmm1\n"
    "vmovaps 128(%[bias]), %%zmm2\n"
    "vmovaps 192(%[bias]), %%zmm3\n"
    "vmovaps 256(%[bias]), %%zmm4\n"
    "vmovaps 0(%[bias]), %%zmm5\n"
    "vmovaps 64(%[bias]), %%zmm6\n"
    "vmovaps 128(%[bias]), %%zmm7\n"
    "vmovaps 192(%[bias]), %%zmm8\n"
    "vmovaps 256(%[bias]), %%zmm9\n"
    "vmovaps 0(%[bias]), %%zmm10\n"
    "vmovaps 64(%[bias]), %%zmm11\n"
    "vmovaps 128(%[bias]), %%zmm12\n"
    "vmovaps 192(%[bias]), %%zmm13\n"
    "vmovaps 256(%[bias]), %%zmm14\n"
    "vmovaps 0(%[bias]), %%zmm15\n"
    "vmovaps 64(%[bias]), %%zmm16\n"
    "vmovaps 128(%[bias]), %%zmm17\n"
    "vmovaps 192(%[bias]), %%zmm18\n"
    "vmovaps 256(%[bias]), %%zmm19\n"
    "jmp 2f\n"
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
    "2:\n"
    :
    : [ dst_0 ] "r"(dst), [ bias ] "r"(bias), [ dst_stride ] "r"(dst_stride_t), [ inc_flag ] "r"(inc_flag),
      [ dst_3 ] "r"(dst_3)
    : "%zmm0", "%zmm1", "%zmm2", "%zmm3", "%zmm4", "%zmm5", "%zmm6", "%zmm7", "%zmm8", "%zmm9", "%zmm10", "%zmm11",
      "%zmm12", "%zmm13", "%zmm14", "%zmm15", "%zmm16", "%zmm17", "%zmm18", "%zmm19");
  const float *src_3 = src + 3 * src_stride;
  size_t src_stride_t = src_stride << 2;
  asm volatile(
    "0:\n"
    // block 0
    "vmovups 0(%[weight]), %%zmm31\n"
    "vmovups 64(%[weight]), %%zmm30\n"
    "vmovups 128(%[weight]), %%zmm29\n"
    "vmovups 192(%[weight]), %%zmm28\n"
    "vmovups 256(%[weight]), %%zmm27\n"
    "vbroadcastss 0(%[src_0]), %%zmm26\n"
    "vbroadcastss 0(%[src_0], %[src_stride], 1), %%zmm25\n"
    "vbroadcastss 0(%[src_0], %[src_stride], 2), %%zmm24\n"
    "vbroadcastss 0(%[src_3]), %%zmm23\n"
    "vfmadd231ps %%zmm31, %%zmm26, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm26, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm26, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm26, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm26, %%zmm4\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm6\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm7\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm8\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm9\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm11\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm12\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm13\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm14\n"
    "vfmadd231ps %%zmm31, %%zmm23, %%zmm15\n"
    "vfmadd231ps %%zmm30, %%zmm23, %%zmm16\n"
    "vfmadd231ps %%zmm29, %%zmm23, %%zmm17\n"
    "vfmadd231ps %%zmm28, %%zmm23, %%zmm18\n"
    "vfmadd231ps %%zmm27, %%zmm23, %%zmm19\n"
    // block 1
    "vmovups 320(%[weight]), %%zmm31\n"
    "vmovups 384(%[weight]), %%zmm30\n"
    "vmovups 448(%[weight]), %%zmm29\n"
    "vmovups 512(%[weight]), %%zmm28\n"
    "vmovups 576(%[weight]), %%zmm27\n"
    "vbroadcastss 4(%[src_0]), %%zmm26\n"
    "vbroadcastss 4(%[src_0], %[src_stride], 1), %%zmm25\n"
    "vbroadcastss 4(%[src_0], %[src_stride], 2), %%zmm24\n"
    "vbroadcastss 4(%[src_3]), %%zmm23\n"
    "vfmadd231ps %%zmm31, %%zmm26, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm26, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm26, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm26, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm26, %%zmm4\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm6\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm7\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm8\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm9\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm11\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm12\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm13\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm14\n"
    "vfmadd231ps %%zmm31, %%zmm23, %%zmm15\n"
    "vfmadd231ps %%zmm30, %%zmm23, %%zmm16\n"
    "vfmadd231ps %%zmm29, %%zmm23, %%zmm17\n"
    "vfmadd231ps %%zmm28, %%zmm23, %%zmm18\n"
    "vfmadd231ps %%zmm27, %%zmm23, %%zmm19\n"
    // block 2
    "vmovups 640(%[weight]), %%zmm31\n"
    "vmovups 704(%[weight]), %%zmm30\n"
    "vmovups 768(%[weight]), %%zmm29\n"
    "vmovups 832(%[weight]), %%zmm28\n"
    "vmovups 896(%[weight]), %%zmm27\n"
    "vbroadcastss 8(%[src_0]), %%zmm26\n"
    "vbroadcastss 8(%[src_0], %[src_stride], 1), %%zmm25\n"
    "vbroadcastss 8(%[src_0], %[src_stride], 2), %%zmm24\n"
    "vbroadcastss 8(%[src_3]), %%zmm23\n"
    "vfmadd231ps %%zmm31, %%zmm26, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm26, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm26, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm26, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm26, %%zmm4\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm6\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm7\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm8\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm9\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm11\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm12\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm13\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm14\n"
    "vfmadd231ps %%zmm31, %%zmm23, %%zmm15\n"
    "vfmadd231ps %%zmm30, %%zmm23, %%zmm16\n"
    "vfmadd231ps %%zmm29, %%zmm23, %%zmm17\n"
    "vfmadd231ps %%zmm28, %%zmm23, %%zmm18\n"
    "vfmadd231ps %%zmm27, %%zmm23, %%zmm19\n"
    // block 3
    "vmovups 960(%[weight]), %%zmm31\n"
    "vmovups 1024(%[weight]), %%zmm30\n"
    "vmovups 1088(%[weight]), %%zmm29\n"
    "vmovups 1152(%[weight]), %%zmm28\n"
    "vmovups 1216(%[weight]), %%zmm27\n"
    "vbroadcastss 12(%[src_0]), %%zmm26\n"
    "vbroadcastss 12(%[src_0], %[src_stride], 1), %%zmm25\n"
    "vbroadcastss 12(%[src_0], %[src_stride], 2), %%zmm24\n"
    "vbroadcastss 12(%[src_3]), %%zmm23\n"
    "vfmadd231ps %%zmm31, %%zmm26, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm26, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm26, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm26, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm26, %%zmm4\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm6\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm7\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm8\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm9\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm11\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm12\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm13\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm14\n"
    "vfmadd231ps %%zmm31, %%zmm23, %%zmm15\n"
    "vfmadd231ps %%zmm30, %%zmm23, %%zmm16\n"
    "vfmadd231ps %%zmm29, %%zmm23, %%zmm17\n"
    "vfmadd231ps %%zmm28, %%zmm23, %%zmm18\n"
    "vfmadd231ps %%zmm27, %%zmm23, %%zmm19\n"
    // block 4
    "vmovups 1280(%[weight]), %%zmm31\n"
    "vmovups 1344(%[weight]), %%zmm30\n"
    "vmovups 1408(%[weight]), %%zmm29\n"
    "vmovups 1472(%[weight]), %%zmm28\n"
    "vmovups 1536(%[weight]), %%zmm27\n"
    "vbroadcastss 16(%[src_0]), %%zmm26\n"
    "vbroadcastss 16(%[src_0], %[src_stride], 1), %%zmm25\n"
    "vbroadcastss 16(%[src_0], %[src_stride], 2), %%zmm24\n"
    "vbroadcastss 16(%[src_3]), %%zmm23\n"
    "vfmadd231ps %%zmm31, %%zmm26, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm26, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm26, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm26, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm26, %%zmm4\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm6\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm7\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm8\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm9\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm11\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm12\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm13\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm14\n"
    "vfmadd231ps %%zmm31, %%zmm23, %%zmm15\n"
    "vfmadd231ps %%zmm30, %%zmm23, %%zmm16\n"
    "vfmadd231ps %%zmm29, %%zmm23, %%zmm17\n"
    "vfmadd231ps %%zmm28, %%zmm23, %%zmm18\n"
    "vfmadd231ps %%zmm27, %%zmm23, %%zmm19\n"
    // block 5
    "vmovups 1600(%[weight]), %%zmm31\n"
    "vmovups 1664(%[weight]), %%zmm30\n"
    "vmovups 1728(%[weight]), %%zmm29\n"
    "vmovups 1792(%[weight]), %%zmm28\n"
    "vmovups 1856(%[weight]), %%zmm27\n"
    "vbroadcastss 20(%[src_0]), %%zmm26\n"
    "vbroadcastss 20(%[src_0], %[src_stride], 1), %%zmm25\n"
    "vbroadcastss 20(%[src_0], %[src_stride], 2), %%zmm24\n"
    "vbroadcastss 20(%[src_3]), %%zmm23\n"
    "vfmadd231ps %%zmm31, %%zmm26, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm26, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm26, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm26, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm26, %%zmm4\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm6\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm7\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm8\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm9\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm11\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm12\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm13\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm14\n"
    "vfmadd231ps %%zmm31, %%zmm23, %%zmm15\n"
    "vfmadd231ps %%zmm30, %%zmm23, %%zmm16\n"
    "vfmadd231ps %%zmm29, %%zmm23, %%zmm17\n"
    "vfmadd231ps %%zmm28, %%zmm23, %%zmm18\n"
    "vfmadd231ps %%zmm27, %%zmm23, %%zmm19\n"
    // block 6
    "vmovups 1920(%[weight]), %%zmm31\n"
    "vmovups 1984(%[weight]), %%zmm30\n"
    "vmovups 2048(%[weight]), %%zmm29\n"
    "vmovups 2112(%[weight]), %%zmm28\n"
    "vmovups 2176(%[weight]), %%zmm27\n"
    "vbroadcastss 24(%[src_0]), %%zmm26\n"
    "vbroadcastss 24(%[src_0], %[src_stride], 1), %%zmm25\n"
    "vbroadcastss 24(%[src_0], %[src_stride], 2), %%zmm24\n"
    "vbroadcastss 24(%[src_3]), %%zmm23\n"
    "vfmadd231ps %%zmm31, %%zmm26, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm26, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm26, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm26, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm26, %%zmm4\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm6\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm7\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm8\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm9\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm11\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm12\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm13\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm14\n"
    "vfmadd231ps %%zmm31, %%zmm23, %%zmm15\n"
    "vfmadd231ps %%zmm30, %%zmm23, %%zmm16\n"
    "vfmadd231ps %%zmm29, %%zmm23, %%zmm17\n"
    "vfmadd231ps %%zmm28, %%zmm23, %%zmm18\n"
    "vfmadd231ps %%zmm27, %%zmm23, %%zmm19\n"
    // block 7
    "vmovups 2240(%[weight]), %%zmm31\n"
    "vmovups 2304(%[weight]), %%zmm30\n"
    "vmovups 2368(%[weight]), %%zmm29\n"
    "vmovups 2432(%[weight]), %%zmm28\n"
    "vmovups 2496(%[weight]), %%zmm27\n"
    "vbroadcastss 28(%[src_0]), %%zmm26\n"
    "vbroadcastss 28(%[src_0], %[src_stride], 1), %%zmm25\n"
    "vbroadcastss 28(%[src_0], %[src_stride], 2), %%zmm24\n"
    "vbroadcastss 28(%[src_3]), %%zmm23\n"
    "vfmadd231ps %%zmm31, %%zmm26, %%zmm0\n"
    "vfmadd231ps %%zmm30, %%zmm26, %%zmm1\n"
    "vfmadd231ps %%zmm29, %%zmm26, %%zmm2\n"
    "vfmadd231ps %%zmm28, %%zmm26, %%zmm3\n"
    "vfmadd231ps %%zmm27, %%zmm26, %%zmm4\n"
    "vfmadd231ps %%zmm31, %%zmm25, %%zmm5\n"
    "vfmadd231ps %%zmm30, %%zmm25, %%zmm6\n"
    "vfmadd231ps %%zmm29, %%zmm25, %%zmm7\n"
    "vfmadd231ps %%zmm28, %%zmm25, %%zmm8\n"
    "vfmadd231ps %%zmm27, %%zmm25, %%zmm9\n"
    "vfmadd231ps %%zmm31, %%zmm24, %%zmm10\n"
    "vfmadd231ps %%zmm30, %%zmm24, %%zmm11\n"
    "vfmadd231ps %%zmm29, %%zmm24, %%zmm12\n"
    "vfmadd231ps %%zmm28, %%zmm24, %%zmm13\n"
    "vfmadd231ps %%zmm27, %%zmm24, %%zmm14\n"
    "vfmadd231ps %%zmm31, %%zmm23, %%zmm15\n"
    "vfmadd231ps %%zmm30, %%zmm23, %%zmm16\n"
    "vfmadd231ps %%zmm29, %%zmm23, %%zmm17\n"
    "vfmadd231ps %%zmm28, %%zmm23, %%zmm18\n"
    "vfmadd231ps %%zmm27, %%zmm23, %%zmm19\n"
    "dec %[deep]\n"
    "add $2560, %[weight]\n"
    "add $32, %[src_0]\n"
    "add $32, %[src_3]\n"
    "jg 0b\n"

    "and $0x2, %[inc_flag]\n"
    "je 3f\n"
    "movq %[act_flag], %%rax\n"
    "and $0x3, %%eax\n"
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
    "and $0x1, %%eax\n"
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
    "3:\n"
    "vmovups %%zmm0, 0(%[dst_0])\n"
    "vmovups %%zmm1, 64(%[dst_0])\n"
    "vmovups %%zmm2, 128(%[dst_0])\n"
    "vmovups %%zmm3, 192(%[dst_0])\n"
    "vmovups %%zmm4, 256(%[dst_0])\n"
    "vmovups %%zmm5, 0(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm6, 64(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm7, 128(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm8, 192(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm9, 256(%[dst_0], %[dst_stride], 1)\n"
    "vmovups %%zmm10, 0(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm11, 64(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm12, 128(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm13, 192(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm14, 256(%[dst_0], %[dst_stride], 2)\n"
    "vmovups %%zmm15, 0(%[dst_3])\n"
    "vmovups %%zmm16, 64(%[dst_3])\n"
    "vmovups %%zmm17, 128(%[dst_3])\n"
    "vmovups %%zmm18, 192(%[dst_3])\n"
    "vmovups %%zmm19, 256(%[dst_3])\n"
    :
    : [ src_0 ] "r"(src), [ src_stride ] "r"(src_stride_t), [ weight ] "r"(weight), [ deep ] "r"(deep_t),
      [ inc_flag ] "r"(inc_flag), [ act_flag ] "r"(act_flag), [ dst_0 ] "r"(dst), [ dst_stride ] "r"(dst_stride_t),
      [ dst_3 ] "r"(dst_3), [ src_3 ] "r"(src_3)
    : "%rax", "%zmm0", "%zmm1", "%zmm2", "%zmm3", "%zmm4", "%zmm5", "%zmm6", "%zmm7", "%zmm8", "%zmm9", "%zmm10",
      "%zmm11", "%zmm12", "%zmm13", "%zmm14", "%zmm15", "%zmm16", "%zmm17", "%zmm18", "%zmm19", "%zmm20", "%zmm21",
      "%zmm22", "%zmm23", "%zmm24", "%zmm25", "%zmm26", "%zmm27", "%zmm28", "%zmm29", "%zmm30", "%zmm31");
}
