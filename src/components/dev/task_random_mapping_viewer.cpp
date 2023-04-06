#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/dev/task_random_mapping_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  void RandomMappingsViewerTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;

    // Initialize texture generation subtasks
    auto e_texture_size = e_appl_data.loaded_texture.size();
    m_texture_subtasks.init(info, 0, [](uint i) { return fmt::format("gen_texture_{}", i); },
    [=](auto &, uint i) { return TextureSubTask {{ 
      .input_key = { fmt::format("gen_random_mappings.gen_mapping_{}", i), "colr_buffer" }, 
      .output_key    = "texture",
      .texture_info  = { .size = e_texture_size },
      .lrgb_to_srgb  = true
    }}; });
  }

  bool RandomMappingsViewerTask::is_active(SchedulerHandle &info) {
    return info("gen_random_constraints", "constraints").is_init();
  }

  void RandomMappingsViewerTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    if (ImGui::Begin("Random mappings viewer")) {
      // Get external resources
      const auto &e_appl_data  = info.global("appl_data").read_only<ApplicationData>();
      const auto &e_proj_data  = e_appl_data.project_data;
      const auto &e_constraints = info("gen_random_constraints", "constraints").read_only<
        std::vector<std::vector<ProjectData::Vert>>
      >();
      uint e_images_n = e_constraints.size();

      // Set up drawing a nr. of textures in a column-based layout; determine texture res.
      uint n_cols = 2;
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      eig::Array2f texture_size = viewport_size
                                * e_appl_data.loaded_texture.size().cast<float>().y()
                                / e_appl_data.loaded_texture.size().cast<float>().x()
                                * 0.985f / static_cast<float>(n_cols);

      // Adjust nr. of subtasks for texture generation
      m_texture_subtasks.eval(info, e_images_n);
      
      // Iterate n_cols, n_rows, and n_mappings
      for (uint i = 0, i_col = 0; i < e_images_n; ++i) {
        ImGui::PushID(fmt::format("mapping_viewer_texture_{}", i).c_str());

        // Get externally shared resources; note, resources may not be created yet as tasks are
        // added into the schedule at the end of a loop, not during
        std::string gen_name = fmt::format("{}.gen_texture_{}", info.task().key(), i); // subtask name
        guard_continue(info.task(gen_name).is_init());
        const auto &e_texture = info(gen_name, "texture").read_only<gl::Texture2d4f>();

        // Bulk of content
        ImGui::BeginGroup();
        
        // Header line
        ImGui::SetNextItemWidth(texture_size.x() * 0.6);
        ImGui::Text(fmt::format("Random #{}", i).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Apply")) {
          auto &e_proj_data = info.global("appl_data").writeable<ApplicationData>().project_data;
          for (uint j = 0; j < e_proj_data.verts.size(); ++j) {
            const auto &constr = e_constraints[i][j];
            e_proj_data.verts[j].csys_j = constr.csys_j;
            e_proj_data.verts[j].colr_j = constr.colr_j;
          }
        }
        // if (ImGui::SmallButton("Export")) eval_save(info, i);
        
        // Main image
        ImGui::Image(ImGui::to_ptr(e_texture.object()), texture_size);

        ImGui::EndGroup();
        
        // Increment column count; insert same-line if on same row and not last item
        i_col++;
        if (i_col >= n_cols) {
          i_col = 0;
        } else if (i < e_images_n - 1) {
          ImGui::SameLine();
        }

        ImGui::PopID();
      }
    }
    ImGui::End();
  }
} // namespace met