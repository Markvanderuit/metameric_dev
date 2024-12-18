// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/serialization.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/scene/detail/utility.hpp>

namespace met::detail {
  /* Scene component.
     Wrapper around objects/emitters/etc present in the scene, to handle component name, active flag,
     and specializable state tracking to detect internal changes for e.g. the Uplifting object. */
  template <typename Ty>
  struct Component {
    using value_type = Ty;
    using state_type = SceneStateHandler<value_type>; 

  public:
    std::string name  = "";  // Loaded name of component
    value_type  value = { }; // Underlying component value
    state_type  state = { }; // State tracking object to detect internal changes
    
  public: // Boilerplate
    constexpr operator bool() const { return state.is_mutated(); };
    
    constexpr const value_type* operator->() const { return &value; }
    constexpr       value_type* operator->()       { return &value; }
    constexpr const value_type& operator*()  const { return value;  }
    constexpr       value_type& operator*()        { return value;  }

    constexpr friend auto operator<=>(const Component &, const Component &) = default;

  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      io::to_stream(name,  str);
      io::to_stream(value, str);
    }

    void from_stream(std::istream &str) {
      met_trace();
      io::from_stream(name,  str);
      io::from_stream(value, str);
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

  /* Scene component vector.
     Encapsulates std::vector<Component<Ty>> to handle named component lookups and
     and state tracking. */
  template <typename Ty>
  struct ComponentVector {
    using value_type = Ty;
    using cmpnt_type = Component<value_type>;
    using gl_type    = SceneGLHandler<value_type>;

  private:
    mutable bool            m_mutated = true;
    mutable bool            m_resized = false;
    mutable size_t          m_size    = 0;
    std::vector<cmpnt_type> m_data;

  public: // GL-side packing; always accessible to the underlying pipeline
    mutable gl_type gl;

  public: // State handling
    // Test each internal component for an update and, if component state is changed,
    // update the gl-side packed data
    bool update(const met::Scene &scene) {
      met_trace();

      if (m_data.size() == m_size) {
        for (auto &rsrc : m_data)
          rsrc.state.update(rsrc.value);
        m_resized = m_mutated = false;
        for (const auto &rsrc : m_data)
          if (rsrc.state.is_mutated())
            m_mutated = true;
      } else {
        for (auto &rsrc : m_data)
          rsrc.state.update(rsrc.value);
        m_resized = m_mutated = true;
        m_size    = m_data.size();
      }

      // If a gl packing type is specialized for the component type, update gl packing data
      gl.update(scene);          

      return m_mutated;
    }
    
    constexpr void set_mutated(bool b) {
      met_trace();
      for (auto &comp : m_data)
        comp.state.set_mutated(b);
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

    // operator[i] exposes throwing at()
    constexpr const cmpnt_type &operator[](uint i) const { return m_data.at(i); }
    constexpr       cmpnt_type &operator[](uint i)       { return m_data.at(i); }

    // operator(s) exposes throwing at()
    constexpr const cmpnt_type &operator()(std::string_view s) const {
      auto it = rng::find(m_data, s, &cmpnt_type::name);
      debug::check_expr(it != m_data.end(), 
        fmt::format("Could not find component of name {}", s));
      return *it;
    }
    constexpr cmpnt_type &operator()(std::string_view s) {
      auto it = rng::find(m_data, s, &cmpnt_type::name);
      debug::check_expr(it != m_data.end(), 
        fmt::format("Could not find component of name {}", s));
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
    constexpr void push_back(const cmpnt_type& v) 
                                    { m_data.push_back(v);   }
    constexpr void pop_back()       { m_data.pop_back();     }
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

    void from_stream(std::istream &str) {
      met_trace();
      io::from_stream(m_data, str);
      set_mutated(true);
    }
  };
} // namespace met::detail

/* Here follows structured binding support for met::Component/met::Resource types.
   I'm sure this won't backfire at all! */
namespace std {
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