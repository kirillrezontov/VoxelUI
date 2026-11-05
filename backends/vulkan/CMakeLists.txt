# CPU geometry/cache stage remains independent of Vulkan headers and loader.
add_library(voxelui_vulkan_geometry STATIC geometry.cpp)
target_link_libraries(voxelui_vulkan_geometry PUBLIC voxelui)
target_include_directories(voxelui_vulkan_geometry PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include/voxelui/vulkan>)
if(VOXELUI_TESTS)
  add_executable(voxelui-vulkan-geometry-tests geometry_tests.cpp)
  target_link_libraries(voxelui-vulkan-geometry-tests PRIVATE voxelui_vulkan_geometry voxelui_software)
  add_test(NAME voxelui-vulkan-geometry-tests COMMAND voxelui-vulkan-geometry-tests)
endif()

if(VOXELUI_VULKAN)
  find_package(Vulkan 1.3 REQUIRED COMPONENTS glslc)
  foreach(stage vert frag)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/solid.${stage}.inc
      COMMAND Vulkan::glslc --target-env=vulkan1.1 -mfmt=c
        ${CMAKE_CURRENT_SOURCE_DIR}/solid.${stage}
        -o ${CMAKE_CURRENT_BINARY_DIR}/solid.${stage}.inc
      DEPENDS solid.${stage} VERBATIM)
  endforeach()
  add_library(voxelui_vulkan STATIC renderer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/solid.vert.inc ${CMAKE_CURRENT_BINARY_DIR}/solid.frag.inc)
  target_link_libraries(voxelui_vulkan PUBLIC voxelui_vulkan_geometry Vulkan::Vulkan)
  target_include_directories(voxelui_vulkan PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()
if(VOXELUI_VULKAN AND VOXELUI_TESTS)
  add_executable(voxelui-vulkan-gpu-tests gpu_tests.cpp)
  target_compile_definitions(voxelui-vulkan-gpu-tests PRIVATE VOXELUI_TEST_ASSETS="${PROJECT_SOURCE_DIR}/examples/assets")
  target_link_libraries(voxelui-vulkan-gpu-tests PRIVATE voxelui_vulkan voxelui_software)
  add_test(NAME voxelui-vulkan-gpu-tests COMMAND voxelui-vulkan-gpu-tests)
  set_tests_properties(voxelui-vulkan-gpu-tests PROPERTIES LABELS graphics TIMEOUT 60)
endif()

if(VOXELUI_VULKAN)
  add_library(VoxelUI::vulkan ALIAS voxelui_vulkan)
  set_target_properties(voxelui_vulkan PROPERTIES EXPORT_NAME vulkan)
  install(TARGETS voxelui_vulkan voxelui_vulkan_geometry EXPORT VoxelUITargets)
  install(FILES renderer.hpp geometry.hpp atlas.hpp tessellation.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/voxelui/vulkan)
endif()
