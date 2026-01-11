function(create_test TARGET_NAME)
  file(GLOB src_files
    ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}/src/*.cpp)

  add_executable(${TARGET_NAME} ${src_files})

  set_target_properties(${TARGET_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib/tests>
    RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin/tests>)

  target_include_directories(${TARGET_NAME} PRIVATE ${NSLIB_SRC_DIR})
  target_link_libraries(${TARGET_NAME} PUBLIC ${NSLIB_TARGET_NAME})
endfunction()

function(add_sample_compile_shaders_target TARGET_NAME)
  add_compile_shaders_target(${TARGET_NAME}/src/shaders bin/tests/data/shaders)
endfunction()

function(add_copy_data_command TARGET_NAME)
  add_copy_data_dir_command(${TARGET_NAME} ${TARGET_NAME}/data bin/tests/data)
  add_copy_data_dir_command(${TARGET_NAME} ${TARGET_NAME}/import bin/tests/import)
endfunction()

create_test(basic_window)
create_test(input_keymaps)
create_test(containers)
create_test(pack_unpack)
