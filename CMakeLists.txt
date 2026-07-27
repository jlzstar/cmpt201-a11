cmake_minimum_required(VERSION 3.14)

project(a11)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(THREADS_PREFER_THREAD_FLAG ON)
find_package(Threads REQUIRED)


include_directories(include)

add_executable(server src/server.c src/helper.c)
add_executable(client src/client.c src/helper.c)

target_link_libraries(server PRIVATE Threads::Threads)
target_link_libraries(client PRIVATE Threads::Threads)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# address sanitizer
if(ENABLE_ASAN)
  target_compile_options(server PRIVATE -fsanitize=address -g)
  target_link_options(server PRIVATE -fsanitize=address)

  target_compile_options(client PRIVATE -fsanitize=address -g)
  target_link_options(client PRIVATE -fsanitize=address)
endif()

# thread sanitizer
if(ENABLE_TSAN)
  target_compile_options(server PRIVATE -fsanitize=thread -g)
  target_link_options(server PRIVATE -fsanitize=thread)

  target_compile_options(client PRIVATE -fsanitize=thread -g)
  target_link_options(client PRIVATE -fsanitize=thread)
endif()

