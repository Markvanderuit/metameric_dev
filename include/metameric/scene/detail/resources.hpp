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
    constexpr operator bool()    const { return m_mutated; }

  public: // Boilerplate
    constexpr const value_type &value() const { return m_value; }
    constexpr       value_type &value()       { set_mutated(true); return m_value; }
    
    constexpr const value_type *operator->() const { return &m_value;                    }
    constexpr       value_type *operator->()       { set_mutated(true); return &m_value; }
    constexpr const value_type &operator*()  const { return m_value;                     }
    constexpr       value_type &operator*()        { set_mutated(true); return m_value;  }

    constexpr friend auto operator<=>(const Resource &, const Resource &) = default;

  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      io::to_stream(name,    str);
      io::to_stream(m_value, str);
    }

    void from_stream(std::istream &str) {
      met_trace();
      io::from_stream(name,    str);
      io::from_stream(m_value, str);
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

  /* Scene resource vector.
     Encapsulates std::vector<Resource<Ty>> to handle named resource lookups and some minor
     syntactic sugar for easy resource initialization. */
  template <typename Ty>
  struct ResourceVector {
    using value_type = Ty;
    using resrc_type = Resource<value_type>;
    using gl_type    = SceneGLHandler<value_type>;

  private:
    std::vector<resrc_type> m_data;
    
  public: // GL-side packing; always accessible to the underlying pipeline
    mutable gl_type gl;

  public: // State handling
    // Reset each internal resource's state and, if state was changed, updated
    // the gl-side packed data
    bool update(const met::Scene &scene) {
      met_trace();
      
      // Get current state as return value
      bool mutated = is_mutated();

      // If a gl packing type is specialized for the component type, update gl packing data
      gl.update(scene);          

      // Reset state for next iteration
      set_mutated(false);

      return mutated;
    }

    constexpr void set_mutated(bool b) {
      met_trace();
      for (auto &rsrc : m_data)
        rsrc.set_mutated(b);
    }

    constexpr bool is_mutated() const {
      met_trace();
      for (const auto &rsrc : m_data)
        if (rsrc.is_mutated())
          return true;
      return false;
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

    // operator[...] exposes throwing at()
    constexpr const resrc_type &operator[](uint i) const { return m_data.at(i); }
    constexpr       resrc_type &operator[](uint i)       { return m_data.at(i); }

    // Bookkeeping; expose the underlying std::vector, instead of a direct pointer
    constexpr const auto &data() const { return m_data; }
    constexpr       auto &data()       { return m_data; }

    // Bookkeeping; expose miscellaneous std::vector member functions
    constexpr void insert(size_t i, const resrc_type& v) 
                                    { m_data.insert(m_data.begin() + i, v); }
    constexpr void resize(size_t i) { m_data.resize(i);      }
    constexpr void erase(size_t i)  { m_data.erase(m_data.begin() + i); }
    constexpr void push_back(const resrc_type& v) 
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

/* Here follows structured binding support for met::Resource types.
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
} // namespace std