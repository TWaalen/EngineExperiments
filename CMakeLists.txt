cmake_minimum_required (VERSION 3.8)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/meta/cmake")

project (VulkanTutorial CXX)

add_executable (VulkanTutorial "main.cpp"
                               "datastructures/fixed_vector.h"
                               "datastructures/optional.h"
                               "datastructures/vector.h"
                               "engine/backend/vulkan/formatters.h"
                               "engine/backend/vulkan/renderer.cpp"
                               "engine/backend/vulkan/renderer.h"
                               "engine/utils.h"
                               "engine/window.h"
                               "io/file.cpp"
                               "io/file.h"
                               "math/math.h")
set_target_properties(VulkanTutorial PROPERTIES CXX_STANDARD 23)
target_include_directories(VulkanTutorial PUBLIC ".")
target_include_directories(VulkanTutorial SYSTEM PUBLIC "dependencies/")

find_package(Vulkan REQUIRED)
target_include_directories(VulkanTutorial SYSTEM PUBLIC ${Vulkan_INCLUDE_DIRS})
include(vulkan_utils)

compile_shader(VulkanTutorial SOURCES "shaders/triangle.vert.glsl"
                                      "shaders/triangle.frag.glsl")

if (WIN32)
    target_sources(VulkanTutorial PRIVATE "engine/platform/windows/backend/vulkan/renderer.cpp"
                                          "engine/platform/windows/utils.cpp"
                                          "engine/platform/windows/utils.h"
                                          "engine/platform/windows/window.cpp"
                                          "engine/platform/windows/window.h")
    target_compile_definitions(VulkanTutorial PRIVATE _CRT_SECURE_NO_WARNINGS
                                                      NO
                                                      NOATOM
                                                      NOCLIPBOARD
                                                      NOCOLOR
                                                      NOCOMM
                                                      NOCTLMGR
                                                      NODEFERWINDOWPOS
                                                      NODRAWTEXT
                                                      NOGDI
                                                      NOHELP
                                                      NOICONS
                                                      NOIMAGE
                                                      NOIME
                                                      NOGDICAPMASKS
                                                      NOKANJI
                                                      NOKEYSTATES
                                                      NOMB
                                                      NOMCX
                                                      NOMEMMGR
                                                      NOMENUS
                                                      NOMETAFILE
                                                      NOMINMAX
                                                      NOOPENFILE
                                                      NOPROFILER
                                                      NOPROXYSTUB
                                                      NORASTEROPS
                                                      NOSCROLL
                                                      NOSERVICE
                                                      NOSOUND
                                                      NOSYSCOMMANDS
                                                      NOSYSMETRICS
                                                      NOSYSPARAMSINFO
                                                      NOTAPE
                                                      NOTEXTMETRIC
                                                      NOVIRTUALKEYCODES
                                                      NOWH
                                                      NOWINABLE
                                                      NOWINRES
                                                      OEMRESOURCE
                                                      UNICODE
                                                      VK_USE_PLATFORM_WIN32_KHR
                                                      WIN32_EXTRA_LEAN
                                                      WIN32_LEAN_AND_MEAN)
    target_link_options(VulkanTutorial PUBLIC /subsystem:windows /entry:mainCRTStartup)
endif()

if (MSVC)
    set_target_properties(VulkanTutorial PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()
