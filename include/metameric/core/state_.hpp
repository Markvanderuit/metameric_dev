#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/data_.hpp>
#include <metameric/core/utility.hpp>
#include <ranges>
#include <vector>

namespace met {
  namespace detail {
    struct StateObjectBase {
      constexpr
      virtual bool is_dirty() const = 0;

      constexpr 
      operator bool() const { return is_dirty; };
    };

    /* Helper object to track potential changes to an object. This requires
       storing a copy of the object's previously compared version. */
    template <typename Ty, typename Comparator = rng::equal_to>
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

    template <typename Ty, 
              typename Comparator = rng::equal_to, 
              typename STy        = StateObject<Ty, Comparator>>
    class StateVector : public StateObjectBase {
      bool             m_dirty = true;
      std::vector<STy> m_v  	 = { };

    public:
      constexpr
      virtual bool is_dirty() const override { return m_dirty };
      
      constexpr
      bool compare(const std::vector<Ty> &other) {
        if (m_v.size() == other.size()) {
          // Handle simplest (non-resize) case first
          for (uint i = 0; i < m_v.size(); ++i)
            m_v[i].compare(other[i]);
          m_dirty = rng::fold_left(m_v, false, std::logical_or<bool>{}, [](const auto &v) { return v.is_dirty(); });
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
  } // namespace detail

  class UpliftingState : detail::StateObjectBase {
    bool m_dirty = true;

  public:
    // It is useful to have refined queries of constraint property states available for gl uniform
    // handling, so constraints are specialized
    class ConstraintState : detail::StateObjectBase {
      bool m_dirty = true;

    public:
      detail::StateObject<Uplifting::Constraint::Type> type;
      detail::StateObject<Colr>                        colr_i;
      detail::StateObject<uint>                        csys_i;
      detail::StateVector<Colr>                        colr_j;
      detail::StateVector<uint>                        csys_j;
      detail::StateObject<Spec>                        spec;

    public:
      constexpr
      virtual bool is_dirty() const override { return m_dirty; };

      constexpr
      bool compare(const Scene::Component<Uplifting::Constraint> &other) {
        m_dirty = type.compare(other.data.type)     ||
                  colr_i.compare(other.data.colr_i) ||
                  csys_i.compare(other.data.csys_i) ||
                  colr_j.compare(other.data.colr_j) ||
                  csys_j.compare(other.data.csys_j) ||
                  spec.compare(other.data.spec);
        return m_dirty;
      }
    };

  public:
    detail::StateObject<Uplifting::Type> meshing_type;
    detail::StateObject<uint>            basis_i;
    detail::StateVector<Uplifting::Vert, 
      rng::equal_to, ConstraintState>    verts;
    detail::StateVector<Uplifting::Elem> elems;

  public:
    constexpr
    virtual bool is_dirty() const override { return m_dirty; };

    constexpr
    bool compare(const Uplifting &other) {
      m_dirty 
         = meshing_type.compare(other.meshing_type) 
        || basis_i.compare(other.basis_i)
        || verts.compare(other.verts)
        || elems.compare(other.elems);
      return m_dirty;
    }
  };

  class SceneState : detail::StateObjectBase {
    bool m_dirty = true;

  public:
    

  public:
    detail::StateVector<Scene::Component<Scene::Object>>     objects;
    detail::StateVector<Scene::Component<Scene::Emitter>>    emitters;
    detail::StateVector<Scene::Component<Scene::Material>>   materials;
    detail::StateVector<Scene::Component<Uplifting>, 
                      rng::equal_to, UpliftingState>         upliftings;
    detail::StateVector<Scene::Component<Scene::ColrSystem>> colr_systems;

  };
} // namespace met