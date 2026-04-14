cmake_minimum_required(VERSION 3.10)
project(ODB_Parser VERSION 1.0.0 LANGUAGES CXX)

# 导出编译命令给 IntelliSense 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 可执行文件
add_executable(odb_parser
    main.cpp
    # 头文件会自动包含，无需在此列出
)

# 如果使用 GCC 或 Clang，可能需要链接 filesystem 库
# C++17 的 <filesystem> 在某些编译器中需要显式链接
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 8 及以上需要链接 stdc++fs
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
        target_link_libraries(odb_parser PUBLIC stdc++fs)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang 可能需要链接 c++fs
    target_link_libraries(odb_parser PUBLIC c++fs)
elseif(MSVC)
    # MSVC 不需要额外链接
endif()

# 可选：启用更多警告
if(MSVC)
    target_compile_options(odb_parser PRIVATE /W4 /WX)
else()
    target_compile_options(odb_parser PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# 安装目标（可选）
install(TARGETS odb_parser DESTINATION bin)