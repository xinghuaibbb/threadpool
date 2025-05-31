# 所有源文件
aux_source_directory(. SRC_LIST)

# 动态库文件
set(LIB_LIST threadpool.cc)

# 编译成动态库

add_library(threadpool SHARED ${LIB_LIST})

add_executable(thpoolmain ${SRC_LIST})