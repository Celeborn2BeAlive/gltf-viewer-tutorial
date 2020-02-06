cmake_minimum_required(VERSION 2.8)

cmake_policy(SET CMP0048 NEW)

project(gltf-viewer-tutorial VERSION 0.0.1)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(GLMLV_USE_BOOST_FILESYSTEM "Use boost for filesystem library instead of experimental std lib" OFF)

set(IMGUI_DIR imgui-1.74)
set(GLFW_DIR glfw-3.3.1)
set(GLM_DIR glm-0.9.9.7)
set(GLAD_DIR glad)
set(TINYGLTF_DIR tinygltf-bcf2ce586ee8bf2a2a816afa6bfe2f8692ba6ac2)
set(ARGS_DIR args-6.2.2)

# Add GLFW subdirectory, set some options to OFF by default (the user can still enable them by modifying its CMakeCache.txt)
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_TESTS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
add_subdirectory(third-party/${GLFW_DIR})

if(${CMAKE_VERSION} VERSION_LESS "3.10.0")
    set(OpenGL_GL_PREFERENCE LEGACY)
else()
    set(OpenGL_GL_PREFERENCE GLVND)
endif()
find_package(OpenGL REQUIRED)

if(GLMLV_USE_BOOST_FILESYSTEM)
    find_package(Boost COMPONENTS system filesystem REQUIRED)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(c2ba-glsl-shaders)
include(c2ba-copy-dll-post-build)
include(c2ba-assets-directory)

if(MSVC)
    set(SHADER_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin/\$\(Configuration\)/shaders)
    set(ASSET_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin/\$\(Configuration\)/assets)
else()
    set(SHADER_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin/shaders)
    set(ASSET_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin/assets)
endif()

set(
    IMGUI_SRC_FILES
    third-party/${IMGUI_DIR}/imgui.cpp
    third-party/${IMGUI_DIR}/imgui_demo.cpp
    third-party/${IMGUI_DIR}/imgui_draw.cpp
    third-party/${IMGUI_DIR}/imgui_widgets.cpp
    third-party/${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp
    third-party/${IMGUI_DIR}/examples/imgui_impl_glfw.cpp
)

set(
    THIRD_PARTY_SRC_FILES
    ${IMGUI_SRC_FILES}
    third-party/${GLAD_DIR}/src/glad.c
)

set(
    LIBRARIES
    ${OPENGL_LIBRARIES}
    glfw
)

set(CXXFLAGS ${CXXFLAGS} std=c++14)
if (GLMLV_USE_BOOST_FILESYSTEM)
    set(LIBRARIES ${LIBRARIES} ${Boost_SYSTEM_LIBRARY} ${Boost_FILESYSTEM_LIBRARY})
else()    
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CXXFLAGS ${CXXFLAGS} std=c++17)
        set(LIBRARIES ${LIBRARIES} stdc++fs)
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.0.0")
            set(USE_STD_FILESYSTEM 1)
        endif()
    
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "[Cc]lang")
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0.0")
            set(LIBRARIES ${LIBRARIES} stdc++fs)
        else()
            set(CXXFLAGS ${CXXFLAGS} std=c++17)
            set(USE_STD_FILESYSTEM 1)
        endif()
    endif()
endif()

source_group ("glsl" REGULAR_EXPRESSION ".*/*.glsl")
source_group ("third-party" REGULAR_EXPRESSION "third-party/*.*")

file(GLOB APP_DIRECTORIES "apps/*")
foreach(DIR ${APP_DIRECTORIES})
    get_filename_component(APP ${DIR} NAME)

    file(
        GLOB_RECURSE
        SRC_FILES
        apps/${APP}/*.cpp apps/${APP}/*.hpp apps/${APP}/*.glsl apps/${APP}/assets/*
    )
    
    add_executable(
        ${APP}
        ${SRC_FILES}
        ${THIRD_PARTY_SRC_FILES}
    )
    
    if (USE_STD_FILESYSTEM)
        target_compile_definitions(
            ${APP}
            PUBLIC
            USE_STD_FILESYSTEM
        )
    endif()
    if(GLMLV_USE_BOOST_FILESYSTEM)
        target_include_directories (
            ${APP}
            PUBLIC
            ${Boost_INCLUDE_DIRS}
        )
        target_compile_definitions(
            ${APP}
            PUBLIC
            GLMLV_USE_BOOST_FILESYSTEM
        )
    endif()

    target_include_directories(
        ${APP}
        PUBLIC
        ${OPENGL_INCLUDE_DIRS}
        third-party/${GLFW_DIR}/include
        third-party/${GLM_DIR}
        third-party/${GLAD_DIR}/include
        third-party/${IMGUI_DIR}
        third-party/${IMGUI_DIR}/examples/
        third-party/${TINYGLTF_DIR}/include
        third-party/${ARGS_DIR}
        lib/include
    )
    
    
    target_compile_definitions(
        ${APP}
        PUBLIC
        IMGUI_IMPL_OPENGL_LOADER_GLAD
        GLM_ENABLE_EXPERIMENTAL
    )
    
    if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
        set_property(TARGET ${APP} PROPERTY CXX_STANDARD 14)
    else()
        set_property(TARGET ${APP} PROPERTY CXX_STANDARD 17)
    endif()
        

    target_link_libraries(
        ${APP}
        ${LIBRARIES}
    )

    install(
        TARGETS ${APP}
        DESTINATION .
    )

    c2ba_add_shader_directory(${DIR}/shaders ${SHADER_OUTPUT_PATH}/${APP})
    c2ba_add_assets_directory(${DIR}/assets ${ASSET_OUTPUT_PATH}/${APP})

    install(
        DIRECTORY ${DIR}/shaders/
        DESTINATION shaders/${APP}
        FILES_MATCHING PATTERN "*.glsl"
    )

    if(EXISTS ${DIR}/assets/)
        install(
            DIRECTORY ${DIR}/assets/
            DESTINATION assets/${APP}
        )
    endif()
endforeach()
