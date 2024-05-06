/**
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CORE_UTILS_OVERLOAD_H_
#define MINDSPORE_CORE_UTILS_OVERLOAD_H_

#include <list>
#include <utility>
#include <vector>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include "utils/hash_map.h"

namespace mindspore {
constexpr size_t kThresholdToDisplayIndex = 10;
template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &v) {
  os << "[const vector]{";
  const bool display_index = (v.size() > kThresholdToDisplayIndex);
  size_t last_index = v.size() - 1;
  for (size_t i = 0; i < v.size(); ++i) {
    if (display_index) {
      os << "[" << i << "]:{";
    }
    os << v[i];
    if (display_index) {
      os << "}";
    }
    if (i != last_index) {
      os << ", ";
    }
  }
  os << "}";
  return os;
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::list<T> &v) {
  os << "[const list]{";
  const bool display_index = (v.size() > kThresholdToDisplayIndex);
  size_t i = 0;
  size_t last_index = v.size() - 1;
  for (auto it = v.begin(); it != v.end(); ++it, ++i) {
    if (display_index) {
      os << "[" << i << "]:{";
    }
    os << *it;
    if (display_index) {
      os << "}";
    }
    if (i != last_index) {
      os << ", ";
    }
  }
  os << "}";
  return os;
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::initializer_list<T> &v) {
  os << "[const initializer_list]{";
  const bool display_index = (v.size() > kThresholdToDisplayIndex);
  size_t i = 0;
  size_t last_index = v.size() - 1;
  for (auto it = v.begin(); it != v.end(); ++it, ++i) {
    if (display_index) {
      os << "[" << i << "]:{";
    }
    os << *it;
    if (display_index) {
      os << "}";
    }
    if (i != last_index) {
      os << ", ";
    }
  }
  os << "}";
  return os;
}

template <typename T>
bool operator==(const std::initializer_list<T> &lhs, const std::initializer_list<T> &rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  auto lit = lhs.begin();
  auto rit = rhs.begin();
  while (lit != lhs.end()) {
    if (!(*lit == *rit)) {
      return false;
    }
    lit++;
    rit++;
  }
  return true;
}

template <typename T1, typename T2>
std::ostream &operator<<(std::ostream &os, const std::pair<T1, T2> &pair) {
  os << "[const pair]";

  return os;
}

template <typename T1, typename T2, typename T3>
std::ostream &operator<<(std::ostream &os, const mindspore::HashMap<T1, T2, T3> &) {
  os << "[const hash_map]";
  return os;
}

template <typename T1, typename T2, typename T3>
std::ostream &operator<<(std::ostream &os, const std::map<T1, T2, T3> &map) {
  os << "[const map]";
  return os;
}

template <typename T>
std::string ToString(const std::vector<T> &vec) {
  std::ostringstream buffer;

  buffer << vec;
  return buffer.str();
}

template <typename T1, typename T2>
std::string ToString(const mindspore::HashMap<T1, T2> &map) {
  std::ostringstream buffer;

  buffer << map;
  return buffer.str();
}

template <typename T1, typename T2>
std::string ToString(const std::map<T1, T2> &map) {
  std::ostringstream buffer;

  buffer << map;
  return buffer.str();
}
}  // namespace mindspore

#endif  // MINDSPORE_CORE_UTILS_OVERLOAD_H_
