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
#include "src/litert/runtime_packed_node_pass.h"
#include "nnacl/op_base.h"
#include "nnacl/matmul_parameter.h"
#include "nnacl/nnacl_kernel.h"
#include "nnacl/kernel/matmul_struct.h"
#include "common/string_utils.h"

using RecoveryWeightFunc = void (*)(void *, void *, int, int, bool);
namespace mindspore {
namespace {
constexpr size_t kFlatbuffersBuilderInitSize = 1024;
constexpr auto kActivationType = "activation_type";
constexpr auto kTransposeA = "transpose_a";
constexpr auto kTransposeB = "transpose_b";
constexpr auto kArm64SimdDot = "ARM64SIMD_DOT";
const std::vector<std::string> kAttrString = {"activation_type", "transpose_a", "transpose_b", "b_batch", "col", "deep",
                                              "col_align",       "deep_align"};
}  // namespace

namespace lite {
PackedNodePass::~PackedNodePass() {
  for (auto &pack_info : node_pack_info_map_) {
    delete pack_info.second;
  }
  node_pack_info_map_.clear();
}

void PackedNodePass::Run(Model *model, const std::vector<Tensor *> &tensors) {
  CHECK_NULL_RETURN_VOID(model);
  LiteModel *lite_model = reinterpret_cast<LiteModel *>(model);
  CHECK_NULL_RETURN_VOID(lite_model);
  for (auto &node : model->graph_.all_nodes_) {
    MS_ASSERT(node != nullptr);
    if (node->node_type_ != static_cast<int>(schema::PrimitiveType_Custom)) {
      continue;
    }
    auto *primitive = reinterpret_cast<const schema::Primitive *>(node->primitive_);
    if (primitive == nullptr) {
      MS_LOG(ERROR) << "Op " << node->name_ << " should exist in model!";
      return;
    }
    auto custom = primitive->value_as_Custom();
    if (custom == nullptr || custom->type() == nullptr) {
      MS_LOG(ERROR) << "Custom node is nullptr";
      return;
    }
    auto custom_type = custom->type()->str();
    if (custom_type != "MatmulFusionPacked") {
      continue;
    }
    flatbuffers::FlatBufferBuilder fbb(kFlatbuffersBuilderInitSize);

    auto custom_attr = custom->attr();
    std::map<std::string, std::string> attr_map;
    for (uint32_t i = 0; i < custom_attr->size(); ++i) {
      auto attr = custom_attr->Get(i);
      auto attr_key = attr->name()->str();
      auto data_bytes = attr->data();
      uint32_t data_size = data_bytes->size();
      std::string attr_value;
      for (uint32_t j = 0; j < data_size; j++) {
        attr_value.push_back(static_cast<char>(data_bytes->Get(j)));
      }
      attr_map[attr_key] = attr_value;
    }
    for (auto &str : kAttrString) {
      if (attr_map.find(str) == attr_map.end() || !IsStrNumeric(attr_map[str])) {
        MS_LOG(ERROR) << "Custom attr error.";
        return;
      }
    }
    auto val_offset = schema::CreateMatMulFusion(
      fbb, static_cast<bool>(std::stoi(attr_map[kTransposeA])), static_cast<bool>(std::stoi(attr_map[kTransposeB])),
      static_cast<schema::ActivationType>(std::stoi(attr_map[kActivationType])));
    auto prim_offset = schema::CreatePrimitive(fbb, schema::PrimitiveType_MatMulFusion, val_offset.o);
    fbb.Finish(prim_offset);
    void *prim = malloc(fbb.GetSize());
    if (prim == nullptr) {
      MS_LOG(ERROR) << "malloc primitive failed.";
      return;
    }
    (void)memcpy(prim, reinterpret_cast<void *>(fbb.GetBufferPointer()), fbb.GetSize());
    auto custom_primitive = flatbuffers::GetRoot<schema::Primitive>(prim);
    fbb.Clear();
    PackInfo *pack_info = new (std::nothrow) PackInfo();
    if (pack_info == nullptr) {
      free(prim);
      MS_LOG(ERROR) << "new PackInfo failed.";
      return;
    }
    lite_model->node_bufs_.push_back(prim);
    node->primitive_ = custom_primitive;
    pack_info->is_packed_ = true;
    pack_info->b_batch_ = std::stoi(attr_map["b_batch"]);
    pack_info->col_ = std::stoi(attr_map["col"]);
    pack_info->deep_ = std::stoi(attr_map["deep"]);
    pack_info->col_align_ = std::stoi(attr_map["col_align"]);
    pack_info->deep_align_ = std::stoi(attr_map["deep_align"]);
    pack_info->b_transpose_ = static_cast<bool>(std::stoi(attr_map[kTransposeB]));
    pack_info->cpu_option_ = attr_map["cpu_option"];
    AddNodePackInfo(node->name_, pack_info);
    if (node->quant_type_ == static_cast<int>(schema::QuantType_QUANT_DYNAMIC)) {
      pack_info->weight_sums_index_ = static_cast<int>(node->input_indices_.back());
      node->input_indices_.pop_back();
      if (!(lite_model->keep_model_buf())) {
        auto index = pack_info->weight_sums_index_;
        if (index < 0 || index > static_cast<int>(tensors.size())) {
          MS_LOG(ERROR) << "weight sums tensor index is error.";
          return;
        }
        auto tensor = tensors[static_cast<size_t>(index)];
        CopyWeightBiasSumsTensor(tensor);
      }
    }

    node->node_type_ = static_cast<int>(schema::PrimitiveType_MatMulFusion);
  }
}

void PackedNodePass::CopyWeightBiasSumsTensor(Tensor *tensor) {
  if (!tensor->IsConst() && tensor->data() != nullptr) {
    return;
  }
  if (!tensor->IsConst() || tensor->own_data()) {
    return;
  }
  if (tensor->data_type() == kObjectTypeTensorType) {
    MS_ASSERT(tensor->data() == nullptr);
  } else {
    auto copy_tensor = Tensor::CopyTensor(*tensor, true);
    if (copy_tensor == nullptr) {
      MS_LOG(ERROR) << "Copy tensor failed";
      return;
    }
    tensor->FreeData();
    tensor->set_data(copy_tensor->data());
    tensor->set_own_data(true);
    copy_tensor->set_data(nullptr);
    delete copy_tensor;
  }
}

void MatmulDynamicSdotInt8Unpack(void *src, void *dst, int row, int col, bool transpose) {
  auto src_int8 = static_cast<int8_t *>(src);
  auto dst_int8 = static_cast<int8_t *>(dst);
  if (!transpose) {
    // RowMajor2Col4x16MajorInt8
    int row_4 = UP_ROUND(row, C4NUM);
    int stride = C16NUM * C4NUM;
    for (int r = 0; r < row_4; ++r) {
      for (int c = 0; c < col; ++c) {
        int stride_idx = c / C16NUM * (row_4 / C4NUM) + r / C4NUM;
        if (r < row) {
          int src_idx = r * col + c;
          src_int8[src_idx] = dst_int8[stride * stride_idx + c % C16NUM * C4NUM + r % C4NUM];
        }
      }
    }
  } else {
    int temp = row;
    row = col;
    col = temp;
    // RowMajor2Row4x16MajorInt8
    int col4 = UP_ROUND(col, C4NUM);
    for (int r = 0; r < row; r++) {
      int rd16 = r / C16NUM;
      int rm16 = r % C16NUM;
      for (int c = 0; c < col; c++) {
        int cd4 = c / C4NUM;
        int cm4 = c % C4NUM;
        int dst_index = rd16 * col4 * C16NUM + cd4 * C16NUM * C4NUM + rm16 * C4NUM + cm4;
        int src_index = r * col + c;
        src_int8[src_index] = dst_int8[dst_index];
      }
    }
  }
}

void MatmulFp32BaseUnpack(void *src, void *dst, int row, int col, bool transpose) {
  if (!transpose) {
    // RowMajor2Row8MajorParallel
    auto src_r = static_cast<float *>(src);
    auto dst_r = static_cast<float *>(dst);
    for (int r = 0; r < row; r++) {
      float *src_c = src_r + r * col;
      int c = 0;
      for (; c < col; c++) {
        int cd8 = c / C8NUM;
        int cm8 = c % C8NUM;
        src_c[c] = dst_r[cd8 * C8NUM * row + r * C8NUM + cm8];
      }
    }
    return;
  }
  // RowMajor2Col8MajorParallel
  auto src_r = static_cast<float *>(src);
  auto dst_r = static_cast<float *>(dst);
  int row8 = row / C8NUM * C8NUM;
  int col_skip = col / C4NUM * C4NUM;
  int skip_size = C4NUM;

  int ri = 0;
  for (; ri < row8; ri += C8NUM) {
    int ci = 0;
    for (; ci < col_skip; ci += skip_size) {
      float *src_c = src_r + ci;
      float *dst_c = dst_r + ci * C8NUM;
      for (int tr = 0; tr < C8NUM; tr++) {
        for (int tc = 0; tc < C4NUM; tc++) {
          src_c[tr * col + tc] = dst_c[tc * C8NUM + tr];
        }
      }
    }
    for (; ci < col; ci++) {
      float *src_c = src_r + ci;
      float *dst_c = dst_r + ci * C8NUM;
      for (int i = 0; i < C8NUM; i++) {
        src_c[i * col] = dst_c[i];
      }
    }
    src_r += C8NUM * col;
    dst_r += C8NUM * col;
  }
  for (; ri < row; ri++, src_r += col, dst_r++) {
    for (int i = 0; i < col; i++) {
      src_r[i] = dst_r[i * C8NUM];
    }
  }
}

RecoveryWeightFunc GetRecoveryWeightFunc(const int quant_type, const TypeId data_type, const int node_type,
                                         const std::string &cpu_option) {
  if (cpu_option == kArm64SimdDot && node_type == static_cast<int>(schema::PrimitiveType_MatMulFusion) &&
      quant_type == static_cast<int>(schema::QuantType_QUANT_DYNAMIC) && data_type == kNumberTypeInt8) {
    return MatmulDynamicSdotInt8Unpack;
  }

  if (cpu_option == kArm64SimdDot && node_type == static_cast<int>(schema::PrimitiveType_MatMulFusion) &&
      data_type == kNumberTypeFloat32) {
    return MatmulFp32BaseUnpack;
  }
  return nullptr;
}

int PackedMatmulKernelExec(kernel::KernelExec *kernel_exec, const std::vector<Tensor *> &tensors) {
  auto pack_info = PackedNodePass::GetInstance().GetNodePackInfo(kernel_exec->name());
  if (pack_info == nullptr) {
    return RET_OK;
  }
  MS_CHECK_TRUE_MSG(kernel_exec->in_tensors().size() >= kInputSize1, lite::RET_ERROR,
                    "kernel doesn't have weight tensor.");
  auto dst_tensor = kernel_exec->in_tensors()[SECOND_INPUT];
  auto kernel = reinterpret_cast<Kernel *>(kernel_exec->kernel());
  MS_CHECK_TRUE_MSG(kernel != nullptr, lite::RET_NULL_PTR, "kernel is nullptr.");
  MS_CHECK_TRUE_MSG(kernel_exec->op_parameter() != nullptr, lite::RET_NULL_PTR, "kernel parameter is nullptr.");
  auto param = reinterpret_cast<MatMulParameter *>(kernel_exec->op_parameter());
  const KernelBase *kernel_base = reinterpret_cast<const nnacl::NNACLKernel *>(kernel_exec->kernel())->Kernel();
  if (dst_tensor->data_type() == kNumberTypeFloat32) {
    const MatmulStruct *matmul = reinterpret_cast<const MatmulStruct *>(kernel_base);
    if (matmul->matmul_type_ == kNotImplemented) {
      return RecoveryPackedWeight(dst_tensor, static_cast<int>(kernel->quant_type()), dst_tensor->data_type(),
                                  static_cast<int>(schema::PrimitiveType_MatMulFusion), *pack_info);
    }
  }

  if (dst_tensor->data_type() == kNumberTypeInt8 && param->matmul_type_ != kMatmulDynamicSdotInt8Cpu &&
      pack_info->cpu_option_ == kArm64SimdDot) {
    return RecoveryPackedWeight(dst_tensor, static_cast<int>(kernel->quant_type()), dst_tensor->data_type(),
                                static_cast<int>(schema::PrimitiveType_MatMulFusion), *pack_info);
  }

  auto lite_kernel = static_cast<kernel::LiteKernel *>(kernel);
  lite::Tensor *weight_sums = nullptr;
  auto index = pack_info->weight_sums_index_;
  if (index >= 0 && static_cast<size_t>(index) < tensors.size()) {
    weight_sums = tensors.at(index);
  }
  return lite_kernel->PreparePackedWeight(weight_sums);
}

int RecoveryPackedWeight(Tensor *weight, const int quant_type, const TypeId data_type, const int node_type,
                         const PackInfo &pack_info) {
  auto recovery_func = GetRecoveryWeightFunc(quant_type, data_type, node_type, pack_info.cpu_option_);
  if (recovery_func == nullptr) {
    MS_LOG(ERROR) << "unsupported recovery func.";
    return RET_NULL_PTR;
  }
  void *unpack_data = malloc(weight->Size());
  if (unpack_data == nullptr) {
    MS_LOG(ERROR) << "malloc unpack_data failed.";
    return RET_NULL_PTR;
  }
  void *pack_b_ptr = weight->data();
  for (int i = 0; i < pack_info.b_batch_; i++) {
    void *current_weight;
    void *current_b_pack;
    if (weight->data_type() == kNumberTypeInt8) {
      current_weight = static_cast<void *>(static_cast<int8_t *>(unpack_data) + i * pack_info.deep_ * pack_info.col_);
      current_b_pack =
        static_cast<void *>(static_cast<int8_t *>(pack_b_ptr) + i * pack_info.col_align_ * pack_info.deep_align_);
    } else if (weight->data_type() == kNumberTypeFloat32) {
      current_weight = static_cast<void *>(static_cast<float *>(unpack_data) + i * pack_info.deep_ * pack_info.col_);
      current_b_pack =
        static_cast<void *>(static_cast<float *>(pack_b_ptr) + i * pack_info.col_align_ * pack_info.deep_);
    } else {
      free(unpack_data);
      MS_LOG(ERROR) << "unsupported data type.";
      return RET_ERROR;
    }
    recovery_func(current_weight, current_b_pack, pack_info.deep_, pack_info.col_, pack_info.b_transpose_);
  }
  weight->FreeData();
  weight->set_data(unpack_data);
  return RET_OK;
}

int PackKernelExec(kernel::KernelExec *kernel_exec, const std::vector<Tensor *> &tensors) {
  if (kernel_exec->type() == schema::PrimitiveType_MatMulFusion) {
    return PackedMatmulKernelExec(kernel_exec, tensors);
  }
  return RET_OK;
}
}  // namespace lite
}  // namespace mindspore
