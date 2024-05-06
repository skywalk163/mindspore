if(ENABLE_GITEE OR ENABLE_GITEE_EULER) # Channel GITEE_EULER is NOT supported now, use GITEE instead.
  set(REQ_URL "https://gitee.com/mirrors/openssl/repository/archive/OpenSSL_1_1_1k.tar.gz")
  set(SHA256 "b92f9d3d12043c02860e5e602e50a73ed21a69947bcc74d391f41148e9f6aa95")
else()
  set(REQ_URL "https://github.com/openssl/openssl/archive/refs/tags/OpenSSL_1_1_1k.tar.gz")
  set(SHA256 "b92f9d3d12043c02860e5e602e50a73ed21a69947bcc74d391f41148e9f6aa95")
endif()

if(BUILD_LITE)
  set(OPENSSL_PATCH_ROOT ${TOP_DIR}/third_party/patch/openssl)
else()
  set(OPENSSL_PATCH_ROOT ${CMAKE_SOURCE_DIR}/third_party/patch/openssl)
endif()

if(BUILD_LITE)
    if(PLATFORM_ARM64 AND ANDROID_NDK_TOOLCHAIN_INCLUDED)
        set(openssl_USE_STATIC_LIBS OFF)
        set(ANDROID_NDK_ROOT $ENV{ANDROID_NDK})
        set(PATH
            ${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin:
            ${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin:
            $ENV{PATH})
        mindspore_add_pkg(openssl
                VER 1.1.1k
                LIBS ssl crypto
                URL ${REQ_URL}
                SHA256 ${SHA256}
                CONFIGURE_COMMAND ./Configure android-arm64 -D__ANDROID_API__=29 no-zlib no-afalgeng
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-3711.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-3712.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-4160.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-0778.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-1292.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-2068.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-2097.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-4304.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-4450.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0215.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0286.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0464.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0465.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0466.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-2650.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-3446.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-3817.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-4807.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-5678.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2024-0727.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2024-2511.patch
                )
    elseif(PLATFORM_ARM32 AND ANDROID_NDK_TOOLCHAIN_INCLUDED)
        set(openssl_USE_STATIC_LIBS OFF)
        set(ANDROID_NDK_ROOT $ENV{ANDROID_NDK})
        set(PATH
            ${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin:
            ${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin:
            $ENV{PATH})
        mindspore_add_pkg(openssl
                VER 1.1.1k
                LIBS ssl crypto
                URL ${REQ_URL}
                SHA256 ${SHA256}
                CONFIGURE_COMMAND ./Configure android-arm -D__ANDROID_API__=19 no-zlib no-afalgeng
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-3711.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-3712.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-4160.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-0778.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-1292.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-2068.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-2097.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-4304.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-4450.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0215.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0286.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0464.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0465.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0466.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-2650.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-3446.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-3817.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-4807.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-5678.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2024-0727.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2024-2511.patch
                )
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR APPLE)
        set(openssl_CFLAGS -fvisibility=hidden)
        mindspore_add_pkg(openssl
                VER 1.1.1k
                LIBS ssl crypto
                URL ${REQ_URL}
                SHA256 ${SHA256}
                CONFIGURE_COMMAND ./config no-zlib no-shared no-afalgeng
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-3711.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-3712.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-4160.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-0778.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-1292.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-2068.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-2097.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-4304.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-4450.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0215.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0286.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0464.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0465.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0466.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-2650.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-3446.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-3817.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-4807.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-5678.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2024-0727.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2024-2511.patch
                )
    else()
        MESSAGE(FATAL_ERROR "openssl does not support compilation for the current environment.")
    endif()
    include_directories(${openssl_INC})
    add_library(mindspore::ssl ALIAS openssl::ssl)
    add_library(mindspore::crypto ALIAS openssl::crypto)
else()
    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR APPLE)
        set(openssl_CFLAGS -fvisibility=hidden)
        mindspore_add_pkg(openssl
                VER 1.1.1k
                LIBS ssl crypto
                URL ${REQ_URL}
                SHA256 ${SHA256}
                CONFIGURE_COMMAND ./config no-zlib no-shared no-afalgeng
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-3711.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-3712.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2021-4160.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-0778.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-1292.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-2068.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-2097.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-4304.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2022-4450.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0215.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0286.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0464.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0465.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-0466.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-2650.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-3446.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-3817.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-4807.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2023-5678.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2024-0727.patch
                PATCHES ${OPENSSL_PATCH_ROOT}/CVE-2024-2511.patch
                )
        include_directories(${openssl_INC})
        add_library(mindspore::ssl ALIAS openssl::ssl)
        add_library(mindspore::crypto ALIAS openssl::crypto)
    endif()
endif()
