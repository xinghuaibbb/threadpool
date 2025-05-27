# cmake
cmake_minimum_required(VERSION 3.10)
project(threadpool)

# 配置编译选项
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)
# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 14)
# 配置最终的可执行文件输出的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 配置 头文件 搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 加载子目录  src   既然进去, 就有 CMakeLists.txt
add_subdirectory(src)
 