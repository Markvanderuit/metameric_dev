#include <metameric/core/cuda.hpp>
// #include <cuda_runtime.h>
// #include <cuda_runtime_api.h>

namespace met {
  // namespace detail {
  //   template <typename F>
  //   __global__
  //   void CUDAKernel(uint n_items, F function) {
  //     uint i = blockIdx.x * blockDim.x + threadIdx.x;
  //     if (i >= n_items) {
  //       return;
  //     }

  //     function(i);
  //   }
  // } // namespace detail

  // /* template <typename F>
  // void parallel_cuda(uint n_items, F function) {
  //   // Launch CUDA GPU kernel
  //   auto kernel = &CUDAKernel<F>; // TODO: are these cached???
  //   uint block_size = 256; // TODO: determine optimal block size on the fly
  //   uint grid_size = (n_items + block_size - 1) / block_size;
  //   kernel<<<grid_size, block_size>>>(n_items, function); 
  // } */

  // void parallel_func(uint n_items, FunctionType function) {
  //   auto kernel = &detail::CUDAKernel<FunctionType>; // TODO: are these cached???
  //   uint block_size = 256;                   // TODO: determine optimal block size on the fly
  //   uint grid_size = (n_items + block_size - 1) / block_size;
  //   kernel<<<grid_size, block_size>>>(n_items, function); 
  // }
} // namespace met