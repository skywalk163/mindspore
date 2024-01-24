# path variables for graphengine submodule, it has to be included after mindspore/core
# and minspore/ccsrc to prevent conflict of op headers

if(ENABLE_TESTCASES OR ENABLE_CPP_ST OR ENABLE_C_ST)
    message("Note: compile ut & st with include file to mock: ${GRAPHENGINE_PATH}/third_party/fwkacllib/inc/runtime")
    include_directories(${GRAPHENGINE_PATH}/third_party/fwkacllib/inc)
    include_directories(${GRAPHENGINE_PATH}/third_party/fwkacllib/inc/runtime)
    include_directories(${GRAPHENGINE_PATH}/third_party/fwkacllib/inc/aoe)
    include_directories(${GRAPHENGINE_PATH}/metadef/inc)
    include_directories(${GRAPHENGINE_PATH}/metadef/inc/external)
    include_directories(${GRAPHENGINE_PATH}/metadef/inc/external/platform/)
    include_directories(${GRAPHENGINE_PATH}/inc/)
    include_directories(${GRAPHENGINE_PATH}/inc/external/)
    include_directories(${GRAPHENGINE_PATH}/inc/external/dvpp)
    include_directories(${GRAPHENGINE_PATH}/inc/run/aicpu/aicpu_kernel/inc)
    include_directories(${GRAPHENGINE_PATH}/base/)
elseif(ENABLE_D OR ENABLE_ACL)
    message("Note: compile cpp with include file: ${ASCEND_PATH}/latest/include/")
    include_directories(${ASCEND_PATH}/latest/include/)
    include_directories(${ASCEND_PATH}/latest/include/aoe)
    include_directories(${ASCEND_PATH}/latest/include/exe_graph)
    include_directories(${ASCEND_PATH}/latest/opp/built-in/)
    include_directories(${ASCEND_PATH}/latest/opp/built-in/op_impl/aicpu/aicpu_kernel/inc/)
    include_directories(${ASCEND_PATH}/ascend-toolkit/latest/include/)
    include_directories(${ASCEND_PATH}/ascend-toolkit/latest/include/aoe)
    include_directories(${ASCEND_PATH}/ascend-toolkit/latest/include/exe_graph)
    include_directories(${ASCEND_PATH}/ascend-toolkit/latest/opp/built-in/)
    include_directories(${ASCEND_PATH}/ascend-toolkit/latest/opp/built-in/op_impl/aicpu/aicpu_kernel/inc/)

    include_directories(${GRAPHENGINE_PATH}/third_party/fwkacllib/inc)
    include_directories(${GRAPHENGINE_PATH}/third_party/fwkacllib/)
    include_directories(${GRAPHENGINE_PATH}/metadef/inc)
    include_directories(${GRAPHENGINE_PATH}/metadef/inc/external)
    include_directories(${GRAPHENGINE_PATH}/inc/)
    include_directories(${GRAPHENGINE_PATH}/inc/framework)
    include_directories(${GRAPHENGINE_PATH}/base/)
endif()