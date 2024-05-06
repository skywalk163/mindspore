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
"""Find specific type ast node in specific scope."""

from typing import Type, Any
import ast
import copy
import sys
if sys.version_info >= (3, 9):
    import ast as astunparse # pylint: disable=reimported, ungrouped-imports
else:
    import astunparse


class AstFinder(ast.NodeVisitor):
    """
    Find all specific type ast node in specific scope.

    Args:
        node (ast.AST): An instance of ast node as search scope.
    """

    def __init__(self, node: ast.AST):
        self._scope: ast.AST = node
        self._targets: tuple = ()
        self._results: [ast.AST] = []

    def generic_visit(self, node):
        """
        An override method, iterating over all nodes and save target ast nodes.

        Args:
            node (ast.AST): An instance of ast node which is visited currently.
        """

        if isinstance(node, self._targets):
            self._results.append(node)
        super(AstFinder, self).generic_visit(node)

    def find_all(self, ast_types) -> [ast.AST]:
        """
        Find all matched ast node.

        Args:
            ast_types (Union[tuple(Type), Type]): A tuple of Type or a Type indicates target ast node type.

        Returns:
            A list of instance of ast.AST as matched result.

        Raises:
            ValueError: If input `ast_types` is not a type nor a tuple.
        """

        if isinstance(ast_types, Type):
            self._targets: tuple = (ast_types,)
        else:
            if not isinstance(ast_types, tuple):
                raise ValueError("Input ast_types should be a tuple or a type")
            self._targets: tuple = ast_types

        self._results.clear()
        self.visit(self._scope)
        return self._results


class StrChecker(ast.NodeVisitor):
    """
    Check if specific String exists in specific scope.

    Args:
        node (ast.AST): An instance of ast node as check scope.
    """

    def __init__(self, node: ast.AST):
        self._context = node
        self._pattern = ""
        self._hit = False

    def visit_Attribute(self, node: ast.Attribute) -> Any:
        """Visit a node of type ast.Attribute."""
        if isinstance(node.value, ast.Name) and node.value.id == self._pattern:
            self._hit = True
        return super(StrChecker, self).generic_visit(node)

    def visit_Name(self, node: ast.Name) -> Any:
        """Visit a node of type ast.Name."""
        if node.id == self._pattern:
            self._hit = True
        return super(StrChecker, self).generic_visit(node)

    def generic_visit(self, node: ast.AST) -> Any:
        for _, value in ast.iter_fields(node):
            if self._hit:
                break
            if isinstance(value, (list, tuple)):
                for item in value:
                    if isinstance(item, ast.AST):
                        self.visit(item)
                    if self._hit:
                        break
            elif isinstance(value, dict):
                for item in value.values():
                    if isinstance(item, ast.AST):
                        self.visit(item)
                    if self._hit:
                        break
            elif isinstance(value, ast.AST):
                self.visit(value)

    def check(self, pattern: str) -> bool:
        """
        Check if `pattern` exists in `_context`.

        Args:
            pattern (str): A string indicates target pattern.

        Returns:
            A bool indicates if `pattern` exists in `_context`.
        """
        self._pattern = pattern
        self._hit = False
        self.generic_visit(self._context)
        return self._hit


class FindConstValueInInit(ast.NodeVisitor):
    """
    Check if specific String exists in specific scope.

    Args:
        node (ast.AST): An instance of ast node as check scope.
    """
    def __init__(self, node: ast.AST):
        self._context = node
        self._pattern = ""
        self._hit = False

    def visit_Constant(self, node: ast.Constant):
        if node.value == self._pattern:
            self._hit = True
        return node

    def check(self, pattern: str) -> bool:
        """
        Check if `pattern` exists in `_context`.

        Args:
            pattern (str): A string indicates target pattern.

        Returns:
            A bool indicates if `pattern` exists in `_context`.
        """
        self._pattern = pattern
        self._hit = False
        self.generic_visit(self._context)
        return self._hit


class CheckPropertyIsUsed(ast.NodeVisitor):
    """
    Check whether a property is used.

    Args:
        node (ast.AST): An instance of ast node.
    """
    def __init__(self, node: ast.AST):
        self._context = node
        self._value = ""
        self._attr = ""
        self._hit = False

    def visit_Attribute(self, node: ast.Attribute) -> Any:  # pylint: disable=invalid-name
        """Visit a node of type ast.Attribute."""
        if isinstance(node.value, ast.Name) and node.value.id == self._value and node.attr == self._attr:
            self._hit = True
        return super(CheckPropertyIsUsed, self).generic_visit(node)

    def generic_visit(self, node: ast.AST) -> Any:
        """
        An override method, iterating over all nodes and save target ast nodes.
        """
        if self._hit:
            return
        super(CheckPropertyIsUsed, self).generic_visit(node)

    def check(self, value, attr) -> bool:
        """
        Check whether `value` and `attr` exists.
        """
        self._value = value
        self._attr = attr
        self._hit = False
        self.generic_visit(self._context)
        return self._hit


class GetPropertyOfObj(ast.NodeVisitor):
    """
    Check whether a property is used.

    Args:
        node (ast.AST): An instance of ast node.
    """
    def __init__(self, node: ast.AST):
        self._context = node
        self._property = set()

    def visit_Assign(self, node: ast.Assign) -> Any:  # pylint: disable=invalid-name
        """Visit a node of type ast.Attribute."""
        target = node.targets[0]
        if isinstance(target, ast.Attribute) and isinstance(target.value, ast.Name) and target.value.id == "self":
            self._property.add(target.attr)
        return super(GetPropertyOfObj, self).generic_visit(node)

    def get(self):
        """
        Check whether `value` and `attr` exists.
        """
        self._property = set()
        self.generic_visit(self._context)
        return self._property


class AstAssignFinder(ast.NodeVisitor):
    """
    Get assign definition ast of specifical parameter in specific scope.

    Args:
        node (ast.AST): An instance of ast node as check scope.
    """
    def __init__(self, node: ast.AST):
        self._context = node
        self._scope = ""
        self._value = ""
        self._target = None

    def visit_Assign(self, node: ast.Assign):
        if self._scope and isinstance(node.targets[0], ast.Attribute):
            if node.targets[0].attr == self._value and isinstance(node.targets[0].value, ast.Name) \
                and node.targets[0].value.id == self._scope:
                self._target = node
        elif not self._scope and isinstance(node.targets[0], ast.Name):
            if node.targets[0].id == self._value:
                self._target = node

    def get_ast(self, value: str, scope: str = "") -> bool:
        """
        Get assign ast of specifical parameter in specific ast.

        Args:
            value (str): A string indicates assign target value.
            scope (str): A string indicates assign target scope.

        Returns:
            An assign ast with the same target name as `scope.value` .
        """
        self._scope = scope
        self._value = value
        self.generic_visit(self._context)
        return self._target


class AstClassFinder(ast.NodeVisitor):
    """
    Find all specific name of ast class node in specific scope.

    Args:
        node (ast.AST): An instance of ast node as search scope.
    """

    def __init__(self, node: ast.AST):
        self._scope: ast.AST = node
        self._target: str = ""
        self._results: [ast.ClassDef] = []

    def visit_ClassDef(self, node):
        """
        An override method, iterating over all ClassDef nodes and save target ast nodes.

        Args:
            node (ast.AST): An instance of ast node which is visited currently.
        """

        if node.name == self._target:
            self._results.append(node)

    def find_all(self, class_name: str) -> [ast.AST]:
        """
        Find all matched ast node.

        Args:
            class_name (str): Name of class to be found.

        Returns:
            A list of instance of ast.ClassDef as matched result.

        Raises:
            TypeError: If input `class_name` is not str.
        """
        if not isinstance(class_name, str):
            raise TypeError("Input class_name should be a str")
        self._target = class_name
        self._results.clear()
        self.visit(self._scope)
        return self._results


class AstFunctionFinder(ast.NodeVisitor):
    """
    Find all specific name of ast function node in specific scope.

    Args:
        node (ast.AST): An instance of ast node as search scope.
    """

    def __init__(self, node: ast.AST):
        self._scope: ast.AST = node
        self._target: str = ""
        self._results: [ast.ClassDef] = []

    def visit_FunctionDef(self, node):
        """
        An override method, iterating over all FunctionDef nodes and save target ast nodes.

        Args:
            node (ast.AST): An instance of ast node which is visited currently.
        """

        if node.name == self._target:
            self._results.append(node)

    def find_all(self, func_name: str) -> [ast.AST]:
        """
        Find all matched ast node.

        Args:
            func_name (str): Name of function to be found.

        Returns:
            A list of instance of ast.FunctionDef as matched result.

        Raises:
            TypeError: If input `func_name` is not str.
        """
        if not isinstance(func_name, str):
            raise TypeError("Input func_name should be a str")
        self._target = func_name
        self._results.clear()
        self.visit(self._scope)
        return self._results


class AstImportFinder(ast.NodeVisitor):
    """Find all import nodes from input ast node."""
    def __init__(self, ast_root: ast.AST):
        self.ast_root = ast_root
        self.import_nodes = []
        self.try_nodes = []
        self.imports_str = []

    def visit_Try(self, node: ast.Try) -> Any:
        if isinstance(node.body[0], (ast.Import, ast.ImportFrom)):
            self.try_nodes.append(copy.deepcopy(node))
        return node

    def visit_Import(self, node: ast.Import) -> Any:
        """Iterate over all nodes and save ast.Import nodes."""
        self.import_nodes.append(copy.deepcopy(node))
        self.imports_str.append(astunparse.unparse(node))
        return node

    def visit_ImportFrom(self, node: ast.ImportFrom) -> Any:
        """Iterate over all nodes and save ast.ImportFrom nodes."""
        self.import_nodes.append(copy.deepcopy(node))
        self.imports_str.append(astunparse.unparse(node))
        return node

    def _remove_duplicated_import_in_try(self, node: [ast.Import, ast.ImportFrom]):
        import_str = astunparse.unparse(node)
        if import_str in self.imports_str:
            self.import_nodes.remove(self.import_nodes[self.imports_str.index(import_str)])

    def get_import_node(self):
        self.generic_visit(self.ast_root)
        for try_node in self.try_nodes:
            for body in try_node.body:
                self._remove_duplicated_import_in_try(body)
            for handler in try_node.handlers:
                for body in handler.body:
                    self._remove_duplicated_import_in_try(body)
        self.import_nodes.extend(self.try_nodes)
        return self.import_nodes
