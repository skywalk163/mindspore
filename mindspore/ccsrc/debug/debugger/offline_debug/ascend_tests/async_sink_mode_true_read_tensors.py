# Copyright 2021 Huawei Technologies Co., Ltd
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
# ==============================================================================
"""
Read tensor test script for offline debugger APIs.
"""

import mindspore.offline_debug.dbg_services as d
import numpy as np


def main():

    debugger_backend = d.DbgServices(
        dump_file_path="/opt/nvme2n1/j00455527/dumps/async_sink_true/032421")

    _ = debugger_backend.initialize(net_name="alexnet", is_sync_mode=False)

    # output tensor with zero slot
    info1 = d.TensorInfo(node_name="Default/network-TrainOneStepCell/network-WithLossCell/_backbone-AlexNet/"
                                   "conv3-Conv2d/Conv2D-op169",
                         slot=0, iteration=2, device_id=0, root_graph_id=1, is_parameter=False)
    # output tensor with non-zero slot
    info2 = d.TensorInfo(node_name="Default/network-TrainOneStepCell/network-WithLossCell/_backbone-AlexNet/"
                                   "ReLUV2-op348",
                         slot=1, iteration=2, device_id=0, root_graph_id=1, is_parameter=False)

    tensor_info = [info1, info2]

    tensor_data = debugger_backend.read_tensors(tensor_info)

    print_read_tensors(tensor_info, tensor_data)


def print_read_tensors(tensor_info, tensor_data):
    """Print read tensors."""
    for x, _ in enumerate(tensor_info):
        print("-----------------------------------------------------------")
        print("tensor_info_" + str(x+1) + " attributes:")
        print("node name = ", tensor_info[x].node_name)
        print("slot = ", tensor_info[x].slot)
        print("iteration = ", tensor_info[x].iteration)
        print("device_id = ", tensor_info[x].device_id)
        print("root_graph_id = ", tensor_info[x].root_graph_id)
        print("is_parameter = ", tensor_info[x].is_parameter)
        print()
        print("tensor_data_" + str(x+1) + " attributes:")
        print("data (printed in uint8) = ", np.frombuffer(
            tensor_data[x].data_ptr, np.uint8, tensor_data[x].data_size))
        py_byte_size = len(tensor_data[x].data_ptr)
        c_byte_size = tensor_data[x].data_size
        if c_byte_size != py_byte_size:
            print("The python byte size of ", py_byte_size,
                  " does not match the C++ byte size of ", c_byte_size)
        print("size in bytes = ", tensor_data[x].data_size)
        print("debugger dtype = ", tensor_data[x].dtype)
        print("shape = ", tensor_data[x].shape)


if __name__ == "__main__":
    main()
