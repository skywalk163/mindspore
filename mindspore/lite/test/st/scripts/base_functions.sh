#!/bin/bash

# Convert models:
function Convert() {
  # $1:cfgFileList; $2:inModelPath; $3:outModelPath; $4:logFile; $5:resultFile; $6:failNotReturn; $7:compileType
  fifo_file="fifo_file.txt"
  mkfifo ${fifo_file}
  exec 6<>${fifo_file}
  rm -f ${fifo_file}
  # To mark weather converter_lite failed or not. 
  fail_file="fail.txt"
  touch $fail_file
  max_converter_jobs=6
  for ((i = 0; i < ${max_converter_jobs}; i++)); do echo; done >&6
  local cfg_file_list model_info model_name extra_info model_type cfg_file_name model_file weight_file output_file \
        quant_type config_file train_model in_dtype out_dtype converter_result cfg_file calib_size save_type \
        input_format elapsed_time ret
  cfg_file_list=$1
  mkdir full_quant_debug
  for cfg_file in ${cfg_file_list[*]}; do
    while read line; do
      if [[ $line == \#* || $line == "" ]]; then
        continue
      fi
      if [[ $line =~ "parallel_predict_runtime" ]]; then
        echo "parallel predict not need convert model"
        continue
      fi
      read -u6
      {
        model_info=`echo ${line} | awk -F ' ' '{print $1}'`
        calib_size=`echo ${line} | awk -F ' ' '{print $3}'`
        model_name=`echo ${model_info} | awk -F ';' '{print $1}'`
        input_info=`echo ${model_info} | awk -F ';' '{print $2}'`
        input_shapes=`echo ${model_info} | awk -F ';' '{print $3}'`
        input_format=`echo ${model_info} | awk -F ';' '{print $4}'`
        extra_info=`echo ${model_info} | awk -F ';' '{print $5}'`
        input_data_type=`echo ${model_info} | awk -F ';' '{print $6}'`
        infix_str=`echo ${model_info} | awk -F ';' '{print $7}'`
        input_num=`echo ${input_info} | sed 's/:/;/' | awk -F ';' '{print $1}'`
        input_names=`echo ${input_info} | sed 's/:/;/' | awk -F ';' '{print $2}'`
        model_type=${model_name##*.}
        cfg_file_name=${cfg_file##*/}
        quant_config_path="${cfg_file%/*}/quant"
        ascend_config_path="${cfg_file%/*}/"
        graph_kernel_config_path="${cfg_file%/*}/graph_kernel"
        case $model_type in
          pb)
            model_fmk="TF"
            ;;
          tflite)
            model_fmk="TFLITE"
            ;;
          onnx)
            model_fmk="ONNX"
            ;;
          mindir)
            model_fmk="MINDIR"
            ;;
          *)
            model_type="caffe"
            model_fmk="CAFFE"
            ;;
        esac
        # set parameters
        model_file=$2"/"${model_name}
        if [[ ${extra_info} =~ "parallel_predict" ]]; then
          model_name=`echo ${model_name} | awk -F '/' '{print $1}'`
        fi
        weight_file=""
        if [[ $model_fmk == "CAFFE" ]]; then
          model_file=${model_file}".prototxt"
          weight_file=${model_file%.*}".caffemodel"
        fi
        output_file=$3"/"${model_name}
        quant_type=""
        config_file=""
        spec_shapes=""
        train_model="false"
        in_dtype="DEFAULT"
        out_dtype="DEFAULT"
        fp16_weight="off"
        save_type="MINDIR_LITE"
        optimize="general"
        if [[ ${input_data_type} != "" ]]; then
          in_dtype=${input_data_type}
        fi
        if [[ ${cfg_file_name} =~ "weightquant" ]]; then
          # models_weightquant_${suffix}.cfg
          suffix=${cfg_file_name: 19: -4}
          quant_type="WeightQuant"
          output_file=${output_file}"_${suffix}"
          config_file="${quant_config_path}/weight_quant_${suffix}.cfg"
        elif [[ ${cfg_file_name} =~ "_train" ]]; then
          train_model="true"
        elif [[ ${cfg_file_name} =~ "_ascend" ]]; then
          save_type="MINDIR"
          optimize="ascend_oriented"
          if [[ ${extra_info} =~ "online_convert" ]]; then
            optimize="none"
          fi
          if [[ ${model_fmk} != "TF" &&! ${cfg_file_name} =~ "_ge" &&  -z ${input_format} ]]; then
            input_format="NHWC"
          fi
        elif [[ ${cfg_file_name} =~ "_cloud_ms" ]]; then
          save_type="MINDIR_LITE"
          if [[ ${input_shapes} != "" && ${input_names} != "" ]]; then
            if [[ ${input_num} == "" ]]; then
              input_num=1
            fi
            IFS="," read -r -a name_array <<< ${input_names}
            IFS=":" read -r -a shape_array <<< ${input_shapes}
            for i in $(seq 0 $((${input_num}-1)))
            do
              spec_shapes=${spec_shapes}${name_array[$i]}':'${shape_array[$i]}';'
            done
          fi
        elif [[ ${cfg_file_name} =~ "_cloud" ]]; then
          save_type="MINDIR"
          if [[ ${input_shapes} != "" && ${input_names} != "" ]]; then
            if [[ ${input_num} == "" ]]; then
              input_num=1
            fi
            IFS="," read -r -a name_array <<< ${input_names}
            IFS=":" read -r -a shape_array <<< ${input_shapes}
            for i in $(seq 0 $((${input_num}-1)))
            do
              spec_shapes=${spec_shapes}${name_array[$i]}':'${shape_array[$i]}';'
            done
          fi
        fi

        if [[ ${cfg_file_name} =~ "_graph_kernel" ]]; then
          save_type="MINDIR"
          config_file="${graph_kernel_config_path}/graph_kernel_cpu.cfg"
          if [[ ${cfg_file_name} =~ "_gpu" ]]; then
            config_file="${graph_kernel_config_path}/graph_kernel_gpu.cfg"
            optimize="gpu_oriented"
          elif [[ ${cfg_file_name} =~ "_ascend" ]]; then
            config_file="${graph_kernel_config_path}/graph_kernel_ascend.cfg"
            optimize="ascend_oriented"
          fi
        fi

        if [[ ${cfg_file_name} =~ "posttraining" || ${cfg_file_name} =~ "posttraining_cloud" ]]; then
          quant_type="PostTraining"
          output_file=${output_file}"_posttraining"
          config_file="${quant_config_path}/${model_name}_${cfg_file_name:7:-4}.config"
          QUANT_TRAINING_DIR=$2/../../quantTraining
          if [ ! -d quantTraining ]; then
            echo "link remote QUANT_TRAINING_DIR: ${QUANT_TRAINING_DIR}"
            ln -s ${QUANT_TRAINING_DIR} quantTraining
          fi
        elif [[ ${cfg_file_name} =~ "dynamic_quant" || ${cfg_file_name} =~ "posttraining_cloud" ]]; then
          quant_type="DynamicQuant"
          output_file=${output_file}"_dynamic_quant"
          config_file="${quant_config_path}/dynamic_quant.cfg"
        elif [[ ${cfg_file_name} =~ "awaretraining" || ${extra_info} =~ "aware_training" ]]; then
          in_dtype="FLOAT"
          out_dtype="FLOAT"
        elif [[ ${cfg_file_name} =~ "_ascend_on_the_fly_quant_ge_cloud" ]]; then
          quant_type="ONTHEFLYQuant"
          output_file=${output_file}"_on_the_fly_quant"
          config_file="${quant_config_path}/ascend_on_the_fly_quant_ge_cloud.cfg"
        elif [[ ${cfg_file_name} =~ "_ascend_fake_model_on_the_fly_quant_ge_cloud" ]]; then
          quant_type="FakeModelONTHEFLYQuant"
          output_file=${output_file}"_on_the_fly_quant"
          config_file="${quant_config_path}/ascend_fake_model_on_the_fly_quant_ge_cloud.cfg"
        elif [[ ${cfg_file_name} =~ "_ascend_fake_model_full_quant_ge_cloud" ]]; then
          quant_type="FakeModelFullQuant"
          output_file=${output_file}"_full_quant"
          config_file="${quant_config_path}/ascend_fake_model_full_quant_ge_cloud.cfg"
        fi

        if [[ ${extra_info} =~ "offline_resize" && ${input_shapes} != "" && ${input_names} != "" ]]; then
          if [[ ${input_num} == "" ]]; then
            input_num=1
          fi
          IFS="," read -r -a name_array <<< ${input_names}
          IFS=":" read -r -a shape_array <<< ${input_shapes}
          for i in $(seq 0 $((${input_num}-1)))
          do
            spec_shapes=${spec_shapes}${name_array[$i]}':'${shape_array[$i]}';'
          done
        fi
        if [[ ${cfg_file_name} =~ "_with_config_cloud_ascend" ]]; then
            spec_shapes=""
            config_file="${ascend_config_path}/${model_name}.config"
        fi
        if [[ ${extra_info} =~ "fp16_weight" ]]; then
          fp16_weight="on"
        fi
        if [[ ${extra_info} =~ "online_convert" ]]; then
          optimize="none"
        fi
        if [[ ${extra_info} =~ "large_model" ]]; then
            input_format=""
        fi
        # start running converter
        echo "Convert ${model_name} ${quant_type} ......"
        echo ${model_name} >> "$4"
        elapsed_time=$(date +%s.%N)
        if [[ ${cfg_file_name} =~ "_cloud" ]]; then
            echo "./converter_lite --fmk=${model_fmk} --modelFile=${model_file} --weightFile=${weight_file} --outputFile=${output_file}\
              --inputDataType=${in_dtype} --outputDataType=${out_dtype} --inputShape=${spec_shapes} --fp16=${fp16_weight}\
              --configFile=${config_file} --saveType=${save_type} --optimize=${optimize} \
              --inputDataFormat=${input_format}"
              
            ./converter_lite --fmk=${model_fmk} --modelFile=${model_file} --weightFile=${weight_file} --outputFile=${output_file}\
              --inputDataType=${in_dtype} --outputDataType=${out_dtype} --inputShape="${spec_shapes}" --fp16=${fp16_weight}\
              --configFile=${config_file} --saveType=${save_type} --optimize=${optimize} \
              --inputDataFormat=${input_format} &>> "$4" 
        else
            echo "./converter_lite --fmk=${model_fmk} --modelFile=${model_file} --weightFile=${weight_file} --outputFile=${output_file}\
              --inputDataType=${in_dtype} --outputDataType=${out_dtype} --inputShape=${spec_shapes} --fp16=${fp16_weight}\
              --configFile=${config_file} --trainModel=${train_model}"

            ./converter_lite --fmk=${model_fmk} --modelFile=${model_file} --weightFile=${weight_file} --outputFile=${output_file}\
              --inputDataType=${in_dtype} --outputDataType=${out_dtype} --inputShape="${spec_shapes}" --fp16=${fp16_weight}\
              --configFile=${config_file} --trainModel=${train_model} &>> "$4"
            
        fi
        ret=$?
        elapsed_time=$(printf %.2f "$(echo "$(date +%s.%N) - $elapsed_time" | bc)")
        if [ ${ret} = 0 ]; then
            converter_result='converter '${model_type}''${quant_type}' '${model_name}' '${elapsed_time}' pass';echo ${converter_result} >> $5
            local model_size
            if [[ ${infix_str} != "" ]]; then
              output_file=${output_file}${infix_str}
            fi
            if [ "${save_type}"x == "MINDIR"x ]; then
              output_file=${output_file}".mindir"
              model_size=`ls ${output_file}  -l|awk -F ' ' '{print $5}'`
            else
              output_file=${output_file}".ms"
              model_size=`ls ${output_file}  -l|awk -F ' ' '{print $5}'`
            fi
            echo "${output_file} output size: ${model_size}."
            let calib_final_size=${calib_size}+50
            if [[ -n ${calib_size} ]];then
              if [ ${model_size} -gt ${calib_final_size} ]; then
                echo "${output_file} " model size is " ${model_size} " and calib size is " ${calib_size}"
                converter_result='compare_size '${model_type}''${quant_type}' '${output_file##*/}' '${elapsed_time}' failed';echo ${converter_result} >> $5
                rm -rf ${output_file}
                if [[ $6 != "ON" ]]; then
                  echo 1 > $fail_file
                fi
              else
                converter_result='compare_size '${model_type}''${quant_type}' '${output_file##*/}' '${elapsed_time}' pass';echo ${converter_result} >> $5
              fi
            fi
        else
            converter_result='converter '${model_type}''${quant_type}' '${model_name}' '${elapsed_time}' failed';echo ${converter_result} >> $5
            if [[ $6 != "ON" ]]; then
              echo 1 > $fail_file
            fi
        fi
        echo >&6
      } &
    done < ${cfg_file}
  done
  wait
  exec 6>&-
  fail=0
  if [ -s $fail_file ]; then
    fail=1
  fi
  return ${fail}
}

function Push_Files() {
    # $1:packagePath; $2:platform; $3:version; $4:localPath; $5:logFile; $6:deviceID;
    cd $1 || exit 1
    tar -zxf mindspore-lite-$3-android-$2.tar.gz || exit 1

    # If build with minddata, copy the minddata related libs
    cd $4 || exit 1
    if [ -f $1/mindspore-lite-$3-android-$2/runtime/lib/libminddata-lite.so ]; then
        cp -a $1/mindspore-lite-$3-android-$2/runtime/lib/libminddata-lite.so $4/libminddata-lite.so || exit 1
    fi
    if [ -f $1/mindspore-lite-$3-android-$2/runtime/third_party/hiai_ddk/lib/libhiai.so ]; then
      cp -a $1/mindspore-lite-$3-android-$2/runtime/third_party/hiai_ddk/lib/libhiai.so $4/libhiai.so || exit 1
      cp -a $1/mindspore-lite-$3-android-$2/runtime/third_party/hiai_ddk/lib/libhiai_ir.so $4/libhiai_ir.so || exit 1
      cp -a $1/mindspore-lite-$3-android-$2/runtime/third_party/hiai_ddk/lib/libhiai_ir_build.so $4/libhiai_ir_build.so || exit 1
    fi
    if [ -f $1/mindspore-lite-$3-android-$2/runtime/third_party/hiai_ddk/lib/libhiai_hcl_model_runtime.so ]; then
      cp -a $1/mindspore-lite-$3-android-$2/runtime/third_party/hiai_ddk/lib/libhiai_hcl_model_runtime.so $4/libhiai_hcl_model_runtime.so || exit 1
    fi

    cp -a $1/mindspore-lite-$3-android-$2/runtime/lib/libmindspore-lite.so $4/libmindspore-lite.so || exit 1
    cp -a $1/mindspore-lite-$3-android-$2/tools/benchmark/benchmark $4/benchmark || exit 1

    # adb push all needed files to the phone
    adb -s $6 push $4 /data/local/tmp/ > $5

    arm32_dir=""
    if [[ $2 == "aarch32" ]]; then
      arm32_dir="arm32/"
    fi
    # run adb ,run session ,check the result:
    echo 'cd  /data/local/tmp/benchmark_test' > adb_cmd.txt
    echo 'cp  /data/local/tmp/'$arm32_dir'libc++_shared.so ./' >> adb_cmd.txt
    echo 'chmod 777 benchmark' >> adb_cmd.txt

    adb -s $6 shell < adb_cmd.txt
}

# Run converted models:
function Run_Benchmark() {
  # $1:cfgFileList; $2:modelPath; $3:dataPath; $4:logFile; $5:resultFile; $6:platform; $7:processor; $8:phoneId; $9:failNotReturn;
  local cfg_file_list cfg_file_name line_info model_info spec_acc_limit model_name input_num input_shapes spec_threads \
        extra_info benchmark_mode infix mode model_file ms_model_type input_files output_file data_path threads acc_limit enableFp16 \
        run_result cfg_file input_data_mode enableGLTexture elapsed_time ret
  cfg_file_list=$1
  for cfg_file in ${cfg_file_list[*]}; do
    cfg_file_name=${cfg_file##*/}
    while read line; do
      line_info=${line}
      if [[ $line_info == \#* || $line_info == "" ]]; then
        continue
      fi
      model_info=`echo ${line_info} | awk -F ' ' '{print $1}'`
      spec_acc_limit=`echo ${line_info} | awk -F ' ' '{print $2}'`
      model_name=`echo ${model_info} | awk -F ';' '{print $1}'`
      input_info=`echo ${model_info} | awk -F ';' '{print $2}'`
      input_shapes=`echo ${model_info} | awk -F ';' '{print $3}'`
      spec_threads=`echo ${model_info} | awk -F ';' '{print $4}'`
      extra_info=`echo ${model_info} | awk -F ';' '{print $5}'`
      infix_str=`echo ${model_info} | awk -F ';' '{print $7}'`
      input_num=`echo ${input_info} | sed 's/:/;/' | awk -F ';' '{print $1}'`
      input_names=`echo ${input_info} | sed 's/:/;/' | awk -F ';' '{print $2}'`
      # adjust threads
      threads="2"
      if [[ ${spec_threads} != "" ]]; then
        threads="${spec_threads}"
      fi
      inter_op_parallel_num=1
      use_parallel_predict="false"
      if [[ ${extra_info} =~ "parallel_predict" ]]; then
        use_parallel_predict="true"
        model_name=`echo ${model_name} | awk -F '/' '{print $1}'`
        inter_op_parallel_num=${threads}
      fi
      if [[ ${model_name##*.} == "caffemodel" ]]; then
        model_name=${model_name%.*}
      fi
      echo "Benchmarking ${model_name} $6 $7 ......"
      # adjust benchmark mode
      benchmark_mode="calib"
      # adjust precision mode
      mode="fp32"
      if [[ ${cfg_file_name} =~ "fp16" ]]; then
        mode="fp16"
      fi
      # adjust input data mode
      input_data_mode="cpu"
      if [[ ${cfg_file_name} =~ "gl_texture" ]]; then
        input_data_mode="opengl"
      fi
      # adjust file name
      infix=""
      if [[ ${cfg_file_name} =~ "weightquant" ]]; then
        infix="_${cfg_file_name: 19: -4}"
      elif [[ ${cfg_file_name} =~ "_train" ]]; then
        infix="_train"
      elif [[ ${cfg_file_name} =~ "_posttraining" ]]; then
        model_name=${model_name}"_posttraining"
      elif [[ ${cfg_file_name} =~ "_dynamic_quant" ]]; then
        infix="_dynamic_quant"
      elif [[ ${cfg_file_name} =~ "_process_only" ]]; then
        benchmark_mode="loop"
      elif [[ ${cfg_file_name} =~ "_compatibility" && ${spec_acc_limit} == "" ]]; then
        benchmark_mode="loop"
      fi
      if [[ ${infix_str} != "" ]]; then
        infix=${infix}${infix_str}
      fi
      if [[ ${cfg_file_name} =~ "_cloud_ms" ]]; then
        model_file=$2"/${model_name}${infix}.ms"
        ms_model_type="MindIR_Lite"
      elif [[ ${cfg_file_name} =~ "_cloud" ]]; then
        model_file=$2"/${model_name}${infix}.mindir"
        ms_model_type="MindIR"
      else
        model_file=$2"/${model_name}${infix}.ms"
        ms_model_type="MindIR_Lite"
      fi
      if [[ ${use_parallel_predict} == "true" ]]; then
        export BENCHMARK_WEIGHT_PATH=${model_file}
      fi
      input_files=""
      output_file=""
      data_path=$3"/input_output/"
      if [[ ${input_num} == "" || ${input_num} == 1 ]]; then
        if [[ ${cfg_file_name} =~ "_cloud" ]]; then
          input_files=${data_path}'input/'${model_name}'.bin'
        else
          input_files=${data_path}'input/'${model_name}'.ms.bin'
        fi
      else
        if [[ ${cfg_file_name} =~ "_cloud" ]]; then
          for i in $(seq 1 $input_num)
          do
            input_files=${input_files}${data_path}'input/'${model_name}'.bin_'$i','
          done
        else 
          for i in $(seq 1 $input_num)
          do
            input_files=${input_files}${data_path}'input/'${model_name}'.ms.bin_'$i','
          done
        fi
      fi
      if [[ ${cfg_file_name} =~ "_cloud" ]]; then
        output_file=${data_path}'output/'${model_name}'.out'
      else
        output_file=${data_path}'output/'${model_name}'.ms.out'
      fi

      # set accuracy limitation
      acc_limit="0.5"
      if [[ ${cfg_file_name} =~ "_train" ]]; then
        acc_limit="1.5"
      fi
      if [[ ${spec_acc_limit} != "" ]]; then
        acc_limit="${spec_acc_limit}"
      elif [[ $7 == "GPU" ]] && [[ ${mode} == "fp16" || ${cfg_file_name} =~ "_weightquant" ]]; then
        acc_limit="5"
      fi
      # whether enable fp16
      enableFp16="false"
      if [[ ${mode} == "fp16" ]]; then
        enableFp16="true"
      fi
      # whether enable gl texture
      enableGLTexture="false"
      if [[ ${input_data_mode} == "opengl" ]]; then
        enableGLTexture="true"
      fi

      if [[ $6 == "arm64" && ${extra_info} =~ "need_loop" ]]; then
        benchmark_mode="calib+loop"
      fi
      if [[ ${extra_info} =~ "offline_resize" ]]; then
        input_shapes=""
      fi
      # start running benchmark
      echo "---------------------------------------------------------" >> "$4"
      elapsed_time=$(date +%s.%N)
      if [[ ${benchmark_mode} = "calib" || ${benchmark_mode} = "calib+loop" ]]; then
        echo "$6 $7 ${mode} run calib: ${model_name}, accuracy limit:${acc_limit}" >> "$4"
        if [[ $6 == "arm64" || $6 == "arm32" ]]; then
          echo 'pid=`pidof benchmark`' > kill_benchmark.txt
          echo 'if [[ -n $pid ]]; then kill -9 $pid; fi' >> kill_benchmark.txt
          adb -s $8 shell < kill_benchmark.txt

          echo 'cd  /data/local/tmp/benchmark_test' > adb_run_cmd.txt
          echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/data/local/tmp/benchmark_test' >> adb_run_cmd.txt
          echo './benchmark --enableParallelPredict='${use_parallel_predict}' --modelFile='${model_file}' --inDataFile='${input_files}' --inputShapes='${input_shapes}' --benchmarkDataFile='${output_file}' --enableFp16='${enableFp16}' --accuracyThreshold='${acc_limit}' --device='$7' --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads}' --delegateMode='${10} >> adb_run_cmd.txt
          echo './benchmark --enableParallelPredict='${use_parallel_predict}' --modelFile='${model_file}' --inDataFile='${input_files}' --inputShapes='${input_shapes}' --benchmarkDataFile='${output_file}' --enableFp16='${enableFp16}' --accuracyThreshold='${acc_limit}' --device='$7' --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads}' --delegateMode='${10} >> $4
          cat adb_run_cmd.txt >> "$4"
          adb -s $8 shell < adb_run_cmd.txt >> "$4"
        else
          if [[ ${cfg_file_name} =~ "_reconstitution_cloud" ]]; then  #  for reconstitution extendrt
            echo 'ENABLE_MULTI_BACKEND_RUNTIME=on MSLITE_BENCH_INPUT_NAMES=${input_names} ./benchmark --enableParallelPredict='${use_parallel_predict}' --modelFile='${model_file}' --inDataFile='${input_files}' --inputShapes='${input_shapes}' --benchmarkDataFile='${output_file}' --accuracyThreshold='${acc_limit}' --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads}' --modelType='${ms_model_type} >> "$4"
            ENABLE_MULTI_BACKEND_RUNTIME=on MSLITE_BENCH_INPUT_NAMES=${input_names} ./benchmark --enableParallelPredict=${use_parallel_predict} --modelFile=${model_file} --inDataFile=${input_files} --inputShapes=${input_shapes} --benchmarkDataFile=${output_file} --accuracyThreshold=${acc_limit} --interOpParallelNum=${inter_op_parallel_num} --numThreads=${threads} --modelType=${ms_model_type} >> "$4"
          else
            echo 'MSLITE_BENCH_INPUT_NAMES=${input_names} ./benchmark --enableParallelPredict='${use_parallel_predict}' --modelFile='${model_file}' --inDataFile='${input_files}' --inputShapes='${input_shapes}' --benchmarkDataFile='${output_file}' --accuracyThreshold='${acc_limit}' --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads}' --modelType='${ms_model_type} >> "$4"
            MSLITE_BENCH_INPUT_NAMES=${input_names} ./benchmark --enableParallelPredict=${use_parallel_predict} --modelFile=${model_file} --inDataFile=${input_files} --inputShapes=${input_shapes} --benchmarkDataFile=${output_file} --accuracyThreshold=${acc_limit} --interOpParallelNum=${inter_op_parallel_num} --numThreads=${threads} --modelType=${ms_model_type} >> "$4"
          fi
        fi
        ret=$?
        elapsed_time=$(printf %.2f "$(echo "$(date +%s.%N) - $elapsed_time" | bc)")
        if [ ${ret} = 0 ]; then
          if [[ ${extra_info} =~ "parallel_predict" ]]; then
            run_result="$6_$7_${mode}: ${model_file##*/} ${elapsed_time} parallel_pass"; echo ${run_result} >> $5
          else
            run_result="$6_$7_${mode}: ${model_file##*/} ${elapsed_time} pass"; echo ${run_result} >> $5
          fi
        else
          if [[ ${extra_info} =~ "parallel_predict" ]]; then
            run_result="$6_$7_${mode}: ${model_file##*/} ${elapsed_time} parallel_failed"; echo ${run_result} >> $5
          else
            run_result="$6_$7_${mode}: ${model_file##*/} ${elapsed_time} failed"; echo ${run_result} >> $5
          fi
          if [[ $9 != "ON" ]]; then
              return 1
          fi
        fi
      fi
      # run benchmark without clib data recurrently for guarding the repeated graph execution scene
      elapsed_time=$(date +%s.%N)
      if [[ ${benchmark_mode} = "loop" || ${benchmark_mode} = "calib+loop" ]]; then
        echo "$6 $7 ${mode} run loop: ${model_name}" >> "$4"
        if [[ ! ${extra_info} =~ "input_dependent" ]]; then
          input_files=""
        fi
        if [[ $6 == "arm64" || $6 == "arm32" ]]; then
          echo 'pid=`pidof benchmark`' > kill_benchmark.txt
          echo 'if [[ -n $pid ]]; then kill -9 $pid; fi' >> kill_benchmark.txt
          adb -s $8 shell < kill_benchmark.txt

          echo 'cd  /data/local/tmp/benchmark_test' > adb_run_cmd.txt
          echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/data/local/tmp/benchmark_test' >> adb_run_cmd.txt
          echo './benchmark --enableParallelPredict='${use_parallel_predict}' --inDataFile='${input_files}' --modelFile='${model_file}' --inputShapes='${input_shapes}' --enableFp16='${enableFp16}' --warmUpLoopCount=0 --loopCount=2 --device='$7' --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads} >> adb_run_cmd.txt
          echo './benchmark --enableParallelPredict='${use_parallel_predict}' --inDataFile='${input_files}' --modelFile='${model_file}' --inputShapes='${input_shapes}' --enableFp16='${enableFp16}' --warmUpLoopCount=0 --loopCount=2 --device='$7' --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads} >> $4
          cat adb_run_cmd.txt >> "$4"
          adb -s $8 shell < adb_run_cmd.txt >> "$4"
        else
          if [[ ${cfg_file_name} =~ "_reconstitution_cloud_process_only" ]]; then  #  for reconstitution extendrt
            echo 'ENABLE_MULTI_BACKEND_RUNTIME=on ./benchmark --enableParallelPredict='${use_parallel_predict}' --inDataFile='${input_files}' --modelFile='${model_file}' --inputShapes='${input_shapes}' --warmUpLoopCount=0 --loopCount=2 --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads} >> "$4"
            ENABLE_MULTI_BACKEND_RUNTIME=on ./benchmark --enableParallelPredict=${use_parallel_predict} --inDataFile=${input_files} --modelFile=${model_file} --inputShapes=${input_shapes} --warmUpLoopCount=0 --loopCount=2 --interOpParallelNum=${inter_op_parallel_num} --numThreads=${threads} >> "$4"
          else
            echo './benchmark --enableParallelPredict='${use_parallel_predict}' --inDataFile='${input_files}' --modelFile='${model_file}' --inputShapes='${input_shapes}' --warmUpLoopCount=0 --loopCount=2 --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads} >> "$4"
            ./benchmark --enableParallelPredict=${use_parallel_predict} --inDataFile=${input_files} --modelFile=${model_file} --inputShapes=${input_shapes} --warmUpLoopCount=0 --loopCount=2 --interOpParallelNum=${inter_op_parallel_num} --numThreads=${threads} >> "$4"
          fi
        fi
        ret=$?
        elapsed_time=$(printf %.2f "$(echo "$(date +%s.%N) - $elapsed_time" | bc)")
        if [ ${ret} = 0 ]; then
            run_result="$6_$7_${mode}_loop: ${model_file##*/} ${elapsed_time} pass"; echo ${run_result} >> $5
        else
            run_result="$6_$7_${mode}_loop: ${model_file##*/} ${elapsed_time} failed"; echo ${run_result} >> $5
            if [[ $9 != "ON" ]]; then
                return 1
            fi
        fi
      fi
      # run benchmark with enable gl_texture
      elapsed_time=$(date +%s.%N)
      if [[ ${input_data_mode} == "opengl" ]]; then
        echo "$6 $7 ${mode} run gl texture: ${model_name}, accuracy limit:${acc_limit}" >> "$4"
        if [[ $6 == "arm64" ]]; then
          echo 'pid=`pidof benchmark`' > kill_benchmark.txt
          echo 'if [[ -n $pid ]]; then kill -9 $pid; fi' >> kill_benchmark.txt
          adb -s $8 shell < kill_benchmark.txt

          echo 'cd  /data/local/tmp/benchmark_test' > adb_run_cmd.txt
          echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/data/local/tmp/benchmark_test' >> adb_run_cmd.txt
          echo './benchmark --enableParallelPredict='${use_parallel_predict}' --modelFile='${model_file}' --inDataFile='${input_files}' --inputShapes='${input_shapes}' --benchmarkDataFile='${output_file}' --enableFp16='${enableFp16}' --enableGLTexture='${enableGLTexture}' --accuracyThreshold='${acc_limit}' --device='$7' --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads} >> adb_run_cmd.txt
          echo './benchmark --enableParallelPredict='${use_parallel_predict}' --modelFile='${model_file}' --inDataFile='${input_files}' --inputShapes='${input_shapes}' --benchmarkDataFile='${output_file}' --enableFp16='${enableFp16}' --enableGLTexture='${enableGLTexture}' --accuracyThreshold='${acc_limit}' --device='$7' --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads} >> $4
          cat adb_run_cmd.txt >> "$4"
          adb -s $8 shell < adb_run_cmd.txt >> "$4"
        else
          echo 'MSLITE_BENCH_INPUT_NAMES=${input_names} ./benchmark --enableParallelPredict='${use_parallel_predict}' --modelFile='${model_file}' --inDataFile='${input_files}' --inputShapes='${input_shapes}' --benchmarkDataFile='${output_file}' --accuracyThreshold='${acc_limit}' --interOpParallelNum='${inter_op_parallel_num}' --numThreads='${threads} >> "$4"
          MSLITE_BENCH_INPUT_NAMES=${input_names} ./benchmark --enableParallelPredict=${use_parallel_predict} --modelFile=${model_file} --inDataFile=${input_files} --inputShapes=${input_shapes} --benchmarkDataFile=${output_file} --accuracyThreshold=${acc_limit} --interOpParallelNum=${inter_op_parallel_num} --numThreads=${threads} >> "$4"
        fi
        ret=$?
        elapsed_time=$(printf %.2f "$(echo "$(date +%s.%N) - $elapsed_time" | bc)")
        if [ ${ret} = 0 ]; then
          run_result="$6_$7_${mode}: ${model_file##*/} ${elapsed_time} pass"; echo ${run_result} >> $5
        else
          run_result="$6_$7_${mode}: ${model_file##*/} ${elapsed_time} failed"; echo ${run_result} >> $5
          if [[ $9 != "ON" ]]; then
              return 1
          fi
        fi
      fi
    done < ${cfg_file}
  done
}

function Exist_File_In_Path() {
# $1:Path; $2:FileName;
  for file in $1/*; do
    if [[ ${file##*/} =~ $2 ]]; then
      echo "true"
      return
    fi
  done
  echo "false"
}

# Print start msg before run testcase
function MS_PRINT_TESTCASE_START_MSG() {
    echo ""
    echo -e "--------------------------------------------------------------------------------------------------------------------------------------------"
    echo -e "env                      Testcase                                                                                            Time    Result "
    echo -e "---                      --------                                                                                            ----    ------ "
}

# Print start msg after run testcase
function MS_PRINT_TESTCASE_END_MSG() {
    echo -e "------------------------------------------------------------------------------------------------------------------------------------------------------"
}

function Print_Converter_Result() {
    MS_PRINT_TESTCASE_END_MSG
    echo "CONVERTER RESULT PRINT BEGIN"
    while read line; do
        arr=("${line}")
        printf "%-15s %-20s %-100s %-8s %-7s\n" ${arr[0]} ${arr[1]} ${arr[2]} ${arr[3]} ${arr[4]}
    done < $1
    echo "CONVERTER RESULT PRINT END"
    MS_PRINT_TESTCASE_END_MSG
}

function Print_Benchmark_Result() {
    MS_PRINT_TESTCASE_START_MSG
    echo "BENCHMARK RESULT PRINT BEGIN"
    while read line; do
        arr=("${line}")
        printf "%-25s %-100s %-8s %-7s\n" ${arr[0]} ${arr[1]} ${arr[2]} ${arr[3]}
    done < $1
    echo "BENCHMARK RESULT PRINT END"
    MS_PRINT_TESTCASE_END_MSG
}
