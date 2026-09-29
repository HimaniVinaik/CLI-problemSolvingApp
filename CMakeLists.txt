cmake_minimum_required(VERSION 3.14)
project(leet VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

add_executable(leet
  src/main.cpp
  src/app/app.cpp
  src/app/commands.cpp
  src/app/dev.cpp
  src/app/lineedit.cpp
  src/app/shell.cpp
  src/app/views.cpp
  src/core/config.cpp
  src/core/problem.cpp
  src/core/repository.cpp
  src/core/workspace.cpp
  src/judge/builder.cpp
  src/judge/complexity.cpp
  src/judge/judge.cpp
  src/judge/process.cpp
  src/ui/render.cpp
  src/ui/term.cpp
  src/util/fs.cpp
  src/util/strings.cpp
)
target_include_directories(leet PRIVATE src)
target_compile_options(leet PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(leet PRIVATE Threads::Threads)

# Where the problem set and judge runtime are found at run time.
set(LEET_DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/leet")
target_compile_definitions(leet PRIVATE
  LEET_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  LEET_INSTALL_DATA_DIR="${LEET_DATA_INSTALL_DIR}")

install(TARGETS leet RUNTIME DESTINATION bin)
install(DIRECTORY problems runtime DESTINATION share/leet)
