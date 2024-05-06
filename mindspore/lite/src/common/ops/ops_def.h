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
#ifndef MINDSPORE_LITE_SRC_COMMON_OPS_OPS_DEF_H_
#define MINDSPORE_LITE_SRC_COMMON_OPS_OPS_DEF_H_
#include <string>
#include <map>
#include <memory>
#include <utility>
#include <vector>
#include "src/common/ops/ops_func_declare.h"
#include "src/common/ops/schema_register.h"

#ifdef PRIMITIVE_WRITEABLE
#include "mindspore/core/utils/check_convert_utils.h"
#include "mindspore/core/mindapi/ir/value.h"
#include "schema/inner/model_generated.h"
#include "schema/inner/ops_types_generated.h"
#endif

#ifdef GEN_SCHEMA_DEF
#define OP_TYPE_DEF_BEGIN(type)        \
  namespace mindspore::lite::ops {     \
  std::string Gen##type() {            \
    std::string prims_type = "union "; \
    prims_type.append(#type).append(" {\n");

#define OP_TYPE(OP) prims_type.append("    ").append(#OP).append(",\n");

#define OP_TYPE_DEF_END(type)                   \
  prims_type.append("}\n");                     \
  return prims_type;                            \
  }                                             \
  PrimitiveTypeRegister g_gen##type(Gen##type); \
  }  // namespace mindspore::lite::ops
#else
#define OP_TYPE_DEF_BEGIN(type)
#define OP_TYPE(OP)
#define OP_TYPE_DEF_END(type)
#endif

#ifdef GEN_SCHEMA_DEF
#define OP_SCHEMA_DEF(OP)            \
  namespace mindspore::lite::ops {   \
  std::string Gen##OP##Def() {       \
    std::string op_def = "\ntable "; \
    op_def.append(#OP);              \
    op_def.append(" {\n");

#elif PRIMITIVE_WRITEABLE
#define OP_SCHEMA_DEF(OP)                                                           \
  namespace mindspore::lite::ops {                                                  \
  std::unique_ptr<schema::PrimitiveT> MSOp2SchemaOp(const mindspore::ops::OP *op) { \
    auto schema_op = std::make_unique<schema::OP##T>();                             \
    if (schema_op == nullptr) {                                                     \
      return nullptr;                                                               \
    }
#else
#define OP_SCHEMA_DEF(OP)
#endif

#ifdef GEN_SCHEMA_DEF
#define OP_ATTR(key, type) op_def.append("    ").append(#key).append(": ").append(#type).append(";\n");
#define OP_ATTR_ENUM(key, type) op_def.append("    ").append(#key).append(": ").append(#type).append(";\n");
#define OP_ATTR_VEC2D(key, type) op_def.append("    ").append(#key).append(": ").append(#type).append(";\n");
#define OP_ATTR_ENUM_SRC(dstkey, dsttype, srckey, srctype) \
  op_def.append("    ").append(#dstkey).append(": ").append(#dsttype).append(";\n");
#define OP_ATTR_RAW(dstkey, dsttype, srckey, srctype) \
  op_def.append("    ").append(#dstkey).append(": ").append(#dsttype).append(";\n");
#define OP_LONG_ATTR_RAW_WITH_VALUE(dstkey, dsttype, srckey, srctype, dstvalue) \
  op_def.append("    ").append(#dstkey).append(": ").append(#dsttype).append(" = ").append(#dstvalue).append(";\n");
#define OP_ATTR_RAW_VEC(dstkey, dsttype, srckey) \
  op_def.append("    ").append(#dstkey).append(": ").append(#dsttype).append(";\n");
#elif PRIMITIVE_WRITEABLE
#define OP_ATTR(key, type)            \
  if (op->GetAttr(#key) != nullptr) { \
    schema_op->key = op->get_##key(); \
  }

#define OP_ATTR_ENUM(key, type)                                  \
  if (op->GetAttr(#key) != nullptr) {                            \
    schema_op->key = static_cast<schema::type>(op->get_##key()); \
  }

#define OP_ATTR_VEC2D(key, type)                              \
  if (op->GetAttr(#key) != nullptr) {                         \
    auto vec2d = std::make_unique<schema::Vec2DT>();          \
    if (vec2d == nullptr) {                                   \
      return nullptr;                                         \
    }                                                         \
    auto data = op->get_##key();                              \
    for (size_t i = 0; i < data.size(); ++i) {                \
      auto vec = std::make_unique<schema::VecT>();            \
      if (vec == nullptr) {                                   \
        return nullptr;                                       \
      }                                                       \
      vec->data.assign(data.at(i).begin(), data.at(i).end()); \
      vec2d->data.push_back(std::move(vec));                  \
    }                                                         \
    schema_op->key = std::move(vec2d);                        \
  }

#define OP_ATTR_ENUM_SRC(dstkey, dsttype, srckey, srctype)                \
  if (op->GetAttr(#srckey) != nullptr) {                                  \
    schema_op->dstkey = static_cast<schema::dsttype>(op->get_##srckey()); \
  }

#define OP_ATTR_RAW(dstkey, dsttype, srckey, srctype)                                  \
  if (op->GetAttr(#srckey) != nullptr) {                                               \
    schema_op->dstkey = static_cast<dsttype>(GetValue<srctype>(op->GetAttr(#srckey))); \
  }

#define OP_LONG_ATTR_RAW_WITH_VALUE(dstkey, dsttype, srckey, srctype, dstvalue) \
  if (op->GetAttr(#srckey) != nullptr) {                                        \
    schema_op->dstkey = GetValue<int64_t>(op->GetAttr(#srckey));                \
  } else {                                                                      \
    schema_op->dstkey = dstvalue;                                               \
  }

#define OP_ATTR_RAW_VEC(dstkey, dsttype, srckey)                              \
  if (op->GetAttr(#srckey) != nullptr) {                                      \
    schema_op->dstkey = GetValue<std::vector<int64_t>>(op->GetAttr(#srckey)); \
  }

#else
#define OP_ATTR(key, type)
#define OP_ATTR_ENUM(key, type)
#define OP_ATTR_VEC2D(key, type)
#define OP_ATTR_ENUM_SRC(dstkey, dsttype, srckey, srctype)
#define OP_ATTR_RAW(dstkey, dsttype, srckey, srctype)
#define OP_LONG_ATTR_RAW_WITH_VALUE(dstkey, dsttype, srckey, srctype, dstvalue)
#define OP_ATTR_RAW_VEC(dstkey, dsttype, srckey)
#endif

#ifdef GEN_SCHEMA_DEF
#define OP_ATTR_WITH_VALUE(key, type, value) \
  op_def.append("    ").append(#key).append(": ").append(#type).append(" = ").append(#value).append(";\n");
#define NEW_OP_ATTR_WITH_VALUE(key, type, value) \
  op_def.append("    ").append(#key).append(": ").append(#type).append(";\n");
#define OP_ATTR_ENUM_WITH_VALUE(key, type, value) \
  op_def.append("    ").append(#key).append(": ").append(#type).append(" = ").append(#value).append(";\n");
#elif PRIMITIVE_WRITEABLE
#define OP_ATTR_WITH_VALUE(key, type, value) \
  if (op->GetAttr(#key) != nullptr) {        \
    schema_op->key = op->get_##key();        \
  } else {                                   \
    schema_op->key = value;                  \
  }

#define NEW_OP_ATTR_WITH_VALUE(key, type, value) \
  if (op->GetAttr(#key) != nullptr) {            \
    schema_op->key = op->get_##key();            \
  } else {                                       \
    schema_op->key = value;                      \
  }

#define OP_ATTR_ENUM_WITH_VALUE(key, type, value)                \
  if (op->GetAttr(#key) != nullptr) {                            \
    schema_op->key = static_cast<schema::type>(op->get_##key()); \
  }
#else
#define OP_ATTR_WITH_VALUE(key, type, value)
#define NEW_OP_ATTR_WITH_VALUE(key, type, value)
#define OP_ATTR_ENUM_WITH_VALUE(key, type, value)
#endif

#ifdef GEN_SCHEMA_DEF
#define OP_SCHEMA_DEF_END(OP)                      \
  op_def.append("}\n");                            \
  return op_def;                                   \
  }                                                \
  SchemaOpRegister g_schema_op_##OP(Gen##OP##Def); \
  }  // namespace mindspore::lite::ops
#elif PRIMITIVE_WRITEABLE
#define OP_SCHEMA_DEF_END(OP)                         \
  auto prim = std::make_unique<schema::PrimitiveT>(); \
  if (prim == nullptr) {                              \
    return nullptr;                                   \
  }                                                   \
  prim->value.value = schema_op.release();            \
  prim->value.type = schema::PrimitiveType_##OP;      \
  return prim;                                        \
  }                                                   \
  }  // namespace mindspore::lite::ops
#else
#define OP_SCHEMA_DEF_END(OP)
#endif

#ifdef GEN_SCHEMA_DEF
#define OP_SCHEMA_DEF_ONLY(OP)       \
  namespace mindspore::lite::ops {   \
  std::string Gen##OP##Def() {       \
    std::string op_def = "\ntable "; \
    op_def.append(#OP);              \
    op_def.append(" {\n");
#else
#define OP_SCHEMA_DEF_ONLY(OP)
#endif

#ifdef GEN_SCHEMA_DEF
#define OP_ATTR_ONLY(key, type) op_def.append("    ").append(#key).append(": ").append(#type).append(";\n");
#define OP_ATTR_ONLY_WITH_VALUE(key, type, value) \
  op_def.append("    ").append(#key).append(": ").append(#type).append(" = ").append(#value).append(";\n");
#else
#define OP_ATTR_ONLY(key, type)
#define OP_ATTR_ONLY_WITH_VALUE(key, type, value)
#endif

#ifdef GEN_SCHEMA_DEF
#define OP_SCHEMA_DEF_ONLY_END(OP)                 \
  op_def.append("}\n");                            \
  return op_def;                                   \
  }                                                \
  SchemaOpRegister g_schema_op_##OP(Gen##OP##Def); \
  }  // namespace mindspore::lite::ops
#else
#define OP_SCHEMA_DEF_ONLY_END(OP)
#endif

#endif  // MINDSPORE_LITE_SRC_COMMON_OPS_OPS_DEF_H_
