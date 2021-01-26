cmake_minimum_required(VERSION 3.0.0)
project(RaytracingTest VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20)

set(SOURCES_DIR ${PROJECT_SOURCE_DIR}/src)

include_directories(RaytracingTest ${SOURCES_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/vendor)
link_directories(${PROJECT_SOURCE_DIR}/lib)

#find_package(glfw3 REQUIRED)
#find_package(Vulkan REQUIRED)

set(SOURCE_FILES
  ${SOURCES_DIR}/main.cpp
  ${SOURCES_DIR}/Context.cpp
  ${SOURCES_DIR}/Buffer.cpp
  ${SOURCES_DIR}/Image.cpp
  ${SOURCES_DIR}/Descriptors.cpp
  ${SOURCES_DIR}/CmdUtils.cpp
  ${SOURCES_DIR}/FileIncluder.cpp
  ${SOURCES_DIR}/Blas.cpp
  ${SOURCES_DIR}/Camera.cpp
  ${SOURCES_DIR}/RtProgram.cpp
  ${SOURCES_DIR}/Scene.cpp
  )

add_executable(RaytracingTest ${SOURCE_FILES}
  ${PROJECT_SOURCE_DIR}/include/vendor/imgui.h
  ${SOURCES_DIR}/imgui.cpp
  ${SOURCES_DIR}/imgui_draw.cpp
  ${SOURCES_DIR}/imgui_widgets.cpp
  ${SOURCES_DIR}/imgui_impl_glfw.cpp
  ${SOURCES_DIR}/imgui_impl_vulkan.cpp
  ${SOURCES_DIR}/imgui_demo.cpp
  )

#set_property(TARGET RaytracingTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "C:/Users/varun/programming/RaytracingTest")
target_link_libraries(RaytracingTest ${CMAKE_BUILD_TYPE}/glfw3 ${CMAKE_BUILD_TYPE}/vulkan-1 ${CMAKE_BUILD_TYPE}/shaderc_combined ${CMAKE_BUILD_TYPE}/spdlogd gdi32 user32 kernel32 ${CMAKE_DL_LIBS})
