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
#ifndef MINDSPORE_LITE_SRC_EXTENDRT_CXX_API_FILE_UTILS_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_CXX_API_FILE_UTILS_H_

#include <string>
#include <vector>
#include "include/api/types.h"

namespace mindspore {
Buffer ReadFile(const std::string &file);
std::vector<std::string> ReadFileNames(const std::string &dir);
}  // namespace mindspore
#endif  // MINDSPORE_LITE_SRC_EXTENDRT_CXX_API_FILE_UTILS_H_
