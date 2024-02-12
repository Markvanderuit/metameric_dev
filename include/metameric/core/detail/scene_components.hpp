#pragma once

#include <metameric/core/serialization.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/scene_state.hpp>
#include <metameric/core/detail/scene_components_fwd.hpp>
#include <concepts>
#include <functional>
#include <type_traits>

namespace met {
  namespace detail {
    // Concept for class to have own, overriding ComponentStateBase<Ty> implementation
    // linked as state_type
    template <typename Ty>
    concept has_state_ty = requires { 
      std::is_class_v<Ty>; 
      typename Ty::state_type;
    };

    // Default, empty struct if a GL-side type is not specified for a component or resource
    struct SceneGLDefault { };

    // Define component_state_t<Ty> to deduce required state object
    template <typename Ty> struct ComponentStateSelector;
    template <typename Ty> requires (!has_state_ty<Ty>)
    struct ComponentStateSelector<Ty> { using type = ComponentState<Ty>; };
    template <typename Ty> requires (has_state_ty<Ty>)
    struct ComponentStateSelector<Ty> { using type = typename Ty::state_type; };
    template <typename Ty> using component_state_t = ComponentStateSelector<Ty>::type;

    // Base template for gl-side packing of components/resources
    // If not explicitly specialized, no gl-side packing occurs 
    template <typename Ty>
    struct GLPacking { /* ... */ };

    // Concept to determine if GLPacking supplies specializable update() function,
    // in which case gpu-side data is pushed each frame on component update
    template <typename Ty, typename Tc>
    concept is_gl_packing_implemented = requires(Ty ty, std::span<const Tc> tc, const Scene &scene) {
      ty.update(tc, scene);
    };
    
    /* Scene component.
      Wrapper around objects/emitters/etc present in the scene, to handle component name, active flag,
      and specializable state tracking to detect internal changes for e.g. the Uplifting object. */
    template <typename Ty>
    struct Component {
      using value_type = Ty;
      using state_type = component_state_t<value_type>;

    public:
      std::string name  = "";  // Loaded name of component
      value_type  value = { }; // Underlying component value
      state_type  state = { }; // State tracking object to detect internal changes
      
    public: // Boilerplate
      constexpr
      operator bool() const { return state.is_mutated(); };
      
      constexpr friend 
      auto operator<=>(const Component &, const Component &) = default;
      
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
      std::tuple_element_t<Index, Component<value_type>> const& get() const& {
        static_assert(Index < 2);
        if constexpr (Index == 0) return value;
        if constexpr (Index == 1) return state;
      } 

      template<size_t Index>
      std::tuple_element_t<Index, Component<value_type>> & get() & {
        static_assert(Index < 2);
        if constexpr (Index == 0) return value;
        if constexpr (Index == 1) return state;
      } 
    };

    /* Scene resource.
      Wrapper around meshes/textures/spectra used by components in the scene, to handle resource
      name, and especially simple state tracking without storing a resource duplicate. */
    template <typename Ty>
    struct Resource {
      using value_type = Ty;

    private:
      bool       m_mutated;             // Simplified state tracking; modified or not
      value_type m_value;               // Underlying resource value, access with .value()

    public:
      std::string name         = "";    // Loaded name of resource
      bool        is_deletable = false; // Safeguard program-loaded resources from deletion, e.g. D65

      Resource() = default;
      Resource(std::string_view name, const value_type &value, bool deletable = true) 
      : m_mutated(true), name(name), m_value(value), is_deletable(deletable) { }
      Resource(std::string_view name, value_type &&value, bool deletable = true) 
      : m_mutated(true), name(name), m_value(std::move(value)), is_deletable(is_deletable) { }

    public: // State handling
      constexpr void set_mutated(bool b) { m_mutated = b;    }
      constexpr bool is_mutated()  const { return m_mutated; }
      constexpr operator bool()    const { return m_mutated; };

    public: // Boilerplate
      constexpr const value_type &value() const { return m_value; }
      constexpr       value_type &value()       { set_mutated(true); return m_value; }

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
      std::tuple_element_t<Index, Resource<value_type>> const& get() const& {
        static_assert(Index < 2);
        if constexpr (Index == 0) return m_value;
        if constexpr (Index == 1) return m_mutated;
      } 

      template<size_t Index>
      std::tuple_element_t<Index, Resource<value_type>> & get() & {
        static_assert(Index < 2);
        set_mutated(true);
        if constexpr (Index == 0) return m_value;
        if constexpr (Index == 1) return m_mutated;
      } 
    };

    /* Scene component vector.
      Encapsulates std::vector<Component<Ty>> to handle named component lookups and
      and state tracking. */
    template <typename Ty>
    struct Components {
      using value_type = Ty;
      using cmpnt_type = Component<value_type>;
      using gl_type    = GLPacking<value_type>;

    private:
      mutable bool            m_mutated = true;
      mutable bool            m_resized = false;
      mutable size_t          m_size    = 0;
      std::vector<cmpnt_type> m_data;

    public: // State handling
      // Test each internal component for an update and, if component state is changed,
      // update the gl-side packed data
      bool update(const met::Scene &scene) {
        met_trace();

        if (m_data.size() == m_size) {
          rng::for_each(m_data, [](auto &rsrc) { rsrc.state.update(rsrc.value); });
          m_resized = false;
          m_mutated = rng::any_of(m_data, [](auto &rsrc) { return rsrc.state.is_mutated(); });
        } else {
          rng::for_each(m_data, [](auto &rsrc) { rsrc.state.update(rsrc.value); });
          m_resized = true;
          m_mutated = true;
          m_size    = m_data.size();
        }

        // If a gl packing type is specialized for the component type, update gl packing data
        if constexpr (is_gl_packing_implemented<gl_type, cmpnt_type>) {
          gl.update(m_data, scene);          
        }

        return m_mutated;
      }
      
      constexpr void set_mutated(bool b) {
        met_trace();
        rng::for_each(m_data, [b](auto &comp) { comp.state.set_mutated(b); });
      }
      constexpr bool is_mutated() const { return m_mutated; }
      constexpr bool is_resized() const { return m_resized; }
      constexpr operator bool()   const { return m_mutated; };

    public: // Vector overloads
      constexpr void push(std::string_view name, const value_type &value) {
        m_data.push_back(cmpnt_type { .name = std::string(name), .value = value });
      }

      constexpr void emplace(std::string_view name, value_type &&value) {
        m_data.emplace_back(cmpnt_type { .name = std::string(name), .value = std::move(value) });
      }
      
      constexpr void erase(std::string_view name) {
        auto it = rng::find(m_data, name, &cmpnt_type::name);
        debug::check_expr(it != m_data.end(), "Erased scene component does not exist");
        m_data.erase(it);
      }

      // operator[...] exposes throwing at()
      constexpr const cmpnt_type &operator[](uint i) const { return m_data.at(i); }
      constexpr       cmpnt_type &operator[](uint i)       { return m_data.at(i); }

      // operator("...") enables named lookup
      constexpr const cmpnt_type &operator()(std::string_view name) const {
        auto it = rng::find(m_data, name, &cmpnt_type::name);
        debug::check_expr(it != m_data.end(), "Queried scene component does not exist");
        return *it;
      }
      constexpr cmpnt_type &operator()(std::string_view name) {
        auto it = rng::find(m_data, name, &cmpnt_type::name);
        debug::check_expr(it != m_data.end(), "Queried scene component does not exist");
        return *it;
      }

      // Bookkeeping; expose the underlying std::vector, instead of a direct pointer
      constexpr const auto & data() const { return m_data; }
      constexpr       auto & data()       { return m_data; }

      // Bookkeeping; expose miscellaneous std::vector member functions
      constexpr void insert(size_t i, const cmpnt_type& v) 
                                      { m_data.insert(m_data.begin() + i, v); }
      constexpr void resize(size_t i) { m_data.resize(i);      }
      constexpr void erase(size_t i)  { m_data.erase(m_data.begin() + i); }
      constexpr void clear()          { m_data.clear();        }
      constexpr auto empty()    const { return m_data.empty(); }
      constexpr auto size()     const { return m_data.size();  }
      constexpr auto begin()    const { return m_data.begin(); }
      constexpr auto end()      const { return m_data.end();   }
      constexpr auto begin()          { return m_data.begin(); }
      constexpr auto end()            { return m_data.end();   }
    
    public: // GL-side packing; always accessible to the underlying pipeline
      mutable gl_type gl;

    public: // Serialization
      void to_stream(std::ostream &str) const {
        met_trace();
        io::to_stream(m_data, str);
      }

      void fr_stream(std::istream &str) {
        met_trace();
        io::fr_stream(m_data, str);
        set_mutated(true);
      }
    };

    /* Scene resource vector.
      Encapsulates std::vector<Resource<Ty>> to handle named resource lookups and some minor
      syntactic sugar for easy resource initialization. */
    template <typename Ty>
    struct Resources {
      using value_type = Ty;
      using resrc_type = Resource<value_type>;
      using gl_type    = GLPacking<value_type>;

    private:
      std::vector<resrc_type> m_data;

    public: // State handling
      // Reset each internal resource's state and, if state was changed, updated
      // the gl-side packed data
      bool update(const met::Scene &scene) {
        met_trace();
        
        // Get current state as return value
        bool mutated = is_mutated();

        // If a gl packing type is specialized for the component type, update gl packing data
        if constexpr (is_gl_packing_implemented<gl_type, resrc_type>) {
          gl.update(m_data, scene);          
        }

        // Reset state for next iteration
        set_mutated(false);

        return mutated;
      }

      constexpr void set_mutated(bool b) {
        met_trace();
        rng::for_each(m_data, [b](auto &rsrc) { rsrc.set_mutated(b); });
      }

      constexpr bool is_mutated() const {
        met_trace();
        return rng::any_of(m_data, [](const auto &rsrc) { return rsrc.is_mutated(); });
      }

      constexpr operator bool() const { 
        return is_mutated();
      };

    public: // Vector overloads
      constexpr void push(std::string_view name, const value_type &value, bool deletable = true) {
        met_trace();
        m_data.push_back(resrc_type(std::string(name), value, deletable));
      }

      constexpr void emplace(std::string_view name, value_type &&value, bool deletable = true) {
        met_trace();
        m_data.emplace_back(resrc_type(std::string(name), std::move(value), deletable));
      }
      
      constexpr void erase(std::string_view name) {
        met_trace();
        auto it = rng::find(m_data, name, &resrc_type::name);
        debug::check_expr(it != m_data.end(), "Erased scene resource does not exist");
        m_data.erase(it);
      }

      // operator[...] exposes throwing at()
      constexpr const resrc_type &operator[](uint i) const { return m_data.at(i); }
      constexpr       resrc_type &operator[](uint i)       { return m_data.at(i); }

      // operator("...") enables named lookup
      constexpr const resrc_type &operator()(std::string_view name) const {
        met_trace();
        auto it = rng::find(m_data, name, &resrc_type::name);
        debug::check_expr(it != m_data.end(), "Queried scene resource does not exist");
        return *it;
      }

      // operator("...") enables named lookup
      constexpr resrc_type &operator()(std::string_view name) {
        met_trace();
        auto it = rng::find(m_data, name, &resrc_type::name);
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
      
    public: // GL-side packing; always accessible to the underlying pipeline
      mutable gl_type gl;

    public: // Serialization
      void to_stream(std::ostream &str) const {
        met_trace();
        io::to_stream(m_data, str);
      }

      void fr_stream(std::istream &str) {
        met_trace();
        io::fr_stream(m_data, str);
        set_mutated(true);
      }
    };
  } // namespace detail

  // Component/Resource test helpers; check if Ty instantiates Component<>/Resource<>
  template <typename Ty>
  concept is_component = requires(Ty ty) { { detail::Component { ty } } -> std::same_as<Ty>; };  
  template <typename Ty>
  concept is_resource = requires(Ty ty) { { detail::Resource { ty } } -> std::same_as<Ty>; };  
  template <typename Ty>
  concept is_scene_data = is_component<Ty> || is_resource<Ty>;
} // namespace met

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

  template <typename Ty>
  struct tuple_size<::met::detail::Component<Ty>> 
  : integral_constant<size_t, 2> {};

  template <typename Ty>
  struct tuple_element<0, ::met::detail::Component<Ty>> { 
    using type = met::detail::Component<Ty>::value_type;
  };

  template <typename Ty>
  struct tuple_element<1, ::met::detail::Component<Ty>> {
    using type = met::detail::Component<Ty>::state_type;
  };
} // namespace std