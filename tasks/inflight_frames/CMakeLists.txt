add_executable(inflight_frames
  main.cpp
  App.cpp
)

add_compile_definitions(inflight_frames PRIVATE TEXTURES_ROOT="${PROJECT_SOURCE_DIR}/resources/textures/")

target_link_libraries(inflight_frames
  PRIVATE glfw etna glm::glm wsi gui)

target_add_shaders(inflight_frames
  shaders/toy.frag
  shaders/tex.frag
  shaders/rect.vert
)

target_link_libraries(inflight_frames PUBLIC render_utils scene)
