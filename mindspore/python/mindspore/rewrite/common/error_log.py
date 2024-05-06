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
"""Error Log for Rewrite."""

import sys
import ast
if sys.version_info >= (3, 9):
    import ast as astunparse # pylint: disable=reimported, ungrouped-imports
else:
    import astunparse


def error_str(reason: str, child_node: ast.expr = None, father_node: ast.expr = None) -> str:
    """Raise error func for Rewrite."""
    output_estr = "Unsupported grammar in MindSpore Rewrite, "
    output_estr += reason
    if child_node:
        output_estr += "\n" + "-" * 100 + "\n"
        output_estr += astunparse.unparse(child_node).strip()
        output_estr += "\n" + "-" * 100 + "\n"
        if father_node:
            output_estr += "in"
    if father_node:
        output_estr += "\n" + "-" * 100 + "\n"
        output_estr += astunparse.unparse(father_node).strip()
        output_estr += "\n" + "-" * 100 + "\n"
    return output_estr
