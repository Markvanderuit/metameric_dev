#pragma once

#include <application.hpp>

using namespace met;

const fs::path scene_path = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Siggraph Asia Presentation/scenes";
const fs::path render_path = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Siggraph Asia Presentation/renders/rework";

std::queue<RenderTaskInfo> generate_task_queue() {
  // Queue processes all moved info objects
  std::queue<RenderTaskInfo> queue;

  // VIDEO 1 (opening scene)
  // A bunny is visible. A second bunny falls from the sky
  /* queue.push(RenderTaskInfo {
    .scene_path   = scene_path  / "opening v2.json",
    .out_path     = render_path / "opening_bunny_fl11_appear.mp4",
    .view_name    = "Default view",
    .view_scale   = 1.f,
    .fps          = 60u,
    .spp          = 256u,
    .spp_per_step = 1u,
    .start_time   = 0.f,
    .end_time     = 1.0f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();
      
      auto &D65l   = *scene.components.emitters("D65 (l)");
      auto &D65r   = *scene.components.emitters("D65 (r)");
      auto &FL11   = *scene.components.emitters("FL11 (l)");
      auto &bunny2 = *scene.components.objects("bunny 2");
      auto &cube2  = *scene.components.objects("cube 2");

      // Scene setup
      D65l.is_active = false;
      FL11.is_active = true;
      D65r.is_active = true;

      // Move objects out of and then slide them into view
      float bunny2_target = bunny2.transform.position.y();
      float cube2_target  = cube2.transform.position.y();
      anim::add_twokey<float>(info.events, {
        .handle = bunny2.transform.position.y(),
        .values = { .55f, bunny2_target },
        .times  = { 0.f, 1.0f },
        .fps    = info.fps
      });
      anim::add_twokey<float>(info.events, {
        .handle = cube2.transform.position.y(),
        .values = { -0.1, cube2_target },
        .times  = { 0.f, 1.0f },
        .fps    = info.fps
      });
    }
  }); */

  // VIDEO 2 (fold scene)
  // A ball falls from the sky, two walls appear
  /* queue.push(RenderTaskInfo {
    .scene_path   = scene_path  / "fold.json",
    .out_path     = render_path / "opening_fold_appear.mp4",
    .view_name    = "Default view",
    .view_scale   = 1.f,
    .fps          = 60u,
    .spp          = 256u,
    .spp_per_step = 1u,
    .start_time   = 0.f,
    .end_time     = 1.f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();
      
      auto &wall1  = *scene.components.objects("wall 1");
      auto &wall2  = *scene.components.objects("wall 2");
      auto &sphere = *scene.components.objects("sphere");

      // Make walls come through floor
      anim::add_twokey<float>(info.events, {
        .handle = wall1.transform.position.y(),
        .values = { -0.46f, 0.f },
        .times  = { 0.f, 1.f },
        .fps    = info.fps
      });
      anim::add_twokey<float>(info.events, {
        .handle = wall2.transform.position.y(),
        .values = { -0.46f, 0.f },
        .times  = { 0.f, 1.f },
        .fps    = info.fps
      });

      // Make sphere fall from above
      sphere.transform.position.y() = 0.65f;
      anim::add_twokey<float>(info.events, {
        .handle = sphere.transform.position.y(),
        .values = { 0.65f, 0.f },
        .times  = { 0.25f, 1.f },
        .fps    = info.fps
      });
    }
  }); */

  
  // VIDEO 3 (fold scene)
  // A gnome andd ball fall from the sky, two walls appear
  queue.push(RenderTaskInfo {
    .scene_path   = scene_path  / "result v3.json",
    .out_path     = render_path / "results_gnome_appear.mp4",
    .view_name    = "Default",
    .view_scale   = 1.f,
    .fps          = 60u,
    .spp          = 256u,
    .spp_per_step = 1u,
    .start_time   = 0.f,
    .end_time     = 1.f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();
      
      auto &wall1  = *scene.components.objects("wall 1");
      auto &wall2  = *scene.components.objects("wall 2");
      auto &sphere = *scene.components.objects("sphere");
      auto &box    = *scene.components.objects("box");
      auto &gnome  = *scene.components.objects("safety gnome");

      // Make walls and box come through floor
      anim::add_twokey<float>(info.events, {
        .handle = wall1.transform.position.y(),
        .values = { -0.46f, 0.f },
        .times  = { 0.f, 1.f },
        .fps    = info.fps
      });
      anim::add_twokey<float>(info.events, {
        .handle = wall2.transform.position.y(),
        .values = { -0.46f, 0.f },
        .times  = { 0.f, 1.f },
        .fps    = info.fps
      });
      anim::add_twokey<float>(info.events, {
        .handle = box.transform.position.y(),
        .values = { -0.07f, 0.f },
        .times  = { 0.f, 1.f },
        .fps    = info.fps
      });

      // Make sphere/gnonme fall from above
      sphere.transform.position.y() = 0.67f;
      gnome.transform.position.y() = 0.67f;
      anim::add_twokey<float>(info.events, {
        .handle = sphere.transform.position.y(),
        .values = { 0.67f, 0.f },
        .times  = { 0.25f, 1.f },
        .fps    = info.fps
      });
      anim::add_twokey<float>(info.events, {
        .handle = gnome.transform.position.y(),
        .values = { 0.67f, 0.070f },
        .times  = { 0.25f, 1.f    },
        .fps    = info.fps
      });
    }
  });
  
  /* 
    challenging scene vertex positions
    start: 0.07, 0.073, 0.071
    end 1: 0.107, 0.084, 0.104
    end 2: 0.064, 0.088, 0.082
    end 3: 0.116, 0.092, 0.070
  */

  // VIDEO 5 (challenginng scene)
  // Metameric recoloring 1/2/3
  /* queue.push(RenderTaskInfo {
    .scene_path   = scene_path  / "challenging.json",
    .out_path     = render_path / "5_1.mp4",
    .view_name    = "Default view",
    .view_scale   = 1.f,
    .fps          = 60u,
    .spp          = 256u,
    .spp_per_step = 1u,
    .start_time   = 0.f,
    .end_time     = .6f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();
      // Make walls come through floor
      auto &vert = scene.components.upliftings[0]->verts[0];
      anim::add_twokey<Uplifting::Vertex>(info.events, {
        .handle = vert,
        .values = { vert.get_mismatch_position(), Colr { 0.107, 0.084, 0.104 } },
        .times  = { 0.f, .6f },
        .fps    = info.fps
      });
    }
  });
  queue.push(RenderTaskInfo {
    .scene_path   = scene_path  / "challenging.json",
    .out_path     = render_path / "5_2.mp4",
    .view_name    = "Default view",
    .view_scale   = 1.f,
    .fps          = 60u,
    .spp          = 256u,
    .spp_per_step = 1u,
    .start_time   = 0.f,
    .end_time     = .6f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();
      // Make walls come through floor
      auto &vert = scene.components.upliftings[0]->verts[0];
      anim::add_twokey<Uplifting::Vertex>(info.events, {
        .handle = vert,
        .values = { vert.get_mismatch_position(), Colr { 0.064, 0.088, 0.082 } },
        .times  = { 0.f, .6f },
        .fps    = info.fps
      });
    }
  });
  queue.push(RenderTaskInfo {
    .scene_path   = scene_path  / "challenging.json",
    .out_path     = render_path / "5_3.mp4",
    .view_name    = "Default view",
    .view_scale   = 1.f,
    .fps          = 60u,
    .spp          = 256u,
    .spp_per_step = 1u,
    .start_time   = 0.f,
    .end_time     = .6f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();
      // Make walls come through floor
      auto &vert = scene.components.upliftings[0]->verts[0];
      anim::add_twokey<Uplifting::Vertex>(info.events, {
        .handle = vert,
        .values = { vert.get_mismatch_position(), Colr { 0.116, 0.092, 0.070 } },
        .times  = { 0.f, .6f },
        .fps    = info.fps
      });
    }
  }); */
  
  /* // Frame 6 (path scene)
  // Make objects appear in the scene just like that
  queue.push(RenderTaskInfo {
    .scene_path   = scene_path  / "path.json",
    .out_path     = render_path / "6.mp4",
    .view_name    = "default",
    .view_scale   = 1.f,
    .fps          = 60u,
    .spp          = 256u,
    .spp_per_step = 1u,
    .start_time   = 0.f,
    .end_time     = 1.0f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();

      // Get objects, emitters
      auto &wall1 = *scene.components.objects("wall 1");
      auto &wall2 = *scene.components.objects("wall 2");
      auto &box   = *scene.components.objects("box");
      auto &mug   = *scene.components.objects("mug");
      auto &d65l  = *scene.components.emitters("D65 (l)");
      auto &d65r  = *scene.components.emitters("D65 (r)");
      auto &fl2   = *scene.components.emitters("FL2");
      auto &led   = *scene.components.emitters("LED");

      // Set initial emitter config
      d65l.is_active = false;
      d65r.is_active = false;
      fl2.is_active  = true;
      led.is_active  = true;

      // Make walls and box move up
      anim::add_twokey<float>(info.events, {
        .handle = wall1.transform.position.y(),
        .values = { -0.47f, 0.f },
        .times  = { 0.f, 1.0f },
        .fps    = info.fps
      });
      anim::add_twokey<float>(info.events, {
        .handle = wall2.transform.position.y(),
        .values = { -0.47f, 0.f },
        .times  = { 0.f, 1.0f },
        .fps    = info.fps
      });
      anim::add_twokey<float>(info.events, {
        .handle = box.transform.position.y(),
        .values = { -0.11f, 0.f },
        .times  = { 0.f, 1.0f },
        .fps    = info.fps
      });

      // Make mug fall from above
      mug.transform.position.y() = 0.65f;
      anim::add_twokey<float>(info.events, {
        .handle = mug.transform.position.y(),
        .values = { 0.65f, 0.1f },
        .times  = { 0.25f, 1.0f },
        .fps    = info.fps
      });
    }
  }); */

  // Frame 7 (path scene)
  // Change illuminant from D65 to LED/FL11
  // Rendered as two images instead, mixed in PPT
  
  // VIDEO 8 (path scene)
  // Perform camera move towards mug
  /* queue.push(RenderTaskInfo {
    .scene_path   = scene_path  / "result v3.json",
    .out_path     = render_path / "result_zoom.mp4",
    .view_name    = "Default",
    .view_scale   = 1.f,
    .fps          = 60u,
    .spp          = 256u,
    .spp_per_step = 1u,
    .start_time   = 0.f,
    .end_time     = 1.f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();

      // Get views
      auto &old_view = *scene.components.views("Default");
      auto &new_view = *scene.components.views("Zoomed");

      // Make camera move from far to mug
      anim::add_twokey<eig::Vector3f>(info.events, {
        .handle = old_view.camera_trf.position,
        .values = { old_view.camera_trf.position, new_view.camera_trf.position },
        .times  = { 0.f, 1.0f },
        .fps    = info.fps
      });
      anim::add_twokey<eig::Vector3f>(info.events, {
        .handle = old_view.camera_trf.rotation,
        .values = { old_view.camera_trf.rotation, new_view.camera_trf.rotation },
        .times  = { 0.f, 1.0f },
        .fps    = info.fps
      });
    }
  }); */

  // Frame 9 (path scene)
  // Still image of camera in mug view
  // Rendered by hand
  

  /* 
    target values
    vert 0
      
    vert 1

    
   */
  
  // VIDEO 10a/b/c
  // Do some weird stuff
  /* queue.push(RenderTaskInfo {
    .scene_path   = scene_path  / "path.json",
    .out_path     = render_path / "10.mp4",
    .view_name    = "mug",
    .view_scale   = 1.f,
    .fps          = 60u,
    .spp          = 1u,
    .spp_per_step = 1u,
    .start_time   = 0.f,
    .end_time     = 1.f,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();

      // Get emitters
      auto &d65l  = *scene.components.emitters("D65 (l)");
      auto &d65r  = *scene.components.emitters("D65 (r)");
      auto &fl2   = *scene.components.emitters("FL2");
      auto &led   = *scene.components.emitters("LED");

      // Set initial emitter config
      d65l.is_active = false;
      d65r.is_active = false;
      fl2.is_active  = true;
      led.is_active  = true;
    }
  }); */

  /* queue.push(RenderTaskInfo {
    .scene_path   = scene_path  / "fold.json",
    .out_path     = render_path / "fold_test.mp4",
    .view_name    = "Default view",
    .view_scale   = 1.f,
    .fps          = 60u,
    .spp          = 4u,
    .spp_per_step = 1u,
    .init_events  = [](auto &info, Scene &scene) {
      met_trace();

      auto &vert = scene.components.upliftings[0]->verts[0];

      std::array<Colr, 5> values = {
        vert.get_mismatch_position(), // 0 -> 1 
        Colr { 0.351, 0.404, 0.447 }, // 1 -> 2 
        Colr { 0.436, 0.409, 0.457 }, // 2 -> 3 
        Colr { 0.437, 0.442, 0.402 }, // 3 -> 4 
        Colr { 0.354, 0.425, 0.390 }  // 4 -> 0
      };

      for (uint i = 0; i < values.size(); ++i) {
        anim::add_twokey<Uplifting::Vertex>(info.events, {
          .handle = vert,
          .values = { values[i], values[(i + 1) % values.size()] },
          .times  = { 2.f * static_cast<float>(i), 2.f * static_cast<float>(i + 1) - .05f },
          .motion = anim::MotionType::eLinear,
          .fps    = info.fps
        });
      }
    }
  }); */

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
      auto task = queue.front();
      queue.pop();

      debug::check_expr(fs::exists(task.scene_path));
      fmt::print("Starting {}\n", task.scene_path.string());
      
      // RenderTask consumes task
      RenderTask app(std::move(task));
      app.run();
    }
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}