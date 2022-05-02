#pragma once

namespace metameric {

#define MET_VERSION_MAJOR 1
#define MET_VERSION_MINOR 0

#define MET_ENABLE_DEBUG_ASSERT 1
#define MET_ENABLE_DEBUG_CALLBACK 1

// Enabled if we are on CUDA compiler
#ifdef __CUDACC__
#define MET_IS_CUDA_ENABLED
#endif

// Enabled if we are on CUDA device
#ifdef __CUDA_ARCH__
#define MET_IS_CUDA_CODE
#endif

#ifdef MET_IS_CUDA_ENABLED
#define MET_CPU_GPU __host__ __device__
#define MET_GPU __device__
#else
#define MET_CPU_GPU
#define MET_GPU
#endif

using uint = unsigned int;

// Extremely simple guard statement sugar
#define guard(expr,...) if (!(expr)) { return __VA_ARGS__ ; }
#define guard_continue(expr) if (!(expr)) { continue; }
#define guard_break(expr) if (!(expr)) { continue; }

// Declare move constr/operator and set copy constr/operator to explicitly deleted
// Assumes a swap(other) is implemented
#define MET_DECLARE_NONCOPYABLE(T)\
  T(const T &) = delete;\
  T & operator= (const T &) = delete;\
  T(T &&o) noexcept { swap(o); }\
  inline T & operator= (T &&o) noexcept { swap(o); return *this; }
  
// Declare common bitflag operands for a given (enum class) type
#define MET_DECLARE_BITFLAG(T)\
  constexpr T operator~(T a) { return (T) (~ (uint) a); }\
  constexpr T operator|(T a, T b) { return (T) ((uint) a | (uint) b); }\
  constexpr T operator&(T a, T b) { return (T) ((uint) a & (uint) b); }\
  constexpr T operator^(T a, T b) { return (T) ((uint) a ^ (uint) b); }\
  constexpr T& operator|=(T &a, T b) { return a = a | b; }\
  constexpr T& operator&=(T &a, T b) { return a = a & b; }\
  constexpr T& operator^=(T &a, T b) { return a = a ^ b; }\
  constexpr bool has_flag(T flags, T t) { return (uint) (flags & t) != 0u; }
} // namespace metameric