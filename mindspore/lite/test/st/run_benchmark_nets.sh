#!/bin/bash

# Example:sh call_scipts.sh -r /home/temp_test -m /home/temp_test/models -d "8KE5T19620002408" -e arm_cpu
while getopts "r:m:d:e:l:" opt; do
    case ${opt} in
        r)
            release_path=${OPTARG}
            echo "release_path is ${OPTARG}"
            ;;
        m)
            models_path=${OPTARG}
            echo "models_path is ${OPTARG}"
            ;;
        d)
            device_id=${OPTARG}
            echo "device_id is ${OPTARG}"
            ;;
        e)
            backend=${OPTARG}
            echo "backend is ${OPTARG}"
            ;;
        l)
            level=${OPTARG}
            echo "level is ${OPTARG}"
            ;;
        ?)
        echo "unknown para"
        exit 1;;
    esac
done

cur_path=$(pwd)
echo "cur_path is "$cur_path
# This value could not be set to ON.
fail_not_return="OFF"
level=${level:-"level0"}

if [[ $backend == "all" || $backend == "arm64_cpu" || $backend == "arm64_tflite" || $backend == "arm64_mindir" || \
      $backend == "arm64_tf" || $backend == "arm64_caffe" || $backend == "arm64_onnx" || $backend == "arm64_quant" ]]; then
    sh $cur_path/scripts/run_benchmark_arm64.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level -p $fail_not_return
    arm64_status=$?
    if [[ $arm64_status -ne 0 ]]; then
      echo "Run arm64 failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "arm32_cpu" || $backend == "arm32_fp32" || $backend == "arm32_fp16" ]]; then
    sh $cur_path/scripts/run_benchmark_arm32.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level -p $fail_not_return
    arm32_status=$?
    if [[ $arm32_status -ne 0 ]]; then
      echo "Run arm32 failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "gpu" || $backend == "gpu_onnx_mindir" || $backend == "gpu_tf_caffe" || \
      $backend == "gpu_tflite" || $backend == "gpu_gl_texture" ]]; then
    sh $cur_path/scripts/run_benchmark_gpu.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level -p $fail_not_return
    gpu_status=$?
    if [[ $gpu_status -ne 0 ]]; then
      echo "Run gpu failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "npu" ]]; then
    sh $cur_path/scripts/run_benchmark_npu.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level -p $fail_not_return
    npu_status=$?
    if [[ $npu_status -ne 0 ]]; then
      echo "Run npu failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "x86-all" || $backend == "x86_onnx" || $backend == "x86_tf" || \
      $backend == "x86_tflite" || $backend == "x86_caffe" || $backend == "x86_mindir" || $backend == "linux_arm64_tflite" || \
      $backend == "x86_quant"  ]]; then
    sh $cur_path/scripts/run_benchmark_x86.sh -r $release_path -m $models_path -e $backend -l $level -p $fail_not_return
    x86_status=$?
    if [[ $x86_status -ne 0 ]]; then
      echo "Run x86 failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "x86_cloud_onnx" || $backend == "x86_cloud_tf" ]]; then
    sh $cur_path/scripts/run_benchmark_x86_cloud.sh -r $release_path -m $models_path -e $backend -l $level -p $fail_not_return
    x86_status=$?
    if [[ $x86_status -ne 0 ]]; then
      echo "Run x86_cloud failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "x86-all" || $backend == "x86_avx512" || $backend == "x86_avx512_onnx" || $backend == "x86_avx512_tf" || \
      $backend == "x86_avx512_tflite" || $backend == "x86_avx512_caffe" || $backend == "x86_avx512_mindir" ]]; then
    sh $cur_path/scripts/run_benchmark_x86.sh -r $release_path -m $models_path -e $backend -l $level -p $fail_not_return
    x86_status=$?
    if [[ $x86_status -ne 0 ]]; then
      echo "Run x86 failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "codegen" || $backend == "cortex_codegen" || $backend == "quant_codegen" ]]; then
    # run codegen
    sh $cur_path/scripts/run_benchmark_codegen.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level
    x86_status=$?
    if [[ $x86_status -ne 0 ]]; then
      echo "Run codegen failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "train" ]]; then
    # run train
    sh $cur_path/scripts/run_net_train.sh -r $release_path -m ${models_path}/../../models_train -d $device_id -e $backend -l $level
    x86_status=$?
    if [[ $x86_status -ne 0 ]]; then
      echo "Run train failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "x86_asan" ]]; then
    sh $cur_path/scripts/run_benchmark_asan.sh -r $release_path -m $models_path -e $backend -l $level
    x86_asan_status=$?
    if [[ $x86_asan_status -ne 0 ]]; then
      echo "Run x86 asan failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "arm32_3516D" ]]; then
    sh $cur_path/scripts/nnie/run_converter_nnie.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level
    hi3516_status=$?
    if [[ $hi3516_status -ne 0 ]]; then
      echo "Run nnie hi3516 failed"
      exit 1
    fi
    sh $cur_path/scripts/nnie/run_converter_nnie_micro.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level
    hi3516_micro_status=$?
    if [[ $hi3516_micro_status -ne 0 ]]; then
      echo "Run micro nnie hi3516 failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "simulation_sd3403" ]]; then
    sh $cur_path/scripts/dpico/run_simulation_3403.sh -r $release_path -m $models_path -e $backend -l $level
    simulation_sd3403_status=$?
    if [[ simulation_sd3403_status -ne 0 ]]; then
      echo "Run dpico simulation_sd3403 failed."
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "arm64_cpu_cropping" ]]; then
    sh $cur_path/scripts/run_benchmark_cropping_size.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level
    cpu_cropping_status=$?
    if [[ $cpu_cropping_status -ne 0 ]]; then
      echo "Run arm64_cpu_cropping failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "x86_gpu" ]]; then
    sh $cur_path/scripts/run_benchmark_tensorrt.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level
    tensorrt_status=$?
    if [[ $tensorrt_status -ne 0 ]]; then
      echo "Run x86 tensorrt gpu failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend =~ "x86_ascend310" || $backend =~ "x86_ascend710" || $backend =~ "arm_ascend310" ||
      $backend =~ "ascend310_ge" ]]; then
    sh $cur_path/scripts/ascend/run_ascend.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level -p $fail_not_return
    ascend_status=$?
    if [[ ascend_status -ne 0 ]]; then
      echo "Run ${backend} failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "server_inference_x86" || $backend == "server_inference_arm" ]]; then
    sh $cur_path/scripts/run_benchmark_server_inference.sh -r $release_path -m $models_path -e $backend -l $level
    server_inference_status=$?
    if [[ server_inference_status -ne 0 ]]; then
      echo "Run server inference failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "server_inference_x86_gpu" ]]; then
    sh $cur_path/scripts/run_benchmark_server_inference_tensorrt.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level
    server_inference_gpu_status=$?
    if [[ server_inference_gpu_status -ne 0 ]]; then
      echo "Run server inference gpu failed"
      exit 1
    fi
fi

if [[ $backend == "all" || $backend == "server_inference_x86_cloud_gpu" ]]; then
    sh $cur_path/scripts/run_benchmark_server_inference_tensorrt_cloud.sh -r $release_path -m $models_path -d $device_id -e $backend -l $level
    server_inference_gpu_status=$?
    if [[ server_inference_gpu_status -ne 0 ]]; then
      echo "Run server inference gpu failed"
      exit 1
    fi
fi

# two whl in release_path
if [[ $backend == "all" || $backend == "import_ms_and_mslite" ]]; then
    sh $cur_path/scripts/run_import_ms_and_mslite.sh -r $release_path -m $models_path -e $backend -l $level
    import_ms_and_mslite=$?
    if [[ import_ms_and_mslite -ne 0 ]]; then
      echo "Run import ms and mslite failed"
      exit 1
    fi
fi

# test tritonserver
if [[ $backend == "all" || $backend == "tritonserver" ]]; then
  sh $cur_path/scripts/test_triton.sh -r $release_path -m $models_path -l $level
  test_triton=$?
  if [[ test_triton -ne 0 ]]; then
    echo "Run tritonserver failed"
    exit 1
  fi
fi

# two whl in release_path
if [[ $backend == "all" || $backend == "plugin_custom_ops" ]]; then
    sh $cur_path/scripts/run_plugin_custom_ops.sh -r $release_path -m $models_path -e $backend -l $level
    plugin_custom_ops=$?
    if [[ plugin_custom_ops -ne 0 ]]; then
      echo "Run mslite ascend plugin custom ops failed"
      exit 1
    fi
fi

# test graph kernel
if [[ $backend == "all" || $backend =~ "graph_kernel" ]]; then
  if [[ $backend =~ "cpu" ]]; then
    device_id="null"
  fi
    sh $cur_path/scripts/run_benchmark_graph_kernel.sh -r $release_path -m $models_path -e $backend -l $level -d $device_id
    graph_kernel_status=$?
    if [[ graph_kernel_status -ne 0 ]]; then
      echo "Run graph kernel failed"
      exit 1
    fi
fi


if [[ $backend == "all" || $backend == "mslite_large_model_inference_arm_ascend910B" ]]; then
  echo "Run large model in ascend910B....."
  sh $cur_path/scripts/ascend/run_large_models.sh -r $release_path -m $models_path -e $backend -l $level -d $device_id
  ascend_status=$?
  if [[ ascend_status -ne 0 ]]; then
    echo "Run ${backend} failed"
    exit 1
  fi

  echo "Run Python ST in ascend910B....."
  sh $cur_path/scripts/ascend/run_python_api_ascend.sh -r $release_path
  ascend_status=$?
  if [[ ascend_status -ne 0 ]]; then
    echo "Run MSLite Python ST on Arm Ascend failed"
    exit 1
  fi

  #echo "Run AKG Cutsom Ops ST in ascend910B....."
  #sh $cur_path/scripts/ascend/run_akg_custom_ops.sh -r $release_path
  #ascend_status=$?
  #if [[ ascend_status -ne 0 ]]; then
  #  echo "Run AKG Cutsom Ops ST on Arm Ascend failed"
  #  exit 1
  #fi
fi

if [[ $backend == "all" || $backend == "mslite_large_model_cloud_infer" ]]; then
  sh $cur_path/scripts/ascend/run_large_models_cloud_infer.sh -r $release_path -m $models_path -e $backend -l $level
  ascend_status=$?
  if [[ ascend_status -ne 0 ]]; then
    echo "Run ${backend} failed"
    exit 1
  fi
fi
