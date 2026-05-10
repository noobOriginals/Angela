cmake_minimum_required(VERSION 3.26.0)

if(APPLE)
    project(Angela VERSION 0.1.0 LANGUAGES C CXX OBJCXX)
else()
    project(Angela VERSION 0.1.0 LANGUAGES C CXX)
endif()

set(FullOutputDir "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${FullOutputDir}/static-libs")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${FullOutputDir}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${FullOutputDir}")

set(SrcPath "${CMAKE_SOURCE_DIR}/src/sources")
set(HeaderPath "${CMAKE_SOURCE_DIR}/src/include")
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${SrcPath}/*.cpp" "${SrcPath}/*.c")

if(APPLE)
    file(GLOB_RECURSE ObjcxxSources CONFIGURE_DEPENDS "${SrcPath}/*.mm")
    list(APPEND Sources ${ObjcxxSources})
endif()

include_directories(${HeaderPath} $ENV{OPENGL_PATH})

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} ${Sources})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_options(${PROJECT_NAME} PRIVATE -O3)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Metal"
        "-framework Foundation"
    )
endif()

