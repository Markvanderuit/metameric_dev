#include <algorithm>
#include <iostream>
#include <numeric>
#include <vector>
#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <thrust/for_each.h>

#define MET_IS_CUDA_ENABLED

#ifdef MET_IS_CUDA_ENABLED
#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#define MET_GPU __device__
#define MET_CPU_GPU __host__ __device__
#endif

#ifndef MET_IS_CUDA_ENABLED
#define MET_GPU
#define MET_CPU_GPU
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#endif

using uint = unsigned int;



template <typename F>
void parallel_for(uint n_items, F function) {
#ifdef MET_IS_CUDA_ENABLED
  // Launch CUDA GPU kernel
  auto kernel = &CUDAKernel<F>; // TODO: are these cached???
  uint block_size = 256; // TODO: determine optimal block size on the fly
  uint grid_size = (n_items + block_size - 1) / block_size;
  kernel<<<grid_size, block_size>>>(n_items, function);
#else
  // Launch TBB operation instead
  tbb::parallel_for<size_t>(0, n_items, function);
  // tbb::parallel_for<size_t>(0, n_items, [&](auto range) {
  //   function(range);
  // });
#endif
}

/* template <typename Value, typename F>
Value parallel_reduce(uint n_items, Value base, F function) {
#ifdef MET_IS_CUDA_ENABLED
  
#else
  return tbb::parallel_reduce<size_t>(0, n_items, base, function);
  // return tbb::parallel_reduce<size_t>(0, n_items, base, [=](tbb::blocked_range<int> r, Value t) {
  //   for (int i = r.begin(); i < r.end(); ++i) {
  //     function(t);
  //   }
  // });
#endif
} */



int main(int argc, char** argv) {
  uint n_items = 32;
  thrust::host_vector<uint> v(n_items);
  std::iota(v.begin(), v.end(), 0);
  thrust::device_vector<uint> v_ = v;

  thrust::for_each(v_.begin(), v_.end(), [] MET_CPU_GPU (uint &i) {
    printf("%i\n", i);
  });

  auto v_ptr = thrust::raw_pointer_cast(v_.data());
  parallel_for(v_.size(), [=] MET_CPU_GPU (uint &i) mutable {
    printf("%i\n", v_ptr[i]);
  });

  // for (auto& i : v) {
  //   printf("%i\n", v_[i]);
  // }




  // std::vector<uint> v(n_items);

  // parallel_for(n_items, [&] MET_CPU_GPU (uint i) {
  //   printf("%i\n", v[i]);
  // });

  // uint r = parallel_reduce(n_items, 0, [&] MET_CPU_GPU (uint i, uint total) {
  //   total += v[i];
  // });
  
  // printf("total: %i\n", r);

  return 0;
}