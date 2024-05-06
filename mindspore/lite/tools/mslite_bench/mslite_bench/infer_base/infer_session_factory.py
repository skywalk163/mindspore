# Copyright 2023 Huawei Technologies Co., Ltd
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
infer session factory for unified api
"""
import importlib

from mslite_bench.common.model_info_enum import FrameworkType
from mslite_bench.utils.infer_log import InferLogger
from mslite_bench.common.task_common_func import CommonFunc


_logger = InferLogger().logger


class InferSessionFactory:
    """
    infer session factory
    """
    @classmethod
    def create_infer_session_by_args(cls,
                                     args,
                                     logger=None):
        """
        params:
        args: input arguments
        logger: logger for mslite bench
        return: model session
        """
        if logger is None:
            logger = _logger
        model_path = args.model_file
        param_path = args.params_file
        cfg = CommonFunc.get_framework_config(model_path,
                                              args)

        model_session = InferSessionFactory.create_infer_session(model_path,
                                                                 cfg,
                                                                 params_file=param_path)
        logger.debug('Create model session success')
        return model_session

    @classmethod
    def create_infer_session(cls,
                             model_file,
                             cfg,
                             params_file=None):
        """
        params:
        model_file: path to AI model
        cfg: framework related config
        params_file: path to model weight file, for paddle, caffe etc.
        return: model session
        """
        infer_framework_type = cfg.infer_framework
        if infer_framework_type == FrameworkType.TF.value:
            try:
                infer_module = cls.import_module('mslite_bench.infer_base.tf_infer_session')
            except ImportError as e:
                _logger.info('import tf session failed: %s', e)
                raise
            infer_session = infer_module.TFSession(model_file, cfg)
        elif infer_framework_type == FrameworkType.ONNX.value:
            try:
                infer_module = cls.import_module('mslite_bench.infer_base.onnx_infer_session')
            except ImportError as e:
                _logger.info('import onnx session failed: %s', e)
                raise
            infer_session = infer_module.OnnxSession(model_file, cfg)
        elif infer_framework_type == FrameworkType.PADDLE.value:
            try:
                infer_module = cls.import_module('mslite_bench.infer_base.paddle_infer_session')
            except ImportError as e:
                _logger.info('import paddle session failed: %s', e)
                raise
            infer_session = infer_module.PaddleSession(model_file, cfg, params_file=params_file)
        elif infer_framework_type == FrameworkType.MSLITE.value:
            try:
                infer_module = cls.import_module('mslite_bench.infer_base.mslite_infer_session')
            except ImportError as e:
                _logger.info('import paddle session failed: %s', e)
                raise
            infer_session = infer_module.MsliteSession(model_file, cfg)
        else:
            raise NotImplementedError(f'{infer_framework_type} is not supported yet')
        return infer_session

    @staticmethod
    def import_module(module_name, file_path=None):
        """import module functions"""
        return importlib.import_module(module_name, package=file_path)
