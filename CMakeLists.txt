cmake_minimum_required(VERSION 3.11.0)

project(nslib)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(nslib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(NSLIB_VERSION_MAJOR 1)
set(NSLIB_VERSION_MINOR 0)
set(NSLIB_VERSION_PATCH 0)

set(NSLIB_TARGET_NAME noblesteed-${NSLIB_VERSION_MAJOR}.${NSLIB_VERSION_MINOR}.${NSLIB_VERSION_PATCH})
set(NSLIB_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(TESTS_DIR ${CMAKE_SOURCE_DIR}/tests)
set(DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)

if(${CMAKE_BUILD_TYPE} STREQUAL Release)
  set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build/release)
  message("Building release")
elseif(${CMAKE_BUILD_TYPE} STREQUAL RelWithDebInfo)
  set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build/relwdeb)
  message("Building release with debug info")
else()
  set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build/debug)
  message("Building debug")
endif()

find_package(Vulkan REQUIRED)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

# Add dependencies
add_subdirectory(${DEPS_DIR})

# Set the src files for the project
file(GLOB_RECURSE src_files
  ${NSLIB_SRC_DIR}/*.c*
)

# Create lib
add_library(${NSLIB_TARGET_NAME} SHARED ${src_files})
set_target_properties(${NSLIB_TARGET_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>
  ARCHIVE_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>
  LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>
  WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_compile_definitions(${NSLIB_TARGET_NAME} PUBLIC
  # USE_IMGUI
  # PROFILING_ENABLED
  NSLIB_VERSION_MAJOR=${NSLIB_VERSION_MAJOR}
  NSLIB_VERSION_MINOR=${NSLIB_VERSION_MINOR}
  NSLIB_VERSION_PATCH=${NSLIB_VERSION_PATCH})
target_include_directories(${NSLIB_TARGET_NAME} PRIVATE ${SDL_INCLUDE} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${NSLIB_TARGET_NAME} PRIVATE SDL3::SDL3 PUBLIC ${Vulkan_LIBRARIES})


if(WIN32)
  add_custom_target(link_target ALL
    COMMAND ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}/compile_commands.json)
  target_compile_definitions(${NSLIB_TARGET_NAME} PUBLIC
    NSLIB_ENABLE_SIMD=1
    _CRT_SECURE_NO_WARNINGS)
  # Must mark this as private or else all samples will be defining it which is problematic (unresolved external symbols)
  target_compile_definitions(${NSLIB_TARGET_NAME} PRIVATE
    DLL_EXPORTS)
  if(MSVC)
    message("MSVC build detected")
  elseif(MSYS)
    message("MSYS build detected")
  elseif(MINGW)
    message("MINGW build detected")
  else()
    message("Unknown windows build")
  endif()
else() # Unix
  message("Unix build detected")
  target_compile_options(${NSLIB_TARGET_NAME} PUBLIC -Wno-nullability-completeness)
  if(APPLE)
    message("Apple build detected")
  else()
    message("Linux build detected")
    target_compile_options(${NSLIB_TARGET_NAME} PUBLIC -msse4.1)
    target_compile_definitions(${NSLIB_TARGET_NAME} PUBLIC NSLIB_ENABLE_SIMD=1)
  endif()
endif()

# Add tests
add_subdirectory(${TESTS_DIR})

# Add rdev
add_subdirectory(${CMAKE_SOURCE_DIR}/rdev)


# target_link_libraries(${TARGET_NAME} lib1 lib2 ... libn)
