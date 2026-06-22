file(GLOB src_files
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(rdev ${src_files})

set_target_properties(rdev PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib/rdev>
  RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin/rdev>)

target_include_directories(rdev PRIVATE ${NSLIB_SRC_DIR})
target_link_libraries(rdev PUBLIC ${NSLIB_TARGET_NAME})

add_compile_shaders_target(src/shaders bin/rdev/data/shaders)
add_copy_data_dir_command(rdev data bin/rdev/data)
add_copy_data_dir_command(rdev import bin/rdev/import)
