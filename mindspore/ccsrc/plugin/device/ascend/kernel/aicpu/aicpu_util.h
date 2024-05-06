/**
 * Copyright 2020-2023 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_AICPU_AICPU_UTIL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_AICPU_AICPU_UTIL_H_

#include <cstdint>
#include <utility>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <string>
#include "kernel/kernel.h"
#include "ops/structure_op_name.h"
#include "ops/nn_op_name.h"
#include "ops/framework_op_name.h"
namespace mindspore {
namespace kernel {
constexpr auto kLibAicpuKernelSoName = "libaicpu_kernels.so";
constexpr auto kLibCpuKernelSoName = "libcpu_kernels.so";
constexpr auto kDataFormat = "data_format";
constexpr auto kDropoutGenMaskOpName = "DropoutGenMask";
constexpr auto kInitDataSetQueue = "InitDataSetQueue";
constexpr auto kInitData = "InitData";
constexpr auto kCTCLossV2 = "CTCLossV2";
constexpr auto kCTCLossV2Grad = "CTCLossV2Grad";
constexpr auto kGetNext = "GetNext";
constexpr auto kPrint = "Print";
constexpr auto kPack = "Pack";
constexpr auto kCumMax = "CumMax";
constexpr auto kCumSum = "CumSum";
constexpr auto kCumProd = "CumProd";
constexpr auto kMeshgrid = "Meshgrid";
constexpr auto kOutputTypes = "output_types";
constexpr auto kOutputShapes = "output_shapes";
constexpr auto kChannelName = "channel_name";
constexpr auto kSharedName = "shared_name";
constexpr auto kShapes = "shapes";
constexpr auto kTypes = "types";
constexpr auto kQueueName = "queue_name";
constexpr auto kNameRangeV2 = "RangeV2";
constexpr auto kSparseTensorDenseMatmul = "SparseTensorDenseMatmul";
constexpr auto kSeed = "seed";
constexpr auto kSeed0 = "Seed0";
constexpr auto kSeed1 = "Seed1";
constexpr auto kSeed2 = "seed2";
constexpr auto kTopK = "TopK";
constexpr auto kTopKV2 = "TopKV2";
constexpr auto kStack = "Stack";
constexpr auto kUnstack = "Unstack";
constexpr auto kStackInit = "StackInit";
constexpr auto kStackPush = "StackPush";
constexpr auto kStackPop = "StackPop";
constexpr auto kStackDestroy = "StackDestroy";
constexpr auto kStridedSliceV2 = "StridedSliceV2";
constexpr auto kStridedSliceV2Grad = "StridedSliceV2Grad";
constexpr auto kEditDistance = "EditDistance";
constexpr auto kGatherD = "GatherD";
constexpr auto kGather = "Gather";
constexpr auto kReverseSequence = "ReverseSequence";
constexpr auto kHistogram = "Histogram";
constexpr auto kIdentity = "Identity";
constexpr auto kIdentityN = "IdentityN";
constexpr auto kIndexPut = "IndexPut";
constexpr auto kInplaceIndexAdd = "InplaceIndexAdd";
constexpr auto kConcatOffset = "ConcatOffset";
constexpr auto kConcatOffsetV1 = "ConcatOffsetV1";
constexpr auto kRandomChoiceWithMask = "RandomChoiceWithMask";
constexpr auto kGatherDGradV2 = "GatherDGradV2";
constexpr auto kGenerateEodMask = "GenerateEodMask";
constexpr auto kResizeNearestNeighborV2 = "ResizeNearestNeighborV2";
constexpr auto kResizeNearestNeighborV2Grad = "ResizeNearestNeighborV2Grad";
constexpr auto kUpdateCache = "UpdateCache";
constexpr auto kIm2Col = "Im2Col";
constexpr auto kCol2Im = "Col2Im";
constexpr auto kCacheSwapTable = "CacheSwapTable";
constexpr auto kSubAndFilter = "SubAndFilter";
constexpr auto kPadAndShift = "PadAndShift";
constexpr auto kCpuRunApi = "RunCpuKernel";
constexpr auto kDropout2D = "Dropout2D";
constexpr auto kDropout3D = "Dropout3D";
constexpr auto kNonMaxSuppressionV3 = "NonMaxSuppressionV3";
constexpr auto kMaskedSelect = "MaskedSelect";
constexpr auto kMaskedSelectGrad = "MaskedSelectGrad";
constexpr auto kDynamicStitch = "DynamicStitch";
constexpr auto kSort = "Sort";
constexpr auto kSearchSorted = "SearchSorted";
constexpr auto kLinSpace = "LinSpace";
constexpr auto kResizeBilinear = "ResizeBilinear";
constexpr auto kResizeBilinearGrad = "ResizeBilinearGrad";
constexpr auto kTensorScatterElements = "TensorScatterElements";
constexpr auto kExtractGlimpse = "ExtractGlimpse";
constexpr auto kUpsampleNearest3D = "UpsampleNearest3D";
constexpr auto kUpsampleNearest3DGrad = "UpsampleNearest3DGrad";
constexpr auto kUpsampleTrilinear3D = "UpsampleTrilinear3D";
constexpr auto kUpsampleTrilinear3DGrad = "UpsampleTrilinear3DGrad";
constexpr auto kEnvironCreate = "EnvironCreate";
constexpr auto kEnvironSet = "EnvironSet";
constexpr auto kEnvironGet = "EnvironGet";
constexpr auto kEnvironDestroyAll = "EnvironDestroyAll";
constexpr auto kKLDivLoss = "KLDivLoss";
constexpr auto kKLDivLossGrad = "KLDivLossGrad";
constexpr auto kSampleDistortedBoundingBoxV2 = "SampleDistortedBoundingBoxV2";
constexpr auto kSequenceAdd = "SequenceAdd";
constexpr auto kAbs = "Abs";
constexpr auto kSequenceAddN = "SequenceAddN";
constexpr auto kSequenceAddOffset = "SequenceAddOffset";
constexpr auto kSequenceConcat = "SequenceConcat";
constexpr auto kSequenceStack = "SequenceStack";
constexpr auto kSparseToDenseV2 = "SparseToDenseV2";
constexpr auto kSparseSoftmaxCrossEntropyWithLogitsV2 = "SparseSoftmaxCrossEntropyWithLogitsV2";
constexpr auto kPriorityReplayBufferCreate = "PriorityReplayBufferCreate";
constexpr auto kPriorityReplayBufferPush = "PriorityReplayBufferPush";
constexpr auto kPriorityReplayBufferSample = "PriorityReplayBufferSample";
constexpr auto kPriorityReplayBufferUpdate = "PriorityReplayBufferUpdate";
constexpr auto kPriorityReplayBufferDestroy = "PriorityReplayBufferDestroy";
constexpr auto kReservoirReplayBufferCreate = "ReservoirReplayBufferCreate";
constexpr auto kReservoirReplayBufferPush = "ReservoirReplayBufferPush";
constexpr auto kReservoirReplayBufferSample = "ReservoirReplayBufferSample";
constexpr auto kReservoirReplayBufferDestroy = "ReservoirReplayBufferDestroy";
constexpr auto kSparseConcat = "SparseConcat";
constexpr auto kReLUV3 = "ReLUV3";
constexpr auto kNonZero = "NonZero";
constexpr auto kMaxPoolV1 = "MaxPoolV1";
constexpr auto kMaxPoolGradV1 = "MaxPoolGradV1";
constexpr auto kAdaptiveMaxPool2D = "AdaptiveMaxPool2D";
constexpr auto kAdaptiveMaxPool2DGrad = "AdaptiveMaxPool2DGrad";
constexpr auto kAvgPoolV1 = "AvgPoolV1";
constexpr auto kAvgPoolGradV1 = "AvgPoolGradV1";
constexpr auto kAdaptiveAvgPool3D = "AdaptiveAvgPool3D";
constexpr auto kAdaptiveAvgPool3DGrad = "AdaptiveAvgPool3DGrad";
constexpr auto kUniqueConsecutive = "UniqueConsecutive";
constexpr auto kRandomShuffle = "RandomShuffle";
constexpr auto kHSigmoid = "HSigmoid";
constexpr auto kHSigmoidGrad = "HSigmoidGrad";
constexpr auto kIsInf = "IsInf";
constexpr auto kIsNan = "IsNan";
constexpr auto kLogMatrixDeterminant = "LogMatrixDeterminant";
constexpr auto kSegmentMean = "SegmentMean";
constexpr auto kSegmentSum = "SegmentSum";
constexpr auto kCross = "Cross";
constexpr auto kGridSampler2D = "GridSampler2D";
constexpr auto kGridSampler2DGrad = "GridSampler2DGrad";
constexpr auto kGridSampler3D = "GridSampler3D";
constexpr auto kGridSampler3DGrad = "GridSampler3DGrad";
constexpr auto kScatterNdMax = "ScatterNdMax";
constexpr auto kScatterNdMin = "ScatterNdMin";
constexpr auto kScatterAddWithAxis = "ScatterAddWithAxis";
constexpr auto kTril = "Tril";
constexpr auto kSub = "Sub";
constexpr auto kDiv = "Div";
constexpr auto kNeg = "Neg";
constexpr auto kNotEqual = "NotEqual";
constexpr auto kConj = "Conj";
constexpr auto kConjugateTranspose = "ConjugateTranspose";
constexpr auto kCheckNumerics = "CheckNumerics";
constexpr auto kLog1p = "Log1p";
constexpr auto kRsqrt = "Rsqrt";
constexpr auto kSquare = "Square";
constexpr auto kSparseSegmentMeanGrad = "SparseSegmentMeanGrad";
constexpr auto kACos = "ACos";
constexpr auto kAcosh = "Acosh";
constexpr auto kAsin = "Asin";
constexpr auto kAsinh = "Asinh";
constexpr auto kLess = "Less";
constexpr auto kAtanh = "Atanh";
constexpr auto kAdaptiveMaxPool3DGrad = "AdaptiveMaxPool3DGrad";
constexpr auto kCosh = "Cosh";
constexpr auto kTan = "Tan";
constexpr auto kTanhGrad = "TanhGrad";
constexpr auto kRound = "Round";
constexpr auto kRightShift = "RightShift";
constexpr auto kFloorDiv = "FloorDiv";
constexpr auto kAddcdiv = "Addcdiv";
constexpr auto kAddcmul = "Addcmul";
constexpr auto kAdd = "Add";
constexpr auto kTriu = "Triu";
constexpr auto kUniform = "Uniform";
constexpr auto kUniformCandidateSampler = "UniformCandidateSampler";
constexpr auto kExpand = "Expand";
constexpr auto kExpandDims = "ExpandDims";
constexpr auto kCast = "Cast";
constexpr auto kReshape = "Reshape";
constexpr auto kFlatten = "Flatten";
constexpr auto kSqueeze = "Squeeze";
constexpr auto kMatrixBandPart = "MatrixBandPart";
constexpr auto kMatrixDiagPartV3 = "MatrixDiagPartV3";
constexpr auto kMatrixDiagV3 = "MatrixDiagV3";
constexpr auto kBetainc = "Betainc";
constexpr auto kCompareAndBitpack = "CompareAndBitpack";
constexpr auto kZeta = "Zeta";
constexpr auto kSquaredDifference = "SquaredDifference";
constexpr auto kZerosLike = "ZerosLike";
constexpr auto kEqual = "Equal";
constexpr auto kGreaterEqual = "GreaterEqual";
constexpr auto kGreater = "Greater";
constexpr auto kOnesLike = "OnesLike";
constexpr auto kSign = "Sign";
constexpr auto kFmax = "Fmax";
constexpr auto kGLU = "GLU";
constexpr auto kFmin = "Fmin";
constexpr auto kFillV2 = "FillV2";
constexpr auto kArgmax = "Argmax";
constexpr auto kArgmin = "Argmin";
constexpr auto kResizeV2 = "ResizeV2";
constexpr auto kResizeV2Grad = "ResizeV2Grad";
constexpr auto kRange = "Range";
constexpr auto kSliceGrad = "SliceGrad";
constexpr auto kStatelessDropOutGenMask = "StatelessDropOutGenMask";
constexpr auto kRaggedTensorToTensor = "RaggedTensorToTensor";
constexpr auto kRaggedTensorToSparse = "RaggedTensorToSparse";
constexpr auto kAdaptiveMaxPool3D = "AdaptiveMaxPool3D";
constexpr auto kRandpermV2 = "RandpermV2";
constexpr auto kSmoothL1Loss = "SmoothL1Loss";
constexpr auto kSmoothL1LossGrad = "SmoothL1LossGrad";
constexpr auto kSparseCross = "SparseCross";
constexpr auto kChannelShuffle = "ChannelShuffle";
constexpr auto kQuantDTypeCast = "QuantDTypeCast";
constexpr auto kFSEDecode = "FSEDecode";
constexpr auto kSparseSegmentSum = "SparseSegmentSum";
constexpr auto kRealDiv = "RealDiv";
constexpr auto kMaskedFill = "MaskedFill";
constexpr auto kDeformableOffsets = "DeformableOffsets";
constexpr auto kDeformableOffsetsGrad = "DeformableOffsetsGrad";
constexpr auto kAffineGrid = "AffineGrid";
constexpr auto kSTFT = "STFT";
constexpr auto kRandomCategorical = "RandomCategorical";
constexpr auto kStandardNormal = "StandardNormal";
constexpr auto kUniformInt = "UniformInt";
constexpr auto kUniformReal = "UniformReal";
constexpr auto kStandardLaplace = "StandardLaplace";
constexpr auto kLogUniformCandidateSampler = "LogUniformCandidateSampler";
constexpr auto kGamma = "Gamma";

const std::set<std::string> kCpuKernelOps{kIdentity,
                                          kMaskedFill,
                                          kGather,
                                          kSTFT,
                                          kGreater,
                                          kDynamicStitch,
                                          kSort,
                                          kCTCLossV2,
                                          kCTCLossV2Grad,
                                          kSearchSorted,
                                          kSparseSegmentSum,
                                          kAdaptiveMaxPool2D,
                                          kResizeBilinear,
                                          kReverseSequence,
                                          kRandpermV2,
                                          kResizeBilinearGrad,
                                          kTensorScatterElements,
                                          kAdd,
                                          kLess,
                                          kLinSpace,
                                          kIsInf,
                                          kIsNan,
                                          kLogMatrixDeterminant,
                                          kCross,
                                          kGridSampler2D,
                                          kGridSampler2DGrad,
                                          kGridSampler3D,
                                          kGridSampler3DGrad,
                                          kScatterAddWithAxis,
                                          kScatterNdMax,
                                          kScatterNdMin,
                                          kTril,
                                          kSub,
                                          kDiv,
                                          kNeg,
                                          kNonZero,
                                          kNotEqual,
                                          kConjugateTranspose,
                                          kCheckNumerics,
                                          kCumMax,
                                          kCumSum,
                                          kInplaceIndexAdd,
                                          kLog1p,
                                          kRsqrt,
                                          kSquare,
                                          kACos,
                                          kAcosh,
                                          kAsin,
                                          kAsinh,
                                          kAtanh,
                                          kCosh,
                                          kTan,
                                          kTanhGrad,
                                          kRound,
                                          kFloorDiv,
                                          kAddcdiv,
                                          kAddcmul,
                                          kTriu,
                                          kExpand,
                                          kMatrixBandPart,
                                          kMatrixDiagPartV3,
                                          kMatrixDiagV3,
                                          kBetainc,
                                          kCompareAndBitpack,
                                          kZeta,
                                          kSquaredDifference,
                                          kZerosLike,
                                          kEqual,
                                          kOnesLike,
                                          kStatelessDropOutGenMask,
                                          kTopK,
                                          kSign,
                                          kRealDiv,
                                          kGreaterEqual,
                                          kAffineGrid};
const std::set<std::string> kCacheKernelOps{kUpdateCache, kCacheSwapTable,      kSubAndFilter, kPadAndShift, kDropout3D,
                                            kDropout2D,   kNonMaxSuppressionV3, kGetNext,      kInitData,    kPrint};
const std::set<std::string> kCpuKernelBaseOps{kDropoutGenMaskOpName,
                                              kRandomCategorical,
                                              kRandomChoiceWithMask,
                                              kStandardNormal,
                                              kStandardLaplace,
                                              kUniformInt,
                                              kUniformReal,
                                              kLogUniformCandidateSampler,
                                              kEnvironCreate,
                                              kEnvironSet,
                                              kEnvironGet,
                                              kEnvironDestroyAll,
                                              kPriorityReplayBufferCreate,
                                              kPriorityReplayBufferPush,
                                              kPriorityReplayBufferSample,
                                              kPriorityReplayBufferUpdate,
                                              kPriorityReplayBufferDestroy,
                                              kReservoirReplayBufferCreate,
                                              kReservoirReplayBufferPush,
                                              kReservoirReplayBufferSample,
                                              kReservoirReplayBufferDestroy,
                                              kConcatOffset,
                                              kSequenceAdd,
                                              kSequenceAddN,
                                              kSequenceAddOffset,
                                              kSequenceConcat,
                                              kSequenceStack,
                                              kRandomShuffle,
                                              kRange,
                                              kQuantDTypeCast,
                                              kFSEDecode,
                                              kReshape,
                                              kFlatten,
                                              kSqueeze,
                                              kUniformCandidateSampler,
                                              kExpandDims,
                                              kCast,
                                              kGamma};
const std::set<std::string> kDynamicInputOps{kRaggedTensorToTensor,
                                             kSparseCross,
                                             kRaggedTensorToSparse,
                                             kPrint,
                                             kPack,
                                             kMeshgrid,
                                             kStackInitOpName,
                                             kStackDestroyOpName,
                                             kStackPushOpName,
                                             kStackPopOpName,
                                             kDynamicStitch,
                                             kPriorityReplayBufferPush,
                                             kPriorityReplayBufferSample,
                                             kReservoirReplayBufferPush,
                                             kReservoirReplayBufferSample,
                                             kIdentityN,
                                             kIndexPut,
                                             kSparseConcat,
                                             kConcatOffsetV1};
const std::map<std::string, std::string> kOpNameToAicpuOpNameMap{
  {kKLDivLoss, "KLDiv"},
  {kKLDivLossGrad, "KlDivLossGrad"},
  {kMaxPoolV1, "MaxPool"},
  {kCol2Im, "Col2im"},
  {kIm2Col, "Im2col"},
  {kMaxPoolGradV1, "MaxPoolGrad"},
  {kUpsampleNearest3D, "UpsampleNearest3d"},
  {kUpsampleNearest3DGrad, "UpsampleNearest3dGrad"},
  {kNameRangeV2, "Range"},
  {kReLUV3, "Relu"},
  {kSparseTensorDenseMatmul, "SparseTensorDenseMatMul"},
  {kFillV2, "Fill"},
  {kUpsampleTrilinear3D, "UpsampleTrilinear3d"},
  {kUpsampleTrilinear3DGrad, "UpsampleTrilinear3dGrad"},
  {kStack, "Pack"},
  {kUnstack, "Unpack"},
  {kGather, "GatherV2"},
  {kCumSum, "Cumsum"},
  {kCumProd, "Cumprod"},
  {kSampleDistortedBoundingBoxV2, "SampleDistortedBoundingBoxExt2"},
  {kSparseSoftmaxCrossEntropyWithLogitsV2, "SparseSoftmaxCrossEntropyWithLogits"},
  {kSparseToDenseV2, "SparseToDense"},
  {kSmoothL1Loss, "SmoothL1LossV2"},
  {kSmoothL1LossGrad, "SmoothL1LossGradV2"},
  {kAvgPoolV1, "AvgPool"},
  {kNonZero, "Where"},
  {kAvgPoolGradV1, "AvgPoolGrad"},
  {kAdaptiveMaxPool2D, "AdaptiveMaxPool2d"},
  {kAdaptiveMaxPool2DGrad, "AdaptiveMaxPool2dGrad"},
  {kConcatOffsetV1, "ConcatOffset"},
  {kAdaptiveAvgPool3D, "AdaptiveAvgPool3d"},
  {kAdaptiveAvgPool3DGrad, "AdaptiveAvgPool3dGrad"},
  {kTensorScatterElements, "ScatterElements"},
  {kACos, "Acos"},
  {kHSigmoid, "HardSigmoid"},
  {kFmin, "Minimum"},
  {kFmax, "Maximum"},
  {kHSigmoidGrad, "HardSigmoidGrad"},
  {kArgmax, "ArgMax"},
  {kArgmin, "ArgMin"},
  {kResizeV2, "Resize"},
  {kResizeV2Grad, "ResizeGrad"},
  {kGLU, "Glu"},
  {kChannelShuffle, "ShuffleChannel"},
  {kStridedSliceV2, "StridedSlice"},
  {kAdaptiveMaxPool3D, "AdaptiveMaxPool3d"},
  {kRandpermV2, "StatelessRandperm"},
  {kStridedSliceV2Grad, "StridedSliceGrad"},
  {kAdaptiveMaxPool3DGrad, "AdaptiveMaxPool3dGrad"}};

class AicpuOpUtil {
 public:
  static int MsTypeToProtoType(TypeId ms_type);
  static int ProtoTypeToMsType(int proto_type);

 private:
  // kernel id
  static uint64_t KernelId_;
};

class OpKernelBin {
 public:
  OpKernelBin(std::string name, std::vector<char> &&data) : name_(std::move(name)), data_(std::move(data)) {}
  ~OpKernelBin() = default;

  const std::string &GetName() const { return name_; }
  const uint8_t *GetBinData() const { return reinterpret_cast<const uint8_t *>(data_.data()); }
  size_t GetBinDataSize() const { return data_.size(); }
  OpKernelBin(const OpKernelBin &) = delete;
  const OpKernelBin &operator=(const OpKernelBin &) = delete;

  bool loaded() const { return loaded_; }
  void SetLoaded(bool flag) { loaded_ = flag; }

 private:
  std::string name_;
  std::vector<char> data_;
  bool loaded_{false};
};

using OpKernelBinPtr = std::shared_ptr<OpKernelBin>;
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_AICPU_AICPU_UTIL_H_
