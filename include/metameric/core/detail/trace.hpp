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

// Insert Tracy scope statements if tracing is enabled
#ifndef MET_ENABLE_TRACY
  #define met_trace()
  #define met_trace_n(name)
  #define met_trace_gpu()
  #define met_trace_gpu_n(name)
  #define met_trace_full()
  #define met_trace_full_n(name)
  #define met_trace_frame()
  #define met_trace_alloc(ptr, size)
  #define met_trace_free(ptr)
  #define met_trace_alloc_n(alloc_name, ptr, size)
  #define met_trace_free_n(alloc_name, ptr)
  #define met_trace_init_context()
  #define met_trace_frame()
#else // MET_ENABLE_TRACY
  #include <glad/glad.h>
  #include <tracy/Tracy.hpp>
  #include <tracy/TracyOpenGL.hpp>

  // Insert CPU event trace
  #define met_trace()            ZoneScoped;
  #define met_trace_n(name)      ZoneScopedN(name)

  // Insert GPU event trace
  #define met_trace_gpu()        TracyGpuZone(__FUNCTION__)      
  #define met_trace_gpu_n(name)  TracyGpuZone(name)    
  
  // Insert CPU+GPU event traces
  #define met_trace_full()       met_trace(); met_trace_gpu();
  #define met_trace_full_n(name) met_trace_n(name); met_trace_gpu_n(name);
  
  // Insert memory event trace
  #define met_trace_alloc(ptr, size)                \
    TracyAlloc(ptr, size);
  #define met_trace_alloc_n(name, ptr, size)        \
    TracyAllocN(ptr, size, name);
  #define met_trace_free(ptr)                       \
    TracyFree(ptr);
  #define met_trace_free_n(name, ptr)               \
    TracyFreeN(ptr, name);
  #define met_trace_realloc(ptr, new_size)          \
    met_trace_free(ptr)                             \
    met_trace_alloc(ptr, new_size)
  #define met_trace_realloc_n(name, ptr, new_size)  \
    met_trace_free_n(name, ptr)                     \
    met_trace_alloc_n(name, ptr, new_size)

  // Signal active gpu context
  #define met_trace_init_context() TracyGpuContext;
  
  // Signal end of frame for event trace
  #define met_trace_frame()        TracyGpuCollect; FrameMark;

  #ifndef TRACY_ENABLE
    #define TRACY_ENABLE
  #endif // TRACY_ENABLE
  #ifndef TRACY_ON_DEMAND
  #define TRACY_ON_DEMAND
  #endif // TRACY_ON_DEMAND

  // Overload new/delete to enable tracy to track the malloc/free heap
  // NOTE: Nope. Not a good idea.
  /* inline
  void* operator new(std::size_t count) {
    auto ptr = malloc(count);
    met_trace_alloc(ptr, count);
    return ptr;
  }

  inline
  void operator delete(void* ptr) noexcept {
    free(ptr);
    met_trace_free(ptr);
  } */
#endif // MET_ENABLE_TRACY