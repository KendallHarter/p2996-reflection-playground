cmake_minimum_required(VERSION 3.28)

project(p2996_playground)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

# Just assume Clang
add_compile_options(-stdlib=libc++ -freflection-latest)
add_link_options(-stdlib=libc++)

add_executable(commands src/commands.cpp)
add_executable(named_params src/named_params.cpp)
add_executable(runtime_name_setter src/runtime_name_setter.cpp)
add_executable(named_params2 src/named_params2.cpp)
add_executable(named_construct src/named_construct.cpp)
add_executable(json_struct src/json_struct.cpp)
add_executable(json_schema src/json_schema.cpp)
add_executable(json_schema2 src/json_schema2/main.cpp)
add_executable(module_test src/module_test_main.cpp)
add_executable(json_schema3 src/json_schema3.cpp)
add_executable(etc src/etc.cpp)

target_sources(
   module_test PUBLIC
   FILE_SET
      modules
   TYPE
      CXX_MODULES
   FILES
      src/module_test.cppm)
