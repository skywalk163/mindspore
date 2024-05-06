#!/bin/bash

function Run_python_ST() {
  # $1:basePath; $2:whlPath; $3:modelPath; $4:cfgFileList; $5:target;
  base_path=$1
  whl_path=$2
  model_path=$3
  in_data_path=$4
  model_hiai_path=$in_data_path
  cfg_file_list=$5
  target=$6
  suffix=".mindir"
  bin_suffix=''
  if [ $# -gt 6 ]; then
    suffix=$7
    bin_suffix=$7
  fi
  mindspore_lite_whl=`ls ${whl_path}/*.whl`
  if [[ -f "${mindspore_lite_whl}" ]]; then
    pip uninstall mindspore_lite -y || exit 1
    pip install ${mindspore_lite_whl} --user || exit 1
    echo "install python whl success."
  else
    echo "not find python whl.."
    exit 1
  fi
  echo "Run_python st..."
  echo "-----------------------------------------------------------------------------------------"
  cd ${base_path}/python/ || exit 1
  run_python_log=./run_python_log.txt
  result_python_log=./result_python_log.txt
  local elapsed_time
  for cfg_file in ${cfg_file_list[*]}; do
    while read line; do
      line_info=${line}
      if [[ $line_info == \#* || $line_info == "" ]]; then
        continue
      fi
      model_info=`echo ${line_info} | awk -F ' ' '{print $1}'`
      model_name=`echo ${model_info} | awk -F ';' '{print $1}'`
      input_info=`echo ${model_info} | awk -F ';' '{print $2}'`
      input_shapes=`echo ${model_info} | awk -F ';' '{print $3}'`
      input_num=`echo ${input_info} | sed 's/:/;/' | awk -F ';' '{print $1}'`
      input_files=""
      data_path=${in_data_path}"/input_output/"
      if [[ ${input_num} == "" || ${input_num} == 1 ]]; then
        input_files=${data_path}'input/'${model_name}${bin_suffix}'.bin'
      else
        for i in $(seq 1 $input_num)
        do
          input_files=${input_files}${data_path}'input/'${model_name}${bin_suffix}'.bin_'$i','
        done
      fi
      model_file=${model_path}'/'${model_name}${suffix}
      elapsed_time=$(date +%s.%N)
      python test_inference_cloud.py ${model_file} ${input_files} ${input_shapes} ${target} >> ${run_python_log}
      Run_python_st_status=$?
      elapsed_time=$(printf %.2f "$(echo "$(date +%s.%N) - $elapsed_time" | bc)")
      if [[ ${Run_python_st_status} != 0 ]];then
        echo "RunPythonModel:     ${model_name} ${elapsed_time} failed" >> ${result_python_log}
        cat ${run_python_log}
        echo "Run_python_st_status failed"
      fi
      if [[ ${Run_python_st_status} != 0 ]]; then
        echo "-----------------------------------------------------------------------------------------"
        Print_Benchmark_Result ${result_python_log}
        echo "-----------------------------------------------------------------------------------------"
        exit 1
      fi
      echo "RunPythonModel: ${model_name} ${elapsed_time} pass" >> ${result_python_log}
    done < ${cfg_file}
  done
  echo "-----------------------------------------------------------------------------------------"

  elapsed_time=$(date +%s.%N)
  python test_inference_cloud_nocofig.py ${model_hiai_path} ${target} >> ${run_python_log}
  Run_python_st_status=$?
  elapsed_time=$(printf %.2f "$(echo "$(date +%s.%N) - $elapsed_time" | bc)")
  if [[ ${Run_python_st_status} != 0 ]];then
    echo "RunPython test_inference_cloud_nocofig ${elapsed_time} failed" >> ${result_python_log}
    cat ${run_python_log}
  else
    echo "RunPython test_inference_cloud_nocofig ${elapsed_time} pass" >> ${result_python_log}
  fi
  echo "-----------------------------------------------------------------------------------------"
  Print_Benchmark_Result ${result_python_log}
  echo "-----------------------------------------------------------------------------------------"

  if [[ ${Run_python_st_status} != 0 ]];then
    echo "Run_python_st_status failed"
    exit 1
  fi
}
