/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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
#include <string>

#include "minddata/dataset/engine/opt/pre/node_offload_pass.h"
#include "minddata/dataset/engine/ir/datasetops/map_node.h"
#include "minddata/dataset/engine/ir/datasetops/batch_node.h"
#include "minddata/dataset/core/config_manager.h"
#include "minddata/dataset/kernels/ir/tensor_operation.h"

namespace mindspore {
namespace dataset {
NodeOffloadPass::OffloadNodes::OffloadNodes()
    : prev_map_offloaded_(true), auto_offload_(GlobalContext::config_manager()->get_auto_offload()) {}

// Perform MapNode offload check.
Status NodeOffloadPass::OffloadNodes::Visit(std::shared_ptr<MapNode> node, bool *const modified) {
  *modified = false;
  ManualOffloadMode manual_offload = node->GetOffload();
  bool offload_successful = false;

  // Check if the node is set to manually offload, or if auto_offload is enabled while manual offload is not False.
  if ((manual_offload == ManualOffloadMode::ENABLED) ||
      ((auto_offload_ == true) && (manual_offload != ManualOffloadMode::DISABLED))) {
    bool offload_supported = true;
    MS_LOG(INFO) << "Pre pass: node offload of map class is true.";

    // Currently offload not supported for different output_columns.
    if (node->InputColumns() != node->OutputColumns()) {
      MS_LOG(WARNING) << "Cannot offload map operation with output_columns != input_columns. Turning offload off.";
      offload_supported = false;
    }

    // Check if map operation is at the end of the pipeline.
    if (!prev_map_offloaded_) {
      MS_LOG(WARNING) << "Map operation is not at the end of the pipeline (there exists a non-offloaded map after this "
                         "one). Turning offload off.";
      offload_supported = false;
    }

    if (offload_supported) {
      std::vector<std::string> invalid_ops;
      std::vector<std::shared_ptr<TensorOperation>> temp_operations = node->operations();
      bool all_valid_ops = true;
      int last_invalid_op_pos = 1;
      int pos = 1;

      // Check individual operations to see if they are supported by offload.
      for (auto operation : temp_operations) {
        std::string op_name = operation->Name();
        if (supported_ops_.find(op_name) == supported_ops_.end()) {
          last_invalid_op_pos = pos;
          invalid_ops.push_back(op_name);
          all_valid_ops = false;
        }
        pos++;
      }

      if (all_valid_ops) {
        // All operations can be offloaded.
        nodes_to_offload_.push_back(std::static_pointer_cast<DatasetNode>(node));
        offload_successful = true;
      } else {
        // Some operation(s) cannot be offloaded.
        MS_LOG(WARNING)
          << "In Map Node, offload is set to True, but offload is not supported by the following operation(s): "
          << invalid_ops;

        // See if the operations can be split into two Map Nodes
        if (last_invalid_op_pos != temp_operations.size()) {
          MS_LOG(WARNING) << "Map operation will be split after " << invalid_ops.back()
                          << ", with the second map operation being offloaded.";
          std::vector<std::shared_ptr<TensorOperation>> non_offload_ops(temp_operations.begin(),
                                                                        temp_operations.begin() + last_invalid_op_pos);
          std::vector<std::shared_ptr<TensorOperation>> offload_ops(temp_operations.begin() + last_invalid_op_pos,
                                                                    temp_operations.end());

          // First set operations to offload_ops to prepare for copy
          node->setOperations(offload_ops);
          // Copy node (returns a copy of the node, but without children)
          std::shared_ptr<DatasetNode> offload_node = node->Copy();
          // Set the number of parallel workers of the new node to be the same as current one.
          offload_node->SetNumWorkers(node->NumWorkers());
          node->setOperations(non_offload_ops);
          // Insert the split offload map node above the original map node in the ir tree.
          node->InsertAbove(offload_node);
          // Add the offload map node to nodes_to_offload
          nodes_to_offload_.push_back(offload_node);
        } else {
          MS_LOG(WARNING) << "No operations can be offloaded through splitting.";
        }
      }
    }
  }
  if (!offload_successful) {
    // Offload of the original node without modification did not take place.
    // Since map nodes are visited in reverse order, no other map ops can be offloaded after this.
    prev_map_offloaded_ = false;
  }
  return Status::OK();
}

// constructor
NodeOffloadPass::NodeOffloadPass() {}

// Walk the tree to collect the nodes to offload, fill the offload_json object, then remove the node.
Status NodeOffloadPass::RunOnTree(std::shared_ptr<DatasetNode> root_ir, bool *const modified) {
  MS_LOG(INFO) << "Pre pass: node offload pass started.";
  // Create the offload node pass which can identify which nodes need to be offloaded.
  std::unique_ptr<NodeOffloadPass::OffloadNodes> offload_nodes = std::make_unique<NodeOffloadPass::OffloadNodes>();
  RETURN_IF_NOT_OK(offload_nodes->Run(root_ir, modified));

  // Update modified flag if there were any nodes identified to be offloaded
  if (offload_nodes->nodes_to_offload().empty() == false) {
    *modified = true;
  }

  // Then, execute the offloading of any nodes that were set up to be offloaded
  for (auto node : offload_nodes->nodes_to_offload()) {
    RETURN_IF_NOT_OK(node->to_json(&offload_json_));
    offload_json_["op_type"] = node->Name();

    // Add the single offloaded node to the list of offloaded nodes and remove the node from the ir tree
    offload_json_list_.push_back(offload_json_);
    RETURN_IF_NOT_OK(node->Drop());
  }
  MS_LOG(INFO) << "Pre pass: offload node removal pass complete.";
  return Status::OK();
}
}  // namespace dataset
}  // namespace mindspore
