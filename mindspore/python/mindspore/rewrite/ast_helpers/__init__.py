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
"""
`ast_helpers` package of MindSpore Rewrite package.
Define some ast helpers for manipulating python ast.
"""

from .ast_finder import AstFinder, StrChecker, CheckPropertyIsUsed, GetPropertyOfObj, \
    AstAssignFinder, AstClassFinder, AstFunctionFinder, AstImportFinder
from .ast_replacer import AstReplacer
from .ast_modifier import AstModifier
from .ast_converter import AstConverter
from .ast_flattener import AstFlattener
