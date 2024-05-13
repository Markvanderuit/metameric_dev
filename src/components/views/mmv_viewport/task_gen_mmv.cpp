#include <metameric/core/distribution.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/mmv_viewport/task_gen_mmv.hpp>
#include <small_gl/array.hpp>
#include <small_gl/dispatch.hpp>
#include <algorithm>
#include <execution>

namespace met {
  constexpr bool export_unitized_mesh = true; // Scale mesh to a [0,1]-bounding box for better input later on

  void GenMMVTask::init(SchedulerHandle &info) {
    met_trace();

    // Make vertex array object available, uninitialized
    info("chull_trnf").set<eig::Matrix4f>(eig::Matrix4f::Identity());
    info("chull_array").set<gl::Array>({ });
    info("chull_draw").set<gl::DrawInfo>({ });
    info("chull_center").set<eig::Array3f>(.5f);
  }

  void GenMMVTask::eval(SchedulerHandle &info) {
    met_trace();

    // Get shared resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &e_cs    = info.parent()("selection").getr<ConstraintRecord>();
    auto uplf_handle    = info.task(std::format("gen_upliftings.gen_uplifting_{}", e_cs.uplifting_i)).mask(info);

    // Exit early unless inputs have changed somehow
    guard(is_first_eval() || uplf_handle("mismatch_hulls").is_mutated());

    // Get list of convex hulls for this uplifting; exit early if it doesn't exist
    auto &i_array       = info("chull_array").getw<gl::Array>();
    auto &i_draw        = info("chull_draw").getw<gl::DrawInfo>();
    auto &i_trnf        = info("chull_trnf").getw<eig::Matrix4f>();
    const auto &e_hulls = uplf_handle("mismatch_hulls").getr<std::vector<ConvexHull>>();
    const auto &e_hull  = e_hulls[e_cs.vertex_i];
    
    // Get specific convex hull; exit early and reset if it is empty
    if (e_hull.hull.empty()) {
      i_array = {};
      i_draw  = {};
      i_trnf  = eig::Matrix4f::Identity();
      return;
    }

    // Determine extents of current point sets
    // auto maxb = rng::fold_left_first(e_hull.hull.verts, [](auto a, auto b) { return a.max(b).eval(); }).value();
    // auto minb = rng::fold_left_first(e_hull.hull.verts, [](auto a, auto b) { return a.min(b).eval(); }).value();

    // // If a set of points is available, generate an approximate center for the camera
    // auto new_center = eig::Array3f(.5f); // (minb + 0.5 * (maxb - minb)).eval();
    // info.relative("viewport_camera")("arcball").getw<detail::Arcball>().set_center(new_center);
    
    // Generate output mesh    
    auto mesh = convert_mesh<AlMesh>(e_hull.hull);
    auto lrgb = mesh.verts;
    eig::Matrix4f trf = export_unitized_mesh 
                      ? unitize_mesh<AlMesh>(mesh) 
                      : eig::Matrix4f::Identity();

    // If a convex hull is available, generate a vertex array object and corresponding draw obj
    // for rendering purposes
    m_chull_verts  = {{ .data = cnt_span<const std::byte>(mesh.verts) }};
    m_chull_elems  = {{ .data = cnt_span<const std::byte>(mesh.elems) }};
    m_chull_colors = {{ .data = cnt_span<const std::byte>(lrgb)       }};
    i_trnf  = trf;
    i_array = {{
      .buffers  = {{ .buffer = &m_chull_verts,  .index = 0, .stride = sizeof(eig::Array4f)  },
                   { .buffer = &m_chull_colors, .index = 1, .stride = sizeof(eig::Array4f)  }},
      .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 },
                   { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_chull_elems
    }};
    i_draw = { .type           = gl::PrimitiveType::eTriangles,
               .vertex_count   = (uint) (m_chull_elems.size() / sizeof(uint)),
               .bindable_array = &i_array };
  }
} // namespace met