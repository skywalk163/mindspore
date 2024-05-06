# Copyright 2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
"""Rewrite module api: ValueType and ScopedValue."""
from enum import Enum
from typing import Optional, Union, List, Tuple
from mindspore import _checkparam as Validator


class ValueType(Enum):
    """
    ValueType represents type of `ScopedValue`.

    - A `NamingValue` represents a reference to another variable.
    - A `CustomObjValue` represents an instance of custom class or an object whose type is out of range of base-type
      and container-type of ValueType.
    """

    # constant type
    ConstantValue = 0
    # container type
    TupleValue = 20
    ListValue = 21
    DictValue = 22
    # variable type
    NamingValue = 40
    CustomObjValue = 41
    # unsupported type
    UnsupportedValue = 50


class ScopedValue:
    """
    `ScopedValue` represents a value with its full-scope.

    `ScopedValue` is used to express: a left-value such as target of an assign statement, or a callable object such as
    func of a call statement, or a right-value such as args and kwargs of an assign statement.

    Args:
        arg_type (ValueType): A `ValueType` represents type of current value.
        scope (str, optional): A string represents scope of current value. Take "self.var1" as an example,
            `scope` of this var1 is "self". Default: ``""`` .
        value: A handler represents value of current value. The type of value is corresponding to `arg_type`.
            Default: ``None`` .
    """

    def __init__(self, arg_type: ValueType, scope: str = "", value=None):
        Validator.check_value_type("arg_type", arg_type, [ValueType], "ScopedValue")
        Validator.check_value_type("scope", scope, [str], "ScopedValue")
        self.type = arg_type
        self.scope = scope
        self.value = value

    @classmethod
    def create_variable_value(cls, value) -> Optional['ScopedValue']:
        """
        Create `ScopedValue` from a variable.

        `ScopedValue`'s type is determined by type of value. `ScopedValue`'s scope is empty.

        Args:
            value: The value to be converted to `ScopedValue`.

        Returns:
            An instance of `ScopedValue`.

        Examples:
            >>> from mindspore.rewrite import ScopedValue
            >>> variable = ScopedValue.create_variable_value(2)
            >>> print(variable)
            2
        """
        if isinstance(value, (type(None), int, float, str, bool)):
            return cls(ValueType.ConstantValue, "", value)
        if isinstance(value, tuple):
            return cls(ValueType.TupleValue, "",
                       tuple(cls.create_variable_value(single_value) for single_value in value))
        if isinstance(value, list):
            return cls(ValueType.ListValue, "",
                       list(cls.create_variable_value(single_value) for single_value in value))
        if isinstance(value, dict):
            return cls(ValueType.DictValue, "",
                       dict((cls.create_variable_value(key),
                             cls.create_variable_value(single_value)) for key, single_value in value.items()))
        return cls(ValueType.CustomObjValue, "", value)

    @classmethod
    def create_naming_value(cls, name: str, scope: str = "") -> 'ScopedValue':
        """
        Create a naming `ScopedValue`. A `NamingValue` represents a reference to another variable.

        Args:
            name (str): A string represents the identifier of another variable.
            scope (str, optional): A string represents the scope of another variable. Default: ``""`` .

        Returns:
            An instance of `ScopedValue`.

        Raises:
            TypeError: If `name` is not `str`.
            TypeError: If `scope` is not `str`.

        Examples:
            >>> from mindspore.rewrite import ScopedValue
            >>> variable = ScopedValue.create_naming_value("conv", "self")
            >>> print(variable)
            self.conv
        """
        Validator.check_value_type("name", name, [str], "ScopedValue")
        Validator.check_value_type("scope", scope, [str], "ScopedValue")
        return cls(ValueType.NamingValue, scope, name)

    @staticmethod
    def create_name_values(names: Union[List[str], Tuple[str]],
                           scopes: Union[List[str], Tuple[str]] = None) -> List['ScopedValue']:
        """
        Create a list of naming `ScopedValue`.

        Args:
            names (List[str] or Tuple[str]): List or tuple of `str` represents names of referenced variables.
            scopes (List[str] or Tuple[str], optional): List or tuple of `str` represents scopes of
                referenced variables. Default: ``None`` .

        Returns:
            An list of instance of `ScopedValue`.

        Raises:
            TypeError: If `names` is not `list` or `tuple` and name in `names` is not `str`.
            TypeError: If `scopes` is not `list` or `tuple` and scope in `scopes` is not `str`.
            ValueError: If the length of names is not equal to the length of scopes when scopes are not None.

        Examples:
            >>> from mindspore.rewrite import ScopedValue
            >>> variables = ScopedValue.create_name_values(names=["z", "z_1"], scopes=["self", "self"])
            >>> print(variables)
            [self.z, self.z_1]
        """
        Validator.check_element_type_of_iterable("names", names, [str], "ScopedValue")
        if scopes is not None:
            Validator.check_element_type_of_iterable("scopes", scopes, [str], "ScopedValue")
            if len(names) != len(scopes):
                raise ValueError("Length of names should be equal to length of scopes")
        result = []
        for index, name in enumerate(names):
            if scopes is not None:
                scope = scopes[index]
            else:
                scope = ""
            result.append(ScopedValue.create_naming_value(name, scope))
        return result

    def __str__(self):
        if self.type == ValueType.ConstantValue:
            return str(self.value)
        if self.type == ValueType.NamingValue:
            return f"{self.scope}.{self.value}" if self.scope else str(self.value)
        if self.type == ValueType.CustomObjValue:
            return f"CustomObj: {str(self.value)}"
        return f"Illegal ValueType: {str(self.type)}"

    def __eq__(self, other):
        if id(self) == id(other):
            return True
        return self.type == other.type and self.scope == other.scope and self.value == other.value

    def __repr__(self):
        return str(self)

    def __hash__(self):
        return hash((self.type, self.scope, self.value))
