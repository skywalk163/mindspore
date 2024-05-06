/**
 * Copyright 2022-2024 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_OCR_OPS_DECLARE_H_
#define MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_OCR_OPS_DECLARE_H_

#include "transform/graph_ir/op_declare/op_declare_macro.h"

DECLARE_OP_ADAPTER(OCRDetectionPreHandle)
DECLARE_OP_USE_OUTPUT(OCRDetectionPreHandle)

DECLARE_OP_ADAPTER(OCRFindContours)
DECLARE_OP_USE_OUTPUT(OCRFindContours)

DECLARE_OP_ADAPTER(BatchDilatePolys)
DECLARE_OP_USE_OUTPUT(BatchDilatePolys)

DECLARE_OP_ADAPTER(ResizeAndClipPolys)
DECLARE_OP_USE_OUTPUT(ResizeAndClipPolys)

DECLARE_OP_ADAPTER(OCRDetectionPostHandle)
DECLARE_OP_USE_OUTPUT(OCRDetectionPostHandle)

DECLARE_OP_ADAPTER(OCRIdentifyPreHandle)
DECLARE_OP_USE_OUTPUT(OCRIdentifyPreHandle)

DECLARE_OP_ADAPTER(BatchEnqueue)
DECLARE_OP_USE_OUTPUT(BatchEnqueue)

DECLARE_OP_ADAPTER(Dequeue)
DECLARE_OP_USE_OUTPUT(Dequeue)

DECLARE_OP_ADAPTER(OCRRecognitionPreHandle)
DECLARE_OP_USE_OUTPUT(OCRRecognitionPreHandle)
#endif  // MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_OCR_OPS_DECLARE_H_
