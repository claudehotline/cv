cmake_minimum_required(VERSION 3.16)
project(VideoAnalysisSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 编译选项
if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_WIN32_WINNT=0x0601)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# 查找依赖库
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 添加子项目
add_subdirectory(video-source-manager)
add_subdirectory(video-analyzer)

# 创建安装脚本
configure_file(
    "${PROJECT_SOURCE_DIR}/scripts/install_deps.sh.in"
    "${PROJECT_BINARY_DIR}/install_deps.sh"
    @ONLY
)

# 安装规则
install(TARGETS VideoSourceManager VideoAnalyzer
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY config/
    DESTINATION etc/video-analysis-system
)

# 包配置
include(CPack)
set(CPACK_PACKAGE_NAME "VideoAnalysisSystem")
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "基于机器视觉的视频分析软件系统")
set(CPACK_PACKAGE_VENDOR "VideoAnalysisTeam")

print_summary()