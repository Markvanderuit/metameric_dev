#pragma once

#include <metameric/core/fwd.hpp>
#include <functional>

#define MET_IS_CUDA_ENABLED

#ifdef MET_IS_CUDA_ENABLED
#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <nvfunctional>
#define MET_GPU __device__
#define MET_CPU_GPU __host__ __device__
#endif // MET_IS_CUDA_ENABLED

#ifndef MET_IS_CUDA_ENABLED
#define MET_GPU
#define MET_CPU_GPU
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#endif // n MET_IS_CUDA_ENABLED

namespace met {
  namespace detail {
#ifdef MET_IS_CUDA_ENABLED
    using FunctionType = nvstd::function<void(uint)>;

    __global__
    void CUDAKernel(uint n_items, FunctionType function) {
      uint i = blockIdx.x * blockDim.x + threadIdx.x;
      if (i >= n_items) {
        return;
      }

      function(i);
    }
#endif // MET_IS_CUDA_ENABLED
  }

void parallel_for(uint n_items, detail::FunctionType function) {
#ifdef MET_IS_CUDA_ENABLED
  // Launch CUDA GPU kernel
  auto kernel = &detail::CUDAKernel; // TODO: are these cached???
  uint block_size = 256; // TODO: determine optimal block size on the fly
  uint grid_size = (n_items + block_size - 1) / block_size;
  kernel<<<grid_size, block_size>>>(n_items, function);
#else
  // Launch TBB operation instead
  tbb::parallel_for<size_t>(0, n_items, function);
  // tbb::parallel_for<size_t>(0, n_items, [&](auto range) {
  //   function(range);
  // });
#endif // MET_IS_CUDA_ENABLED
}

  // using FunctionType = std::function<void(uint)>;
  
  // void parallel_func(uint n_items, FunctionType function);

  // template <typename F>
  // void parallel_cuda(uint n_items, F function);
} // namespace met