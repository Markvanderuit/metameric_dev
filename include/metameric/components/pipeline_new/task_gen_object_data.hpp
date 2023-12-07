#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/scheduler_subtasks.hpp>
#include <metameric/components/misc/detail/scene.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/dispatch.hpp>

namespace met {
  class GenObjectDataTask : public detail::TaskNode {
    uint m_object_i;
    
    struct UnifLayout {
      alignas(4) uint         object_i;
      alignas(8) eig::Array2u dispatch_n;
    };

    // Packed wrapper data for tetrahedron; 64 bytes for std430 
    struct ElemPack {
      eig::Matrix<float, 4, 3> inv; // Last column is padding
      eig::Matrix<float, 4, 1> sub; // Last value is padding
    };

    gl::ComputeInfo  m_dispatch;
    gl::Program      m_program;
    gl::Buffer       m_unif_buffer;
    UnifLayout      *m_unif_map;

  public:
    GenObjectDataTask(uint object_i);

    bool is_active(SchedulerHandle &) override;
    void init(SchedulerHandle &)      override;
    void eval(SchedulerHandle &)      override;
  };

  class GenObjectsTask : public detail::TaskNode {
    detail::Subtasks<GenObjectDataTask> m_subtasks;

  public:
    void init(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_objects = e_scene.components.objects;

      // Add texture atlas object, non-initialized
      info("bary_data").init<detail::TextureAtlas<float, 4>>({
        .levels  = 1,
        .padding = 0u,
        .method  = detail::TextureAtlasBase::BuildMethod::eSpread,
      });

      // Add subtasks to perform mapping
      m_subtasks.init(info, e_objects.size(), 
        [](uint i)         { return fmt::format("gen_object_{}", i); },
        [](auto &, uint i) { return GenObjectDataTask(i);                });
    }

    void eval(SchedulerHandle &info) override {
      met_trace();

      // Get external resources
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &e_objects = e_scene.components.objects;

      // Adjust nr. of subtasks
      m_subtasks.eval(info, e_objects.size());

      // Check if atlas needs rebuilding
      if (!e_objects.empty()) {
        // Get external resources
        const auto &e_images   = e_scene.resources.images;
        const auto &e_settings = e_scene.components.settings;

        // Get weight atlas handle and value
        auto i_bary_handle = info("bary_data");
        const auto &i_bary_data = i_bary_handle.getr<detail::TextureAtlas<float, 4>>();

        // Gather necessary texture sizes for each object
        std::vector<eig::Array2u> inputs(e_objects.size());
        rng::transform(e_objects, inputs.begin(), [&](const auto &comp) -> eig::Array2u {
          const auto &e_obj = comp.value;
          if (auto value_ptr = std::get_if<uint>(&e_obj.diffuse)) {
            // Texture index specified; insert texture size in the atlas inputs
            const auto &e_img = e_images[*value_ptr].value();
            return e_img.size();
          } else {
            // Color specified directly; a small 2x2 patch suffices
            return { 256, 256 };
          }
        });

        // Determine maximum texture sizes, and scale atlas inputs w.r.t. to this value and
        // specified texture settings
        eig::Array2u maximal_4f = rng::fold_left(inputs, eig::Array2u(0), 
          [](auto a, auto b) { return a.cwiseMax(b).eval(); });
        eig::Array2u clamped_4f = detail::clamp_size_by_setting(e_settings.value.texture_size, maximal_4f);
        eig::Array2f scaled_4f  = clamped_4f.cast<float>() / maximal_4f.cast<float>();
        for (auto &input : inputs)
          input = (input.cast<float>() * scaled_4f).max(2.f).cast<uint>().eval();

        // Test if the necessitated inputs match exactly to the atlas' reserved patches
        bool is_exact_fit = rng::equal(inputs, i_bary_data.patches(),
          eig::safe_approx_compare<eig::Array2u>, {}, &detail::TextureAtlasBase::PatchLayout::size);

        // Internally refit atlas if inputs don't match the atlas' current layout
        if (!is_exact_fit) {
          i_bary_handle.getw<detail::TextureAtlas<float, 4>>().resize(inputs);

          fmt::print("Rebuilt atlas\n");
          for (const auto &patch : i_bary_data.patches()) {
            fmt::print("\toffs = {}, size = {}, uv0 = {}, uv1 = {}\n", patch.offs, patch.size, patch.uv0, patch.uv1);
          }

        }
      }
    }
  };
} // namespace met