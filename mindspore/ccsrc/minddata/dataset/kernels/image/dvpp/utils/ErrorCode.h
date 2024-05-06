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

#ifndef MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_DVPP_UTILS_ERROR_CODE_H_
#define MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_DVPP_UTILS_ERROR_CODE_H_

#include <string>

using APP_ERROR = int;
// define the data tpye of error code
enum {
  APP_ERR_OK = 0,

  // define the error code of ACL model, this is same with the aclError which is
  // error code of ACL API Error codes 1~999 are reserved for the ACL. Do not
  // add other error codes. Add it after APP_ERR_COMMON_ERR_BASE.
  APP_ERR_ACL_FAILURE = -1,                   // ACL: general error
  APP_ERR_ACL_INVALID_PARAM = 1,              // ACL: invalid parameter
  APP_ERR_ACL_BAD_ALLOC = 2,                  // ACL: memory allocation fail
  APP_ERR_ACL_RT_FAILURE = 3,                 // ACL: runtime failure
  APP_ERR_ACL_GE_FAILURE = 4,                 // ACL: Graph Engine failure
  APP_ERR_ACL_OP_NOT_FOUND = 5,               // ACL: operator not found
  APP_ERR_ACL_OP_LOAD_FAILED = 6,             // ACL: fail to load operator
  APP_ERR_ACL_READ_MODEL_FAILURE = 7,         // ACL: fail to read model
  APP_ERR_ACL_PARSE_MODEL = 8,                // ACL: parse model failure
  APP_ERR_ACL_MODEL_MISSING_ATTR = 9,         // ACL: model missing attribute
  APP_ERR_ACL_DESERIALIZE_MODEL = 10,         // ACL: deserialize model failure
  APP_ERR_ACL_EVENT_NOT_READY = 12,           // ACL: event not ready
  APP_ERR_ACL_EVENT_COMPLETE = 13,            // ACL: event complete
  APP_ERR_ACL_UNSUPPORTED_DATA_TYPE = 14,     // ACL: unsupported data type
  APP_ERR_ACL_REPEAT_INITIALIZE = 15,         // ACL: repeat initialize
  APP_ERR_ACL_COMPILER_NOT_REGISTERED = 16,   // ACL: compiler not registered
  APP_ERR_ACL_IO = 17,                        // ACL: IO failed
  APP_ERR_ACL_INVALID_FILE = 18,              // ACL: invalid file
  APP_ERR_ACL_INVALID_DUMP_CONFIG = 19,       // ACL: invalid dump comfig
  APP_ERR_ACL_INVALID_PROFILING_CONFIG = 20,  // ACL: invalid profiling config
  APP_ERR_ACL_OP_TYPE_NOT_MATCH = 21,         // ACL: operator type not match
  APP_ERR_ACL_OP_INPUT_NOT_MATCH = 22,        // ACL: operator input not match
  APP_ERR_ACL_OP_OUTPUT_NOT_MATCH = 23,       // ACL: operator output not match
  APP_ERR_ACL_OP_ATTR_NOT_MATCH = 24,         // ACL: operator attribute not match
  APP_ERR_ACL_API_NOT_SUPPORT = 25,           // ACL: API not support
  APP_ERR_ACL_CREATE_DATA_BUF_FAILED = 26,    // ACL: create data buffer fail
  APP_ERR_ACL_END,                            // Not an error code, define the range of ACL error code

  // define the common error code, range: 1001~1999
  APP_ERR_COMM_BASE = 1000,
  APP_ERR_COMM_FAILURE = APP_ERR_COMM_BASE + 1,              // General Failed
  APP_ERR_COMM_INNER = APP_ERR_COMM_BASE + 2,                // Internal error
  APP_ERR_COMM_INVALID_POINTER = APP_ERR_COMM_BASE + 3,      // Invalid Pointer
  APP_ERR_COMM_INVALID_PARAM = APP_ERR_COMM_BASE + 4,        // Invalid parameter
  APP_ERR_COMM_UNREALIZED = APP_ERR_COMM_BASE + 5,           // Not implemented
  APP_ERR_COMM_OUT_OF_MEM = APP_ERR_COMM_BASE + 6,           // Out of memory
  APP_ERR_COMM_ALLOC_MEM = APP_ERR_COMM_BASE + 7,            // memory allocation error
  APP_ERR_COMM_FREE_MEM = APP_ERR_COMM_BASE + 8,             // free memory error
  APP_ERR_COMM_OUT_OF_RANGE = APP_ERR_COMM_BASE + 9,         // out of range
  APP_ERR_COMM_NO_PERMISSION = APP_ERR_COMM_BASE + 10,       // NO Permission
  APP_ERR_COMM_TIMEOUT = APP_ERR_COMM_BASE + 11,             // Timed out
  APP_ERR_COMM_NOT_INIT = APP_ERR_COMM_BASE + 12,            // Not initialized
  APP_ERR_COMM_INIT_FAIL = APP_ERR_COMM_BASE + 13,           // initialize failed
  APP_ERR_COMM_INPROGRESS = APP_ERR_COMM_BASE + 14,          // Operation now in progress
  APP_ERR_COMM_EXIST = APP_ERR_COMM_BASE + 15,               // Object, file or other resource already exist
  APP_ERR_COMM_NO_EXIST = APP_ERR_COMM_BASE + 16,            // Object, file or other resource doesn't exist
  APP_ERR_COMM_BUSY = APP_ERR_COMM_BASE + 17,                // Object, file or other resource is in use
  APP_ERR_COMM_FULL = APP_ERR_COMM_BASE + 18,                // No available Device or resource
  APP_ERR_COMM_OPEN_FAIL = APP_ERR_COMM_BASE + 19,           // Device, file or resource open failed
  APP_ERR_COMM_READ_FAIL = APP_ERR_COMM_BASE + 20,           // Device, file or resource read failed
  APP_ERR_COMM_WRITE_FAIL = APP_ERR_COMM_BASE + 21,          // Device, file or resource write failed
  APP_ERR_COMM_DESTORY_FAIL = APP_ERR_COMM_BASE + 22,        // Device, file or resource destroy failed
  APP_ERR_COMM_EXIT = APP_ERR_COMM_BASE + 23,                // End of data stream, stop the application
  APP_ERR_COMM_CONNECTION_CLOSE = APP_ERR_COMM_BASE + 24,    // Out of connection, Communication shutdown
  APP_ERR_COMM_CONNECTION_FAILURE = APP_ERR_COMM_BASE + 25,  // connection fail
  APP_ERR_COMM_STREAM_INVALID = APP_ERR_COMM_BASE + 26,      // ACL stream is null pointer
  APP_ERR_COMM_END,                                          // Not an error code, define the range of common error code

  // define the error code of DVPP
  APP_ERR_DVPP_BASE = 2000,
  APP_ERR_DVPP_CROP_FAIL = APP_ERR_DVPP_BASE + 1,            // DVPP: crop fail
  APP_ERR_DVPP_RESIZE_FAIL = APP_ERR_DVPP_BASE + 2,          // DVPP: resize fail
  APP_ERR_DVPP_CROP_RESIZE_FAIL = APP_ERR_DVPP_BASE + 3,     // DVPP: corp and resize fail
  APP_ERR_DVPP_CONVERT_FROMAT_FAIL = APP_ERR_DVPP_BASE + 4,  // DVPP: convert image format fail
  APP_ERR_DVPP_VPC_FAIL = APP_ERR_DVPP_BASE + 5,             // DVPP: VPC(crop, resize, convert format) fail
  APP_ERR_DVPP_JPEG_DECODE_FAIL = APP_ERR_DVPP_BASE + 6,     // DVPP: decode jpeg or jpg fail
  APP_ERR_DVPP_JPEG_ENCODE_FAIL = APP_ERR_DVPP_BASE + 7,     // DVPP: encode jpeg or jpg fail
  APP_ERR_DVPP_PNG_DECODE_FAIL = APP_ERR_DVPP_BASE + 8,      // DVPP: encode png fail
  APP_ERR_DVPP_H26X_DECODE_FAIL = APP_ERR_DVPP_BASE + 9,     // DVPP: decode H264 or H265 fail
  APP_ERR_DVPP_H26X_ENCODE_FAIL = APP_ERR_DVPP_BASE + 10,    // DVPP: encode H264 or H265 fail
  APP_ERR_DVPP_HANDLE_NULL = APP_ERR_DVPP_BASE + 11,         // DVPP: acldvppChannelDesc is nullptr
  APP_ERR_DVPP_PICDESC_FAIL = APP_ERR_DVPP_BASE + 12,        // DVPP: fail to create acldvppCreatePicDesc or
  // fail to set acldvppCreatePicDesc
  APP_ERR_DVPP_CONFIG_FAIL = APP_ERR_DVPP_BASE + 13,  // DVPP: fail to set dvpp configuration,such as
  // resize configuration,crop configuration
  APP_ERR_DVPP_OBJ_FUNC_MISMATCH = APP_ERR_DVPP_BASE + 14,       // DVPP: DvppCommon object mismatch the function
  APP_ERR_DVPP_NORMALIZE_FAIL = APP_ERR_DVPP_BASE + 15,          // DVPP: normalize fail
  APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL = APP_ERR_DVPP_BASE + 16,  // DVPP: adjust brightness fail
  APP_ERR_DVPP_ADJUST_CONTRAST_FAIL = APP_ERR_DVPP_BASE + 17,    // DVPP: adjust contrast fail
  APP_ERR_DVPP_ADJUST_HUE_FAIL = APP_ERR_DVPP_BASE + 18,         // DVPP: adjust hue fail
  APP_ERR_DVPP_ADJUST_SATURATION_FAIL = APP_ERR_DVPP_BASE + 19,  // DVPP: adjust saturation fail
  APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL = APP_ERR_DVPP_BASE + 20,    // DVPP: Horizontal Flip
  APP_ERR_DVPP_VERTICAL_FLIP_FAIL = APP_ERR_DVPP_BASE + 21,      // DVPP: vertical Flip
  APP_ERR_DVPP_PERSPECTIVE_FAIL = APP_ERR_DVPP_BASE + 22,        // DVPP: perspective fail
  APP_ERR_DVPP_RESIZED_CROP_FAIL = APP_ERR_DVPP_BASE + 23,       // DVPP: crop and resize fail
  APP_ERR_DVPP_PAD_FAIL = APP_ERR_DVPP_BASE + 24,                // DVPP: pad fail
  APP_ERR_DVPP_AFFINE_FAIL = APP_ERR_DVPP_BASE + 25,             // DVPP: affine fail
  APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL = APP_ERR_DVPP_BASE + 26,      // DVPP: gaussian blur fail
  APP_ERR_DVPP_END,  // Not an error code, define the range of common error code

  // define the error code of inference
  APP_ERR_INFER_BASE = 3000,
  APP_ERR_INFER_SET_INPUT_FAIL = APP_ERR_INFER_BASE + 1,          // Infer: set input fail
  APP_ERR_INFER_SET_OUTPUT_FAIL = APP_ERR_INFER_BASE + 2,         // Infer: set output fail
  APP_ERR_INFER_CREATE_OUTPUT_FAIL = APP_ERR_INFER_BASE + 3,      // Infer: create output fail
  APP_ERR_INFER_OP_SET_ATTR_FAIL = APP_ERR_INFER_BASE + 4,        // Infer: set op attribute fail
  APP_ERR_INFER_GET_OUTPUT_FAIL = APP_ERR_INFER_BASE + 5,         // Infer: get model output fail
  APP_ERR_INFER_FIND_MODEL_ID_FAIL = APP_ERR_INFER_BASE + 6,      // Infer: find model id fail
  APP_ERR_INFER_FIND_MODEL_DESC_FAIL = APP_ERR_INFER_BASE + 7,    // Infer: find model description fail
  APP_ERR_INFER_FIND_MODEL_MEM_FAIL = APP_ERR_INFER_BASE + 8,     // Infer: find model memory fail
  APP_ERR_INFER_FIND_MODEL_WEIGHT_FAIL = APP_ERR_INFER_BASE + 9,  // Infer: find model weight fail

  APP_ERR_INFER_END,  // Not an error code, define the range of inference error
  // code

  // define the error code of transmission
  APP_ERR_TRANS_BASE = 4000,

  APP_ERR_TRANS_END,  // Not an error code, define the range of transmission
  // error code

  // define the error code of blocking queue
  APP_ERR_QUEUE_BASE = 5000,
  APP_ERR_QUEUE_EMPTY = APP_ERR_QUEUE_BASE + 1,   // Queue: empty queue
  APP_ERR_QUEUE_STOPED = APP_ERR_QUEUE_BASE + 2,  // Queue: queue stopped
  APP_ERROR_QUEUE_FULL = APP_ERR_QUEUE_BASE + 3,  // Queue: full queue

  // define the error code of destory
  APP_ERR_DESTORY_BASE = 6000,
  APP_ERR_DESTORY_TENSOR = APP_ERR_DESTORY_BASE + 1,
  APP_ERR_DESTORY_SCALAR = APP_ERR_DESTORY_BASE + 2,
  APP_ERR_DESTORY_INT_ARRAY = APP_ERR_DESTORY_BASE + 3,
  APP_ERR_DESTORY_FLOAT_ARRAY = APP_ERR_DESTORY_BASE + 4,
  APP_ERR_DESTORY_BOOL_ARRAY = APP_ERR_DESTORY_BASE + 5,
  APP_ERR_DESTORY_TENSOR_LIST = APP_ERR_DESTORY_BASE + 6,
  APP_ERR_DESTORY_SCALAR_LIST = APP_ERR_DESTORY_BASE + 7,

  // define the idrecognition web error code
  APP_ERROR_FACE_WEB_USE_BASE = 10000,
  APP_ERROR_FACE_WEB_USE_SYSTEM_ERROR = APP_ERROR_FACE_WEB_USE_BASE + 1,  // Web: system error
  APP_ERROR_FACE_WEB_USE_MUL_FACE = APP_ERROR_FACE_WEB_USE_BASE + 2,      // Web: multiple cheeks
  APP_ERROR_FACE_WEB_USE_REPEAT_REG = APP_ERROR_FACE_WEB_USE_BASE + 3,    // Web: repeat registration
  APP_ERROR_FACE_WEB_USE_PART_SUCCESS = APP_ERROR_FACE_WEB_USE_BASE + 4,  // Web: partial search succeeded
  APP_ERROR_FACE_WEB_USE_NO_FACE = APP_ERROR_FACE_WEB_USE_BASE + 5,       // Web: no cheek detected
  APP_ERR_QUEUE_END,  // Not an error code, define the range of blocking queue
  // error code
};

const std::string APP_ERR_ACL_LOG_STRING[] = {
  "Success",                            // APP_ERR_OK
  "ACL: invalid parameter",             // APP_ERR_ACL_INVALID_PARAM
  "ACL: memory allocation fail",        // APP_ERR_ACL_BAD_ALLOC
  "ACL: runtime failure",               // APP_ERR_ACL_RT_FAILURE
  "ACL: Graph Engine failure",          // APP_ERR_ACL_GE_FAILURE
  "ACL: operator not found",            // APP_ERR_ACL_OP_NOT_FOUND
  "ACL: fail to load operator",         // APP_ERR_ACL_OP_LOAD_FAILED
  "ACL: fail to read model",            // APP_ERR_ACL_READ_MODEL_FAILURE
  "ACL: parse model failure",           // APP_ERR_ACL_PARSE_MODEL
  "ACL: model missing attribute",       // APP_ERR_ACL_MODEL_MISSING_ATTR
  "ACL: deserialize model failure",     // APP_ERR_ACL_DESERIALIZE_MODEL
  "Placeholder",                        // 11
  "ACL: event not ready",               // APP_ERR_ACL_EVENT_NOT_READY
  "ACL: event complete",                // APP_ERR_ACL_EVENT_COMPLETE
  "ACL: unsupported data type",         // APP_ERR_ACL_UNSUPPORTED_DATA_TYPE
  "ACL: repeat initialize",             // APP_ERR_ACL_REPEAT_INITIALIZE
  "ACL: compiler not registered",       // APP_ERR_ACL_COMPILER_NOT_REGISTERED
  "ACL: IO failed",                     // APP_ERR_ACL_IO
  "ACL: invalid file",                  // APP_ERR_ACL_INVALID_FILE
  "ACL: invalid dump comfig",           // APP_ERR_ACL_INVALID_DUMP_CONFIG
  "ACL: invalid profiling config",      // APP_ERR_ACL_INVALID_PROFILING_CONFIG
  "ACL: operator type not match",       // APP_ERR_ACL_OP_TYPE_NOT_MATCH
  "ACL: operator input not match",      // APP_ERR_ACL_OP_INPUT_NOT_MATCH
  "ACL: operator output not match",     // APP_ERR_ACL_OP_OUTPUT_NOT_MATCH
  "ACL: operator attribute not match",  // APP_ERR_ACL_OP_ATTR_NOT_MATCH
  "ACL: API not supported",             // APP_ERR_ACL_API_NOT_SUPPORT
  "ACL: create data buffer fail",       // APP_ERR_ACL_CREATE_DATA_BUF_FAILED
};

const std::string APP_ERR_COMMON_LOG_STRING[] = {
  "Placeholder",  // 0
  "General Failed",
  "Internal error",
  "Invalid Pointer",  // 3
  "Invalid parameter",
  "Not implemented",  // 5
  "Out of memory",
  "memory allocation error",
  "free memory error",  // 8
  "out of range",
  "NO Permission ",  // 10
  "Timed out",
  "Not initialized",
  "initialize failed",  // 13
  "Operation now in progress ",
  "Object, file or other resource already exist",  // 15
  "Object, file or other resource already doesn't exist",
  "Object, file or other resource is in use",
  "No available Device or resource",  // 18
  "Device, file or resource open failed",
  "Device, file or resource read failed",  // 20
  "Device, file or resource write failed",
  "Device, file or resource destory failed",  // 22
  " ",
  "Out of connection, Communication shutdown",  // 24
  "connection fail",
  "ACL stream is null pointer",  // 26
};

const std::string APP_ERR_DVPP_LOG_STRING[] = {
  "Placeholder",  // 0
  "DVPP: crop fail",
  "DVPP: resize fail",
  "DVPP: corp and resize fail",
  "DVPP: convert image format fail",
  "DVPP: VPC(crop, resize, convert format) fail",  // 5
  "DVPP: decode jpeg or jpg fail",
  "DVPP: encode jpeg or jpg fail",
  "DVPP: encode png fail",
  "DVPP: decode H264 or H265 fail",
  "DVPP: encode H264 or H265 fail",  // 10
  "DVPP: acldvppChannelDesc is nullptr",
  "DVPP: fail to create or set acldvppCreatePicDesc",
  "DVPP: fail to set dvpp configuration",
  "DVPP: DvppCommon object mismatch the function",  // 14
};

const std::string APP_ERR_INFER_LOG_STRING[] = {
  "Placeholder",  // 0
  "Infer: set input fail",
  "Infer: set output fail",
  "Infer: create output fail",
  "Infer: set op attribute fail",
  "Infer: get model output fail",  // 5
  "Infer: find model id fail",
  "Infer: find model description fail",
  "Infer: find model memory fail",
  "Infer: find model weight fail",  // 9
};

const std::string APP_ERR_QUEUE_LOG_STRING[] = {
  "Placeholder",
  "empty queue",
  "queue stopped",
  "full queue",
};

const std::string APP_ERR_FACE_LOG_STRING[] = {
  "Placeholder",               // 0
  "system error",              // 1
  "multiple faces",            // 2
  "repeat registration",       // 3
  "partial search succeeded",  // 4
  "no face detected",          // 5
};

std::string GetAppErrCodeInfo(APP_ERROR err);
void AssertErrorCode(int code, const std::string &file, const std::string &function, int line);
void CheckErrorCode(int code, const std::string &file, const std::string &function, int line);

#define RtAssert(code) AssertErrorCode(code, DATASET_SRC_FILE_NAME, __FUNCTION__, __LINE__);
#define RtCheckError(code) CheckErrorCode(code, DATASET_SRC_FILE_NAME, __FUNCTION__, __LINE__);

#endif  // MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_DVPP_UTILS_ERROR_CODE_H_
