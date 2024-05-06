include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

if(CMAKE_SYSTEM_NAME MATCHES "Windows" AND ${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.17.0)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .dll ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

function(mindspore_add_submodule_obj des_submodule_objs sub_dir submodule_name_obj)

    add_subdirectory(${sub_dir})

    if(NOT TARGET ${submodule_name_obj})
        message(FATAL_ERROR "Can not find submodule '${submodule_name_obj}'. in ${CMAKE_CURRENT_LIST_FILE}")
    endif()
    if("$<TARGET_OBJECTS:${submodule_name_obj}>" IN_LIST ${des_submodule_objs})
        message(FATAL_ERROR "submodule '${submodule_name_obj}' added more than once. in ${CMAKE_CURRENT_LIST_FILE}")
    endif()

    set(${des_submodule_objs} ${${des_submodule_objs}} $<TARGET_OBJECTS:${submodule_name_obj}> PARENT_SCOPE)

endfunction()

if(DEFINED ENV{MSLIBS_CACHE_PATH})
    set(_MS_LIB_CACHE  $ENV{MSLIBS_CACHE_PATH})
else()
    set(_MS_LIB_CACHE ${CMAKE_BINARY_DIR}/.mslib)
endif()
message("MS LIBS CACHE PATH:  ${_MS_LIB_CACHE}")

if(NOT EXISTS ${_MS_LIB_CACHE})
    file(MAKE_DIRECTORY ${_MS_LIB_CACHE})
endif()

if(DEFINED ENV{MSLIBS_SERVER} AND NOT ENABLE_GITEE)
    set(LOCAL_LIBS_SERVER  $ENV{MSLIBS_SERVER})
    message("LOCAL_LIBS_SERVER:  ${LOCAL_LIBS_SERVER}")
endif()

include(ProcessorCount)
ProcessorCount(N)
if(JOBS)
    set(THNUM ${JOBS})
else()
    set(JOBS 8)
    if(${JOBS} GREATER ${N})
        set(THNUM ${N})
    else()
        set(THNUM ${JOBS})
    endif()
endif()
message("set make thread num: ${THNUM}")

if(LOCAL_LIBS_SERVER)
    if(NOT ENV{no_proxy})
        set(ENV{no_proxy} "${LOCAL_LIBS_SERVER}")
    else()
        string(FIND $ENV{no_proxy} ${LOCAL_LIBS_SERVER} IP_POS)
        if(${IP_POS} EQUAL -1)
            set(ENV{no_proxy} "$ENV{no_proxy},${LOCAL_LIBS_SERVER}")
        endif()
    endif()
endif()

function(__download_pkg pkg_name pkg_url pkg_sha256)

    if(LOCAL_LIBS_SERVER)
        set(REGEX_IP_ADDRESS "^([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)$")
        get_filename_component(_URL_FILE_NAME ${pkg_url} NAME)
        if(${LOCAL_LIBS_SERVER} MATCHES ${REGEX_IP_ADDRESS})
            set(pkg_url "http://${LOCAL_LIBS_SERVER}:8081/libs/${pkg_name}/${_URL_FILE_NAME}" ${pkg_url})
        else()
            set(pkg_url "https://${LOCAL_LIBS_SERVER}/libs/${pkg_name}/${_URL_FILE_NAME}" ${pkg_url})
        endif()
    endif()

    FetchContent_Declare(
            ${pkg_name}
            URL      ${pkg_url}
            URL_HASH SHA256=${pkg_sha256}
    )
    FetchContent_GetProperties(${pkg_name})
    message("download: ${${pkg_name}_SOURCE_DIR} , ${pkg_name} , ${pkg_url}")
    if(NOT ${pkg_name}_POPULATED)
        FetchContent_Populate(${pkg_name})
        set(${pkg_name}_SOURCE_DIR ${${pkg_name}_SOURCE_DIR} PARENT_SCOPE)
    endif()

endfunction()

function(__download_pkg_with_git pkg_name pkg_url pkg_git_commit pkg_sha256)

    if(LOCAL_LIBS_SERVER)
        set(pkg_url "http://${LOCAL_LIBS_SERVER}:8081/libs/${pkg_name}/${pkg_git_commit}")
        FetchContent_Declare(
                ${pkg_name}
                URL      ${pkg_url}
                URL_HASH SHA256=${pkg_sha256}
    )
    else()
    FetchContent_Declare(
            ${pkg_name}
        GIT_REPOSITORY      ${pkg_url}
        GIT_TAG             ${pkg_git_commit})
    endif()
    FetchContent_GetProperties(${pkg_name})
    message("download: ${${pkg_name}_SOURCE_DIR} , ${pkg_name} , ${pkg_url}")
    if(NOT ${pkg_name}_POPULATED)
        FetchContent_Populate(${pkg_name})
        set(${pkg_name}_SOURCE_DIR ${${pkg_name}_SOURCE_DIR} PARENT_SCOPE)
    endif()

endfunction()


function(__find_pkg_then_add_target pkg_name pkg_exe lib_path)
    set(options)
    set(oneValueArgs PATH)
    set(multiValueArgs SUFFIXES_PATH NAMES)
    cmake_parse_arguments(LIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    unset(${pkg_name}_LIBS)

    message("_FIND:${${pkg_name}_BASE_DIR}")

    if(pkg_exe)
        unset(${pkg_exe}_EXE CACHE)
        find_program(${pkg_exe}_EXE ${pkg_exe} PATHS ${${pkg_name}_BASE_DIR}/bin NO_DEFAULT_PATH)
        if(NOT ${pkg_exe}_EXE)
            return()
        endif()
        add_executable(${pkg_name}::${pkg_exe} IMPORTED GLOBAL)
        set_target_properties(${pkg_name}::${pkg_exe} PROPERTIES
                IMPORTED_LOCATION ${${pkg_exe}_EXE}
                )
        message("found ${${pkg_exe}_EXE}")
    endif()

    foreach(_LIB_NAME ${LIB_NAMES})
        set(_LIB_SEARCH_NAME ${_LIB_NAME})
        if(MSVC AND ${pkg_name}_Debug)
            set(_LIB_SEARCH_NAME ${_LIB_SEARCH_NAME}d)
        endif()
        set(_LIB_TYPE SHARED)
        if(${pkg_name}_USE_STATIC_LIBS)
            set(_LIB_SEARCH_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}${_LIB_SEARCH_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX}")
            set(_LIB_TYPE STATIC)
        endif()
        set(${_LIB_NAME}_LIB ${_LIB_NAME}_LIB-NOTFOUND)
        if(APPLE)
            find_library(${_LIB_NAME}_LIB ${_LIB_SEARCH_NAME} PATHS ${${pkg_name}_BASE_DIR}/${lib_path}
                    PATH_SUFFIXES ${LIB_SUFFIXES_PATH} NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
        else()
            find_library(${_LIB_NAME}_LIB ${_LIB_SEARCH_NAME} PATHS ${${pkg_name}_BASE_DIR}/${lib_path}
                    PATH_SUFFIXES ${LIB_SUFFIXES_PATH} NO_DEFAULT_PATH)
        endif()
        if(NOT ${_LIB_NAME}_LIB)
            message("not find ${_LIB_SEARCH_NAME} in path: ${${pkg_name}_BASE_DIR}/${lib_path}")
            return()
        endif()

        add_library(${pkg_name}::${_LIB_NAME} ${_LIB_TYPE} IMPORTED GLOBAL)
        if(WIN32 AND ${_LIB_TYPE} STREQUAL "SHARED")
            if(DEBUG_MODE)
                set_target_properties(${pkg_name}::${_LIB_NAME} PROPERTIES IMPORTED_IMPLIB_DEBUG ${${_LIB_NAME}_LIB})
            else()
                set_target_properties(${pkg_name}::${_LIB_NAME} PROPERTIES IMPORTED_IMPLIB_RELEASE ${${_LIB_NAME}_LIB})
            endif()
        else()
            set_target_properties(${pkg_name}::${_LIB_NAME} PROPERTIES IMPORTED_LOCATION ${${_LIB_NAME}_LIB})
        endif()

        if(EXISTS ${${pkg_name}_BASE_DIR}/include)
            set_target_properties(${pkg_name}::${_LIB_NAME} PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${${pkg_name}_BASE_DIR}/include")
        endif()

        list(APPEND ${pkg_name}_LIBS ${pkg_name}::${_LIB_NAME})
        message("found ${${_LIB_NAME}_LIB}")
        STRING(REGEX REPLACE "(.+)/(.+)" "\\1" LIBPATH ${${_LIB_NAME}_LIB})
        set(${pkg_name}_LIBPATH ${LIBPATH} CACHE STRING INTERNAL)
    endforeach()

    set(${pkg_name}_LIBS ${${pkg_name}_LIBS} PARENT_SCOPE)
endfunction()

function(__exec_cmd)
    set(options)
    set(oneValueArgs WORKING_DIRECTORY)
    set(multiValueArgs COMMAND)

    cmake_parse_arguments(EXEC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    execute_process(COMMAND ${EXEC_COMMAND}
            WORKING_DIRECTORY ${EXEC_WORKING_DIRECTORY}
            RESULT_VARIABLE RESULT)
    if(NOT RESULT EQUAL "0")
        message(FATAL_ERROR "error! when ${EXEC_COMMAND} in ${EXEC_WORKING_DIRECTORY}")
    endif()
endfunction()

function(__check_patches pkg_patches)
    # check patches
    if(PKG_PATCHES)
        file(TOUCH ${_MS_LIB_CACHE}/${pkg_name}_patch.sha256)
        file(READ ${_MS_LIB_CACHE}/${pkg_name}_patch.sha256 ${pkg_name}_PATCHES_SHA256)

        message("patches sha256:${${pkg_name}_PATCHES_SHA256}")

        set(${pkg_name}_PATCHES_NEW_SHA256)
        foreach(_PATCH ${PKG_PATCHES})
            file(SHA256 ${_PATCH} _PF_SHA256)
            set(${pkg_name}_PATCHES_NEW_SHA256 "${${pkg_name}_PATCHES_NEW_SHA256},${_PF_SHA256}")
        endforeach()

        if(NOT ${pkg_name}_PATCHES_SHA256 STREQUAL ${pkg_name}_PATCHES_NEW_SHA256)
            set(${pkg_name}_PATCHES ${PKG_PATCHES})
            file(REMOVE_RECURSE "${_MS_LIB_CACHE}/${pkg_name}-subbuild")
            file(WRITE ${_MS_LIB_CACHE}/${pkg_name}_patch.sha256 ${${pkg_name}_PATCHES_NEW_SHA256})
            message("patches changed : ${${pkg_name}_PATCHES_NEW_SHA256}")
        endif()
    endif()
endfunction()

set(MS_FIND_NO_DEFAULT_PATH NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH
                            NO_CMAKE_BUILDS_PATH NO_CMAKE_PACKAGE_REGISTRY NO_CMAKE_SYSTEM_PATH
                            NO_CMAKE_SYSTEM_PACKAGE_REGISTRY)
function(mindspore_add_pkg pkg_name)

    set(options)
    set(oneValueArgs URL SHA256 GIT_REPOSITORY GIT_TAG VER EXE DIR HEAD_ONLY CMAKE_PATH RELEASE
            LIB_PATH CUSTOM_CMAKE)
    set(multiValueArgs
            CMAKE_OPTION LIBS PRE_CONFIGURE_COMMAND CONFIGURE_COMMAND BUILD_OPTION INSTALL_INCS
            INSTALL_LIBS PATCHES SUBMODULES SOURCEMODULES ONLY_MAKE ONLY_MAKE_INCS ONLY_MAKE_LIBS
            LIB_SUFFIXES_PATH)
    cmake_parse_arguments(PKG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT PKG_LIB_PATH)
        set(PKG_LIB_PATH lib)
    endif()

    if(NOT PKG_EXE)
        set(PKG_EXE 0)
    endif()

    set(__FIND_PKG_NAME ${pkg_name})
    string(TOLOWER ${pkg_name} pkg_name)
    message("pkg name:${__FIND_PKG_NAME},${pkg_name}")

    set(${pkg_name}_PATCHES_HASH)
    foreach(_PATCH ${PKG_PATCHES})
        file(SHA256 ${_PATCH} _PF_SHA256)
        set(${pkg_name}_PATCHES_HASH "${${pkg_name}_PATCHES_HASH},${_PF_SHA256}")
    endforeach()

    # strip directory variables to ensure third party packages are installed in consistent locations
    string(REPLACE ${TOP_DIR} "" ARGN_STRIPPED ${ARGN})
    string(REPLACE ${_MS_LIB_CACHE} "" ARGN_STRIPPED ${ARGN_STRIPPED})
    # check options
    set(${pkg_name}_CONFIG_TXT
            "${CMAKE_CXX_COMPILER_VERSION}-${CMAKE_C_COMPILER_VERSION}
            ${ARGN_STRIPPED}-${${pkg_name}_USE_STATIC_LIBS}-${${pkg_name}_PATCHES_HASH}
            ${${pkg_name}_CXXFLAGS}-${${pkg_name}_CFLAGS}-${${pkg_name}_LDFLAGS}")
    if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(${pkg_name}_CONFIG_TXT "${${pkg_name}_CONFIG_TXT}--${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
    string(REPLACE ";" "-" ${pkg_name}_CONFIG_TXT ${${pkg_name}_CONFIG_TXT})
    string(SHA256 ${pkg_name}_CONFIG_HASH ${${pkg_name}_CONFIG_TXT})

    message("${pkg_name} config hash: ${${pkg_name}_CONFIG_HASH}")

    set(${pkg_name}_BASE_DIR ${_MS_LIB_CACHE}/${pkg_name}_${PKG_VER}_${${pkg_name}_CONFIG_HASH})
    set(${pkg_name}_DIRPATH ${${pkg_name}_BASE_DIR} CACHE STRING INTERNAL)

    if(EXISTS ${${pkg_name}_BASE_DIR}/options.txt AND PKG_HEAD_ONLY)
        set(${pkg_name}_INC ${${pkg_name}_BASE_DIR}/${PKG_HEAD_ONLY} PARENT_SCOPE)
        add_library(${pkg_name} INTERFACE)
        target_include_directories(${pkg_name} INTERFACE ${${pkg_name}_INC})
        if(${PKG_RELEASE})
            __find_pkg_then_add_target(${pkg_name} ${PKG_EXE} ${PKG_LIB_PATH}
                    SUFFIXES_PATH ${PKG_LIB_SUFFIXES_PATH}
                    NAMES ${PKG_LIBS})
        endif()
        return()
    endif()

    set(${__FIND_PKG_NAME}_ROOT ${${pkg_name}_BASE_DIR})
    set(${__FIND_PKG_NAME}_ROOT ${${pkg_name}_BASE_DIR} PARENT_SCOPE)

    if(PKG_LIBS)
        __find_pkg_then_add_target(${pkg_name} ${PKG_EXE} ${PKG_LIB_PATH}
                SUFFIXES_PATH ${PKG_LIB_SUFFIXES_PATH}
                NAMES ${PKG_LIBS})
        if(${pkg_name}_LIBS)
            set(${pkg_name}_INC ${${pkg_name}_BASE_DIR}/include PARENT_SCOPE)
            message("Found libs: ${${pkg_name}_LIBS}")
            return()
        endif()
    elseif(NOT PKG_HEAD_ONLY)
        find_package(${__FIND_PKG_NAME} ${PKG_VER} PATHS ${${pkg_name}_BASE_DIR} ${MS_FIND_NO_DEFAULT_PATH})
        if(${__FIND_PKG_NAME}_FOUND)
            set(${pkg_name}_INC ${${pkg_name}_BASE_DIR}/include PARENT_SCOPE)
            message("Found pkg: ${__FIND_PKG_NAME}")
            return()
        endif()
    endif()

    if(NOT PKG_DIR)
        if(PKG_GIT_REPOSITORY)
            __download_pkg_with_git(${pkg_name} ${PKG_GIT_REPOSITORY} ${PKG_GIT_TAG} ${PKG_SHA256})
        else()
            __download_pkg(${pkg_name} ${PKG_URL} ${PKG_SHA256})
        endif()
        foreach(_SUBMODULE_FILE ${PKG_SUBMODULES})
            STRING(REGEX REPLACE "(.+)_(.+)" "\\1" _SUBMODEPATH ${_SUBMODULE_FILE})
            STRING(REGEX REPLACE "(.+)/(.+)" "\\2" _SUBMODENAME ${_SUBMODEPATH})
            file(GLOB ${pkg_name}_INSTALL_SUBMODULE ${_SUBMODULE_FILE}/*)
            file(COPY ${${pkg_name}_INSTALL_SUBMODULE} DESTINATION ${${pkg_name}_SOURCE_DIR}/3rdparty/${_SUBMODENAME})
        endforeach()
    else()
        set(${pkg_name}_SOURCE_DIR ${PKG_DIR})
    endif()
    file(WRITE ${${pkg_name}_BASE_DIR}/options.txt ${${pkg_name}_CONFIG_TXT})
    message("${pkg_name}_SOURCE_DIR : ${${pkg_name}_SOURCE_DIR}")

    foreach(_PATCH_FILE ${PKG_PATCHES})
        get_filename_component(_PATCH_FILE_NAME ${_PATCH_FILE} NAME)
        set(_LF_PATCH_FILE ${CMAKE_BINARY_DIR}/_ms_patch/${_PATCH_FILE_NAME})
        configure_file(${_PATCH_FILE} ${_LF_PATCH_FILE} NEWLINE_STYLE LF @ONLY)

        message("patching ${${pkg_name}_SOURCE_DIR} -p1 < ${_LF_PATCH_FILE}")
        execute_process(COMMAND ${Patch_EXECUTABLE} -p1 INPUT_FILE ${_LF_PATCH_FILE}
                WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR}
                RESULT_VARIABLE Result)
        if(NOT Result EQUAL "0")
            message(FATAL_ERROR "Failed patch: ${_LF_PATCH_FILE}")
        endif()
    endforeach()
    foreach(_SOURCE_DIR ${PKG_SOURCEMODULES})
        file(GLOB ${pkg_name}_INSTALL_SOURCE ${${pkg_name}_SOURCE_DIR}/${_SOURCE_DIR}/*)
        file(COPY ${${pkg_name}_INSTALL_SOURCE} DESTINATION ${${pkg_name}_BASE_DIR}/${_SOURCE_DIR}/)
    endforeach()
    file(LOCK ${${pkg_name}_BASE_DIR} DIRECTORY GUARD FUNCTION RESULT_VARIABLE ${pkg_name}_LOCK_RET TIMEOUT 600)
    if(NOT ${pkg_name}_LOCK_RET EQUAL "0")
        message(FATAL_ERROR "error! when try lock ${${pkg_name}_BASE_DIR} : ${${pkg_name}_LOCK_RET}")
    endif()

    if(PKG_CUSTOM_CMAKE)
        file(GLOB ${pkg_name}_cmake ${PKG_CUSTOM_CMAKE}/CMakeLists.txt)
        file(COPY ${${pkg_name}_cmake} DESTINATION ${${pkg_name}_SOURCE_DIR})
    endif()

    if(${pkg_name}_SOURCE_DIR)
        if(PKG_HEAD_ONLY)
            file(GLOB ${pkg_name}_SOURCE_SUBDIRS ${${pkg_name}_SOURCE_DIR}/*)
            file(COPY ${${pkg_name}_SOURCE_SUBDIRS} DESTINATION ${${pkg_name}_BASE_DIR})
            set(${pkg_name}_INC ${${pkg_name}_BASE_DIR}/${PKG_HEAD_ONLY} PARENT_SCOPE)
            if(NOT PKG_RELEASE)
                add_library(${pkg_name} INTERFACE)
                target_include_directories(${pkg_name} INTERFACE ${${pkg_name}_INC})
            endif()

        elseif(PKG_ONLY_MAKE)
            __exec_cmd(COMMAND ${CMAKE_MAKE_PROGRAM} ${${pkg_name}_CXXFLAGS} -j${THNUM}
                    WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR})
            set(PKG_INSTALL_INCS ${PKG_ONLY_MAKE_INCS})
            set(PKG_INSTALL_LIBS ${PKG_ONLY_MAKE_LIBS})
            file(GLOB ${pkg_name}_INSTALL_INCS ${${pkg_name}_SOURCE_DIR}/${PKG_INSTALL_INCS})
            file(GLOB ${pkg_name}_INSTALL_LIBS ${${pkg_name}_SOURCE_DIR}/${PKG_INSTALL_LIBS})
            file(COPY ${${pkg_name}_INSTALL_INCS} DESTINATION ${${pkg_name}_BASE_DIR}/include)
            file(COPY ${${pkg_name}_INSTALL_LIBS} DESTINATION ${${pkg_name}_BASE_DIR}/lib)

        elseif(PKG_CMAKE_OPTION)
            # in cmake
            file(MAKE_DIRECTORY ${${pkg_name}_SOURCE_DIR}/_build)
            if(${pkg_name}_CFLAGS)
                set(${pkg_name}_CMAKE_CFLAGS "-DCMAKE_C_FLAGS=${${pkg_name}_CFLAGS}")
            endif()
            if(${pkg_name}_CXXFLAGS)
                set(${pkg_name}_CMAKE_CXXFLAGS "-DCMAKE_CXX_FLAGS=${${pkg_name}_CXXFLAGS}")
            endif()

            if(${pkg_name}_LDFLAGS)
                if(${pkg_name}_USE_STATIC_LIBS)
                    #set(${pkg_name}_CMAKE_LDFLAGS "-DCMAKE_STATIC_LINKER_FLAGS=${${pkg_name}_LDFLAGS}")
                else()
                    set(${pkg_name}_CMAKE_LDFLAGS "-DCMAKE_SHARED_LINKER_FLAGS=${${pkg_name}_LDFLAGS}")
                endif()
            endif()
            if(APPLE)
                __exec_cmd(COMMAND ${CMAKE_COMMAND} -DCMAKE_CXX_COMPILER_ARG1=${CMAKE_CXX_COMPILER_ARG1}
                        -DCMAKE_C_COMPILER_ARG1=${CMAKE_C_COMPILER_ARG1} ${PKG_CMAKE_OPTION}
                        ${${pkg_name}_CMAKE_CFLAGS} ${${pkg_name}_CMAKE_CXXFLAGS} ${${pkg_name}_CMAKE_LDFLAGS}
                        -DCMAKE_INSTALL_PREFIX=${${pkg_name}_BASE_DIR} ${${pkg_name}_SOURCE_DIR}/${PKG_CMAKE_PATH}
                        WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR}/_build)
                __exec_cmd(COMMAND ${CMAKE_COMMAND} --build . --target install --
                        WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR}/_build)
            else()
                __exec_cmd(COMMAND ${CMAKE_COMMAND} -DCMAKE_CXX_COMPILER_ARG1=${CMAKE_CXX_COMPILER_ARG1}
                        -DCMAKE_C_COMPILER_ARG1=${CMAKE_C_COMPILER_ARG1} ${PKG_CMAKE_OPTION} -G ${CMAKE_GENERATOR}
                    ${${pkg_name}_CMAKE_CFLAGS} ${${pkg_name}_CMAKE_CXXFLAGS} ${${pkg_name}_CMAKE_LDFLAGS}
                    -DCMAKE_INSTALL_PREFIX=${${pkg_name}_BASE_DIR} ${${pkg_name}_SOURCE_DIR}/${PKG_CMAKE_PATH}
                    WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR}/_build)
                if(MSVC)
                    set(CONFIG_TYPE Release)
                    if(DEBUG_MODE)
                        set(CONFIG_TYPE Debug)
                    endif()
                    __exec_cmd(COMMAND ${CMAKE_COMMAND} --build . --config ${CONFIG_TYPE} --target install --
                        WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR}/_build)
                else()
                    __exec_cmd(COMMAND ${CMAKE_COMMAND} --build . --target install -- -j${THNUM}
                        WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR}/_build)
                endif()
            endif()
        else()
            if(${pkg_name}_CFLAGS)
                set(${pkg_name}_MAKE_CFLAGS "CFLAGS=${${pkg_name}_CFLAGS}")
            endif()
            if(${pkg_name}_CXXFLAGS)
                set(${pkg_name}_MAKE_CXXFLAGS "CXXFLAGS=${${pkg_name}_CXXFLAGS}")
            endif()
            if(${pkg_name}_LDFLAGS)
                set(${pkg_name}_MAKE_LDFLAGS "LDFLAGS=${${pkg_name}_LDFLAGS}")
            endif()
            # in configure && make
            if(PKG_PRE_CONFIGURE_COMMAND)
                __exec_cmd(COMMAND ${PKG_PRE_CONFIGURE_COMMAND}
                        WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR})
            endif()

            if(PKG_CONFIGURE_COMMAND)
                __exec_cmd(COMMAND ${PKG_CONFIGURE_COMMAND}
                        ${${pkg_name}_MAKE_CFLAGS} ${${pkg_name}_MAKE_CXXFLAGS} ${${pkg_name}_MAKE_LDFLAGS}
                        --prefix=${${pkg_name}_BASE_DIR}
                        WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR})
            endif()
            set(${pkg_name}_BUILD_OPTION ${PKG_BUILD_OPTION})
            if(NOT PKG_CONFIGURE_COMMAND)
                set(${pkg_name}_BUILD_OPTION ${${pkg_name}_BUILD_OPTION}
                        ${${pkg_name}_MAKE_CFLAGS} ${${pkg_name}_MAKE_CXXFLAGS} ${${pkg_name}_MAKE_LDFLAGS})
            endif()
            # build
            if(APPLE)
                __exec_cmd(COMMAND ${CMAKE_MAKE_PROGRAM} ${${pkg_name}_BUILD_OPTION}
                        WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR})
            else()
                __exec_cmd(COMMAND ${CMAKE_MAKE_PROGRAM} ${${pkg_name}_BUILD_OPTION} -j${THNUM}
                        WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR})
            endif()

            if(PKG_INSTALL_INCS OR PKG_INSTALL_LIBS)
                file(GLOB ${pkg_name}_INSTALL_INCS ${${pkg_name}_SOURCE_DIR}/${PKG_INSTALL_INCS})
                file(GLOB ${pkg_name}_INSTALL_LIBS ${${pkg_name}_SOURCE_DIR}/${PKG_INSTALL_LIBS})
                file(COPY ${${pkg_name}_INSTALL_INCS} DESTINATION ${${pkg_name}_BASE_DIR}/include)
                file(COPY ${${pkg_name}_INSTALL_LIBS} DESTINATION ${${pkg_name}_BASE_DIR}/lib)
            else()
                __exec_cmd(COMMAND ${CMAKE_MAKE_PROGRAM} install WORKING_DIRECTORY ${${pkg_name}_SOURCE_DIR})
            endif()
        endif()
    endif()

    if(PKG_LIBS)
        __find_pkg_then_add_target(${pkg_name} ${PKG_EXE} ${PKG_LIB_PATH}
                SUFFIXES_PATH ${PKG_LIB_SUFFIXES_PATH}
                NAMES ${PKG_LIBS})
        set(${pkg_name}_INC ${${pkg_name}_BASE_DIR}/include PARENT_SCOPE)
        if(NOT ${pkg_name}_LIBS)
            message(FATAL_ERROR "Can not find pkg: ${pkg_name}")
        endif()
    else()
        find_package(${__FIND_PKG_NAME} ${PKG_VER} QUIET ${MS_FIND_NO_DEFAULT_PATH})
        if(${__FIND_PKG_NAME}_FOUND)
            set(${pkg_name}_INC ${${pkg_name}_BASE_DIR}/include PARENT_SCOPE)
            message("Found pkg: ${${__FIND_PKG_NAME}_LIBRARIES}")
            return()
        endif()
    endif()
endfunction()

function(src_separate_compile)
    set(options)
    set(oneValueArgs OBJECT_NAME OBJECT_SIZE)
    set(multiValueArgs SRC_LIST)
    cmake_parse_arguments(STUDENT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    list(LENGTH STUDENT_SRC_LIST SRC_LIST_SIZE)

    set(SEPARATE_SIZE 100)
    set(SEPARATE_INDEX 0)
    set(OBJECT_COUNT 0)
    math(EXPR SRC_LIST_MAX_INDEX "${SRC_LIST_SIZE} - 1")
    while(${SRC_LIST_SIZE} GREATER ${SEPARATE_INDEX})
        math(EXPR SEPARATE_END "${SEPARATE_INDEX} + ${SEPARATE_SIZE} - 1")
        if(${SEPARATE_END} GREATER ${SRC_LIST_MAX_INDEX})
            math(EXPR SEPARATE_SIZE "${SRC_LIST_SIZE} - ${SEPARATE_INDEX}")
        endif()
        list(SUBLIST STUDENT_SRC_LIST ${SEPARATE_INDEX} ${SEPARATE_SIZE} new_sub_list)
        math(EXPR OBJECT_COUNT "${OBJECT_COUNT} + 1")
        math(EXPR SEPARATE_INDEX "${SEPARATE_INDEX} + ${SEPARATE_SIZE}")
        add_library(${STUDENT_OBJECT_NAME}_${OBJECT_COUNT} OBJECT ${new_sub_list})
    endwhile()
    set(${STUDENT_OBJECT_SIZE} "${OBJECT_COUNT}" PARENT_SCOPE)
    message("${STUDENT_OBJECT_SIZE} object count is ${OBJECT_COUNT}")
endfunction()

function(enable_target_when_only_build_plugins target)
    if(ONLY_BUILD_DEVICE_PLUGINS)
        get_target_property(target_type ${target} TYPE)
        if(target_type STREQUAL "INTERFACE_LIBRARY")
            return()
        endif()
        set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL FALSE)
    endif()
endfunction()

function(disable_target_when_only_build_plugins target)
    if(ONLY_BUILD_DEVICE_PLUGINS)
        get_target_property(target_type ${target} TYPE)
        if(target_type STREQUAL "INTERFACE_LIBRARY")
            return()
        endif()
        get_property(is_set TARGET ${target} PROPERTY EXCLUDE_FROM_ALL)
        if(NOT DEFINED is_set)
            set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
        endif()
    endif()
endfunction()

function(enable_directory_when_only_build_plugins dir)
    get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target ${targets})
        enable_target_when_only_build_plugins(${target})
    endforeach()
    get_property(items DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(item ${items})
        enable_directory_when_only_build_plugins(${item})
    endforeach()
endfunction()

function(disable_directory_when_only_build_plugins dir)
    get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target ${targets})
        disable_target_when_only_build_plugins(${target})
    endforeach()
    get_property(items DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(item ${items})
        disable_directory_when_only_build_plugins(${item})
    endforeach()
endfunction()

function(add_subdirectory_with_faster_option dir)
    if(ONLY_BUILD_DEVICE_PLUGINS)
        add_subdirectory(${dir})
        disable_directory_when_only_build_plugins(${dir})
    else()
        add_subdirectory(${dir})
    endif()
endfunction()

function(find_and_use_mold)
    find_program(MOLD_LINKER mold)
    if(MOLD_LINKER)
        message(STATUS "using mold to speed linking libraries")
        get_filename_component(MOLD_LINKER_PATH ${MOLD_LINKER} DIRECTORY)
        file(GLOB MOLD_LINKER_PATH "${MOLD_LINKER_PATH}/../libexec/mold")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -B${MOLD_LINKER_PATH}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -B${MOLD_LINKER_PATH}")
    endif()
endfunction()