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
  #include <Tracy.hpp>
  #include <TracyOpenGL.hpp>

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
  #define met_trace_alloc(ptr, size)\
    TracyAlloc(ptr, size)
  #define met_trace_alloc_n(alloc_name, ptr, size)\
    TracyAllocN(ptr, size, alloc_name)
  #define met_trace_free(ptr)\
    TracyFree(ptr)
  #define met_trace_free_n(alloc_name, ptr)\
    TracyFreeN(ptr, alloc_name)

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
#endif // MET_ENABLE_TRACY