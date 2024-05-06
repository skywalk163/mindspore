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
for onnx infer session
"""
from abc import ABC
from typing import Dict

import onnx
import onnxruntime
import numpy as np

from mslite_bench.infer_base.abs_infer_session import AbcInferSession


class OnnxSession(AbcInferSession, ABC):
    """onnx infer session"""
    def __init__(self,
                 model_file,
                 cfg=None):
        super().__init__(model_file, cfg)
        self.model = onnx.load(model_file)
        self.output_nodes = self._get_all_output_nodes()
        self.output_tensor_names = self._get_output_tensor_names()
        self.model_session = self._create_infer_session()

    def infer(self, input_data_map: Dict[str, np.ndarray]) -> Dict[str, np.ndarray]:
        """onnx infer"""
        outputs = self.model_session.run(self.output_tensor_names,
                                         input_data_map)
        result = {}
        for key, value in zip(self.output_tensor_names, outputs):
            result[key] = value
        return result

    def _create_infer_session(self):
        """create infer session"""
        model_session = onnxruntime.InferenceSession(self.model_file,
                                                     providers=['CPUExecutionProvider'])
        self.logger.debug('onnx Session create successfully')
        return model_session

    def _get_all_input_nodes(self):
        """get all input nodes"""
        all_input_nodes = self.model.graph.input
        input_initializer_nodes = self.model.graph.initializer

        return list(set(all_input_nodes) - set(input_initializer_nodes))

    def _get_all_output_nodes(self):
        """get all output nodes"""
        return self.model.graph.output

    def _get_output_tensor_names(self):
        """get output tensor names"""
        if self.output_tensor_names is None:
            self.output_tensor_names = [node.name for node in self.output_nodes]
        return self.output_tensor_names
