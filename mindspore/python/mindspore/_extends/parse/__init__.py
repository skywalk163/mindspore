# Copyright 2020-2024 Huawei Technologies Co., Ltd
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
Interfaces for parser module in c++.
"""

from __future__ import absolute_import
from .parser import (Parser, create_instance, is_supported_create_instance_type, generate_scope, get_attr_from_object,
                     get_bprop_method_of_class, get_class_instance_type, get_class_member_namespace_symbol, get_dtype,
                     create_slice_obj, get_obj_id, get_module_namespace, get_obj_type, get_object_key, resolve_symbol,
                     get_ast_type, get_node_type, get_args, get_args_default_values, get_ast_namespace_symbol,
                     get_operation_symbol, get_operation_namespace_symbol, get_parse_method_of_class, get_scope_name,
                     eval_script, get_script_id_attrs, expand_expr_statement, is_class_member_of_self, parse_cb,
                     convert_to_ms_tensor, get_object_description, get_ms_class_name,
                     is_class_type, check_obj_bool, python_isinstance, ms_isinstance, convert_to_ms_csrtensor,
                     convert_to_ms_cootensor, convert_class_to_function, convert_cell_list_to_sequence, is_cell_list,
                     get_obj_from_sequence, get_type, is_class_member_recursive, get_global_params,
                     get_adapter_tensor_attr, get_obj_defined_from_obj_type, is_from_third_party_library,
                     get_const_abs, get_const_round, get_const_len, is_adapter_tensor_class,
                     is_adapter_parameter_class, convert_to_namedtuple, check_attrs, generate_lambda_object,
                     check_is_subclass, check_attr_is_property, get_method_info, can_constant_fold, is_ms_tensor_method)

__all__ = ['Parser', 'create_instance', 'is_supported_create_instance_type', 'generate_scope', 'get_attr_from_object',
           'get_bprop_method_of_class', 'get_class_instance_type', 'get_class_member_namespace_symbol',
           'create_slice_obj', 'get_obj_id', 'get_module_namespace', 'get_obj_type', 'get_object_key',
           'get_ast_type', 'get_node_type', 'get_args', 'get_args_default_values', 'get_ast_namespace_symbol',
           'get_operation_symbol', 'get_operation_namespace_symbol', 'get_parse_method_of_class', 'get_scope_name',
           'eval_script', 'get_script_id_attrs', 'expand_expr_statement', 'is_class_member_of_self', 'parse_cb',
           'resolve_symbol', 'convert_to_ms_tensor', 'get_object_description',
           'get_ms_class_name', 'is_class_type', 'check_obj_bool', 'python_isinstance', 'ms_isinstance',
           'convert_to_ms_csrtensor', 'convert_to_ms_cootensor', 'convert_class_to_function',
           'convert_cell_list_to_sequence', 'is_cell_list', 'get_obj_from_sequence', 'get_type',
           'is_class_member_recursive', 'get_adapter_tensor_attr', 'get_obj_defined_from_obj_type',
           'is_from_third_party_library', 'get_const_abs', 'get_const_round', 'get_const_len', 'get_method_info',
           'is_adapter_tensor_class', 'is_adapter_parameter_class', 'is_ms_tensor_method',
           'convert_to_namedtuple', 'check_attrs', 'generate_lambda_object', 'check_is_subclass', 'check_attr_is_property',
           'can_constant_fold']
