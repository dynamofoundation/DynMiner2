cmake_minimum_required(VERSION 3.1)

project(dyn_miner2)

find_package(CURL REQUIRED)
find_package(OpenCL REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory(ThirdParty/xxd)

file(GLOB SRCS src/*.cpp)
xxd_embed("opencl_kernel" "${CMAKE_CURRENT_SOURCE_DIR}/src/dyn_miner2.cl" SRCS)
add_executable(${PROJECT_NAME} ${SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE ${CURL_INCLUDE_DIR} ${OpenCL_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${CURL_LIBRARIES} ${OpenCL_LIBRARIES} Threads::Threads xxd::xxd)

