#pragma once

#include <metameric/core/serialization.hpp>
#include <metameric/core/utility.hpp>
#include <functional>
#include <type_traits>
#include <variant>

namespace met::detail {
  /* Virtual base class for component state tracking in the
     app's Scene class. */
  template <typename Ty>
  class ComponentStateBase {
  protected:
    bool m_mutated = true;

  public:
    constexpr
    void set_mutated(bool b) { m_mutated = b; }

    constexpr
    bool is_mutated() const { return m_mutated; }
    
    constexpr 
    operator bool() const { return m_mutated; };

    virtual 
    bool update(const Ty &o) = 0;
  };

  /* Default implementing class for component state tracking;
     simply stores single state for the entire component. It's
     either changed, or not changed. */
  template <typename Ty>
  class ComponentState : public ComponentStateBase<Ty> {
    using ComponentStateBase<Ty>::m_mutated;
    Ty m_cache  = { };

  public:
    virtual 
    bool update(const Ty &o) override {
      // Eigen's blocks do not support single-component equality comparison,
      // but in general most things handle this just fine; so hack in here
      if constexpr (is_approx_comparable<Ty>)
        m_mutated = !m_cache.isApprox(o);
      else
        m_mutated = (m_cache != o);

      if (m_mutated)
        m_cache = o;
      
      return m_mutated;
    }
  };
  
  template <typename T0, typename T1>
  class ComponentStateVariant : public ComponentStateBase<std::variant<T0, T1>> {
    using Ty = std::variant<T0, T1>;
    using ComponentStateBase<Ty>::m_mutated;

    Ty m_cache = { };

  public:
    virtual 
    bool update(const Ty &o) override {
      // Eigen's blocks do not support single-component equality comparison,
      // but in general most things handle this just fine; so hack in here
      if (o.index() != m_cache.index()) {
        m_mutated = true;
      } else if (o.index() == 0) {
        const auto &o_ = std::get<0>(o), &m_ = std::get<0>(m_cache);
        if constexpr (is_approx_comparable<T0>) m_mutated = !m_.isApprox(o_);
        else                                    m_mutated = (m_ != o_);
      } else if (o.index() == 1) {
        const auto &o_ = std::get<1>(o), &m_ = std::get<1>(m_cache);
        if constexpr (is_approx_comparable<T1>) m_mutated = !m_.isApprox(o_);
        else                                    m_mutated = (m_ != o_);
      }

      if (m_mutated)
        m_cache = o;
      
      return m_mutated;
    }
  };


  /* Wrapper class for component state tracking; tracks
     state across a vector of components, and handles
     resizes of the vector gracefully */
  template <typename Ty, typename State = ComponentState<Ty>>
  requires (std::derived_from<State, ComponentStateBase<Ty>>)
  class ComponentStateVector : public ComponentStateBase<std::vector<Ty>> {
    using ComponentStateBase<std::vector<Ty>>::m_mutated;
    std::vector<State> m_cache  = { };

    bool m_resized = false;

  public:
    virtual 
    bool update(const std::vector<Ty> &o) override {
      if (m_cache.size() == o.size()) {
        // Handle non-resize case first
        for (uint i = 0; i < m_cache.size(); ++i)
          m_cache[i].update(o[i]);
        m_mutated = rng::any_of(m_cache, [](const auto &v) { return v.is_mutated(); });
        m_resized = true;
      } else {
        // Handle shrink/grow
        size_t min_r = std::min(m_cache.size(), o.size()), 
               max_r = std::max(m_cache.size(), o.size());
        m_cache.resize(o.size());

        // First compare for smaller range of remaining elements
        for (uint i = 0; i < min_r; ++i)
          m_cache[i].update(o[i]);

        // Potential added elements always have state as 'true'
        if (max_r == o.size())
          for (uint i = min_r; i < max_r; ++i)
            m_cache[i].update(o[i]);
        
        m_mutated = true;
        m_resized = false;
      }

      return m_mutated;
    }

    constexpr
    bool is_resized() const { return m_resized; }

  public: // Boilerplate
    // operator[...] exposes throwing at()
    constexpr const auto &operator[](uint i) const { return m_cache.at(i); }
    constexpr       auto &operator[](uint i)       { return m_cache.at(i); }
  };

  /* Scene component.
     Wrapper around objects/emitters/etc present in the scene, to handle component name, active flag,
     and specializable state tracking to detect internal changes for e.g. the Uplifting object. */
  template <typename Ty,
            typename State = ComponentState<Ty>>
            requires (std::derived_from<State, ComponentStateBase<Ty>>)
  struct Component {
    std::string name      = "";
    Ty          value     = { };
    State       state     = { };
    
    constexpr friend 
    auto operator<=>(const Component &, const Component &) = default;
    
    constexpr 
    operator bool() const { return state.is_mutated(); };
    
  public: // Types
    using value_type = Ty;
    using state_type = State;

  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      io::to_stream(name,  str);
      io::to_stream(value, str);
    }

    void fr_stream(std::istream &str) {
      met_trace();
      io::fr_stream(name,  str);
      io::fr_stream(value, str);
    }
    
  public: // Structured binding; const auto &[ty, state] = component
    template<size_t Index>
    std::tuple_element_t<Index, Component<Ty, State>> const& get() const& {
      static_assert(Index < 2);
      if constexpr (Index == 0) return value;
      if constexpr (Index == 1) return state;
    } 
    template<size_t Index>
    std::tuple_element_t<Index, Component<Ty, State>> & get() & {
      static_assert(Index < 2);
      if constexpr (Index == 0) return value;
      if constexpr (Index == 1) return state;
    } 
  };

  /* Scene resource.
     Wrapper around meshes/textures/spectra used by components in the scene, to handle resource
     name, and especially simple state tracking without storing a resource duplicate. */
  template <typename Ty>
  class Resource {
    bool m_mutated;
    Ty   m_value;

  public:
    std::string name         = "";
    bool        is_deletable = false;

    Resource() = default;
    Resource(std::string_view name, const Ty &value, bool deletable = true) 
    : m_mutated(true), name(name), m_value(value), is_deletable(deletable) { }
    Resource(std::string_view name, Ty &&value, bool deletable = true) 
    : m_mutated(true), name(name), m_value(std::move(value)), is_deletable(is_deletable) { }

  public: // State handling
    constexpr void set_mutated(bool b) { m_mutated = b;    }
    constexpr bool is_mutated()  const { return m_mutated; }
    constexpr operator bool()    const { return m_mutated; };

  public: // Boilerplate
    constexpr const Ty &value() const { return m_value; }
    constexpr       Ty &value()       { set_mutated(true); return m_value; }

    constexpr friend 
    auto operator<=>(const Resource &, const Resource &) = default;
    
  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      io::to_stream(name,    str);
      io::to_stream(m_value, str);
    }

    void fr_stream(std::istream &str) {
      met_trace();
      io::fr_stream(name,    str);
      io::fr_stream(m_value, str);
    }
  
  public: // Structured binding; const auto &[ty, state] = component
    template<size_t Index>
    std::tuple_element_t<Index, Resource<Ty>> const& get() const& {
      static_assert(Index < 2);
      if constexpr (Index == 0) return m_value;
      if constexpr (Index == 1) return m_mutated;
    } 

    template<size_t Index>
    std::tuple_element_t<Index, Resource<Ty>> & get() & {
      static_assert(Index < 2);
      set_mutated(true);
      if constexpr (Index == 0) return m_value;
      if constexpr (Index == 1) return m_mutated;
    } 
  };

  /* Scene component vector.
     Encapsulates std::vector<Component<Ty>> to handle named component lookups and s
     and state tracking. */
  template <typename Ty,
            typename State = ComponentState<Ty>>
  class ComponentVector {
    using Comp = Component<Ty, State>;

    mutable bool      m_mutated    = true;
    mutable bool      m_resized    = false;
    mutable size_t    m_size_state = 0;
    std::vector<Comp> m_data;

  public: // State handling
    bool test_mutated() {
      met_trace();

      if (m_data.size() == m_size_state) {
        rng::for_each(m_data, [](auto &rsrc) { rsrc.state.update(rsrc.value); });
        m_resized = false;
        m_mutated = rng::any_of(m_data, [](auto &rsrc) { return rsrc.state.is_mutated(); });
      } else {
        rng::for_each(m_data, [](auto &rsrc) { rsrc.state.update(rsrc.value); });
        m_resized    = true;
        m_mutated    = true;
        m_size_state = m_data.size();
      }

      return m_mutated;
    }

    constexpr void set_mutated(bool b) {
      met_trace();
      m_mutated = true;
      rng::for_each(m_data, [b](auto &rsrc) { rsrc.state.set_mutated(b); });
    }

    constexpr bool is_mutated() const {
      return m_mutated; // || rng::any_of(m_data, [](const auto &rsrc) -> bool { return rsrc.state; });
    }

    constexpr bool is_resized() const {
      return m_resized;
    }

  public: // Vector overloads
    constexpr void push(std::string_view name, const Ty &value) {
      m_data.push_back(Comp { .name = std::string(name), .value = value });
    }

    constexpr void emplace(std::string_view name, Ty &&value) {
      m_data.emplace_back(Comp { .name = std::string(name), .value = std::move(value) });
    }
    
    constexpr void erase(std::string_view name) {
      auto it = rng::find(m_data, name, &Comp::name);
      debug::check_expr(it != m_data.end(), "Erased scene component does not exist");
      m_data.erase(it);
    }

    // operator[...] exposes throwing at()
    constexpr const Comp &operator[](uint i) const { return m_data.at(i); }
    constexpr       Comp &operator[](uint i)       { return m_data.at(i); }

    // operator("...") enables named lookup
    constexpr const Comp &operator()(std::string_view name) const {
      auto it = rng::find(m_data, name, &Comp::name);
      debug::check_expr(it != m_data.end(), "Queried scene component does not exist");
      return *it;
    }
    constexpr       Comp &operator()(std::string_view name)       {
      auto it = rng::find(m_data, name, &Comp::name);
      debug::check_expr(it != m_data.end(), "Queried scene component does not exist");
      return *it;
    }

    // Bookkeeping; expose the underlying std::vector, instead of a direct pointer
    constexpr const auto & data() const { return m_data; }
    constexpr       auto & data()       { return m_data; }

    // Bookkeeping; expose miscellaneous std::vector member functions
    constexpr void resize(size_t i) { m_data.resize(i);      }
    constexpr void erase(uint i)    { m_data.erase(m_data.begin() + i); }
    constexpr void clear()          { m_data.clear();        }
    constexpr auto empty()    const { return m_data.empty(); }
    constexpr auto size()     const { return m_data.size();  }
    constexpr auto begin()    const { return m_data.begin(); }
    constexpr auto end()      const { return m_data.end();   }
    constexpr auto begin()          { return m_data.begin(); }
    constexpr auto end()            { return m_data.end();   }
    
  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      io::to_stream(m_data, str);
    }

    void fr_stream(std::istream &str) {
      met_trace();
      io::fr_stream(m_data, str);
    }
  };

  /* Scene resource vector.
     Encapsulates std::vector<Resource<Ty>> to handle named resource lookups and some minor
     syntactic sugar for easy resource initialization. */
  template <typename Ty>
  class ResourceVector {
    using Rsrc = Resource<Ty>;
    std::vector<Rsrc> m_data;

  public: // State handling
    constexpr void set_mutated(bool b) {
      met_trace();
      rng::for_each(m_data, [b](auto &rsrc) { rsrc.set_mutated(b); });
    }

    constexpr bool is_mutated() const {
      met_trace();
      return rng::any_of(m_data, [](const auto &rsrc) { return rsrc.is_mutated(); });
    }

  public: // Vector overloads
    constexpr void push(std::string_view name, const Ty &value, bool deletable = true) {
      met_trace();
      m_data.push_back(Rsrc(std::string(name), value, deletable));
    }

    constexpr void emplace(std::string_view name, Ty &&value, bool deletable = true) {
      met_trace();
      m_data.emplace_back(Rsrc(std::string(name), std::move(value), deletable));
    }
    
    constexpr void erase(std::string_view name) {
      met_trace();
      auto it = rng::find(m_data, name, &Rsrc::name);
      debug::check_expr(it != m_data.end(), "Erased scene resource does not exist");
      m_data.erase(it);
    }

    // operator[...] exposes throwing at()
    constexpr const Rsrc &operator[](uint i) const { return m_data.at(i); }
    constexpr       Rsrc &operator[](uint i)       { return m_data.at(i); }

    // operator("...") enables named lookup
    constexpr const Rsrc &operator()(std::string_view name) const {
      met_trace();
      auto it = rng::find(m_data, name, &Rsrc::name);
      debug::check_expr(it != m_data.end(), "Queried scene resource does not exist");
      return *it;
    }
    constexpr       Rsrc &operator()(std::string_view name)       {
      met_trace();
      auto it = rng::find(m_data, name, &Rsrc::name);
      debug::check_expr(it != m_data.end(), "Queried scene resource does not exist");
      return *it;
    }

    // Bookkeeping; expose the underlying std::vector, instead of a direct pointer
    constexpr const auto & data() const { return m_data; }
    constexpr       auto & data()       { return m_data; }

    // Bookkeeping; expose miscellaneous std::vector member functions
    constexpr void resize(size_t i) { m_data.resize(i);      }
    constexpr void erase(uint i)    { m_data.erase(i);       }
    constexpr void clear()          { m_data.clear();        }
    constexpr auto empty()    const { return m_data.empty(); }
    constexpr auto size()     const { return m_data.size();  }
    constexpr auto begin()    const { return m_data.begin(); }
    constexpr auto end()      const { return m_data.end();   }
    constexpr auto begin()          { return m_data.begin(); }
    constexpr auto end()            { return m_data.end();   }
    
  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      io::to_stream(m_data, str);
    }

    void fr_stream(std::istream &str) {
      met_trace();
      io::fr_stream(m_data, str);
    }
  };
} // namespace met::detail

/* Here follows structured binding support for Scene::Component/Scene::Resource types.
   I'm sure this won't backfire at all! */
namespace std {
  template <typename Ty>
  struct tuple_size<::met::detail::Resource<Ty>> 
  : integral_constant<size_t, 2> {};

  template <typename Ty>
  struct tuple_element<0, ::met::detail::Resource<Ty>> { 
    using type = Ty;
  };

  template <typename Ty>
  struct tuple_element<1, ::met::detail::Resource<Ty>> {
    using type = bool;
  };

  template <typename Ty, typename State>
  struct tuple_size<::met::detail::Component<Ty, State>> 
  : integral_constant<size_t, 2> {};

  template <typename Ty, typename State>
  struct tuple_element<0, ::met::detail::Component<Ty, State>> { 
    using type = met::detail::Component<Ty, State>::value_type;
  };

  template <typename Ty, typename State>
  struct tuple_element<1, ::met::detail::Component<Ty, State>> {
    using type = met::detail::Component<Ty, State>::state_type;
  };
} // namespace std