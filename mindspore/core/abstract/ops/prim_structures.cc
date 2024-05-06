/**
 * This is the C++ adaptation and derivative work of Myia (https://github.com/mila-iqia/myia/).
 *
 * Copyright 2019-2024 Huawei Technologies Co., Ltd
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

#include "abstract/ops/infer_functions.h"
#include "abstract/utils.h"
#include "abstract/param_validator.h"
#include "utils/check_convert_utils.h"
#include "include/common/utils/utils.h"

namespace mindspore {
namespace abstract {
namespace {
void CheckDictKey(const AbstractBasePtr &key, const std::string &op_name) {
  auto key_value = key->BuildValue();
  MS_EXCEPTION_IF_NULL(key_value);
  if (!(key_value->isa<StringImm>() || key_value->isa<Scalar>() || key_value->isa<Type>() || key_value->isa<None>() ||
        (key->isa<AbstractTensor>() && !key_value->ContainsValueAny()) || key->isa<AbstractTuple>())) {
    MS_LOG(EXCEPTION) << op_name << " evaluator key only supports string, number, type, none, "
                      << "constant tensor and tuple, but got " << key->BuildValue()->ToString();
  }
  if (key->isa<AbstractTuple>() && key_value->isa<ValueAny>()) {
    MS_LOG(EXCEPTION) << op_name << " evaluator key should not be tuple that contains variables, but got "
                      << key->BuildValue()->ToString();
  }
}
}  // namespace

void ProcessUnpackDict(const AbstractTuplePtr &key_tuple, const AbstractTuplePtr &value_tuple,
                       std::unordered_map<std::string, AbstractBasePtr> *key_str_value_set,
                       std::vector<AbstractBasePtr> *key_set) {
  // The size of need unpack tuple must be 1
  const auto &key_elements = key_tuple->elements();
  const auto &value_elements = value_tuple->elements();
  if (key_elements.size() != 1) {
    MS_LOG(EXCEPTION) << "The size of need unpack key tuple must be 1, but got " << key_elements.size();
  }
  if (value_elements.size() != 1) {
    MS_LOG(EXCEPTION) << "The size of need unpack value tuple must be 1, but got " << value_elements.size();
  }

  auto unpack_keys = key_elements[0];
  auto unpack_keys_tuple = unpack_keys->cast<AbstractTuplePtr>();
  const auto &unpack_keys_elements = unpack_keys_tuple->elements();

  auto unpack_values = value_elements[0];
  auto unpack_values_tuple = unpack_values->cast<AbstractTuplePtr>();
  const auto &unpack_values_elements = unpack_values_tuple->elements();

  if (unpack_keys_elements.size() != unpack_values_elements.size()) {
    MS_LOG(EXCEPTION) << "The keys' size should be equal to values' size, but the keys' size is "
                      << unpack_keys_elements.size() << ", the values' size is " << unpack_values_elements.size();
  }

  for (size_t inner_index = 0; inner_index < unpack_keys_elements.size(); ++inner_index) {
    auto inner_key = unpack_keys_elements[inner_index];
    auto key_str = inner_key->BuildValue()->ToString();
    (void)key_str_value_set->emplace(key_str, unpack_values_elements[inner_index]);
    (void)key_set->emplace_back(inner_key);
  }
}

AbstractBasePtr InferImplMakeDict(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                  const AbstractBasePtrList &args_abs_list) {
  // Inputs: two tuples.
  const std::string op_name = primitive->name();
  constexpr int args_spec_size = 2;
  CheckArgsSize(op_name, args_abs_list, args_spec_size);
  AbstractSequencePtr keys = CheckArg<AbstractSequence>(op_name, args_abs_list, 0);
  AbstractSequencePtr values = CheckArg<AbstractSequence>(op_name, args_abs_list, 1);

  size_t keys_size = keys->size();
  if (values->size() != keys_size) {
    MS_LOG(EXCEPTION) << op_name << " evaluator keys' size is not equal with values' size";
  }

  AbstractBasePtrList key_list = keys->elements();
  std::unordered_map<std::string, AbstractBasePtr> key_str_value_set;
  std::vector<AbstractBasePtr> key_set;
  std::vector<AbstractElementPair> key_value;
  AbstractBasePtrList value_list = values->elements();
  constexpr auto need_unpack = "need_unpack";
  for (size_t index = 0; index < keys_size; index++) {
    const auto &key = key_list[index];
    bool is_need_unpack = false;
    if (key->isa<AbstractTuple>()) {
      auto key_tuple = key->cast<AbstractTuplePtr>();
      if (key_tuple->HasData(need_unpack)) {
        is_need_unpack = *key_tuple->GetData<bool>(need_unpack);
        if (is_need_unpack) {
          auto value_tuple = value_list[index]->cast<AbstractTuplePtr>();
          MS_EXCEPTION_IF_NULL(value_tuple);
          ProcessUnpackDict(key_tuple, value_tuple, &key_str_value_set, &key_set);
        }
      }
    }
    CheckDictKey(key, op_name);
    auto key_val = key->BuildValue()->ToString();
    auto iter = key_str_value_set.find(key_val);
    // Remove duplicate keys.
    // {Tensor[1]: x, Tensor[1}: y} the length of dict is 2, means the two keys are not duplicate.
    if (iter != key_str_value_set.end() && !key->isa<AbstractTensor>()) {
      iter->second = value_list[index];
    } else if (!is_need_unpack) {
      auto key_str = key->BuildValue()->ToString();
      key_str_value_set.insert(std::pair<std::string, AbstractBasePtr>(key_str, value_list[index]));
      key_set.push_back(key);
    }
  }
  for (auto &key : key_set) {
    auto key_str = key->BuildValue()->ToString();
    (void)key_value.emplace_back(key, key_str_value_set[key_str]);
  }
  return std::make_shared<AbstractDictionary>(key_value);
}

AbstractBasePtr InferImplMakeKeywordArg(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                        const AbstractBasePtrList &args_abs_list) {
  // Inputs: a string and an object of a subclass of AbstractBase.
  const std::string op_name = primitive->name();
  constexpr int args_spec_size = 2;
  CheckArgsSize(op_name, args_abs_list, args_spec_size);
  AbstractScalarPtr key = CheckArg<AbstractScalar>(op_name, args_abs_list, 0);

  ValuePtr keyPtr = key->BuildValue();
  MS_EXCEPTION_IF_NULL(keyPtr);
  if (!keyPtr->isa<StringImm>()) {
    MS_LOG(EXCEPTION) << op_name << " evaluator key should be string, but got " << keyPtr->ToString();
  }
  auto key_string = GetValue<std::string>(keyPtr);
  return std::make_shared<AbstractKeywordArg>(key_string, args_abs_list[1]);
}

AbstractBasePtr InferImplExtractKeywordArg(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                           const AbstractBasePtrList &args_abs_list) {
  // Inputs: a key and a Keyword or only a Keyword.
  const std::string op_name = primitive->name();
  constexpr int only_kw_input_size = 1;
  constexpr int check_key_input_size = 2;
  AbstractKeywordArgPtr kwarg = nullptr;
  if (args_abs_list.size() == check_key_input_size) {
    AbstractScalarPtr key = CheckArg<AbstractScalar>(op_name, args_abs_list, 0);
    kwarg = CheckArg<AbstractKeywordArg>(op_name, args_abs_list, 1);

    ValuePtr key_value = key->BuildValue();
    MS_EXCEPTION_IF_NULL(key_value);
    if (!key_value->isa<StringImm>()) {
      MS_LOG(EXCEPTION) << op_name << " evaluator key should be string, but got " << key_value->ToString();
    }
    auto key_input = GetValue<std::string>(key_value);
    std::string key_actual = kwarg->get_key();
    if (key_actual != key_input) {
      MS_LOG(EXCEPTION) << op_name << " evaluator input key should be same as AbstractKeywordArg' key, but input is "
                        << key_input << ", AbstractKeywordArg' key is " << key_actual;
    }
  } else if (args_abs_list.size() == only_kw_input_size) {
    kwarg = CheckArg<AbstractKeywordArg>(op_name, args_abs_list, 0);
  } else {
    MS_LOG(EXCEPTION) << "For '" << op_name << "', the number of inputs should be 1 or 2, but got "
                      << args_abs_list.size();
  }
  return kwarg->get_arg();
}

void CheckDynamicLengthSequenceSetItem(const std::string &op_name, const AbstractSequencePtr &queue,
                                       const AbstractBasePtr &target) {
  auto element_abs = queue->dynamic_len_element_abs();
  if (element_abs == nullptr) {
    MS_LOG(EXCEPTION) << "Empty variable len sequence can not setitem.";
  }
  const auto precondition_log = "For " + op_name + ", when the queue is dynamic length";
  const auto standard_abs_description = "element within dynamic length sequence";
  const auto differ_abs_description = "target element";
  CheckAndConvertUtils::CheckAbstractTypeAndShapeSame(std::vector<AbstractBasePtr>{element_abs, target},
                                                      precondition_log, standard_abs_description,
                                                      differ_abs_description);
}

template <typename T>
AbstractBasePtr InferTupleOrListSetItem(const std::string &op_name, const AbstractBasePtrList &args_abs_list) {
  // Inputs: a tuple or list, a scalar whose value is an int64 number and an object of a subclass of AbstractBase.
  constexpr int args_spec_size = 3;
  CheckArgsSize(op_name, args_abs_list, args_spec_size);
  auto queue = CheckArg<T>(op_name, args_abs_list, 0);
  AbstractScalarPtr index = CheckArg<AbstractScalar>(op_name, args_abs_list, 1);

  auto index_type = index->BuildType();
  MS_EXCEPTION_IF_NULL(index_type);
  if (index_type->type_id() != kInt64->type_id()) {
    MS_EXCEPTION(IndexError) << op_name << " evaluator index should be an int64 number, but got a "
                             << index_type->ToString() << " number.";
  }
  ValuePtr index_value = index->BuildValue();
  MS_EXCEPTION_IF_NULL(index_value);
  auto target = args_abs_list[kIndex2];
  MS_EXCEPTION_IF_NULL(target);
  if (queue->dynamic_len()) {
    CheckDynamicLengthSequenceSetItem(op_name, queue, target);
    return queue->Clone();
  }
  if (index_value->ContainsValueAny()) {
    // If the index is variable and the sequence is constant length, then all of the element within the sequence
    // should have the same type and shape with the target input. The element within the return sequence should
    // be all broadened.
    const auto &elements = queue->elements();
    if (elements.size() == 0) {
      MS_LOG(EXCEPTION) << "Empty sequence can not setitem.";
    }
    const auto precondition_log = "For " + op_name + ", when the index is variable and the queue is constant length";
    CheckAndConvertUtils::CheckAbstractTypeAndShapeSame(elements, precondition_log);
    auto first_element = elements[kIndex0];
    const auto standard_abs_description = "element within constant length sequence";
    const auto differ_abs_description = "target element";
    CheckAndConvertUtils::CheckAbstractTypeAndShapeSame(std::vector<AbstractBasePtr>{first_element, target},
                                                        precondition_log, standard_abs_description,
                                                        differ_abs_description);
    return CheckAndConvertUtils::BroadenAllSequenceElements(queue);
  }
  auto index_int64_value = GetValue<int64_t>(index_value);
  AbstractBasePtrList elements = queue->elements();
  std::size_t nelems = elements.size();
  if (nelems == 0) {
    MS_EXCEPTION(IndexError) << "Can not setitem for an empty sequence.";
  }
  int64_t index_positive_value = index_int64_value >= 0 ? index_int64_value : index_int64_value + SizeToLong(nelems);
  if (index_positive_value < 0 || index_positive_value >= SizeToLong(nelems)) {
    MS_EXCEPTION(IndexError) << op_name << " evaluator the index: " << index_int64_value << " to set out of range: [-"
                             << nelems << "," << (nelems - 1) << "].";
  }
  size_t index_unsigned_value = LongToSize(index_positive_value);
  elements[index_unsigned_value] = args_abs_list[kIndex2];
  MS_LOG(DEBUG) << "SetItem use flags, index: " << index_unsigned_value << ", for " << queue->ToString();
  return std::make_shared<T>(elements, queue->sequence_nodes());
}

AbstractBasePtr InferImplTupleSetItem(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                      const AbstractBasePtrList &args_abs_list) {
  return InferTupleOrListSetItem<AbstractTuple>(primitive->name(), args_abs_list);
}

AbstractBasePtr InferImplListSetItem(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                     const AbstractBasePtrList &args_abs_list) {
  return InferTupleOrListSetItem<AbstractList>(primitive->name(), args_abs_list);
}

AbstractBasePtr InferImplDictGetItem(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                     const AbstractBasePtrList &args_abs_list) {
  const std::string op_name = primitive->name();
  // dict[key] mean the size of args_abs_list is 2.
  // dict.get('key', default_value=None) mean the size of args_abs_list is 2 too, the key will check in dict_get.
  constexpr int subscript_args_size = 2;
  if (args_abs_list.size() != subscript_args_size) {
    MS_LOG(EXCEPTION) << "For '" << op_name << "', the number of input should be " << subscript_args_size
                      << ", but got " << args_abs_list.size();
  }
  AbstractDictionaryPtr dict = CheckArg<AbstractDictionary>(op_name, args_abs_list, 0);
  const auto &key = args_abs_list[1];
  CheckDictKey(key, op_name);

  ValuePtr key_value = key->BuildValue();
  MS_EXCEPTION_IF_NULL(key_value);
  std::vector<AbstractElementPair> dict_elems = dict->elements();
  auto it = std::find_if(dict_elems.cbegin(), dict_elems.cend(), [&key_value](const AbstractElementPair &item) {
    return *key_value == *item.first->BuildValue();
  });
  if (it == dict_elems.end()) {
    // For dict[key], if key is not exist, will raise a ValueError exception.
    // For dict.get('key', default=None), if key is not exist, will return the default value during dict_get.
    // Python KeyError will print escape character. So use ValueError instead of KeyError here.
    MS_EXCEPTION(ValueError) << "The key " << key_value->ToString()
                             << " does not exist in the dict:" << args_abs_list[0]->BuildValue()->ToString();
  }
  return it->second;
}

AbstractBasePtr InferImplDictSetItem(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                     const AbstractBasePtrList &args_abs_list) {
  // Inputs: a dict and a scalar whose value is a string and an object of a subclass of AbstractBase.
  const std::string op_name = primitive->name();
  constexpr int args_spec_size = 3;
  CheckArgsSize(op_name, args_abs_list, args_spec_size);
  AbstractDictionaryPtr dict = CheckArg<AbstractDictionary>(op_name, args_abs_list, 0);
  const auto &key = args_abs_list[1];
  CheckDictKey(key, op_name);

  ValuePtr key_value = key->BuildValue();
  MS_EXCEPTION_IF_NULL(key_value);
  std::vector<AbstractElementPair> dict_elems = dict->elements();
  auto it = std::find_if(dict_elems.cbegin(), dict_elems.cend(), [&key_value](const AbstractElementPair &item) {
    return *key_value == *item.first->BuildValue();
  });

  MS_EXCEPTION_IF_NULL(args_abs_list[2]);
  auto new_ele = std::make_pair(args_abs_list[1], args_abs_list[2]);
  if (it != dict_elems.end()) {
    int64_t index = it - dict_elems.begin();
    dict_elems[LongToSize(index)] = new_ele;
  } else {
    dict_elems.push_back(new_ele);
  }
  return std::make_shared<AbstractDictionary>(dict_elems);
}

AbstractBasePtr InferImplDictGetKeys(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                     const AbstractBasePtrList &args_abs_list) {
  // Inputs: a dict.
  const std::string op_name = primitive->name();
  constexpr int args_spec_size = 1;
  CheckArgsSize(op_name, args_abs_list, args_spec_size);
  AbstractDictionaryPtr dict = CheckArg<AbstractDictionary>(op_name, args_abs_list, 0);
  std::vector<AbstractElementPair> dict_elems = dict->elements();
  AbstractBasePtrList keys;
  (void)std::transform(dict_elems.cbegin(), dict_elems.cend(), std::back_inserter(keys),
                       [](const AbstractElementPair &item) { return item.first; });
  return std::make_shared<AbstractTuple>(keys);
}

AbstractBasePtr InferImplDictGetValues(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                       const AbstractBasePtrList &args_abs_list) {
  // Inputs: a dict.
  const std::string op_name = primitive->name();
  constexpr int args_spec_size = 1;
  CheckArgsSize(op_name, args_abs_list, args_spec_size);
  AbstractDictionaryPtr dict = CheckArg<AbstractDictionary>(op_name, args_abs_list, 0);
  std::vector<AbstractElementPair> dict_elems = dict->elements();
  AbstractBasePtrList values;
  (void)std::transform(dict_elems.cbegin(), dict_elems.cend(), std::back_inserter(values),
                       [](const AbstractElementPair &item) { return item.second; });
  return std::make_shared<AbstractTuple>(values);
}

AbstractBasePtr InferImplDictItems(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                   const AbstractBasePtrList &args_abs_list) {
  // Inputs: a dict.
  const std::string op_name = primitive->name();
  constexpr int args_spec_size = 1;
  CheckArgsSize(op_name, args_abs_list, args_spec_size);
  AbstractDictionaryPtr dict = CheckArg<AbstractDictionary>(op_name, args_abs_list, 0);
  std::vector<AbstractElementPair> dict_elems = dict->elements();
  AbstractBasePtrList items;
  (void)std::transform(dict_elems.cbegin(), dict_elems.cend(), std::back_inserter(items),
                       [](const AbstractElementPair &item) {
                         return std::make_shared<AbstractTuple>(AbstractBasePtrList{item.first, item.second});
                       });
  return std::make_shared<AbstractList>(items);
}

namespace {
void CheckMutableArgAbstract(const AbstractBasePtr &abs) {
  if (abs->isa<AbstractSequence>()) {
    auto abs_seq = abs->cast_ptr<AbstractSequence>();
    for (const auto &ele : abs_seq->elements()) {
      CheckMutableArgAbstract(ele);
    }
    return;
  }
  if (abs->isa<AbstractDictionary>()) {
    auto abs_dic = abs->cast_ptr<AbstractDictionary>();
    for (const auto &ele : abs_dic->elements()) {
      CheckMutableArgAbstract(ele.second);
    }
    return;
  }
  if (abs->isa<AbstractTensor>()) {
    return;
  }
  if (abs->isa<AbstractScalar>()) {
    auto type_ptr = abs->GetType();
    if (type_ptr->isa<Number>()) {
      return;
    }
  }
  MS_EXCEPTION(TypeError) << "For 'mutable', the 'input_data' should be one of (bool, int, float, Tensor, "
                             "tuple, list, dict) or their nested structures, but got "
                          << abs->ToString();
}
}  // namespace

AbstractBasePtr InferImplMutable(const AnalysisEnginePtr &, const PrimitivePtr &,
                                 const AbstractBasePtrList &args_abs_list) {
  constexpr int min_args_abs_size = 1;
  constexpr int max_args_abs_size = 2;
  auto arg_size = args_abs_list.size();
  if (arg_size != min_args_abs_size && arg_size != max_args_abs_size) {
    MS_LOG(EXCEPTION) << "For 'mutable', the number of inputs should be 1 or 2, but got " << args_abs_list.size();
  }
  bool variable_len = false;
  if (arg_size == max_args_abs_size) {
    auto arg_value = args_abs_list[1]->GetValue();
    MS_EXCEPTION_IF_NULL(arg_value);
    if (!arg_value->isa<BoolImm>()) {
      MS_EXCEPTION(TypeError) << "For 'mutable', the second input should be bool, but got: "
                              << args_abs_list[1]->ToString();
    }
    variable_len = arg_value->cast<BoolImmPtr>()->value();
  }
  auto data = args_abs_list[0];
  MS_EXCEPTION_IF_NULL(data);
  if (!variable_len) {
    if (data->isa<AbstractSequence>() && data->cast<AbstractSequencePtr>()->dynamic_len()) {
      MS_LOG(EXCEPTION) << "For 'mutable', can not convert a dynamic length sequence to constant length.";
    }
    CheckMutableArgAbstract(data);
    return AbstractBroaden(data);
  }
  auto ret = data->Clone();
  if (ret->isa<AbstractAny>()) {
    return ret;
  }
  if (!ret->isa<AbstractSequence>()) {
    MS_EXCEPTION(TypeError) << "For 'mutable', when the variable_len is True, the first input should be"
                            << " list or tuple, but got: " << ret->ToString();
  }
  auto ret_seq = ret->cast<AbstractSequencePtr>();
  if (!ret_seq->dynamic_len()) {
    ret_seq->CheckAndConvertToDynamicLenSequence();
  }
  if (ret->isa<AbstractList>()) {
    // Dynamic length list should not attach python object.
    auto ret_list = ret->cast<AbstractListPtr>();
    ret_list->ClearExtraInfo();
  }
  return ret;
}

namespace {
std::string GetRefKey(const AbstractRefPtr &ref_tensor) {
  const auto &ref_key_value = ref_tensor->ref_key_value();
  MS_EXCEPTION_IF_NULL(ref_key_value);
  auto ref_key = ref_key_value->cast_ptr<RefKey>();
  MS_EXCEPTION_IF_NULL(ref_key);
  return ref_key->value();
}

void GetGradAbstract(const AbstractBasePtr &grads_abs, const std::string &para_name, int64_t position,
                     AbstractBasePtr *ret) {
  auto grad_abs_tuple = grads_abs->cast_ptr<AbstractTuple>();
  if (grad_abs_tuple == nullptr || grad_abs_tuple->elements().size() == 0) {
    return;
  }
  auto abs0 = grad_abs_tuple->elements()[0];
  if (abs0->isa<AbstractScalar>()) {
    auto buildptr = abs0->cast_ptr<AbstractScalar>();
    MS_EXCEPTION_IF_NULL(buildptr);
    auto build_value = buildptr->BuildValue();
    size_t expect_size = 2;
    if (grad_abs_tuple->elements().size() >= expect_size) {
      if (build_value->isa<Int64Imm>()) {
        if (GetValue<int64_t>(build_value) == position) {
          *ret = grad_abs_tuple->elements()[1];
        }
      } else if (build_value->isa<StringImm>()) {
        if (GetValue<std::string>(build_value) == para_name) {
          *ret = grad_abs_tuple->elements()[1];
        }
      }
    }
    return;
  } else {
    for (const auto &abs : grad_abs_tuple->elements()) {
      GetGradAbstract(abs, para_name, position, ret);
    }
    return;
  }
}
}  // namespace

AbstractBasePtr InferImplGetGrad(const AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                 const AbstractBasePtrList &args_abs_list) {
  const std::string &op_name = primitive->name();
  constexpr int expected_args_spec_size = 2;
  CheckArgsSize(op_name, args_abs_list, expected_args_spec_size);
  auto &hash_id_abs = args_abs_list[1];

  int64_t position = -1;
  std::string para_name;
  if (hash_id_abs->isa<AbstractScalar>()) {
    auto buildptr = hash_id_abs->cast_ptr<AbstractScalar>();
    if (buildptr == nullptr) {
      MS_EXCEPTION(TypeError) << "For " << op_name << ", the `x` should be an integer or a Parameter, but got nullptr";
    }
    auto build_value = buildptr->BuildValue();
    if (!build_value->isa<Int64Imm>()) {
      MS_EXCEPTION(TypeError) << "For " << op_name << ", the `x` should be an int64 number, but got "
                              << build_value->ToString();
    }
    position = GetValue<int64_t>(build_value);
  } else if (hash_id_abs->isa<AbstractRefTensor>()) {
    para_name = GetRefKey(hash_id_abs->cast<AbstractRefPtr>());
  } else {
    MS_EXCEPTION(TypeError) << "For " << op_name << ", the `x` should be an integer or a Parameter, but got "
                            << hash_id_abs->ToString();
  }
  AbstractBasePtr ret = nullptr;
  GetGradAbstract(args_abs_list[0], para_name, position, &ret);
  if (ret == nullptr) {
    MS_LOG(EXCEPTION) << "Can not find the gradient for position or Parameter " << args_abs_list[1]->ToString();
  }
  return ret;
}
}  // namespace abstract
}  // namespace mindspore
