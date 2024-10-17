#pragma once

#include <application.hpp>

using namespace met;

std::queue<RenderTaskInfo> generate_task_queue() {
  // Queue processes all moved info objects
  std::queue<RenderTaskInfo> queue;

  queue.push(RenderTaskInfo {
    .scene_path   = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_0.json",
    .out_path     = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_0.mp4",
    .view_name    = "FFW view",
    .view_scale   = 0.25f,
    .fps          = 30u,
    .spp          = 4u,
    .spp_per_step = 4u,
    .start_time   = 0.f,
    .end_time     = 6.f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();
      
      auto &cube1 = scene.components.objects("Cube 1").value;
      auto &cube2 = scene.components.objects("Cube 2").value;

      float move_start_time = 1.f, move_end_time = 3.5f;

      // Move cubes, left to right
      anim::add_twokey<float>(info.events, {
        .handle = cube1.transform.position[0],
        .values = {0.825f, -0.5f},
        .times  = { move_start_time, move_end_time },
        .fps    = info.fps
      });
      anim::add_twokey<float>(info.events, {
        .handle = cube2.transform.position[0],
        .values = {0.5, -0.825f},
        .times  = { move_start_time, move_end_time },
        .fps    = info.fps
      });

      // Rotate cubes, some degrees
      float angle = 1.571f - (2.f - 1.571f);
      anim::add_twokey<float>(info.events, {
        .handle = cube1.transform.rotation[0],
        .values = { 2.f, angle },
        .times  = { move_start_time, move_end_time },
        .fps    = info.fps
      });
      anim::add_twokey<float>(info.events, {
        .handle = cube2.transform.rotation[0],
        .values = { 2.f, angle },
        .times  = { move_start_time, move_end_time },
        .fps    = info.fps
      });
    }
  });

  queue.push(RenderTaskInfo {
    .scene_path   = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1a.json",
    .out_path     = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1a.mp4",
    .view_name    = "FFW view",
    .view_scale   = 0.5f,
    .fps          = 30u,
    .spp          = 4u,
    .spp_per_step = 4u,
    .start_time   = 0.f,
    .end_time     = 6.f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();

      auto &cvert = scene.components.upliftings[0]->verts[0];
      auto &light = scene.components.emitters[0].value;
      
      float move_start_time = 1.f, move_end_time = 4.f;
      
      // Rotate light around
      anim::add_twokey<eig::Vector3f>(info.events,{
        .handle = light.transform.position,
        .values = { light.transform.position, eig::Vector3f { 128, 200, 128 } },
        .times  = { move_start_time, move_end_time },
        .fps    = info.fps
      });
    }
  });

  queue.push(RenderTaskInfo {
    .scene_path   = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1b.json",
    .out_path     = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1b.mp4",
    .view_name    = "FFW view",
    .view_scale   = 0.5f,
    .fps          = 30u,
    .spp          = 4u,
    .spp_per_step = 4u,
    .start_time   = 0.f,
    .end_time     = 6.f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();

      auto &cvert = scene.components.upliftings[0]->verts[0];
      auto &light = scene.components.emitters[0].value;
      
      float move_start_time = 1.f, move_end_time = 3.5f;
      
      // Rotate light around
      anim::add_twokey<eig::Vector3f>(info.events,{
        .handle = light.transform.position,
        .values = { light.transform.position, eig::Vector3f { 128, 200, 128 } },
        .times  = { move_start_time, move_end_time },
        .fps    = info.fps
      });
    }
  });
  
  return queue;
};

// RenderTask entry point
int main() {
  met_trace();

  try {
    // Secondary function generates input tasks
    auto queue = generate_task_queue();
    
    // Process input tasks
    while (!queue.empty()) {
      auto info = queue.front();
      queue.pop();

      debug::check_expr(fs::exists(info.scene_path));
      fmt::print("Starting {}\n", info.scene_path.string());

      // Overwrite quality settings for consistenncy
      info.view_scale   = 1.f;
      info.spp          = 256u;
      info.spp_per_step = 4u;
      
      // RenderTask consumes task
      RenderTask app(std::move(info));
      app.run();
    }
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}