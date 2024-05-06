/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_QUANTIZER_QUANTIZER_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_QUANTIZER_QUANTIZER_H_
#include <utility>
#include <memory>
#include "schema/inner/model_generated.h"
#include "ir/func_graph.h"
#include "ir/anf.h"
#include "base/base.h"
#include "tools/converter/quantizer/quant_param_holder.h"
#include "tools/converter/cxx_api/converter_para.h"

namespace mindspore::lite::quant {
class Quantizer {
 public:
  Quantizer() {}

  explicit Quantizer(const std::shared_ptr<mindspore::ConverterPara> &param) : param_(param) {}

  virtual ~Quantizer() = default;

  virtual int DoQuantize(FuncGraphPtr func_graph) = 0;

 protected:
  const std::shared_ptr<mindspore::ConverterPara> param_{nullptr};
};
}  // namespace mindspore::lite::quant
#endif
