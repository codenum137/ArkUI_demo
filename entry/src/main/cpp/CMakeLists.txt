# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(AI_ffmpeg_demo)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

#因为此三方库中存在汇编编译的部分，所以需要修改CFLAGS参考如下，符号不可抢占且优先使用本地符号
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-int-conversion -Wl,-Bsymbolic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-int-conversion -Wl,-Bsymbolic")



if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()


include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

# Add source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

add_library(entry SHARED ${SOURCES})


#将三方库的头文件加入工程中
target_include_directories(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/FFmpeg/${OHOS_ARCH}/include)
#将三方库加入工程中
target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/FFmpeg/${OHOS_ARCH}/lib/libavcodec.a)
target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/FFmpeg/${OHOS_ARCH}/lib/libavdevice.a)
target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/FFmpeg/${OHOS_ARCH}/lib/libavfilter.a)
target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/FFmpeg/${OHOS_ARCH}/lib/libavformat.a)
target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/FFmpeg/${OHOS_ARCH}/lib/libavutil.a)
target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/FFmpeg/${OHOS_ARCH}/lib/libswresample.a)
target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/FFmpeg/${OHOS_ARCH}/lib/libswscale.a)
target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/zlib/${OHOS_ARCH}/lib/libz.a)

target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/openssl_1_0_2u/${OHOS_ARCH}/lib/libcrypto.a)
target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/openssl_1_0_2u/${OHOS_ARCH}/lib/libssl.a)
target_link_libraries(entry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rtmpdump/${OHOS_ARCH}/lib/librtmp.a)

target_link_libraries(entry PUBLIC libace_napi.z.so)
target_link_libraries(entry PRIVATE hilog_ndk.z)
