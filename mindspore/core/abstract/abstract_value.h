/**
 * This is the C++ adaptation and derivative work of Myia (https://github.com/mila-iqia/myia/).
 *
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CORE_ABSTRACT_ABSTRACT_VALUE_H_
#define MINDSPORE_CORE_ABSTRACT_ABSTRACT_VALUE_H_

#include <cstdint>
#include <utility>
#include <vector>
#include <string>
#include <memory>
#include "utils/log_adapter.h"
#include "utils/hashing.h"
#include "utils/any.h"
#include "utils/hash_map.h"
#include "base/base.h"
#include "base/user_data.h"
#include "ir/dtype.h"
#include "ir/value.h"
#include "ir/tensor.h"
#include "ir/map_tensor.h"
#include "abstract/dshape.h"
#include "abstract/utils.h"
#include "utils/shape_utils.h"
#include "mindspore/core/symbolic_shape/symbol.h"

namespace mindspore {
namespace abstract {
class AbstractBase;
using AbstractBasePtrList = std::vector<AbstractBasePtr>;
/// \brief The base class for abstract value of an anf node.
///
/// The abstract value is used in evaluator to express
/// the type, shape and value of an anf node.
class MS_CORE_API AbstractBase : public Base {
 public:
  using TraceNodeProvider = std::function<void(AnfNodePtr *node)>;

  /// \brief Constructor of AbstractBase.
  ///
  /// \param[in] value The real value (if any) of an anf node. Default: nullptr.
  /// \param[in] type The type of an anf node. Default: kTypeAny.
  /// \param[in] shape The dimension of an anf node. Default: kNoShape.
  explicit AbstractBase(const ValuePtr &value = nullptr, const TypePtr &type = kTypeAny,
                        const BaseShapePtr &shape = kNoShape);

  /// \brief Copy constructor
  /// \param[in] abstract_base an abstract
  AbstractBase(const AbstractBase &other);

  /// \brief Overloads operator '=' for Named.
  ///
  /// \param[in] other An an abstract.
  /// \return An abstract set with the same type, value and shape as abstract_base.
  AbstractBase &operator=(const AbstractBase &other);

  /// \brief Destructor of AbstractBase.
  ~AbstractBase() override = default;
  MS_DECLARE_PARENT(AbstractBase, Base)

  /// \brief Get the hash number of the abstract.
  ///
  /// \return The hash of the object.
  std::size_t hash() const override;

  /// \brief Get the formatted text to describe the abstract.
  ///
  /// \return A string.
  std::string ToString() const override;

  /// \brief Get the formatted text to describe the abstract.
  ///
  /// \return A string.
  virtual std::string ToString(bool verbose) const;

  /// \brief Overwrite the operator '==' to compare other abstract.
  ///
  /// \param[in] other The other abstract to be joined.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  virtual bool operator==(const AbstractBase &other) const;

  /// \brief Set the value for the AbstractBase.
  ///
  /// \param[in] value The value of an anf node.
  void set_value(const ValuePtr &value);

  /// \brief Set the type for the AbstractBase.
  ///
  /// \param[in] type The type of an anf node.
  void set_type(const TypePtr &type);

  /// \brief Set the shape for the AbstractBase.
  ///
  /// \param[in] shape The shape of an anf node.
  virtual void set_shape(const BaseShapePtr &shape);

  /// \brief Set the value description for the AbstractBase.
  ///
  /// \param[in] desc The description of value.
  void set_value_desc(const std::string &desc);

  /// \brief Get the value description.
  ///
  /// \return A string of the value description.
  const std::string &value_desc() const;

  /// \brief Get the abstract value, which is tracked.
  ///
  /// \return A pointer to the Value.
  const ValuePtr &GetValueTrack() const;

  /// \brief Get the abstract type, which is tracked.
  ///
  /// \return A pointer to the Type.
  const TypePtr &GetTypeTrack() const;

  /// \brief Get the abstract shape, which is tracked.
  ///
  /// \return A pointer to the BaseShape.
  const BaseShapePtr &GetShapeTrack() const;

  /// \brief Try to build a real value from an abstract value.
  ///
  /// \note This is a deprecated function, please do not call it, use GetValue instead.
  /// \note If the value cannot be built, a default value (ValueAny) is returned.
  ///
  /// \return A pointer to the Value.
  ValuePtr BuildValue() const;

  /// \brief Build the type of the abstract.
  ///
  /// \note This is a deprecated function, please do not call it, use GetType instead.
  /// \note Use this function to get the actual type, while track type is not enough accurate.
  ///
  /// \return A pointer to the Type.
  virtual TypePtr BuildType() const { MS_LOG(EXCEPTION) << "The method 'BuildType()' doesn't implement"; }

  /// \brief Build the shape of the abstract.
  ///
  /// \note This is a deprecated function, please do not call it, use GetShape instead.
  /// \note Use this function to get the actual shape, while track shape is not enough accurate.
  ///
  /// \return A pointer to the BaseShape.
  virtual BaseShapePtr BuildShape() const;

  /// \brief Get or build the shape of AbstractBase.
  ///
  /// \return The base shape got or built.
  virtual BaseShapePtr GetShape() const;

  /// \brief Get or build the object type of the AbstractBase.
  ///
  /// \return The object type.
  virtual TypePtr GetType() const;

  /// \brief Get or build the value of the AbstractBase.
  ///
  /// \return The value of the AbstractBase if exists, else return kValueAny.
  virtual ValuePtr GetValue() const;

  /// \brief Set the symbolic shape of the abstract.
  void SetSymbolicShape(const ListSymbolPtr &s) { symbolic_shape_ = s; }

  /// \brief Get the symbolic shape of the abstract.
  ///
  /// \return The symbolic shape if exists, else return nullptr.
  const ListSymbolPtr &GetSymbolicShape() const { return symbolic_shape_; }

  /// \brief Set the symbolic shape of the abstract.
  void SetSymbolicValue(const SymbolPtr &s) { symbolic_value_ = s; }

  /// \brief Get the symbolic value of the abstract.
  ///
  /// \return The symbolic value if exists, else return nullptr.
  const SymbolPtr &GetSymbolicValue() const { return symbolic_value_; }

  /// \brief Clone an abstract from the abstract.
  ///
  /// \return A pointer to the cloned abstract.
  virtual AbstractBasePtr Clone() const { MS_LOG(EXCEPTION) << "The method 'Clone()' doesn't implement"; }

  /// \brief Set the function, which prints the debug info.
  ///
  /// \param[in] trace_node_provider The function.
  static void set_trace_node_provider(const TraceNodeProvider &trace_node_provider);

  static TraceNodeProvider trace_node_provider_;

  /// \brief Broaden the abstract. It will upgrade the abstract to a higher level.
  ///
  /// \return A pointer to the broadened abstract.
  virtual AbstractBasePtr Broaden() const;

  /// \brief Combine two abstracts. If two abstracts are different, it will broaden the abstract value.
  ///
  /// \param[in] other The other abstract to be joined.
  ///
  /// \return A pointer to the combined abstract.
  virtual AbstractBasePtr Join(const AbstractBasePtr &other);
  bool IsBroaden() const;

  /// \brief Write the abstract's string to the std::ostream.
  ///
  /// \param[in] os A std::ostream.
  /// \param[in] a An abstract.
  ///
  /// \return A std::ostream.
#ifndef _MSC_VER
  friend std::ostream &operator<<(std::ostream &os, const std::shared_ptr<AbstractBase> &a) {
    os << a->ToString();
    return os;
  }
#endif
  /// \brief Broaden abstract with constraints.
  ///
  /// \return A pointer to the broadened abstract.
  virtual AbstractBasePtr PartialBroaden() const;

  /// \brief Process the abstract with InterpretedObject.
  using InterpretBoolChecker = std::pair<bool, bool> (*)(const AbstractBasePtr &cond);
  static inline InterpretBoolChecker interpret_bool_checker_ = nullptr;
  static void set_interpret_bool_checker(InterpretBoolChecker checker);
  static InterpretBoolChecker interpret_bool_checker();

  /// \brief Process the user date of abstract with PyExecute node.
  using PyExecuteUserDataCatcher = std::pair<bool, ValuePtr> (*)(const AbstractBasePtr &element_abs);
  static inline PyExecuteUserDataCatcher pyexecute_user_data_catcher_ = nullptr;
  static void set_pyexecute_user_data_catcher(PyExecuteUserDataCatcher catcher);
  static inline PyExecuteUserDataCatcher pyexecute_user_data_catcher();

  /// \brief Store for mindir input and output names.
  std::string name() const;
  void set_name(const std::string &name);

  /// \brief Cover *this abstract for inplace primitive. If inplace_abstract() is not null, use it as real abstract.
  AbstractBasePtr inplace_abstract() const;
  void set_inplace_abstract(const AbstractBasePtr &inplace_abstract);

 protected:
  /// \brief Build a value when value is not set.
  ///
  /// \return A pointer to the Value.
  virtual ValuePtr RealBuildValue() const;

  ValuePtr value_;
  TypePtr type_;
  BaseShapePtr shape_;
  std::string value_desc_;  // Store initial value description for error report.
  std::string name_;        // Store for mindir input and output names.
  ListSymbolPtr symbolic_shape_{nullptr};
  SymbolPtr symbolic_value_{nullptr};

 private:
  AbstractBasePtr inplace_abstract_{nullptr};  // Cover *this abstract for inplace primitive.
};

/// \brief Class AbstractScalar describes a scalar's type and value.
class MS_CORE_API AbstractScalar final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractScalar.
  AbstractScalar();

  /// \brief Constructor of AbstractScalar.
  ///
  /// \param[in] value The real value of an anf node.
  /// \param[in] type The type of an anf node.
  AbstractScalar(const ValuePtr &value, const TypePtr &type);

  /// \brief Constructor of AbstractScalar.
  ///
  /// \param[in] value The real value of an anf node.
  explicit AbstractScalar(const ValuePtr &value);

  /// \brief Constructor of AbstractScalar, inited with an int number.
  ///
  /// \param[in] value An int number.
  explicit AbstractScalar(int value);

  /// \brief Constructor of AbstractScalar, inited with an int64 number.
  ///
  /// \param[in] value An int64 number.
  explicit AbstractScalar(int64_t value);

  /// \brief Constructor of AbstractScalar, inited with a float number.
  ///
  /// \param[in] value A float number.
  explicit AbstractScalar(float value);

  /// \brief Constructor of AbstractScalar, inited with a double number.
  ///
  /// \param[in] value A double number.
  explicit AbstractScalar(double value);

  /// \brief Constructor of AbstractScalar, inited with a bool.
  ///
  /// \param[in] value A boolean variable.
  explicit AbstractScalar(bool value);

  /// \brief Constructor of AbstractScalar, inited with a string.
  ///
  /// \param[in] value A string.
  explicit AbstractScalar(const std::string &value);

  /// \brief Constructor of AbstractScalar, inited with a type.
  ///
  /// \param[in] type The type.
  explicit AbstractScalar(const TypePtr &type);

  /// \brief Destructor of AbstractScalar.
  ~AbstractScalar() override = default;
  MS_DECLARE_PARENT(AbstractScalar, AbstractBase)

  /// \brief Set the flag 'is_variable_' for scalar.
  ///
  /// \param[in] is_variable Boolean value for flag 'is_variable_'.
  void set_is_variable(bool is_variable);

  std::size_t hash() const override;

  TypePtr BuildType() const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;

  AbstractBasePtr Join(const AbstractBasePtr &other) override;

 private:
  bool is_variable_{false};
};
using AbstractScalarPtr = std::shared_ptr<AbstractScalar>;

/// \brief Class AbstractType describes the abstract value from a Typeof node.
class MS_CORE_API AbstractType final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractType.
  ///
  /// \param[in] type The type of an anf node.
  explicit AbstractType(const TypePtr &type) : AbstractBase(type, kTypeType) {
    if (type == nullptr) {
      MS_LOG(EXCEPTION) << "type is nullptr";
    }
  }

  /// \brief Destructor of AbstractType.
  ~AbstractType() override = default;
  MS_DECLARE_PARENT(AbstractType, AbstractBase)

  std::string ToString() const override;

  bool operator==(const AbstractBase &other) const override;

  TypePtr BuildType() const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;
};
using AbstractTypePtr = std::shared_ptr<AbstractType>;

/// \brief Class AbstractClass describes the abstract value from a class.
class MS_CORE_API AbstractClass final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractClass.
  ///
  /// \param[in] value A class value.
  explicit AbstractClass(const ValuePtr &value)
      : AbstractBase(value, kClassType),
        hash_(hash_combine({tid(), GetValueTrack()->hash(), GetTypeTrack()->hash()})) {}

  /// \brief Destructor of AbstractClass.
  ~AbstractClass() override = default;
  MS_DECLARE_PARENT(AbstractClass, AbstractBase)

  std::string ToString() const override;

  std::size_t hash() const override { return hash_; }

  bool operator==(const AbstractBase &other) const override;

  TypePtr BuildType() const override { return std::make_shared<MsClassType>(); }

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override { return Clone(); }

  AbstractBasePtr Join(const AbstractBasePtr &other) override;

 private:
  std::size_t hash_;
};
using AbstractClassPtr = std::shared_ptr<AbstractClass>;

/// \brief Class AbstractProblem describes the abstract value from an error.
class MS_CORE_API AbstractProblem final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractProblem.
  ///
  /// \param[in] err the error string.
  /// \param[in] node the binding anf node.
  AbstractProblem(const ValueProblemPtr &err, const AnfNodePtr &node);

  /// \brief Destructor of AbstractProblem.
  ~AbstractProblem() override = default;
  MS_DECLARE_PARENT(AbstractProblem, AbstractBase)

  TypePtr BuildType() const override;

  AbstractBasePtr Broaden() const override;

  AbstractBasePtr Clone() const override;

  std::string ToString() const override;

 private:
  // Origin node been specialized to AbstractProblem, for debug purpose only.
  const AnfNodePtr node_;
};

/// \brief Class AbstractScript describes the script node's type, shape and value.
class MS_CORE_API AbstractScript final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractScript.
  AbstractScript();

  /// \brief Constructor of AbstractScript.
  ///
  /// \param[in] value The real value of an anf node.
  /// \param[in] type The type of an anf node.
  AbstractScript(const ValuePtr &value, const TypePtr &type);

  /// \brief Constructor of AbstractScript.
  ///
  /// \param[in] value The real value to be set.
  explicit AbstractScript(const ValuePtr &value);

  /// \brief Destructor of AbstractScript.
  ~AbstractScript() override = default;
  MS_DECLARE_PARENT(AbstractScript, AbstractBase)

  std::size_t hash() const override;

  TypePtr BuildType() const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;
};
using AbstractScriptPtr = std::shared_ptr<AbstractScript>;

class Evaluator;
using EvaluatorPtr = std::shared_ptr<Evaluator>;
class AnalysisEngine;
using AnalysisEnginePtr = std::shared_ptr<AnalysisEngine>;

class AbstractFunction;
using AbstractFunctionPtr = std::shared_ptr<AbstractFunction>;
class AbstractFuncAtom;
using AbstractFuncAtomPtr = std::shared_ptr<AbstractFuncAtom>;
using AbstractFuncAtomPtrList = std::vector<AbstractFuncAtomPtr>;

/// \brief The base class for the abstract value of the function node.
class MS_CORE_API AbstractFunction : public AbstractBase {
 public:
  /// \brief Constructor of AbstractFunction.
  AbstractFunction() = default;
  /// \brief Destructor of AbstractFunction.
  ~AbstractFunction() override = default;
  MS_DECLARE_PARENT(AbstractFunction, AbstractBase)

  /// \brief Get the unique AbstractFunction.
  ///
  /// If there is exactly one possible function, return it. Otherwise, raise an Exception.
  /// Caller should ensure the uniqueness.
  ///
  /// \return A pointer to AbstractFunction.
  virtual AbstractFunctionPtr GetUnique() = 0;

  TypePtr BuildType() const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;

  /// \brief Copy an AbstractFunction.
  ///
  /// \return A pointer to the copied abstract.
  virtual AbstractFunctionPtr Copy() const = 0;

  /// \brief Combine two abstracts. If two abstracts are different, it will broaden the abstract value.
  ///
  /// \param[in] other The other abstract to be joined.
  ///
  /// \return A pointer to the combined abstract.
  AbstractBasePtr Join(const AbstractBasePtr &other) final;

  /// \brief Combine two abstracts. If two abstracts are different, it will broaden the abstract value.
  ///
  /// \param[in] other The other abstract to be joined.
  ///
  /// \return A pointer to the combined abstract.
  virtual AbstractFunctionPtr Join(const AbstractFunctionPtr &other) = 0;

  /// \brief Handle something with the outer visit function.
  virtual void Visit(std::function<void(const AbstractFuncAtomPtr &)>) const = 0;

  /// \brief Overwrite the operator '==' to compare other abstract.
  ///
  /// \param[in] other The other abstract to be joined.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const final;

  /// \brief Overwrite the operator '==' to compare other AbstractFunction.
  ///
  /// \param[in] other The other instance of AbstractFunction.
  ///
  /// \return A boolean, which indicates whether the other AbstractFunction is same.
  virtual bool operator==(const AbstractFunction &other) const = 0;

  /// \brief Make a AbstractFuncUnion from a list of AbstractFuncAtom.
  ///
  /// \param[in] func_list A list of AbstractFuncAtomPtrList.
  /// \return A point to the AbstractFunction.
  static AbstractFunctionPtr MakeAbstractFunction(const AbstractFuncAtomPtrList &func_list);

  /// \brief Get the tracking id as the memory address of the anf node.
  ///
  /// \return The memory address of to the anf node.
  virtual std::uintptr_t tracking_id() const;

  /// \brief Copy an AbstractFunction without copying tracking id.
  ///
  /// \return A pointer to the copied abstract.
  virtual AbstractFunctionPtr CopyWithoutTrackingId() const;

  /// \brief Get the context which manages the abstract.
  ///
  /// \return A point to the context.
  virtual AnalysisContextPtr context() const;

  static std::uintptr_t ToTrackingId(const AnfNodePtr &node);
};

using AbstractFunctionPtrList = std::vector<AbstractFunctionPtr>;

/// \brief Class AbstractKeywordArg describes an abstract value from a key-value node.
///
/// Represents a key-value pair used in function's parameters.
class MS_CORE_API AbstractKeywordArg final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractKeywordArg.
  ///
  /// \param[in] key The key name of the key-value pair.
  /// \param[in] argument The key value of the key-value pair.
  AbstractKeywordArg(const std::string &key, const AbstractBasePtr &argument);

  /// \brief Destructor of AbstractKeywordArg.
  ~AbstractKeywordArg() override = default;
  MS_DECLARE_PARENT(AbstractKeywordArg, AbstractBase)

  TypePtr BuildType() const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;

  std::size_t hash() const override;

  /// \brief Overwrite the operator '==' to compare other key-value abstract.
  ///
  /// \param[in] other The other abstract to be joined.
  ///
  /// \return  A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractKeywordArg &other) const;

  bool operator==(const AbstractBase &other) const override;

  /// \brief Get the key name of the key-value pair.
  ///
  /// \return A string.
  std::string get_key() const;

  /// \brief Get the key value of the key-value pair.
  ///
  /// \return A point to the abstract.
  AbstractBasePtr get_arg() const;

  std::string ToString() const override;

 protected:
  ValuePtr RealBuildValue() const override;

 private:
  std::string arg_name_;
  AbstractBasePtr arg_value_;
};
using AbstractKeywordArgPtr = std::shared_ptr<AbstractKeywordArg>;

/// \brief Class AbstractUndetermined describes the abstract if anf node has unknown shape, type or value.
class MS_CORE_API AbstractUndetermined : public AbstractBase {
 public:
  /// \brief Constructor of AbstractUndetermined.
  ///
  /// Shape and type are all unknown.
  AbstractUndetermined();

  /// \brief Constructor of AbstractUndetermined.
  ///
  /// Only element, value and shape track are valid member, type track are unknown.
  ///
  /// \param[in] element The abstract which is undetermined.
  /// \param[in] shape The dimension of value.
  explicit AbstractUndetermined(const AbstractBasePtr &element, const BaseShapePtr &shape = std::make_shared<Shape>());

  /// \brief Constructor of AbstractUndetermined.
  ///
  /// \param[in] element_type A type of the undetermined abstract.
  /// \param[in] shape A vector of shape.
  AbstractUndetermined(const TypePtr &element_type, const ShapeVector &shape);

  /// \brief Constructor of AbstractUndetermined.
  ///
  /// \param[in] element_type A type of the undetermined abstract.
  /// \param[in] shape A shape of the undetermined abstract.
  explicit AbstractUndetermined(const TypePtr &element_type, const BaseShapePtr &shape = std::make_shared<Shape>());

  /// \brief Destructor of AbstractUndetermined.
  ~AbstractUndetermined() override = default;
  MS_DECLARE_PARENT(AbstractUndetermined, AbstractBase)

  TypePtr BuildType() const override;

  AbstractBasePtr Clone() const override;

  /// \brief Get the element, which is the tracked undetermined abstract.
  ///
  /// \return A pointer to the bind abstract, which is undetermined.
  AbstractBasePtr element() const;

  /// \brief Get the shape of the undetermined abstract.
  ///
  /// \return A pointer to the shape.
  ShapePtr shape() const;

  void set_shape(const BaseShapePtr &shape) override;

 protected:
  AbstractBasePtr element_;
};

/// \brief Class AbstractTensor describes a tensor's type, shape and value.
class MS_CORE_API AbstractTensor : public AbstractUndetermined {
 public:
  /// \brief Constructor of AbstractTensor.
  ///
  /// \param[in] element The abstract to be wrapper as a abstract tensor.
  /// \param[in] shape The dimension of abstract tensor.
  explicit AbstractTensor(const AbstractBasePtr &element, const BaseShapePtr &shape = std::make_shared<Shape>());

  /// \brief Constructor of AbstractTensor.
  ///
  /// \param[in] element_type The type of abstract tensor.
  /// \param[in] shape A vector of the tensor's shape.
  AbstractTensor(const TypePtr &element_type, const ShapeVector &shape);

  /// \brief Constructor of AbstractTensor.
  ///
  /// \param[in] tensor The tensor to be abstracted.
  explicit AbstractTensor(const tensor::TensorPtr &tensor);

  /// \brief Constructor of AbstractTensor.
  ///
  /// \param[in] element_type The type of a tensor.
  /// \param[in] shape The dimension of a tensor.
  explicit AbstractTensor(const TypePtr &element_type, const BaseShapePtr &shape = std::make_shared<Shape>());

  /// \brief Destructor of AbstractTensor.
  ~AbstractTensor() override = default;
  MS_DECLARE_PARENT(AbstractTensor, AbstractUndetermined)

  TypePtr BuildType() const override;

  BaseShapePtr BuildShape() const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;

  /// \brief Broaden the abstract. It will upgrade the abstract to a higher level.
  ///
  /// \note The shape will be remained.
  ///
  /// \return A pointer to the broadened abstract.
  AbstractBasePtr BroadenWithShape() const;

  AbstractBasePtr Join(const AbstractBasePtr &other) override;

  /// \brief Overwrite the operator '==' to compare other abstract tensor.
  ///
  /// \param[in] other The other instance of AbstractTensor.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  virtual bool operator==(const AbstractTensor &other) const;

  bool operator==(const AbstractBase &other) const override;

  std::string ToString() const override;

  std::size_t hash() const override;

  AbstractBasePtr PartialBroaden() const override;

  bool is_adapter() const;
  void set_is_adapter(bool is_adapter);

 protected:
  bool equal_to(const AbstractTensor &other) const;
  bool is_adapter_ = false;
};
using AbstractTensorPtr = std::shared_ptr<AbstractTensor>;
using AbstractTensorPtrList = std::vector<AbstractTensorPtr>;

/// \brief Class AbstractAny describes a type, whose shape and value is unknown.
///
/// AbstractAny is even not a Tensor type, but any type.
class MS_CORE_API AbstractAny : public AbstractTensor {
 public:
  /// \brief Constructor of AbstractAny.
  ///
  /// \param[in] element The abstract to be wrapper as a abstract tensor.
  /// \param[in] shape The dimension of abstract tensor.
  AbstractAny();

  /// \brief Destructor of AbstractAny.
  ~AbstractAny() override = default;
  MS_DECLARE_PARENT(AbstractAny, AbstractTensor)

  AbstractBasePtr Join(const AbstractBasePtr &other) override;

  AbstractBasePtr Broaden() const override;

  AbstractBasePtr Clone() const override;

  TypePtr BuildType() const override;

  std::string ToString() const override;

  bool supposed_tensor_dtype() const;

  void set_supposed_tensor_dtype(bool flag);

  static TypePtr DefaultDtype();

 private:
  bool supposed_tensor_dtype_{false};
};
using AbstractAnyPtr = std::shared_ptr<AbstractAny>;
using AbstractAnyPtrList = std::vector<AbstractAnyPtr>;

/// \brief Class AbstractNegligible describes a type, whose shape and value is unknown,
/// and should choose other branch in control flow.
///
/// AbstractNegligible is even not a Tensor type, but any type.
class MS_CORE_API AbstractNegligible : public AbstractAny {
 public:
  /// \brief Constructor of AbstractNegligible.
  ///
  /// \param[in] element The abstract to be wrapper as a abstract tensor.
  /// \param[in] shape The dimension of abstract tensor.
  AbstractNegligible() : AbstractAny() {}

  /// \brief Destructor of AbstractNegligible.
  ~AbstractNegligible() override = default;
  MS_DECLARE_PARENT(AbstractNegligible, AbstractAny)

  AbstractBasePtr Join(const AbstractBasePtr &other) override;

  AbstractBasePtr Broaden() const override;

  AbstractBasePtr Clone() const override;

  TypePtr BuildType() const override;

  std::string ToString() const override;
};
using AbstractNegligiblePtr = std::shared_ptr<AbstractNegligible>;
using AbstractNegligiblePtrList = std::vector<AbstractNegligiblePtr>;

/// \brief Class AbstractJoinedAny describes a type, whose shape and value is unknown.
///
/// AbstractJoinedAny is even not a Tensor type, but any type.
class MS_CORE_API AbstractJoinedAny : public AbstractAny {
 public:
  /// \brief Constructor of AbstractJoinedAny.
  AbstractJoinedAny() : AbstractAny() {}

  /// \brief Destructor of AbstractJoinedAny.
  ~AbstractJoinedAny() override = default;
  MS_DECLARE_PARENT(AbstractJoinedAny, AbstractAny)

  enum ExceptionType {
    kDefault,
    kTypeError,
    kValueError,
  };

  const std::string &message() const;
  void set_message(const std::string &message);
  ExceptionType exception() const;
  void set_exception(ExceptionType exception);

  void ThrowException() const;

 private:
  std::string message_;
  ExceptionType exception_{kDefault};
};
using AbstractJoinedAnyPtr = std::shared_ptr<AbstractJoinedAny>;
using AbstractJoinedAnyPtrList = std::vector<AbstractJoinedAnyPtr>;

/// \brief Class AbstractSequence describes the abstract value of a tuple or list.
class MS_CORE_API AbstractSequence : public AbstractBase {
 public:
  /// \brief Constructor of AbstractSequence.
  ///
  /// \param[in] elements A list of abstracts.
  /// \param[in] sequence_nodes The nodes of tuple/list, usually are MakeTuple/MakeList CNodes or tuple/list ValueNodes.
  explicit AbstractSequence(AbstractBasePtrList &&elements, const std::shared_ptr<AnfNodeWeakPtrList> &sequence_nodes);

  /// \brief Constructor of AbstractSequence.
  ///
  /// \param[in] elements A list of abstracts.
  /// \param[in] sequence_nodes The nodes of tuple/list, usually are MakeTuple/MakeList CNodes or tuple/list ValueNodes.
  explicit AbstractSequence(const AbstractBasePtrList &elements,
                            const std::shared_ptr<AnfNodeWeakPtrList> &sequence_nodes);

  /// \brief Destructor of AbstractSequence.
  ~AbstractSequence() override = default;
  MS_DECLARE_PARENT(AbstractSequence, AbstractBase)

  /// \brief Get the all of types.
  ///
  /// \return A vector of types.
  TypePtrList ElementsType() const;

  /// \brief Get the all of shapes.
  ///
  /// \return A vector of shapes.
  BaseShapePtrList ElementsShape() const;

  /// \brief Clone all of the abstracts.
  ///
  /// \return A vector of the cloned abstracts.
  AbstractBasePtrList ElementsClone() const;

  /// \brief Broaden the list of abstracts.
  ///
  /// \return A vector of the broadened abstracts.
  AbstractBasePtrList ElementsBroaden() const;

  /// \brief Broaden abstract with constraints, only when cond_func is true.
  ///
  /// \return A pointer to the broadened abstract.
  AbstractBasePtrList ElementsPartialBroaden() const;

  /// \brief Get real value by specific template.
  ///
  /// \tparam T the class type of value.
  /// \return A point to value.
  template <typename T>
  ValuePtr ElementsBuildValue() const;

  /// \brief Combine other abstract to the sequence of abstracts.
  ///
  /// \tparam T param other's class type.
  /// \param[in] other The other abstract to be joined.
  /// \return A pointer to the combined abstract.
  template <typename T>
  AbstractBasePtr ElementsJoin(const std::shared_ptr<AbstractSequence> &other);

  /// \brief Combine other sequence nodes with this one.
  ///
  /// \param[in] other The other abstract to be joined.
  /// \return A sequence nodes list combined.
  AnfNodeWeakPtrList SequenceNodesJoin(const AbstractBasePtr &other);

  /// \brief Get the size of the stored elements.
  ///
  /// \return A size_t.
  std::size_t size() const;

  /// \brief Get the size of the stored elements.
  ///
  /// \return A size_t.
  bool empty() const;

  /// \brief Get the stored elements.
  ///
  /// \return A vector of elements.
  const AbstractBasePtrList &elements() const;

  /// \brief Purify the elements list, and clean unused elements.
  ///
  /// \return A boolean, which indicates whether success.
  bool PurifyElements();

  /// \brief Get the sequence nodes where these 'AbstractSequence' evaluated from.
  ///
  /// \return The nodes of tuple/list, usually are MakeTuple/MakeList CNodes or tuple/list ValueNodes.
  const std::shared_ptr<AnfNodeWeakPtrList> &sequence_nodes() const;

  /// \brief Set the sequence nodes where these 'AbstractSequence' evaluated from.
  ///
  /// \param[in] sequence_nodes The nodes of tuple/list, usually are MakeTuple/MakeList CNodes or tuple/list ValueNodes.
  void set_sequence_nodes(const std::shared_ptr<AnfNodeWeakPtrList> &sequence_nodes);

  /// \brief Insert a node into the sequence nodes.
  ///
  /// \param[in] sequence_node The node to intert into sequence nodes.
  void InsertSequenceNode(const AnfNodePtr &sequence_node);

  /// \brief Insert nodes into the sequence nodes.
  ///
  /// \param[in] sequence_nodes The nodes to intert into sequence nodes.
  void InsertSequenceNodes(const AnfNodeWeakPtrList &sequence_nodes);

  /// \brief Update the sequence nodes.
  ///
  /// \param[in] old_sequence_node The old node in sequence nodes.
  /// \param[in] new_sequence_node The new node to replace old node in sequence nodes.
  void UpdateSequenceNode(const AnfNodePtr &old_sequence_node, const AnfNodePtr &new_sequence_node);

  /// \brief Check whether all elements of the tuple are tensors.
  ///
  /// \return Whether all elements of the tuple are tensors.
  bool ContainsAllBroadenTensors() const;

  std::size_t hash() const override;

  std::string ToStringInternal() const;
  std::string ToString() const override;
  std::string ToString(bool verbose) const override;

  /// \brief Overwrite the operator '[]' to get an element.
  ///
  /// \param[in] dim The index.
  /// \return A pointer to the abstract.
  const AbstractBasePtr operator[](const std::size_t &dim) const;

  /// \brief Overwrite the operator '==' to compare other abstract sequence.
  ///
  /// \param[in] other The other instance of AbstractSequence.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const override;

  /// \brief Indicate whether the sequence is dynamic length.
  ///
  /// \return Boolean value indicates whether the sequence is dynamic length.
  bool dynamic_len() const;

  /// \brief Set the sequence to be dynamic length or not.
  ///
  /// \param[in] dynamic_len Boolean value to decide whether the sequence is dynamic length.
  void set_dynamic_len(bool dynamic_len);

  /// \brief Return the abstract of element for variable len sequence.
  ///
  /// \return Abstract for element for variable len sequence.
  AbstractBasePtr dynamic_len_element_abs() const;

  /// \brief Set the abstract of element for variable len sequence.
  ///
  /// \param[in] dynamic_len_element_abs Abstract of element for variable len sequence.
  void set_dynamic_len_element_abs(const AbstractBasePtr &dynamic_len_element_abs);

  /// \brief Check and convert the sequence to dynamic length sequence.
  virtual void CheckAndConvertToDynamicLenSequence(bool raise_exception = true);

  std::shared_ptr<AbstractSequence> BroadenToDynamicLenSequence();

  std::shared_ptr<AbstractSequence> DynamicLenSequenceJoin(const std::shared_ptr<AbstractSequence> &other);

  void set_dyn_len_arg();
  bool dyn_len_arg() const;

 protected:
  AbstractBasePtrList elements_;
  // Since there're not too many nodes, we just use vector here.
  std::shared_ptr<AnfNodeWeakPtrList> sequence_nodes_;
  // Dynamic len sequence related.
  bool dynamic_len_ = false;
  size_t space_num_{0};
  AbstractBasePtr dynamic_len_element_abs_ = nullptr;
  bool dyn_len_arg_ = false;
};
using AbstractSequencePtr = std::shared_ptr<AbstractSequence>;

class MS_CORE_API ExtraInfoHolder {
 public:
  ~ExtraInfoHolder() = default;

  /// \brief Set data to ExtraInfoHolder.
  ///
  /// \param[in] key The key for data in ExtraInfoHolder.
  /// \param[in] data The data to store in ExtraInfoHolder.
  template <typename T>
  void SetData(const std::string &key, const std::shared_ptr<T> &data) {
    extra_info_->set<T>(key, data);
  }

  /// \brief Get data from ExtraInfoHolder using key.
  ///
  /// \param[in] key The key for data in ExtraInfoHolder.
  /// \return The corresponding data.
  template <typename T>
  std::shared_ptr<T> GetData(const std::string &key) const {
    return extra_info_->get<T>(key);
  }

  /// \brief Check whether ExtraInfoHolder has specific data.
  ///
  /// \param[in] key The key for data in ExtraInfoHolder.
  /// \return True if it exists, otherwise false.
  bool HasData(const std::string &key) const { return extra_info_->has(key); }

  /// \brief Get corresponding extra info user data.
  ///
  /// \return The corresponding extra info user data.
  UserDataPtr extra_info() const { return extra_info_; }

  /// \brief Set corresponding extra info user data.
  ///
  /// \param[in] extra_info The corresponding extra info user data.
  void set_extra_info(const UserDataPtr &extra_info) { extra_info_ = extra_info; }

  /// \brief Clear corresponding extra info user data.
  void ClearExtraInfo() { extra_info_ = std::make_shared<UserData>(); }

 protected:
  UserDataPtr extra_info_ = std::make_shared<UserData>();
};

/// \brief Class AbstractTuple describes a tuple.
class MS_CORE_API AbstractTuple : public AbstractSequence, public ExtraInfoHolder {
 public:
  /// \brief Constructor of AbstractTuple.
  ///
  /// \param[in] elements A list of abstracts.
  /// \param[in] tuple_nodes The nodes of tuple, usually are MakeTuple CNodes or tuple ValueNodes.
  explicit AbstractTuple(AbstractBasePtrList &&elements,
                         const std::shared_ptr<AnfNodeWeakPtrList> &tuple_nodes = nullptr);

  /// \brief Constructor of AbstractTuple.
  ///
  /// \param[in] elements A list of abstracts.
  /// \param[in] tuple_nodes The nodes of tuple, usually are MakeTuple CNodes or tuple ValueNodes.
  explicit AbstractTuple(const AbstractBasePtrList &elements,
                         const std::shared_ptr<AnfNodeWeakPtrList> &tuple_nodes = nullptr);

  /// \brief Destructor of AbstractTuple.
  ~AbstractTuple() override = default;
  MS_DECLARE_PARENT(AbstractTuple, AbstractSequence)

  /// \brief Set the shape for the AbstractTuple, only use for dynamic shape.
  ///
  /// \param[in] shape The shape that will be set to the AbstractTuple.
  void set_shape(const BaseShapePtr &shape) override;

  TypePtr BuildType() const override;

  BaseShapePtr BuildShape() const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;

  AbstractBasePtr PartialBroaden() const override;

  AbstractBasePtr Join(const AbstractBasePtr &other) override;

  /// \brief Overwrite the operator '==' to compare other abstract tuple.
  ///
  /// \param[in] other The other instance of AbstractTuple.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const override;

 protected:
  ValuePtr RealBuildValue() const override;
};
using AbstractTuplePtr = std::shared_ptr<AbstractTuple>;

/// \brief Class AbstractList describes a list.
class MS_CORE_API AbstractList final : public AbstractSequence, public ExtraInfoHolder {
 public:
  /// \brief Constructor of AbstractList.
  ///
  /// \param[in] elements A list of abstracts.
  /// \param[in] list_nodes The nodes of list, usually are MakeList CNodes or list ValueNodes.
  explicit AbstractList(AbstractBasePtrList &&elements,
                        const std::shared_ptr<AnfNodeWeakPtrList> &list_nodes = nullptr);

  /// \brief Constructor of AbstractList.
  ///
  /// \param[in] elements A list of abstracts.
  /// \param[in] list_nodes The nodes of list, usually are MakeList CNodes or list ValueNodes.
  explicit AbstractList(const AbstractBasePtrList &elements,
                        const std::shared_ptr<AnfNodeWeakPtrList> &list_nodes = nullptr);

  /// \brief Destructor of AbstractList.
  ~AbstractList() override = default;
  MS_DECLARE_PARENT(AbstractList, AbstractSequence)

  TypePtr BuildType() const override;

  BaseShapePtr BuildShape() const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;

  AbstractBasePtr PartialBroaden() const override;

  AbstractBasePtr Join(const AbstractBasePtr &other) override;

  /// \brief Overwrite the operator '==' to compare other abstract list.
  ///
  /// \param[in] other The other instance of AbstractList.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const override;

  /// \brief Check and convert the list to dynamic length list.
  void CheckAndConvertToDynamicLenSequence(bool raise_exception = true) override;

 protected:
  ValuePtr RealBuildValue() const override;
};
using AbstractListPtr = std::shared_ptr<AbstractList>;

/// \brief Class AbstractNamedTuple describes a namedtuple node's abstract value.
class MS_CORE_API AbstractNamedTuple final : public AbstractTuple {
 public:
  /// \brief Constructor of AbstractNamedTuple.
  ///
  /// \param[in] name The name of a namedtuple.
  /// \param[in] values  A List of data in namedtuple.
  /// \param[in] keys A list of label in namedtuple.
  AbstractNamedTuple(const std::string &sub_class_name, const AbstractBasePtrList &keys,
                     const AbstractBasePtrList &values)
      : AbstractTuple(values), sub_class_name_{sub_class_name}, keys_(keys) {}

  /// \brief Destructor of  AbstractNamedTuple.
  ~AbstractNamedTuple() override = default;
  MS_DECLARE_PARENT(AbstractNamedTuple, AbstractTuple)
  /// \brief Get the stored label.
  ///
  /// \return A vector of label.
  const AbstractBasePtrList &key() const { return keys_; }
  /// \brief Get the name of namedtuple object.
  ///
  /// \return A string of namedtuple's type name.
  const std::string &sub_class_name() const { return sub_class_name_; }

 protected:
  ValuePtr RealBuildValue() const override;

 private:
  std::string sub_class_name_;
  AbstractBasePtrList keys_;
};
using AbstractNamedTuplePtr = std::shared_ptr<AbstractNamedTuple>;

/// \brief Class AbstractDictionary describes a dictionary node's abstract value.
class MS_CORE_API AbstractDictionary final : public AbstractBase, public ExtraInfoHolder {
 public:
  /// \brief Constructor of AbstractDictionary.
  ///
  /// \param[in] key_values The vector of AbstractElementPair.
  explicit AbstractDictionary(const std::vector<AbstractElementPair> &key_values);

  /// \brief Destructor of AbstractDictionary.
  ~AbstractDictionary() override = default;
  MS_DECLARE_PARENT(AbstractDictionary, AbstractBase)

  TypePtr BuildType() const override;

  bool operator==(const AbstractBase &other) const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;

  std::string ToString() const override;

  std::size_t hash() const override;

  AbstractBasePtr Join(const AbstractBasePtr &other) override;

  /// \brief Get the size of key values.
  ///
  /// \return A size_t.
  std::size_t size() const;

  /// \brief Get the key values.
  ///
  /// \return A vector of AbstractElementPair.
  const std::vector<AbstractElementPair> &elements() const;

 protected:
  ValuePtr RealBuildValue() const override;
  std::vector<AbstractElementPair> key_values_;
};
using AbstractDictionaryPtr = std::shared_ptr<AbstractDictionary>;

/// \brief Class AbstractSlice describes a slice node's abstract value.
class MS_CORE_API AbstractSlice final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractSlice.
  ///
  /// \param[in] start The start index of slice.
  /// \param[in] stop The stop index of slice.
  /// \param[in] step The step size of slice.
  AbstractSlice(const AbstractBasePtr &start, const AbstractBasePtr &stop, const AbstractBasePtr &step);

  /// \brief Destructor of AbstractSlice.
  ~AbstractSlice() override = default;
  MS_DECLARE_PARENT(AbstractSlice, AbstractBase)

  TypePtr BuildType() const override;

  /// \brief Overwrite the operator '==' to compare other abstract lice.
  ///
  /// \param[in] other The other instance of AbstractSlice.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;

  std::string ToString() const override;

  std::size_t hash() const override;

  /// \brief Get the start index of slice.
  ///
  /// \return A point to the abstract of start index.
  AbstractBasePtr start() const;

  /// \brief Get the stop index of slice.
  ///
  /// \return A point to the abstract of stop index.
  AbstractBasePtr stop() const;

  /// \brief Get the step size of slice.
  ///
  /// \return A point to the abstract of step number.
  AbstractBasePtr step() const;

 protected:
  ValuePtr RealBuildValue() const override;

 private:
  AbstractBasePtr start_;
  AbstractBasePtr stop_;
  AbstractBasePtr step_;
};
using AbstractSlicePtr = std::shared_ptr<AbstractSlice>;

/// \brief Class AbstractJTagged describes a J node's abstract value.
class MS_CORE_API AbstractJTagged final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractJTagged.
  ///
  /// \param[in] element The value to be processed.
  explicit AbstractJTagged(const AbstractBasePtr &element);

  /// \brief Destructor of AbstractJTagged.
  ~AbstractJTagged() override = default;
  MS_DECLARE_PARENT(AbstractJTagged, AbstractBase)

  TypePtr BuildType() const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;

  AbstractBasePtr Join(const AbstractBasePtr &other) override;

  /// \brief Overwrite the operator '==' to compare other AbstractJTagged.
  ///
  /// \param[in] other The other abstract to be joined.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const override;

  std::string ToString() const override;

  /// \brief Get the element.
  ///
  /// \return A pointer to a abstract, which is the element_.
  AbstractBasePtr element();

  std::size_t hash() const override;

 private:
  AbstractBasePtr element_;
};
using AbstractJTaggedPtr = std::shared_ptr<AbstractJTagged>;

/// \brief Class AbstractNone describes a None node's abstract value.
class MS_CORE_API AbstractNone final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractNone.
  AbstractNone();

  /// \brief Destructor of AbstractNone.
  ~AbstractNone() override = default;
  MS_DECLARE_PARENT(AbstractNone, AbstractBase)

  TypePtr BuildType() const override;

  /// \brief Overwrite the operator '==' to compare other AbstractNone.
  ///
  /// \param[in] other The other instance of AbstractNone.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const override;

  AbstractBasePtr Clone() const override;

  std::string ToString() const override;

  AbstractBasePtr Join(const AbstractBasePtr &other) override;

 protected:
  ValuePtr RealBuildValue() const override;
};
using AbstractNonePtr = std::shared_ptr<AbstractNone>;

/// \brief Class AbstractNull describes a Null node's abstract value.
///
/// The unassigned state value for variable,
/// which means the variable is not assigned.
class MS_CORE_API AbstractNull final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractNull.
  AbstractNull();

  /// \brief Destructor of AbstractNull.
  ~AbstractNull() override = default;
  MS_DECLARE_PARENT(AbstractNull, AbstractBase)

  TypePtr BuildType() const override;

  /// \brief Overwrite the operator '==' to compare other AbstractNull.
  ///
  /// \param[in] other The other instance of AbstractNull.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const override;

  AbstractBasePtr Clone() const override;

  std::string ToString() const override;
};
using AbstractNullPtr = std::shared_ptr<AbstractNull>;

/// \brief Class AbstractTimeOut describes a TimeOut node's abstract value.
///
/// The timeout state value for variable, which means
/// the variable is not assigned because it is timeout.
class MS_CORE_API AbstractTimeOut final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractTimeOut.
  AbstractTimeOut();

  /// \brief Destructor of AbstractTimeOut.
  ~AbstractTimeOut() override = default;
  MS_DECLARE_PARENT(AbstractTimeOut, AbstractBase)

  TypePtr BuildType() const override;

  /// \brief Overwrite the operator '==' to compare other AbstractTimeOut.
  ///
  /// \param[in] other The other instance of AbstractTimeOut.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const override;

  AbstractBasePtr Clone() const override;

  std::string ToString() const override;
};
using AbstractTimeOutPtr = std::shared_ptr<AbstractTimeOut>;

/// \brief Class AbstractEllipsis describes a Ellipsis node's abstract value.
class MS_CORE_API AbstractEllipsis final : public AbstractBase {
 public:
  /// \brief Constructor of AbstractEllipsis.
  AbstractEllipsis();

  /// \brief Destructor of AbstractEllipsis.
  ~AbstractEllipsis() override = default;
  MS_DECLARE_PARENT(AbstractEllipsis, AbstractBase)

  TypePtr BuildType() const override;

  /// \brief Overwrite the operator '==' to compare other AbstractEllipsis.
  ///
  /// \param[in] other The other instance of AbstractTimeOut.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const override;

  AbstractBasePtr Clone() const override;

  std::string ToString() const override;
};
using AbstractEllipsisPtr = std::shared_ptr<AbstractEllipsis>;

/// \brief Class AbstractRefTensor describes a RefTensor's abstract value.
class MS_CORE_API AbstractRefTensor final : public AbstractTensor {
 public:
  /// \brief Constructor of AbstractRef.
  ///
  /// \param[in] ref_value The tensor.
  /// \param[in] ref_key_value The ref key of tensor.
  AbstractRefTensor(const AbstractTensorPtr &ref_value, const ValuePtr &ref_key_value);

  /// \brief Destructor of AbstractEllipsis.
  ~AbstractRefTensor() override = default;
  MS_DECLARE_PARENT(AbstractRefTensor, AbstractTensor)

  TypePtr BuildType() const override;

  /// \brief Overwrite the operator '==' to compare other AbstractRef.
  ///
  /// \param[in] other The other instance of AbstractTimeOut.
  ///
  /// \return A boolean, which indicates whether the other abstract is same.
  bool operator==(const AbstractBase &other) const override;

  AbstractBasePtr Clone() const override;

  /// \brief Use parent's AbstractTensor::Clone() to clone an abstract.
  ///
  /// \return A pointer to the cloned abstract.
  AbstractBasePtr CloneAsTensor() const;

  std::string ToString() const override;

  /// \brief Get the abstract tensor, which is referenced.
  ///
  /// \return A pointer to the abstract tensor.
  AbstractTensorPtr ref();

  /// \brief Get the ref key value, ref key is string actually.
  ///
  /// \return A point to the RefKey.
  ValuePtr ref_key_value() const;

  AbstractBasePtr Broaden() const override;

  virtual AbstractBasePtr Join(const std::shared_ptr<AbstractRefTensor> &other);
  AbstractBasePtr Join(const AbstractBasePtr &other) override;

  AbstractBasePtr PartialBroaden() const override;

 private:
  // ref_key_value is the reference key of AbstractRef, the value can be a string value or kValueAny
  ValuePtr ref_key_value_;
};
using AbstractRefPtr = std::shared_ptr<AbstractRefTensor>;

/// \brief Compute the hash of a list of abstracts.
///
/// \param[in] args_abs_list A list of abstracts.
/// \return A hash number.
MS_CORE_API std::size_t AbstractBasePtrListHash(const AbstractBasePtrList &args_abs_list);

/// \brief Determine whether a list of abstracts is equal to another.
///
/// \param[in] lhs The first list of abstracts.
/// \param[in] rhs The second list of abstracts.
/// \return A boolean.
MS_CORE_API bool AbstractBasePtrListDeepEqual(const AbstractBasePtrList &lhs, const AbstractBasePtrList &rhs);

/// \brief Struct AbstractBasePtrListHasher provides a function to compute the hash of a list of abstracts.
struct AbstractBasePtrListHasher {
  std::size_t operator()(const AbstractBasePtrList &args_abs_list) const {
    return AbstractBasePtrListHash(args_abs_list);
  }
};

/// \brief Struct AbstractBasePtrListEqual provides a function to determine whether a list of abstracts is equal to
///        another.
struct AbstractBasePtrListEqual {
  bool operator()(const AbstractBasePtrList &lhs, const AbstractBasePtrList &rhs) const {
    return AbstractBasePtrListDeepEqual(lhs, rhs);
  }
};

class MS_CORE_API AbstractSparseTensor : public AbstractTuple {
 public:
  explicit AbstractSparseTensor(AbstractBasePtrList &&elements,
                                const std::shared_ptr<AnfNodeWeakPtrList> &tuple_nodes = nullptr);

  explicit AbstractSparseTensor(const AbstractBasePtrList &elements,
                                const std::shared_ptr<AnfNodeWeakPtrList> &tuple_nodes = nullptr);

  ~AbstractSparseTensor() override = default;
  MS_DECLARE_PARENT(AbstractSparseTensor, AbstractTuple)

  template <typename T>
  const T GetAbsPtrAt(size_t index) const;
  /// \brief If any element is a tuple, get every element shape in it.
  BaseShapePtrList ElementsShapeTupleRecursive() const;
  TypePtr BuildType() const override;
  BaseShapePtr BuildShape() const override;

  /// \brief Return the TypeId of a Tensor element in SparseTensor.
  ///
  /// \param[in] index The index of element to choose.
  /// \return A TypeId.
  const TypeId GetTensorTypeIdAt(size_t index) const;

  /// \brief Return the TypeId of a shape element in SparseTensor. Note that each element in shape will be transformed
  /// to Tensor(scalar) in the backend.
  /// \param[in] index The index of element to choose.
  /// \return A TypeId.
  const TypeId GetShapeTypeIdAt(size_t index) const;

  const AbstractTuplePtr shape() const;
};
using AbstractSparseTensorPtr = std::shared_ptr<AbstractSparseTensor>;

/// \brief Class AbstractRowTensor describes a RowTensor's abstract value.
class MS_CORE_API AbstractRowTensor final : public AbstractUndetermined {
 public:
  /// \brief Constructor of AbstractRowTensor.
  ///
  /// \param[in] element The abstract which is wrapped to a RowTensor's abstract value.
  /// \param[in] shape A dimension of the abstract.
  explicit AbstractRowTensor(const AbstractBasePtr &element, const BaseShapePtr &shape = std::make_shared<Shape>());

  /// \brief Constructor of AbstractRowTensor.
  ///
  /// \param[in] element_type The type of RowTensor.
  /// \param[in] shape A dimension of RowTensor.
  AbstractRowTensor(const TypePtr &element_type, const ShapeVector &shape);

  /// \brief Destructor of AbstractRowTensor.
  ~AbstractRowTensor() override = default;
  MS_DECLARE_PARENT(AbstractRowTensor, AbstractUndetermined)

  /// \brief Get the indices of RowTensor.
  ///
  /// \return A pointer to the abstract tensor.
  const AbstractTensorPtr indices() const;

  /// \brief Set the indices for abstract.
  ///
  /// \param[in] indices The indices.
  void set_indices(const AbstractTensorPtr &indices);

  /// \brief Get the values.
  ///
  /// \return A pointer to the abstract tensor.
  const AbstractTensorPtr values() const;

  /// \brief Set the values.
  ///
  /// \param[in] values The values of tensor.
  void set_values(const AbstractTensorPtr &values);

  /// \brief Get the dense shape.
  ///
  /// \return A pointer to the tuple of abstracts.
  const AbstractTuplePtr dense_shape() const;

  /// \brief Set the dense shape.
  ///
  /// \param[in] dense_shape The dense shape of RowTensor.
  void set_dense_shape(const AbstractTuplePtr &dense_shape);

  TypePtr BuildType() const override;

  AbstractBasePtr Clone() const override;

  AbstractBasePtr Broaden() const override;

  /// \brief Broaden the abstract with the shape not changing.
  ///
  /// \return A pointer to the broadened abstract.
  AbstractBasePtr BroadenWithShape() const;

  std::string ToString() const override;

 private:
  std::shared_ptr<AbstractRowTensor> MakeAbstract(const BaseShapePtr &shp) const;
  AbstractTensorPtr indices_;
  AbstractTensorPtr values_;
  AbstractTuplePtr dense_shape_;
};
using AbstractRowTensorPtr = std::shared_ptr<AbstractRowTensor>;

// COOTensor is a Tuple with fixed number of elements and specific meaning of each position.
class MS_CORE_API AbstractCOOTensor : public AbstractSparseTensor {
 public:
  explicit AbstractCOOTensor(AbstractBasePtrList &&elements,
                             const std::shared_ptr<AnfNodeWeakPtrList> &tuple_nodes = nullptr);

  explicit AbstractCOOTensor(const AbstractBasePtrList &elements,
                             const std::shared_ptr<AnfNodeWeakPtrList> &tuple_nodes = nullptr);

  ~AbstractCOOTensor() override = default;
  MS_DECLARE_PARENT(AbstractCOOTensor, AbstractSparseTensor)

  const AbstractTensorPtr indices() const;
  const AbstractTensorPtr values() const;

  TypePtr BuildType() const override;
  AbstractBasePtr Clone() const override;
  AbstractBasePtr Broaden() const override;
  AbstractBasePtr PartialBroaden() const override;
  std::string ToString() const override;

  static constexpr size_t kIndicesIdx = 0;
  static constexpr size_t kValuesIdx = 1;
};
using AbstractCOOTensorPtr = std::shared_ptr<AbstractCOOTensor>;

// CSRTensor is a Tuple with fixed number of elements and specific meaning of each position.
class MS_CORE_API AbstractCSRTensor : public AbstractSparseTensor {
 public:
  explicit AbstractCSRTensor(AbstractBasePtrList &&elements,
                             const std::shared_ptr<AnfNodeWeakPtrList> &tuple_nodes = nullptr);

  explicit AbstractCSRTensor(const AbstractBasePtrList &elements,
                             const std::shared_ptr<AnfNodeWeakPtrList> &tuple_nodes = nullptr);

  ~AbstractCSRTensor() override = default;
  MS_DECLARE_PARENT(AbstractCSRTensor, AbstractSparseTensor)

  const AbstractTensorPtr indptr() const;
  const AbstractTensorPtr indices() const;
  const AbstractTensorPtr values() const;

  TypePtr BuildType() const override;
  AbstractBasePtr Clone() const override;
  AbstractBasePtr Broaden() const override;
  AbstractBasePtr PartialBroaden() const override;
  std::string ToString() const override;

  static constexpr size_t kIndptrIdx = 0;
  static constexpr size_t kIndicesIdx = 1;
  static constexpr size_t kValuesIdx = 2;
};
using AbstractCSRTensorPtr = std::shared_ptr<AbstractCSRTensor>;

class MS_CORE_API AbstractMonad : public AbstractBase {
 public:
  ~AbstractMonad() override = default;
  MS_DECLARE_PARENT(AbstractMonad, AbstractBase)

  std::size_t hash() const override;
  TypePtr BuildType() const override;
  AbstractBasePtr Broaden() const override;
  AbstractBasePtr Join(const AbstractBasePtr &other) override = 0;
  std::string ToString() const override;

 protected:
  AbstractMonad(const ValuePtr &value, const TypePtr &type);
};
using AbstractMonadPtr = std::shared_ptr<AbstractMonad>;

class MS_CORE_API AbstractUMonad final : public AbstractMonad {
 public:
  explicit AbstractUMonad(const ValuePtr &value = kUMonad);
  ~AbstractUMonad() override = default;
  MS_DECLARE_PARENT(AbstractUMonad, AbstractMonad)

  AbstractBasePtr Clone() const override;
  AbstractBasePtr Join(const AbstractBasePtr &other) override;
  bool operator==(const AbstractBase &other) const override;
};
using AbstractUMonadPtr = std::shared_ptr<AbstractUMonad>;

class MS_CORE_API AbstractIOMonad final : public AbstractMonad {
 public:
  explicit AbstractIOMonad(const ValuePtr &value = kIOMonad);
  ~AbstractIOMonad() override = default;
  MS_DECLARE_PARENT(AbstractIOMonad, AbstractMonad)

  AbstractBasePtr Clone() const override;
  AbstractBasePtr Join(const AbstractBasePtr &other) override;
  bool operator==(const AbstractBase &other) const override;
};
using AbstractIOMonadPtr = std::shared_ptr<AbstractIOMonad>;
using tensor::MapTensorPtr;
/// \brief Class AbstractMapTensor describes a MapTensor's abstract value.
class MS_CORE_API AbstractMapTensor final : public AbstractBase {
 public:
  explicit AbstractMapTensor(const MapTensorPtr &map_tensor);
  AbstractMapTensor(const MapTensorPtr &map_tensor, const ValuePtr &ref_key_value);
  AbstractMapTensor(const AbstractMapTensor &other);
  AbstractMapTensor(const TypePtr &type, const ShapePtr &value_shape, const ValuePtr &value,
                    const ValuePtr &ref_key_value, const ValuePtr &default_value, const ValuePtr &permit_filter_value,
                    const ValuePtr &evict_filter_value);
  ~AbstractMapTensor() override = default;

  MS_DECLARE_PARENT(AbstractMapTensor, AbstractBase)

  AbstractMapTensor &operator=(const AbstractMapTensor &other);

  MapTensorTypePtr map_tensor_type() const;
  ShapePtr shape() const;
  const ShapePtr &value_shape() const;
  const ValuePtr &ref_key_value() const;
  const ValuePtr &default_value() const;
  const ValuePtr &permit_filter_value() const;
  const ValuePtr &evict_filter_value() const;
  TypePtr BuildType() const override;
  BaseShapePtr BuildShape() const override;

  AbstractBasePtr Clone() const override;
  AbstractBasePtr Join(const AbstractBasePtr &other) override;
  bool operator==(const AbstractBase &other) const override;
  std::size_t hash() const override;
  std::string ToString() const override;

 private:
  // The reference key value, can be a string value or kValueAny.
  ValuePtr ref_key_value_;
  // The default value, a scalar or string with initializer name.
  ValuePtr default_value_;
  // Permission threshold.
  ValuePtr permit_filter_value_;
  // Remove threshold.
  ValuePtr evict_filter_value_;
  // The value shape.
  ShapePtr value_shape_;
};
using AbstractMapTensorPtr = std::shared_ptr<AbstractMapTensor>;

// Define attribute value map
using AttrValueMap = mindspore::HashMap<std::string, ValuePtr>;
using AttrValueMapPtr = std::shared_ptr<AttrValueMap>;

// The class to save evaluated result: abstract value and modified attribute
class EvalResult : public Base {
 public:
  EvalResult(const AbstractBasePtr &abs, const AttrValueMapPtr &attr)
      : abstract_(abs), attribute_(attr), has_side_effect_node_(false) {}
  ~EvalResult() override = default;
  MS_DECLARE_PARENT(EvalResult, Base);
  const AbstractBasePtr &abstract() const { return abstract_; }
  const AttrValueMapPtr &attribute() const { return attribute_; }
  bool has_side_effect_node() const { return has_side_effect_node_; }
  void set_has_side_effect_node(bool has_side_effect_node) { has_side_effect_node_ = has_side_effect_node; }

 private:
  AbstractBasePtr abstract_;
  // Attribute related to PrimEvaluator;
  AttrValueMapPtr attribute_;

  bool has_side_effect_node_;
};
using EvalResultPtr = std::shared_ptr<EvalResult>;

// Superclass for AnfNodeConfig and VirtualConfig.
class Config : public Base {
 public:
  Config() = default;
  ~Config() override = default;
  MS_DECLARE_PARENT(Config, Base);
  virtual EvalResultPtr ObtainEvalResult() = 0;
};

// Config will be stored in AnalysisCache
using ConfigPtr = std::shared_ptr<Config>;
using ConfigPtrList = std::vector<ConfigPtr>;

MS_CORE_API std::string ExtractLoggingInfo(const std::string &info);
MS_CORE_API void SynchronizeSequenceElementsUseFlagsRecursively(const AbstractSequencePtr &lhs_sequence,
                                                                const AbstractSequencePtr &rhs_sequence);
MS_CORE_API ValuePtr GetRefKeyValue(const AbstractBasePtr &abs);
}  // namespace abstract
}  // namespace mindspore
#endif  // MINDSPORE_CORE_ABSTRACT_ABSTRACT_VALUE_H_
