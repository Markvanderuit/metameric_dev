#pragma once

// #define GLM_FORCE_SIZE_FUNC
// #define GLM_MESSAGES
// #define GLM_FORCE_MESSAGES 

#include <glm/glm.hpp>
#include <glm/gtx/scalar_relational.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtx/range.hpp>
#include <glm/gtc/color_space.hpp>

namespace glm {
  namespace detail {
    template<typename V, typename T, bool Aligned>
    struct compute_sum{};

    template<typename T, qualifier Q, bool Aligned>
    struct compute_sum<vec<1, T, Q>, T, Aligned>
    {
      GLM_FUNC_QUALIFIER static T call(vec<1, T, Q> const& v)
      {
        return v.x;
      }
    };

    template<typename T, qualifier Q, bool Aligned>
    struct compute_sum<vec<2, T, Q>, T, Aligned>
    {
      GLM_FUNC_QUALIFIER static T call(vec<2, T, Q> const& v)
      {
        return v.x + v.y;
      }
    };
    
    template<typename T, qualifier Q, bool Aligned>
    struct compute_sum<vec<3, T, Q>, T, Aligned>
    {
      GLM_FUNC_QUALIFIER static T call(vec<3, T, Q> const& v)
      {
        return v.x + v.y + v.z;
      }
    };
    
    template<typename T, qualifier Q, bool Aligned>
    struct compute_sum<vec<4, T, Q>, T, Aligned>
    {
      GLM_FUNC_QUALIFIER static T call(vec<4, T, Q> const& v)
      {
        return v.x + v.y + v.z + v.w;
      }
    };
    
    template<typename V, typename T, bool Aligned>
    struct compute_prod{};

    template<typename T, qualifier Q, bool Aligned>
    struct compute_prod<vec<1, T, Q>, T, Aligned>
    {
      GLM_FUNC_QUALIFIER static T call(vec<1, T, Q> const& v)
      {
        return v.x;
      }
    };

    template<typename T, qualifier Q, bool Aligned>
    struct compute_prod<vec<2, T, Q>, T, Aligned>
    {
      GLM_FUNC_QUALIFIER static T call(vec<2, T, Q> const& v)
      {
        return v.x * v.y;
      }
    };
    
    template<typename T, qualifier Q, bool Aligned>
    struct compute_prod<vec<3, T, Q>, T, Aligned>
    {
      GLM_FUNC_QUALIFIER static T call(vec<3, T, Q> const& v)
      {
        return v.x * v.y * v.z;
      }
    };
    
    template<typename T, qualifier Q, bool Aligned>
    struct compute_prod<vec<4, T, Q>, T, Aligned>
    {
      GLM_FUNC_QUALIFIER static T call(vec<4, T, Q> const& v)
      {
        return v.x * v.y * v.z * v.w;
      }
    };
  } // namespace detail

  // Reduce sum
  template<length_t L, typename T, qualifier Q>
	GLM_FUNC_QUALIFIER T sum(vec<L, T, Q> const& v)
	{
		return detail::compute_sum<vec<L, T, Q>, T, detail::is_aligned<Q>::value>::call(v);
	}

  // Reduce prod
  template<length_t L, typename T, qualifier Q>
	GLM_FUNC_QUALIFIER T prod(vec<L, T, Q> const& v)
	{
		return detail::compute_prod<vec<L, T, Q>, T, detail::is_aligned<Q>::value>::call(v);
	}
} // namespace glm