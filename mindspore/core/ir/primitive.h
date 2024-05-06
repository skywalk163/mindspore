/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CORE_IR_PRIMITIVE_H_
#define MINDSPORE_CORE_IR_PRIMITIVE_H_

#include <vector>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <shared_mutex>
#include <initializer_list>

#include "utils/hash_map.h"
#include "ir/signature.h"
#include "ir/dtype/type.h"
#include "abstract/abstract_value.h"
#include "base/base_ref.h"

namespace mindspore {
// Supported meta type
enum PrimType {
  kPrimTypeUnknown = 0,
  kPrimTypeBegin = kPrimTypeUnknown,
  kPrimTypeBuiltIn,     // Built-in primitive operator
  kPrimTypePyInfer,     // Primitive operator with python infer function
  kPrimTypeUserCustom,  // Primitive operator defined by custom
  kPrimTypePyCheck      // Primitive operator with input args checking method
};

class MS_CORE_API PrimitiveReadLock {
 public:
  explicit PrimitiveReadLock(std::shared_ptr<std::shared_mutex> shared_mutex) : shared_mutex_(std::move(shared_mutex)) {
    if (shared_mutex_ != nullptr) {
      shared_mutex_->lock_shared();
    }
  }
  ~PrimitiveReadLock() {
    if (shared_mutex_ != nullptr) {
      // cppcheck-suppress unreadVariable
      shared_mutex_->unlock_shared();
    }
  }

 private:
  std::shared_ptr<std::shared_mutex> shared_mutex_;
};

class MS_CORE_API PrimitiveWriteLock {
 public:
  explicit PrimitiveWriteLock(std::shared_ptr<std::shared_mutex> shared_mutex)
      : shared_mutex_(std::move(shared_mutex)) {
    if (shared_mutex_ != nullptr) {
      shared_mutex_->lock();
    }
  }
  ~PrimitiveWriteLock() {
    if (shared_mutex_ != nullptr) {
      shared_mutex_->unlock();
    }
  }

 private:
  std::shared_ptr<std::shared_mutex> shared_mutex_;
};

/// \brief Primitive defines a operator primitive of MindSpore.
class MS_CORE_API Primitive : public Named {
 public:
  /// \brief The constructor of Primitive.
  ///
  /// \param[in] name The name of primitive.
  /// \param[in] is_base True means the basic Primitive without BProp function inside.
  /// \param[in] prim_type The type of primitive.
  explicit Primitive(const std::string &name, bool is_base = true, const PrimType prim_type = kPrimTypeBuiltIn,
                     bool inplace_prim = false);
  Primitive(const std::string &name, const mindspore::HashMap<std::string, ValuePtr> &attrs, bool inplace_prim = false);
  /// \brief The constructor for Primitive, create a primitive for another primitive.
  ///
  /// \param[in] prim The input primitive.
  Primitive(const Primitive &prim);
  /// \brief The copy assignment operator for Primitive.
  ///
  /// \param[in] other An existing Primitive object.
  /// \return A Primitive object set with the same members as other.
  Primitive &operator=(const Primitive &other);
  MS_DECLARE_PARENT(Primitive, Named);
  abstract::AbstractBasePtr ToAbstract() override;
  std::string ToString() const override;
  /// \brief Ready to recording the attribute if the attribute needs to be added when deducing shape and type.
  /// This attributes has been recorded needs to add in infer cache.
  void BeginRecordAddAttr() {
    evaluate_added_attrs_.clear();
    record_evaluate_add_attr_ = true;
  }
  /// \brief End recording attribute.
  void EndRecordAddAttr() { record_evaluate_add_attr_ = false; }
  /// \brief Add attribute to primitive attribute map and record the new attribute to evaluate_added_attrs_,
  /// if record_evaluate_add_attr_ is true.
  ///
  /// \param[in] name The name of attribute.
  /// \param[in] attr The value of attribute.
  /// \return The primitive to which attribute has been added.
  Primitive &AddAttr(const std::string &name, const ValuePtr &attr) {
    // cppcheck-suppress unreadVariable
    PrimitiveWriteLock write_lock(shared_mutex_);
    attrs_[name] = attr;
    if (record_evaluate_add_attr_) {
      evaluate_added_attrs_[name] = attr;
    }
    return *this;
  }
  /// \brief Delete the attribute.
  ///
  /// \param[in] name The name of attribute to be delete.
  /// \return The primitive to which attribute has been added.
  Primitive &DelAttr(const std::string &name) {
    // cppcheck-suppress unreadVariable
    PrimitiveWriteLock write_lock(shared_mutex_);
    (void)attrs_.erase(name);
    return *this;
  }
  /// \brief Use add attribute by using a map,all elements of the map will be added in the primitive's attribute map.
  ///
  /// \param[in] attrs The attribute map needs to be added in the primitive attribute.
  /// \return The primitive to which attribute has been added.
  Primitive &SetAttrs(const mindspore::HashMap<std::string, ValuePtr> &attrs) {
    PrimitiveWriteLock write_lock(shared_mutex_);
    for (auto &attr : attrs) {
      attrs_[attr.first] = attr.second;
    }
    return *this;
  }
  /// \brief Use add attribute by using initializer_list, all elements of the vector will be added in the primitive's
  /// attribute map.
  ///
  /// \param[in] attrs The attribute vector needs to be added in the primitive attribute.
  /// \return The primitive to which attribute has been added.
  Primitive &SetAttrs(const std::initializer_list<std::pair<std::string, ValuePtr>> &attrs) {
    PrimitiveWriteLock write_lock(shared_mutex_);
    for (auto &attr : attrs) {
      attrs_[attr.first] = attr.second;
    }
    return *this;
  }
  /// \brief Use add attribute by using a vector, all elements of the vector will be added in the primitive's attribute
  /// map.
  ///
  /// \param[in] attrs The attribute vector needs to be added in the primitive attribute.
  /// \return The primitive to which attribute has been added.
  Primitive &SetAttrs(const std::vector<std::pair<std::string, ValuePtr>> &attrs) {
    PrimitiveWriteLock write_lock(shared_mutex_);
    for (auto &attr : attrs) {
      attrs_[attr.first] = attr.second;
    }
    return *this;
  }
  /// \brief Set attribute to the primitive attribute map.
  void set_attr(const std::string &attrName, const ValuePtr &attr) {
    // cppcheck-suppress unreadVariable
    PrimitiveWriteLock write_lock(shared_mutex_);
    attrs_[attrName] = attr;
  }
  /// \brief Erase attribute to the primitive attribute map.
  void EraseAttr(const std::string &attrName) {
    // cppcheck-suppress unreadVariable
    PrimitiveWriteLock write_lock(shared_mutex_);
    (void)attrs_.erase(attrName);
  }
  /// \brief Run Primitive's compute function if the compute function has been implemented.
  ///
  /// \param[in] args The arguments of primitive need to compute.
  /// \return The primitive's calculation result.
  virtual BaseRef RunComputeFunction(const VectorRef &args) const { return nullptr; }
  /// \brief Get Primitive's attribute.
  ///
  /// \param[in] attrName Primitive attribute name.
  /// \return The value of attribute in primitive attribute map, if the map is not
  ValuePtr GetAttr(const std::string &attrName) const {
    PrimitiveReadLock read_lock(shared_mutex_);
    auto iter = attrs_.find(attrName);
    return iter == attrs_.cend() ? nullptr : iter->second;
  }
  /// \brief Get Primitive's all attributes.
  ///
  /// \return The Primitive's all attribute.
  const mindspore::HashMap<std::string, ValuePtr> &attrs() const { return attrs_; }
  /// \brief Get the attributes added in MindSpore renormalize stage.
  ///
  /// \return Attributes which have been added in MindSpore renormalize stage.
  const mindspore::HashMap<std::string, ValuePtr> &evaluate_added_attrs() const { return evaluate_added_attrs_; }
  /// \brief Use add attribute using a map,all elements of the map will be added in the primitive's attribute map.
  ///
  /// \param[in] attrs The attribute map needs to be added in the primitive attribute.
  void set_evaluate_added_attrs(const mindspore::HashMap<std::string, ValuePtr> &attrs) {
    // cppcheck-suppress unreadVariable
    PrimitiveWriteLock write_lock(shared_mutex_);
    for (auto &attr : attrs) {
      (void)attrs_.insert_or_assign(attr.first, attr.second);
    }
    evaluate_added_attrs_ = attrs;
  }
  /// \brief Check if Primitive has any attribute.
  /// for example Primitives like scalar_add, return, etc, don't have any attribute.
  ///
  /// \return Return ture, If Primitive has attributes, else return false.
  bool HasAttr() const { return !attrs_.empty(); }
  /// \brief Check If Primitive has an attribute named attrName.
  ///
  /// \param[in] attrName The name of attribute.
  /// \return Return true if Primitive has an attribute named attrName,else return false.
  bool HasAttr(const std::string &attrName) const {
    auto iter = attrs_.find(attrName);
    return !(iter == attrs_.cend());
  }
  /// \brief Set the name of primitive.
  ///
  /// \param t The primitive type that needs to be set.
  void set_prim_type(const PrimType t) { prim_type_ = t; }
  /// \brief Clone a Primitive.
  ///
  /// \return A Primitive which cloned by current primitive.
  virtual PrimitivePtr Clone() { return std::make_shared<Primitive>(*this); }
  /// \brief Set primitive instance_name.
  ///
  /// \param[in] s The primitive instance name to be set.
  void set_instance_name(const std::string &s) { instance_name_ = s; }
  /// \brief Check whether the primitive type if has the Python infer function,
  ///
  /// \return Return true if Primitive's type is kPrimTypePyInfer or kPrimTypeUserCustom, else return false.
  bool HasPyEvaluator() const { return prim_type_ == kPrimTypePyInfer || prim_type_ == kPrimTypeUserCustom; }
  /// \brief Check whether the primitive type if has the python infer function,
  ///
  /// \return Return true if Primitive's type is kPrimTypeUserCustom, else return false.
  bool IsCustomPrim() const { return prim_type_ == kPrimTypeUserCustom; }
  /// \brief Get Primitive type.
  ///
  /// \return The type of Primitive.
  PrimType prim_type() const { return prim_type_; }
  /// \brief Get primitive instance name.
  ///
  /// \return The instance name of primitive.
  std::string instance_name() const { return instance_name_; }
  /// \brief Get primitive attribute debug string.
  /// If the attribute name of primitive is a,the value is b
  /// The return value of GetAttrsText function is [a=b].
  ///
  /// \return Get attribute debug string of primitive.
  std::string GetAttrsText() const;
  bool operator==(const Value &other) const override;
  /// \brief To compare whether two Primitive objects are equal.
  ///
  /// \param[in] other The other Primitive be compared with.
  /// \return return true if the name and attributes of primitives are the same,otherwise return false.
  bool operator==(const Primitive &other) const;
  /// \brief Destructor of Primitive.
  ~Primitive() override = default;
  /// \brief The flag to be set in primitive.
  ///
  /// \param[in] has_signature Set the flag whether there is a signature for the primitive.
  void set_has_signature(bool has_signature) { has_signature_ = has_signature; }
  /// \brief Check whether the primitive has signature.
  ///
  /// \return Return true if primitive has signature flag , else return false.
  bool has_signature() const { return has_signature_; }
  /// \brief Set signatures of primitive.
  ///
  /// \param[in] signatures Set signatures of primitive.
  void set_signatures(const std::vector<Signature> &signatures);
  /// \brief Get signatures of primitive.
  ///
  /// \return Return signatures of primitive.
  const std::vector<Signature> &signatures() const { return signatures_; }
  /// \brief Check whether the primitive is a basic primitive.
  ///
  /// \return Return true if the primitive is basic, else return false.
  bool is_base() const { return is_base_; }
  /// \brief Set primitive const flag.
  /// If the const_prim_ of primitive is true means the primitive will be eliminated in constant folding.
  ///
  /// \param is_const_prim The flag of primitive to be set.
  void set_const_prim(bool is_const_prim) { const_prim_ = is_const_prim; }
  /// \brief Check whether the primitive is const primitive.
  ///
  /// \return Return true if primitive is a const primitive, else return false.
  bool const_prim() const { return const_prim_; }
  /// \brief Set const input index for primitive.
  ///
  /// \param const_input_indexes The const input index of the primitive to be set.
  void set_const_input_indexes(const std::vector<size_t> &const_input_indexes) {
    const_input_indexes_ = const_input_indexes;
  }
  /// \brief Get const input index of the primitive.
  ///
  /// \return  Const input indexes of the primitive.
  const std::vector<size_t> &get_const_input_indexes() const { return const_input_indexes_; }
  /// \brief Get Primitive's id.
  ///
  /// \return primitive's Id.
  uint64_t id() const { return id_; }

  /// \brief Check whether the primitive is inplace primitive.
  ///
  /// \return Return true if primitive is a inplace primitive, else return false.
  bool inplace_prim() const { return inplace_prim_; }
  /// \brief Set primitive inplace flag.
  ///
  /// \param inplace_prim The flag of primitive to be set.
  void set_inplace_prim(bool inplace_prim) { inplace_prim_ = inplace_prim; }

  /// \brief Enable primitive read/write lock.
  void EnableSharedMutex() {
    if (shared_mutex_ == nullptr) {
      shared_mutex_ = std::make_shared<std::shared_mutex>();
    }
  }

  /// \brief Get primitive shared_mutex.
  ///
  /// \return Return shared_mutex of the primitive.
  const std::shared_ptr<std::shared_mutex> &shared_mutex() const { return shared_mutex_; }

  virtual bool IsPythonPrim() { return false; }

 protected:
  mindspore::HashMap<std::string, ValuePtr> attrs_;
  mindspore::HashMap<std::string, ValuePtr> evaluate_added_attrs_;

 private:
  std::string instance_name_;
  PrimType prim_type_;
  bool is_base_;
  bool has_signature_;
  std::vector<Signature> signatures_;
  bool record_evaluate_add_attr_;
  bool const_prim_;
  bool inplace_prim_;
  std::vector<size_t> const_input_indexes_;
  uint64_t id_{0};
  std::shared_ptr<std::shared_mutex> shared_mutex_{nullptr};
};

inline std::ostream &operator<<(std::ostream &os, const PrimitivePtr &p) {
  os << *p;
  return os;
}

/// \brief Equal operator for Primitive.
struct MS_CORE_API PrimitiveEqual {
  /// \brief Implementation of Equal operation.
  ///
  /// \param t1 The left Primitive to compare.
  /// \param t2 The right Primitive to compare.
  /// \return The comparison result,Return true if the name and address of t1 and t2 are the same ,else return false.
  bool operator()(PrimitivePtr const &t1, PrimitivePtr const &t2) const {
    MS_EXCEPTION_IF_NULL(t1);
    MS_EXCEPTION_IF_NULL(t2);
    return t1 == t2 || t1->name() == t2->name();
  }
};

/// \brief Implementation of hash operation.
struct MS_CORE_API PrimitiveHasher {
  /// \brief Implementation of hash operation.
  ///
  /// \param name The PrimitiveHasher to be hashed.
  /// \return The hash result.
  std::size_t operator()(PrimitivePtr const &prim) const {
    MS_EXCEPTION_IF_NULL(prim);
    return prim->Hash();
  }
};

/// \brief Equal operator for Primitive.
struct MS_CORE_API PrimitiveTotalEqual {
  /// \brief Implementation of Equal operation.
  ///
  /// \param t1 The left Primitive to compare.
  /// \param t2 The right Primitive to compare.
  /// \return The comparison result,Return true if t1 and t2 are the same,else return false.
  bool operator()(PrimitivePtr const &t1, PrimitivePtr const &t2) const {
    MS_EXCEPTION_IF_NULL(t1);
    MS_EXCEPTION_IF_NULL(t2);
    return *t1 == *t2;
  }
};
}  // namespace mindspore
#endif  // MINDSPORE_CORE_IR_PRIMITIVE_H_
