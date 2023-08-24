#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/data_.hpp>
#include <ranges>
#include <vector>

namespace met {
  namespace detail {
    class StateObjectBase {
      constexpr
      virtual bool is_dirty() const = 0;

      constexpr 
      operator bool() const { return is_dirty; };
    };

    /* Helper object to track potential changes to an object. This requires
       storing a copy of the object's previously compared version. */
    template <typename Ty, typename Comparator = std::ranges::equal_to>
    class StateObject : public StateObjectBase {
      bool m_dirty = true;
      Ty   m_ty    = { };

    public:
      constexpr
      virtual bool is_dirty() const override { return m_dirty };

      constexpr
      bool compare(const Ty &other) {
        m_dirty = Comparator(m_ty, other);
        if (m_dirty)
          m_ty = other;
        return m_dirty;
      }

      constexpr
      bool operator==(const Ty &other) {
        return compare(other);
      }
    };

    template <typename Ty, typename Comparator = std::ranges::equal_to>
    class StateVector : public StateObjectBase {
      bool                                     m_dirty = true;
      std::vector<StateObject<Ty, Comparator>> m_v  	 = { };

    public:
      constexpr
      virtual bool is_dirty() const override { return m_dirty };
      
      constexpr
      bool compare(const std::vector<Ty> &other) {
        if (m_v.size() == other.size()) {
          // Handle simplest (non-resize) case first
          for (uint i = 0; i < m_v.size(); ++i)
            m_v[i].compare(other[i]);
          m_dirty = std::ranges::fold_left(m_v, false, std::logical_or<bool>());
        } else {
          // Handle potential resize
          size_t min_r = std::min(m_v.size(), other.size()), 
                 max_r = std::max(m_v.size(), other.size());
          m_v.resize(other.size());

          // First compare for smaller range of remaining elements
          for (uint i = 0; i < min_r; ++i)
            m_v[i].compare(other[i]);

          // Potential added elements always have state as 'true'
          if (other.size() == max_r)
            for (uint i = min_r; i < max_r; ++i)
              m_v[i].compare(other[i]);
        }

        return m_dirty;
      }

      constexpr
      bool operator==(const std::vector<Ty> &other) {
        return compare(other);
      }
    };

    // Helper object for tracking data mutation across a vector structure
    template <typename Ty>
    struct VectorState : BaseState {
      std::vector<Ty> is_stale;

      constexpr
      const Ty& operator[](uint i) const { return is_stale[i]; }

      constexpr
            Ty& operator[](uint i)       { return is_stale[i]; }
            
      constexpr 
      auto size() const { return is_stale.size(); }
    };
  } // namespace detail

  struct UpliftingState : detail::StateObjectBase {
    struct ConstraintState : detail::StateObjectBase {
      detail::StateObject<Uplifting::Constraint::Type> type;                      type;
      detail::StateObject<Colr> csys_i;
      detail::StateObject<uint> csys_i;
      detail::StateVector<Colr> colr_j;
      detail::StateVector<uint> csys_j;
      detail::StateObject<Spec> spec;
    };

  public:
    detail::StateObject<Uplifting::Type> meshing_type;
    detail::StateObject<uint> basis_i;
    detail::StateVector<Uplifting::Vert> verts;
    detail::StateVector<Uplifting::Elem> verts;
  };

  struct SceneState : detail::BaseState {
    struct ObjectState : detail::BaseState {
      bool mesh_i, material_i, uplifting_i, trf;
    };

    struct MaterialState : detail::BaseState {
      bool diffuse, roughness, metallic, opacity;
    };

    struct EmitterState : detail::BaseState {
      bool p, multiplier, illuminant_i;
    };

    struct ColrSystemState : detail::BaseState {
      bool observer_i, illuminant_i, n_scatters;
    };

    struct BasisState : detail::BaseState {
      bool mean, functions;
    };

  public:
    bool                      observer_i;
    detail::VectorState<uint> objects;
    detail::VectorState<uint> emitters;
    detail::VectorState<uint> materials;
    detail::VectorState<uint> upliftings;
    detail::VectorState<uint> colr_systems;
    detail::VectorState<uint> meshes;
    detail::VectorState<uint> textures_3f;
    detail::VectorState<uint> textures_1f;
    detail::VectorState<uint> illuminants;
    detail::VectorState<uint> observers;
    detail::VectorState<uint> bases;
  };
} // namespace met