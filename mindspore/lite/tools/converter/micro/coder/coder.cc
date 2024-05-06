/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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
#include "tools/converter/micro/coder/coder.h"
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include "tools/converter/micro/coder/session.h"
#include "tools/converter/micro/coder/train/train_session.h"
#include "utils/dir_utils.h"
#include "src/common/file_utils.h"
#include "src/common/utils.h"
#include "tools/converter/micro/coder/config.h"
#include "tools/converter/micro/coder/opcoders/parallel.h"

namespace mindspore::lite::micro {
namespace {
static int kModelIndex = 0;
std::shared_ptr<CoderSession> CreateCoderSession() {
  std::shared_ptr<CoderSession> session;
  auto code_mode = Configurator::GetInstance()->code_mode();
  if (code_mode == CodeMode::Inference) {
    session = std::make_shared<CoderSession>();
  } else if (code_mode == CodeMode::Train) {
    session = std::make_shared<CoderTrainSession>();
  } else {
    MS_LOG(ERROR) << "unsupported code mode. " << code_mode;
    session = nullptr;
  }
  return session;
}

int ParseMicroDynamicShape(const schema::MetaGraphT &graph, micro::MicroParam *micro_param) {
  for (auto index : graph.inputIndex) {
    auto input_name = graph.allTensors.at(index)->name;
    if (micro_param->graph_inputs_origin_info.find(input_name) == micro_param->graph_inputs_origin_info.end() ||
        micro_param->inputs_shape_by_scenes.find(input_name) == micro_param->inputs_shape_by_scenes.end()) {
      MS_LOG(ERROR) << "Micro param: dynamic inputs name is invalid";
      return RET_INPUT_PARAM_INVALID;
    }
    micro_param->graph_inputs_template.emplace_back(micro_param->graph_inputs_origin_info[input_name]);
    micro_param->graph_inputs_shape_infos.emplace_back(micro_param->inputs_shape_by_scenes[input_name]);
  }
  return RET_OK;
}

int ParseMicroDynamicShape(const Model &model, micro::MicroParam *micro_param) {
  for (auto index : model.graph_.input_indices_) {
    auto input_name = model.graph_.all_tensors_.at(index)->name()->str();
    if (micro_param->graph_inputs_origin_info.find(input_name) == micro_param->graph_inputs_origin_info.end() ||
        micro_param->inputs_shape_by_scenes.find(input_name) == micro_param->inputs_shape_by_scenes.end()) {
      MS_LOG(ERROR) << "Micro param: dynamic inputs name is invalid";
      return RET_INPUT_PARAM_INVALID;
    }
    micro_param->graph_inputs_template.emplace_back(micro_param->graph_inputs_origin_info[input_name]);
    micro_param->graph_inputs_shape_infos.emplace_back(micro_param->inputs_shape_by_scenes[input_name]);
  }
  return RET_OK;
}
}  // namespace
int Coder::Run(const void *model_buff, size_t size, const std::string &model_name, bool end_flag, bool enable_fp16) {
  session_ = CreateCoderSession();
  if (session_ == nullptr) {
    MS_LOG(ERROR) << "new session failed while running!";
    return RET_ERROR;
  }
  STATUS status = session_->Init(model_buff, size, kModelIndex, end_flag, enable_fp16);
  if (status != RET_OK) {
    MS_LOG(ERROR) << "Init session failed!";
    return RET_ERROR;
  }
  kModelIndex++;

  status = session_->Build();
  if (status != RET_OK) {
    MS_LOG(ERROR) << "Compile graph failed!";
    return status;
  }
  status = session_->Run(model_name);
  if (status != RET_OK) {
    MS_LOG(ERROR) << "Generate Code Files error!" << status;
    return status;
  }
  status = session_->GenerateCode();
  if (status != RET_OK) {
    MS_LOG(ERROR) << "Generate Code Files error!" << status;
  }
  FreeGlobalVariable();
  FreeThread();
  return status;
}

bool Coder::InitPath(const std::string &output_path) {
  this->save_path_.clear();
  this->model_name_.clear();
  auto pos = output_path.find_last_of('/');
  if (pos == std::string::npos) {
    pos = output_path.find_last_of('\\');
  }
  if (pos == std::string::npos) {
#ifdef _WIN32
    this->save_path_ = ".\\";
#else
    this->save_path_ = "./";
#endif
    this->model_name_ = output_path;
  } else {
    this->save_path_ = output_path.substr(0, pos + 1);
    this->model_name_ = output_path.substr(pos + 1);
  }
  this->save_path_ = RealPath(this->save_path_.c_str());
  if (this->save_path_.empty()) {
    return false;
  }
  auto suffix_pos = this->model_name_.find_last_of('.');
  if (suffix_pos != std::string::npos && this->model_name_.substr(suffix_pos + 1) == "ms") {
    this->model_name_ = this->model_name_.substr(0, suffix_pos);
  }
#ifdef _WIN32
  this->save_path_ = this->save_path_ + "\\";
#else
  this->save_path_ = this->save_path_ + "/";
#endif
  return true;
}

int Coder::MicroSourceCodeGeneration(const schema::MetaGraphT &graph, const std::string &output_path, MicroParam *param,
                                     bool enable_fp16) {
  flatbuffers::FlatBufferBuilder builder(kFlatbuffersBuilderInitSize);
  auto offset = schema::MetaGraph::Pack(builder, &graph);
  builder.Finish(offset);
  schema::FinishMetaGraphBuffer(builder, offset);
  size_t size = builder.GetSize();
  if (!param->dynamic_symbols.empty()) {
    MS_CHECK_TRUE_MSG(ParseMicroDynamicShape(graph, param) == RET_OK, RET_ERROR, "ParseMicroDynamicShape failed.");
  }
  if (ExecuteMicroGeneration(builder.GetBufferPointer(), size, output_path, *param, enable_fp16) != RET_OK) {
    MS_LOG(ERROR) << "Execute Micro failed.";
    return RET_ERROR;
  }
  return RET_OK;
}

int Coder::MicroSourceCodeGeneration(const std::string &model_file, const std::string &output_path, MicroParam *param,
                                     bool enable_fp16) {
  size_t buffer_size;
  auto model_buf = lite::ReadFile(model_file.c_str(), &buffer_size);
  if (model_buf == nullptr) {
    MS_LOG(ERROR) << "Read model-file failed.";
    return RET_NULL_PTR;
  }
  Model *model = lite::Model::Import(model_buf, buffer_size);
  MS_CHECK_PTR(model);
  if (!param->dynamic_symbols.empty()) {
    MS_CHECK_TRUE_MSG(ParseMicroDynamicShape(*model, param) == RET_OK, RET_ERROR, "ParseMicroDynamicShape failed.");
  }
  auto ret = ExecuteMicroGeneration(model_buf, buffer_size, output_path, *param, enable_fp16);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Execute Micro failed.";
  }
  delete[] model_buf;
  return ret;
}

int Coder::ExecuteMicroGeneration(const void *model_buf, size_t size, const std::string &output_path,
                                  const MicroParam &param, bool enable_fp16) {
  micro::Coder code_gen;
  if (!code_gen.InitPath(output_path)) {
    MS_LOG(ERROR) << "Init path failed";
    return RET_ERROR;
  }
  if (!(DirectoryGenerator::GetInstance()->CreateStaticDir(code_gen.save_path_, code_gen.model_name_))) {
    MS_LOG(ERROR) << "Create static directories failed";
    return RET_ERROR;
  }
  if (!(DirectoryGenerator::GetInstance()->CreateDynamicDir(kModelIndex))) {
    MS_LOG(ERROR) << "Create dynamic directories failed";
    return RET_ERROR;
  }
  // codegeneration for micro
  STATUS status = code_gen.Init(param);
  if (status != RET_OK) {
    MS_LOG(ERROR) << "Codegen init Error";
    return RET_ERROR;
  }
  status = code_gen.Run(model_buf, size, code_gen.model_name_, param.is_last_model, enable_fp16);
  if (status != RET_OK) {
    MS_LOG(ERROR) << "Codegen Run Error";
    return RET_ERROR;
  }
  MS_LOG(INFO) << "end of Codegen";
  return RET_OK;
}

int Coder::Init(const MicroParam &param) const {
  static const std::map<std::string, Target> kTargetMap = {
    {"x86", kX86}, {"Cortex-M", kCortex_M}, {"ARM32", kARM32}, {"ARM64", kARM64}, {"All", kAllTargets}};
  static const std::map<std::string, CodeMode> kCodeModeMap = {{"Inference", Inference}, {"Train", Train}};
  Configurator *config = Configurator::GetInstance();

  auto target_item = kTargetMap.find(param.target);
  MS_CHECK_TRUE_MSG(target_item != kTargetMap.end(), RET_ERROR, "unsupported target: " + target);
  config->set_target(target_item->second);

  auto code_item = kCodeModeMap.find(param.codegen_mode);
  MS_CHECK_TRUE_MSG(code_item != kCodeModeMap.end(), RET_ERROR, "unsupported code mode: " + code_mode);
  config->set_code_mode(code_item->second);
  if (code_item->second == CodeMode::Train && config->target() == kCortex_M) {
    MS_LOG(ERROR) << "Cortex-M cannot support train.";
    return RET_ERROR;
  }

  if (param.support_parallel && config->target() == kCortex_M) {
    MS_LOG(ERROR) << "Cortex-M cannot support parallel.";
    return RET_ERROR;
  }
  config->set_support_parallel(param.support_parallel);
  config->set_debug_mode(param.debug_mode);

  config->set_proj_dir(DirectoryGenerator::GetInstance()->project_name());
  config->set_code_path(DirectoryGenerator::GetInstance()->work_dir() +
                        DirectoryGenerator::GetInstance()->project_name());
  config->set_keep_original_weight(param.keep_original_weight);
  config->set_changeable_weights_name(param.changeable_weights_name);
  config->set_graph_inputs_shape_infos(param.graph_inputs_shape_infos);
  config->set_dynamic_symbols(param.dynamic_symbols);
  config->set_dynamic_symbols_num(param.dynamic_symbols_num);
  config->set_dynamic_symbols_map(param.dynamic_symbols_map);
  config->set_user_graph_inputs_template(param.graph_inputs_template);

  auto print_parameter = [](auto name, auto value) {
    MS_LOG(INFO) << std::setw(20) << std::left << name << "= " << value;
  };

  print_parameter("projectName", config->proj_dir());
  print_parameter("target", config->target());
  print_parameter("codePath", config->code_path());
  print_parameter("codeMode", config->code_mode());
  print_parameter("debugMode", config->debug_mode());
  return RET_OK;
}
}  // namespace mindspore::lite::micro
