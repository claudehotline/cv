cmake_minimum_required(VERSION 3.16)
project(VideoSourceManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# 使用vcpkg工具链
set(CMAKE_TOOLCHAIN_FILE "D:/Projects/vcpkg/scripts/buildsystems/vcpkg.cmake")

# 手动配置OpenCV路径
set(OpenCV_DIR "H:/opencv/build")
set(OpenCV_INCLUDE_DIRS "H:/opencv/build/include")
set(OpenCV_LIBRARY_DIRS "H:/opencv/build/x64/vc16/lib")

# 查找依赖库
find_package(jsoncpp CONFIG REQUIRED)
file(GLOB OpenCV_LIBS "${OpenCV_LIBRARY_DIRS}/*.lib")

# 包含目录
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(include)

# 添加所有源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# 排除测试文件
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/test_main.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/simple_main.cpp")

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS} JsonCpp::JsonCpp)

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)