#include <metameric/core/math.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp> 
#include <metameric/core/spectrum.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/schedule.hpp>
#include <metameric/components/misc/task_lambda.hpp>
#include <metameric/components/misc/task_frame_begin.hpp>
#include <metameric/components/misc/task_frame_end.hpp>
#include <metameric/components/views/task_viewport.hpp>
#include <metameric/components/views/task_window.hpp>
#include <metameric/components/views/task_scene_resources_editor.hpp>
#include <metameric/components/views/task_scene_viewport.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/component_edit.hpp>
#include <metameric/components/views/detail/task_viewport.hpp>
#include <metameric/components/views/detail/task_arcball_input.hpp>
#include <metameric/components/pipeline_new/task_gen_uplifting_data.hpp>
#include <metameric/components/pipeline_new/task_gen_object_data.hpp>

namespace met {
  void submit_schedule_debug(detail::SchedulerBase &scheduler) {
    scheduler.task("schedule_view").init<LambdaTask>([&](SchedulerHandle &info) {
      // Check if the scheduler handle implements a MapBasedSchedule
      auto *info_data = dynamic_cast<MapBasedSchedule *>(&info);
      guard(info_data);

      // Temporary window to show runtime schedule
      if (ImGui::Begin("Scheduler info")) {
        // Query MapBasedSchedule information
        const auto &rsrc_map = info_data->resources();
        const auto &task_map = info_data->tasks();
        const auto  schedule = info_data->schedule();

        for (const auto &task_key : schedule) {
          // Split string to get task name without prepend
          auto count = std::count(range_iter(task_key), '.');
          auto split = std::views::split(task_key, '.')
                     | std::views::transform([](const auto &r) { return std::string(range_iter(r)); });
          auto name = (split | std::views::drop(count)).front();
          
          // Indent dependent on how much of a subtask something is
          for (uint i = 0; i < count; ++i) ImGui::Indent(16.f);

          if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
            if (ImGui::IsItemHovered() && rsrc_map.contains(task_key)) {
              ImGui::BeginTooltip();
              for (const auto &[key, _] : rsrc_map.at(task_key)) {
                ImGui::Text(key.c_str());
              }
              ImGui::EndTooltip();
            }
            ImGui::TreePop();
          }

          // Unindent dependent on how much of a subtask something is
          for (uint i = 0; i < count; ++i) ImGui::Unindent(16.f);
        }
      }
      ImGui::End();
    });

    /* // Temporary window to plot pca components
    scheduler.task("plot_models").init<LambdaTask>([](auto &info) {
      if (ImGui::Begin("PCA plots")) {
        eig::Array2f plot_size = (static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin())) 
                               * eig::Array2f(.67f, 0.3f);
        
        // Do some stuff with the PCA bases
        const auto &basis = info.global("scene").getr<Scene>().resources.bases[0].value().func;
        ImGui::PlotSpectra("##basis_plot", { }, basis.colwise() | rng::to<std::vector<Spec>>(), -1.f, 1.f);
      }
      ImGui::End();
    }); */
  }

  void submit_metameric_editor_schedule_unloaded(detail::SchedulerBase &scheduler) {
    met_trace();

    debug::check_expr(scheduler.global("scene").is_init() && 
                      scheduler.global("window").is_init());
    
    scheduler.clear();
    
    scheduler.task("frame_begin").init<FrameBeginTask>();
    scheduler.task("window").init<WindowTask>();
    scheduler.task("frame_end").init<FrameEndTask>(false);
  }

  void submit_metameric_editor_schedule_loaded(detail::SchedulerBase &scheduler) {
    met_trace();

    debug::check_expr(scheduler.global("scene").is_init() && 
                      scheduler.global("window").is_init());
    
    scheduler.clear();
    
    scheduler.task("frame_begin").init<FrameBeginTask>();
    scheduler.task("window").init<WindowTask>();

    // Simple task triggers scene update and filters some edge cases
    // for scene data editing; is run at start of frame
    scheduler.task("scene_handler").init<LambdaTask>([](SchedulerHandle &info) {
      met_trace();

      // Get shared resources
      auto &e_scene = info.global("scene").getw<Scene>();

      // Force check of scene indices to ensure linked components/resources still exist
      for (auto [i, comp] : enumerate_view(e_scene.components.objects)) {
        auto &obj = comp.value;
        if (obj.mesh_i >= e_scene.resources.meshes.size())
          obj.mesh_i = 0u;
        if (obj.uplifting_i >= e_scene.components.upliftings.size())
          obj.uplifting_i = 0u;
      }
      for (auto [i, comp] : enumerate_view(e_scene.components.emitters)) {
        auto &emt = comp.value;
        if (emt.illuminant_i >= e_scene.resources.illuminants.size())
          emt.illuminant_i = 0u;
      }
      for (auto [i, comp] : enumerate_view(e_scene.components.upliftings)) {
        auto &upl = comp.value;
        if (upl.csys_i >= e_scene.components.colr_systems.size())
          upl.csys_i = 0u;
      }

      // Force update of stale gl-side components and state tracking
      e_scene.resources.meshes.update(e_scene);
      e_scene.resources.images.update(e_scene);
      e_scene.resources.illuminants.update(e_scene);
      e_scene.resources.observers.update(e_scene);
      e_scene.resources.bases.update(e_scene);
      e_scene.components.settings.state.update(e_scene.components.settings.value);
      e_scene.components.observer_i.state.update(e_scene.components.observer_i.value);
      e_scene.components.colr_systems.update(e_scene);
      e_scene.components.emitters.update(e_scene);
      e_scene.components.objects.update(e_scene);
      e_scene.components.upliftings.update(e_scene);
      e_scene.components.views.update(e_scene);
    });

    // Pipeline tasks generate uplifting data per object
    scheduler.task("gen_upliftings").init<GenUpliftingsTask>();
    scheduler.task("gen_objects").init<GenObjectsTask>();

    // View tasks handle UI components
    scheduler.task("scene_components_editor").init<LambdaTask>([](auto &info) {
      met_trace();
      if (ImGui::Begin("Scene components")) {
        push_editor<detail::Component<Object>>(info,       { .editor_name = "Objects" });
        push_editor<detail::Component<Emitter>>(info,      { .editor_name = "Emitters" });
        push_editor<detail::Component<Uplifting>>(info,    { .editor_name = "Uplifting models" });
        push_editor<detail::Component<ColorSystem>>(info,  { .editor_name = "Color systems", .edit_name = false });
        push_editor<detail::Component<ViewSettings>>(info, { .editor_name = "Views" });
      }
      ImGui::End();
    });

    // scheduler.task("scene_debugger").init<SceneResourcesEditorTask>();
    scheduler.task("scene_resource_editor").init<LambdaTask>([](auto &info) {
      met_trace();
      if (ImGui::Begin("Scene resources")) {
        push_editor<detail::Resource<Mesh>>(info, { .editor_name  = "Meshes",
                                                    .show_add     = false,
                                                    .show_del     = true,
                                                    .show_dupl    = false });
        push_editor<detail::Resource<Image>>(info, { .editor_name = "Textures",
                                                     .show_add    = false,
                                                     .show_del    = true,
                                                     .show_dupl   = false });
      }
      ImGui::End();
    });

    // scheduler.task("gnome_rotator").init<LambdaTask>([](auto &info) {
    //   met_trace();
    //   try {
    //     auto &e_scene = info.global("scene").getw<Scene>();
    //     auto &e_gnome = e_scene.components.objects[4].value;
    //     e_gnome.transform.rotation.y() += 0.1f;
    //   } catch (const std::exception &e) { /* Ignore; gnome not loaded yet */ }
    // });

    scheduler.task("viewport").init<SceneViewportTask>();

    // Insert temporary unimportant tasks
    // submit_schedule_debug(scheduler);

    scheduler.task("frame_end").init<FrameEndTask>(false);
  }
  
  void submit_metameric_editor_schedule(detail::SchedulerBase &scheduler) {
    met_trace();

    debug::check_expr(scheduler.global("scene").is_init() && 
                      scheduler.global("window").is_init());

    const auto &e_scene = scheduler.global("scene").getr<Scene>();
    if (e_scene.save_state == Scene::SaveState::eUnloaded) {
      submit_metameric_editor_schedule_unloaded(scheduler);
    } else {
      submit_metameric_editor_schedule_loaded(scheduler);
    }
  }
} // namespace met