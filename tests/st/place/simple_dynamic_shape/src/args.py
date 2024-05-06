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

import argparse
import ast


class ClusterArgs():
    def __init__(self, description="Run test case"):
        parser = argparse.ArgumentParser(description=description)
        parser.add_argument("--config_file_path", type=str, default="./config.json")
        parser.add_argument("--enable_ssl", type=ast.literal_eval, default=False)
        parser.add_argument("--client_password", type=str, default="123456")
        parser.add_argument("--server_password", type=str, default="123456")
        parser.add_argument("--device_target", type=str, default="GPU")

        args = parser.parse_args()
        self._config_file_path = args.config_file_path
        self._enable_ssl = args.enable_ssl
        self._client_password = args.client_password
        self._server_password = args.server_password
        self._device_target = args.device_target


    @property
    def config_file_path(self):
        return self._config_file_path


    @property
    def enable_ssl(self):
        return self._enable_ssl


    @property
    def client_password(self):
        return self._client_password


    @property
    def server_password(self):
        return self._server_password


    @property
    def device_target(self):
        return self._device_target
