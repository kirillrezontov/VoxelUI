# Backend-private CPU geometry stage. It does not claim a Vulkan renderer target.
add_library(voxelui_vulkan_geometry STATIC geometry.cpp)
target_link_libraries(voxelui_vulkan_geometry PUBLIC voxelui)
target_include_directories(voxelui_vulkan_geometry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(VOXELUI_TESTS)
  add_executable(voxelui-vulkan-geometry-tests geometry_tests.cpp)
  target_link_libraries(voxelui-vulkan-geometry-tests PRIVATE voxelui_vulkan_geometry voxelui_software)
  add_test(NAME voxelui-vulkan-geometry-tests COMMAND voxelui-vulkan-geometry-tests)
endif()
