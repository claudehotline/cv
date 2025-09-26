cmake_minimum_required(VERSION 3.16)
project(VideoAnalyzer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# 使用vcpkg工具链
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "D:/Projects/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

# 手动配置OpenCV路径
set(OpenCV_DIR "H:/opencv/build")
set(OpenCV_INCLUDE_DIRS "H:/opencv/build/include")
set(OpenCV_LIBRARY_DIRS "H:/opencv/build/x64/vc16/lib")

# 查找依赖库
file(GLOB OpenCV_LIBS "${OpenCV_LIBRARY_DIRS}/*.lib")
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
# 设置libdatachannel路径
set(LibDataChannel_DIR "D:/Projects/vcpkg/installed/x64-windows/share/libdatachannel")
find_package(LibDataChannel CONFIG REQUIRED)

# 尝试通过vcpkg查找jsoncpp
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(jsoncpp QUIET jsoncpp)
endif()

# 如果vcpkg找不到，尝试CONFIG模式
if(NOT jsoncpp_FOUND)
    find_package(jsoncpp CONFIG QUIET)
    if(jsoncpp_FOUND)
        set(JSONCPP_LIBRARIES JsonCpp::JsonCpp)
    endif()
endif()

# 查找ixwebsocket库
find_package(ixwebsocket CONFIG REQUIRED)

# 查找FFmpeg库 (H.264编码器需要) - 使用vcpkg
find_package(unofficial-ffmpeg CONFIG)
if(unofficial-ffmpeg_FOUND)
    set(FFMPEG_LIBRARIES
        unofficial::ffmpeg::avcodec
        unofficial::ffmpeg::avformat
        unofficial::ffmpeg::avutil
        unofficial::ffmpeg::swscale
    )
    set(FFMPEG_FOUND TRUE)
else()
    # 尝试系统默认路径查找
    find_library(AVCODEC_LIBRARY avcodec)
    find_library(AVFORMAT_LIBRARY avformat)
    find_library(AVUTIL_LIBRARY avutil)
    find_library(SWSCALE_LIBRARY swscale)

    if(AVCODEC_LIBRARY AND AVFORMAT_LIBRARY AND AVUTIL_LIBRARY AND SWSCALE_LIBRARY)
        set(FFMPEG_LIBRARIES ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY})
        set(FFMPEG_FOUND TRUE)
    else()
        set(FFMPEG_FOUND FALSE)
        message(WARNING "FFmpeg not found - WebRTC H.264 encoding will be disabled")
    endif()
endif()

# JSON库路径（SignalingServer需要）
find_path(JSONCPP_INCLUDE_DIR
    NAMES json/json.h
    PATHS /usr/local/include
          /usr/include)

find_library(JSONCPP_LIBRARY
    NAMES jsoncpp
    PATHS /usr/local/lib
          /usr/lib)

# 尝试查找ONNX Runtime
find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS /usr/local/include/onnxruntime
          /usr/include/onnxruntime
          ${ONNXRUNTIME_ROOT}/include)

find_library(ONNXRUNTIME_LIB
    NAMES onnxruntime
    PATHS /usr/local/lib
          /usr/lib
          ${ONNXRUNTIME_ROOT}/lib)

# 包含目录
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(include)

# ixwebsocket is included automatically

if(JSONCPP_INCLUDE_DIR)
    include_directories(${JSONCPP_INCLUDE_DIR})
endif()

if(DATACHANNEL_FOUND)
    include_directories(${DATACHANNEL_INCLUDE_DIRS})
endif()

if(ONNXRUNTIME_INCLUDE_DIR)
    include_directories(${ONNXRUNTIME_INCLUDE_DIR})
    add_definitions(-DUSE_ONNXRUNTIME)
endif()

# 添加源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
    Threads::Threads
    Boost::system
)

# 链接Windows系统库
if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        bcrypt
        ws2_32
        winmm)
endif()

# 链接libdatachannel和ixwebsocket
target_link_libraries(${PROJECT_NAME}
    LibDataChannel::LibDataChannel
    ixwebsocket::ixwebsocket)

# 链接FFmpeg库 (如果找到)
if(FFMPEG_FOUND)
    target_link_libraries(${PROJECT_NAME} ${FFMPEG_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_FFMPEG)
endif()

# 链接jsoncpp
if(jsoncpp_FOUND AND JSONCPP_LIBRARIES)
    target_link_libraries(${PROJECT_NAME} ${JSONCPP_LIBRARIES})
elseif(TARGET JsonCpp::JsonCpp)
    target_link_libraries(${PROJECT_NAME} JsonCpp::JsonCpp)
endif()

if(ONNXRUNTIME_LIB)
    target_link_libraries(${PROJECT_NAME} ${ONNXRUNTIME_LIB})
endif()

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")