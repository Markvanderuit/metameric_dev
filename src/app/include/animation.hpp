#pragma once

#include <metameric/core/math.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/core/utility.hpp>

namespace met::anim {
  uint time_to_frame(float time, uint fps) {
    return static_cast<uint>(std::floor(time * fps));
  }

  // Implementation of linear function
  float f_linear(float x) {
    return x;
  }

  // Implementation of smoothstep function
  float f_smooth(float x) {
    if (x <= 0.f)      return 0.f;
    else if (x >= 1.f) return 1.f;
    else               return 3 * x * x - 2 * x * x * x;
  }

  // Harder runoff smoothstep function
  float f_smoother(float x) {
    return f_smooth(f_smooth(x));
  }

  // Type of motion; linear or smoothstep (so almost sigmoidal)
  enum class MotionType { eLinear, eSmooth, eSmoother };

  // Virtual base class of keyed motion types
  struct EventBase {
    virtual int eval(uint frame) = 0; // Return -1 before event, 0 during event, 1 after event
  };

  // Template for one-keyed event; set a value
  // to a specified input at an indicated time
  template <typename Ty>
  struct OneKeyEvent : EventBase { 
    // Aggregate type used internally for data
    struct InfoType {
      Ty &handle;    // Handle to affected value that is updated on motion
      Ty value;      // The then-set value
      float time;    // Time of set
      uint fps = 24; // Baseline fps
    } m_data;

  public:
    // Constructor accepts aggregate type
    OneKeyEvent(InfoType &&data) : m_data(data) {}

    // Operator performs motion between values in timeframe
    int eval(uint frame) override {
      uint frame_a = time_to_frame(m_data.time, m_data.fps);

      if (frame < frame_a) {
        return -1;
      } else if (frame > frame_a) {
        return 1;
      } else {
        m_data.handle = m_data.value;
        return 0;
      }
    }
  };

  // Template for two-keyed event; smoothly or linearly 
  // moves a value from start to finish between two indicated times
  template <typename Ty>
  struct TwoKeyEvent : EventBase {
    // Aggregate type used internally for data
    struct InfoType {
      Ty &handle;                                // Handle to affected value that is updated on motion
      std::array<Ty, 2> values;                  // A/B values between times
      std::array<float, 2> times;                // A/B times, rounded down to frames
      MotionType motion = MotionType::eSmoother; // Linear or smoothstep
      uint fps = 24;                             // Baseline fps
    } m_data;
    
  public:
    // Constructor accepts aggregate type
    TwoKeyEvent(InfoType &&data) : m_data(data) {}
    
    // Operator performs motion between values in timeframe
    int eval(uint frame) override {
      // Compute frame interval
      uint frame_a = time_to_frame(m_data.times[0], m_data.fps), 
            frame_b = time_to_frame(m_data.times[1], m_data.fps);
      
      if (frame < frame_a) {
        return -1;
      } else if (frame > frame_b) {
        return 1;
      } else {
        // Compute interpolation
        float x = (static_cast<float>(frame)   - static_cast<float>(frame_a))
                / (static_cast<float>(frame_b) - static_cast<float>(frame_a));
        float y;
        switch (m_data.motion) {
          case MotionType::eLinear:   y = f_linear(x);   break;
          case MotionType::eSmooth:   y = f_smooth(x);   break;
          case MotionType::eSmoother: y = f_smoother(x); break;
        }

        // Apply interpolation
        m_data.handle = m_data.values[0] + (m_data.values[1] - m_data.values[0]) * y;
        
        return 0;
      }
    }
  };

  // Explicit template instantiation for uplifting vertices, which hide std::variant but have accessor functions
  template<>
  struct TwoKeyEvent<Uplifting::Vertex> : EventBase {
    // Aggregate type used internally for data
    struct InfoType {
      Uplifting::Vertex  &handle;                // Handle to affected value that is updated on motion
      std::array<Colr, 2> values;                // A/B values between times
      std::array<float, 2> times;                // A/B times, rounded down to frames
      MotionType motion = MotionType::eSmoother; // Linear or smoothstep
      uint fps = 24;                             // Baseline fps
    } m_data;
    
  public:
    // Constructor accepts aggregate type
    TwoKeyEvent(InfoType &&data) : m_data(data) {}
    
    // Operator performs motion between values in timeframe
    int eval(uint frame) override {
      // Compute frame interval
      uint frame_a = time_to_frame(m_data.times[0], m_data.fps), 
            frame_b = time_to_frame(m_data.times[1], m_data.fps);
      
      if (frame < frame_a) {
        return -1;
      } else if (frame > frame_b) {
        return 1;
      } else {
        // Compute interpolation
        float x = (static_cast<float>(frame)   - static_cast<float>(frame_a))
                / (static_cast<float>(frame_b) - static_cast<float>(frame_a));
        float y;
        switch (m_data.motion) {
          case MotionType::eLinear:   y = f_linear(x);   break;
          case MotionType::eSmooth:   y = f_smooth(x);   break;
          case MotionType::eSmoother: y = f_smoother(x); break;
        }

        // Apply interpolation
        Colr colr = m_data.values[0] + (m_data.values[1] - m_data.values[0]) * y;
        m_data.handle.set_mismatch_position(colr);

        return 0;
      }
    }
  };

  // Shorthands
  template <typename Ty>
  void add_twokey(auto &events, typename TwoKeyEvent<Ty>::InfoType &&data) {
    events.push_back(std::make_shared<TwoKeyEvent<Ty>>(std::move(data)));
  }
  template <typename Ty>
  void add_onekey(auto &events, typename OneKeyEvent<Ty>::InfoType &&data) {
    events.push_back(std::make_shared<OneKeyEvent<Ty>>(std::move(data)));
  }
} // namespace met::anim