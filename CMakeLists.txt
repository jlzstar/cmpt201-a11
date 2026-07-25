cmake_minium_required(VERSION 3.14)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(THREADS_PREFER_THREAD_FLAG ON)
find_package(Threads REQUIRED)

project(a11)

include_directories(include)

add_executable(server src/server.c)
add_executable(client src/client.c)


# address sanitizer
target_compile_options(server PRIVATE -fsanitize=address -g)
target_link_options(server PRIVATE -fsanitize=address)

target_compile_options(client PRIVATE -fsanitize=address -g)
target_link_options(client PRIVATE -fsanitize=address)


# thread sanitizer
target_compile_options(server PRIVATE -fsanitize=thread -g)
target_link_options(server PRIVATE -fsanitize=thread)

target_compile_options(client PRIVATE -fsanitize=thread -g)
target_link_options(client PRIVATE -fsanitize=thread)


