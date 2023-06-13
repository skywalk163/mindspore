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

#include "include/common/utils/utils.h"

#include <set>
#include <string>

#include "utils/convert_utils_base.h"
namespace mindspore {
namespace {
constexpr size_t kKBToByte = 1024;
constexpr size_t kLineMaxSize = 1024;
}  // namespace
bool IsOneOfPosteriorOperator(const std::string &name) {
  static const std::set<std::string> kPosteriorOperatorSet = {kPullOpName};

  auto iter = kPosteriorOperatorSet.find(name);
  return iter != kPosteriorOperatorSet.end();
}

bool IsOneOfCacheBlackList(const std::string &name) {
  static const std::set<std::string> kOpCacheBlackList = {kUniformCandidateSamplerOpName, kInitDatasetQueueOpName,
                                                          kGetNextOpName};

  auto iter = kOpCacheBlackList.find(name);
  return iter != kOpCacheBlackList.end();
}

bool IsOneOf3DFormat(const std::string &format) {
  static const std::set<std::string> k3DFormatSet = {kOpFormat_NCDHW, kOpFormat_NDC1HWC0, kOpFormat_FRACTAL_Z_3D,
                                                     kOpFormat_NDHWC, kOpFormat_DHWCN,    kOpFormat_DHWNC};

  auto iter = k3DFormatSet.find(format);
  return iter != k3DFormatSet.end();
}

bool IsOneOfNoPaddingFormat(const std::string &format) {
  static const std::set<std::string> kNoPaddingFormatSet = {
    kOpFormat_ChannelLast, kOpFormat_FRAC_NZ, kOpFormat_FRACTAL_ZN_RNN, kOpFormat_ND_RNN_BIAS, kOpFormat_DEFAULT};

  auto iter = kNoPaddingFormatSet.find(format);
  return iter != kNoPaddingFormatSet.end();
}

bool IsOneOfDynamicShapeConstInputToAttrGPU(const std::string &name) {
  static const std::set<std::string> DynamicShapeConstInputToAttrGPU = {
    kCastOpName,      kExpandDimsOpName, kReshapeOpName,    kEmbeddingLookupOpName, kTransposeOpName,
    kReduceSumOpName, kReduceMinOpName,  kReduceMeanOpName, kReduceMaxOpName,       kReduceAllOpName,
    kReduceAnyOpName, kConcatOpName,     kScatterNdOpName,  kGatherOpName,          kAvgPool3DGradOpName};

  auto iter = DynamicShapeConstInputToAttrGPU.find(name);
  return iter != DynamicShapeConstInputToAttrGPU.end();
}

bool IsOneOfCustomAkgType(const std::string &name) {
  const std::set<std::string> kCustomTypeAkg = {"ir_builder", "tvm_compute", "hybrid"};

  auto iter = kCustomTypeAkg.find(name);
  return iter != kCustomTypeAkg.end();
}

bool IsOneOfOperator(const std::string &name) {
  static const std::set<std::string> kOptOperatorSet = {kMomentumOpName,
                                                        kApplyMomentumOpName,
                                                        kApplyMomentumDOpName,
                                                        kApplyAdadeltaOpName,
                                                        kApplyAdadeltaDOpName,
                                                        kApplyAdagradOpName,
                                                        kApplyAdagradDOpName,
                                                        kApplyAdagradDAOpName,
                                                        kApplyAdagradDADOpName,
                                                        kAdamOpName,
                                                        kApplyAdamDOpName,
                                                        kApplyAdamOpName,
                                                        kApplyAdaMaxOpName,
                                                        kApplyAdaMaxDOpName,
                                                        kApplyAddSignOpName,
                                                        kApplyAddSignDOpName,
                                                        kApplyCenteredRMSPOpName,
                                                        kApplyFtrlOpName,
                                                        kApplyFtrlDOpName,
                                                        kApplyFtrlV2OpName,
                                                        kApplyFtrlV2DOpName,
                                                        kApplyGradientDescentOpName,
                                                        kApplyPowerSignOpName,
                                                        kApplyPowerSignDOpName,
                                                        kApplyProximalAdagradOpName,
                                                        kApplyProximalAdagradDOpName,
                                                        kApplyProximalGradientDescentOpName,
                                                        kApplyRMSPropOpName,
                                                        kApplyRMSPropDOpname,
                                                        kAdamApplyOneWithDecayOpName,
                                                        kAdamApplyOneWithDecayAssignOpName,
                                                        kFusedAdamWeightDecayName,
                                                        kAdamWeightDecayName,
                                                        kFusedCastAdamWeightDecayName,
                                                        kFusedAdamName,
                                                        kFusedAdaFactorName,
                                                        kFusedAdaFactorWithGlobalNormName,
                                                        kFusedSparseAdamName,
                                                        kFusedMulApplyMomentumOpName,
                                                        kFusedWeightScaleApplyMomentum,
                                                        kFusedScaleApplyMomentum,
                                                        kApplyCenteredRMSPropOpName,
                                                        kApplyCenteredRMSPropDOpName,
                                                        kFusedSparseFtrlName,
                                                        kFusedSparseProximalAdagradName,
                                                        kFusedSparseLazyAdamName,
                                                        kSparseApplyFtrlOpName,
                                                        kSparseApplyFtrlDOpName,
                                                        kSparseApplyFtrlV2OpName,
                                                        kSparseApplyFtrlV2DOpName,
                                                        kSGDName,
                                                        kLARSUpdateOpName,
                                                        kLarsV2UpdateOpName,
                                                        kCombineWeightDecayScaleMomentumOpName,
                                                        kCombineScaleMomentumOpName,
                                                        kCombineMomentumOpName,
                                                        kScatterAddOpName,
                                                        kScatterUpdateOpName,
                                                        kSparseApplyProximalAdagradOpName,
                                                        kSparseApplyProximalAdagradDOpName,
                                                        kAdaptiveMaxPool2dOpName,
                                                        kApplyKerasMomentumDOpName};

  auto iter = kOptOperatorSet.find(name);
  return iter != kOptOperatorSet.end();
}

bool IsOneOfNotSupportedTransFormat(const std::string &format) {
  static const std::set<std::string> kNotSupportedFormat = {kOpFormat_DHWCN, kOpFormat_NDHWC, kOpFormat_CHWN};
  return (kNotSupportedFormat.find(format) != kNotSupportedFormat.end());
}

bool IsOneOfComputeDepend(const std::string &name) {
  static const std::set<std::string> kComputeDepend = {kUniqueOpName,
                                                       kUniqueConsecutiveOpName,
                                                       kComputeAccidentalHitsOpName,
                                                       kSubAndFilterOpName,
                                                       kPadAndShiftOpName,
                                                       kCTCGreedyDecoderOpName,
                                                       kMaskedSelectOpName,
                                                       kDynamicStitchOpName,
                                                       kGetNextOpName,
                                                       kListDiffOpName,
                                                       kNonMaxSuppressionV3OpName,
                                                       kNonMaxSuppressionWithOverlapsOpName,
                                                       kCoalesceOpName,
                                                       kTruncatedNormal,
                                                       kNonDeterministicInts,
                                                       kFractionalAvgPoolGradOpName,
                                                       kDenseToDenseSetOperation,
                                                       kDenseToSparseSetOperation,
                                                       kSegmentMaxOpName,
                                                       kCSRSparseMatrixToSparseTensorOpName,
                                                       kSegmentMinOpName,
                                                       kLuUnpackOpName,
                                                       kSegmentSumOpName,
                                                       kResizeBicubicOpName,
                                                       kResizeAreaOpName,
                                                       kSegmentMeanOpName,
                                                       kSegmentProdOpName,
                                                       kSparseSliceOpName,
                                                       kNonZeroOpName,
                                                       kSparseSparseMinimumOpName,
                                                       kSparseSparseMaximumOpName,
                                                       kRpcRecvOpName,
                                                       kSparseFillEmptyRows,
                                                       kSparseCrossOpName,
                                                       kAdaptiveMaxPool3DOpName,
                                                       kDynamicBroadcastGradientArgsOpName};

  auto iter = kComputeDepend.find(name);
  return iter != kComputeDepend.end();
}

bool IsOneOfHWSpecialFormat(const std::string &format) {
  static const std::set<std::string> kHWSpecialFormatSet = {
    kOpFormat_FRACTAL_Z_3D,   kOpFormat_NC1KHKWHWC0, kOpFormat_NC1HWC0,       kOpFormat_FRAC_NZ,
    kOpFormat_C1HWNCoC0,      kOpFormat_NC1HWC0_C04, kOpFormat_FRACTAL_Z_C04, kOpFormat_FRACTAL_ZN_LSTM,
    kOpFormat_FRACTAL_ZN_RNN, kOpFormat_NDC1HWC0,    kOpFormat_FRAC_Z};

  auto iter = kHWSpecialFormatSet.find(format);
  return iter != kHWSpecialFormatSet.end();
}

bool IsOneOfFormat(const std::string &format) {
  static const std::set<std::string> kOpFormatList = {
    kOpFormat_DEFAULT,        kOpFormat_NC1KHKWHWC0,  kOpFormat_ND,
    kOpFormat_NCHW,           kOpFormat_NHWC,         kOpFormat_HWCN,
    kOpFormat_CHWN,           kOpFormat_NC1HWC0,      kOpFormat_FRAC_Z,
    kOpFormat_C1HWNCoC0,      kOpFormat_FRAC_NZ,      kOpFormat_NC1HWC0_C04,
    kOpFormat_FRACTAL_Z_C04,  kOpFormat_NDHWC,        kOpFormat_FRACTAL_ZN_LSTM,
    kOpFormat_FRACTAL_ZN_RNN, kOpFormat_ND_RNN_BIAS,  kOpFormat_NDC1HWC0,
    kOpFormat_NCDHW,          kOpFormat_FRACTAL_Z_3D, kOpFormat_DHWNC,
    kOpFormat_DHWCN};

  auto iter = kOpFormatList.find(format);
  return iter != kOpFormatList.end();
}

bool IsOneOfServerFormatC04(const std::string &format) {
  static const std::set<std::string> kServerFormatC04List = {kOpFormat_NC1HWC0_C04, kOpFormat_FRACTAL_Z_C04};
  return kServerFormatC04List.find(format) != kServerFormatC04List.end();
}

bool IsOneOfDynRankNeedPadShape(const std::string &format) {
  const std::set<std::string> kOpFormats = {kOpFormat_NC1HWC0,      kOpFormat_NDC1HWC0,      kOpFormat_FRAC_Z,
                                            kOpFormat_NDC1HWC0,     kOpFormat_C1HWNCoC0,     kOpFormat_NC1HWC0_C04,
                                            kOpFormat_FRACTAL_Z_3D, kOpFormat_FRACTAL_Z_C04, kOpFormat_NCDHW};
  return kOpFormats.find(format) != kOpFormats.end();
}

size_t GetSystemMemorySize(const std::string &key) {
#if defined(_WIN32) || defined(_WIN64) || defined(__APPLE__)
  return SIZE_MAX;
#else
  FILE *file = fopen("/proc/meminfo", "r");
  if (file == nullptr) {
    MS_LOG(ERROR) << "Get system meminfo failed.";
    return 0;
  }

  size_t mem_size = 0;
  char buf[kLineMaxSize] = {0};
  while (fgets(buf, kLineMaxSize, file)) {
    // Get mem title.
    std::string line(buf);
    auto title_end_pos = line.find(":");
    auto title = line.substr(0, title_end_pos);
    // Get mem size.
    if (title == key) {
      auto mem_size_end_pos = line.find_last_of(" ");
      auto mem_size_begin_pos = line.find_last_of(" ", mem_size_end_pos - 1);
      if ((mem_size_end_pos != std::string::npos) && (mem_size_begin_pos != std::string::npos)) {
        auto mem_size_string = line.substr(mem_size_begin_pos, mem_size_end_pos - mem_size_begin_pos);
        mem_size = LongToSize(std::atol(mem_size_string.c_str()));
      }
      break;
    }
    if (memset_s(buf, kLineMaxSize, 0, kLineMaxSize) != EOK) {
      MS_LOG(ERROR) << "Set system meminfo failed.";
      (void)fclose(file);
      return 0;
    }
  }
  (void)fclose(file);

  MS_LOG(INFO) << "Get system memory(" << key << "): " << mem_size << " kB";
  return mem_size * kKBToByte;
#endif
}
}  // namespace mindspore
