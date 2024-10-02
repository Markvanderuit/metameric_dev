#pragma once

#include <metameric/core/image.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/primitives_render.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/window.hpp>
#include <small_gl/detail/program_cache.hpp>
#include <ffmpeg.h>
#include <av.h>
#include <codec.h>
#include <packet.h>
#include "videorescaler.h"
#include "avutils.h"
#include "format.h"
#include "formatcontext.h"
#include "codec.h"
#include "codeccontext.h"

namespace met {
  class VideoOutputStream {
    constexpr static auto output_fmt = "yuv420p";
    constexpr static auto input_fmt  = "rgb24";

    av::OutputFormat        m_ofmt;
    av::FormatContext       m_octx;
    av::Codec               m_codec;
    av::VideoEncoderContext m_encoder;
    av::VideoRescaler       m_rescaler;
    av::Stream              m_stream;
    eig::Array2u            m_size;
    int                     m_fps;
    int                     m_curr_frame;
    
  public:
    VideoOutputStream(fs::path output_path, eig::Array2u size, int fps = 24)
    : m_size(size),
      m_fps(fps),
      m_curr_frame(0)
    {
      met_trace();

      // Init ffmpeg
      av::init();
      av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

      m_ofmt.setFormat("H.264", output_path.filename().string());
      m_octx.setFormat(m_ofmt);

      // Specify encoder and codec
      m_codec = av::findEncodingCodec(m_ofmt);
      m_encoder = av::VideoEncoderContext { m_codec };

      // Specify encoder settings
      m_encoder.setWidth(size.x());
      m_encoder.setHeight(size.y());
      m_encoder.setPixelFormat(av::PixelFormat(output_fmt));
      m_encoder.setTimeBase(av::Rational(1, fps));
      m_encoder.setBitRate(48'000'000);
      m_encoder.open();

      // Prepare stream for write
      av::Stream m_stream = m_octx.addStream(m_encoder);
      m_stream.setFrameRate(fps);
      m_stream.setAverageFrameRate(fps);
      m_stream.setTimeBase(m_encoder.timeBase());

      // Prepare output for write
      m_octx.openOutput(output_path.string());
      m_octx.dump();
      m_octx.writeHeader();
      m_octx.flush();

      // Prepare rescaler
      m_rescaler = av::VideoRescaler(size.x(), size.y(), av::PixelFormat(output_fmt));
    }

    void write(const Image &input) {
      met_trace();

      // Input matches to hardcoded iinput size and format
      debug::check_expr(input.pixel_frmt() == Image::PixelFormat::eRGB);
      debug::check_expr(input.pixel_type() == Image::PixelType::eUChar);
      debug::check_expr(input.size().isApprox(m_size));

      // Copy image data into frame
      auto rgb24 = cast_span<const std::uint8_t>(input.data());
      av::VideoFrame input_frame(rgb24.data(), rgb24.size(), av::PixelFormat(input_fmt), m_size.x(), m_size.y());
      
      // Perform rescale to output format
      av::VideoFrame output_frame = m_rescaler.rescale(input_frame);

      // Generate packet with appropriate time data, and write to stream
      auto packet = m_encoder.encode(output_frame);
      packet.setStreamIndex(0);
      packet.setTimeBase(av::Rational(1, m_fps));
      packet.setPts(m_curr_frame++);
      packet.setDts(packet.pts());
      m_octx.writePacket(packet);
    }

    void close() {
      m_octx.writeTrailer();
      m_octx.close();
    }
  };

  namespace anim {
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
          m_data.handle.set_mismatch_position(m_data.values[0] + (m_data.values[1] - m_data.values[0]) * y);

          return 0;
        }
      }
    };
  } // namespace anim

  struct ApplicationInfo {
    // Direct load scene path
    fs::path scene_path = "";

    // Path of output file
    fs::path out_path = "";
  
    // Shader cache path
    fs::path shader_path = "resources/shaders/shaders.bin";
    
    // View settings
    std::string view_name  = "FFW View";
    float       view_scale = 1.f;

    // General output settings
    uint fps = 24;         // Framerate of video
    uint spp = 4;          // Sample count per frame
    uint spp_per_step = 4; // Samples taken per render call
    
    // Start/end times of config; 0 means not enforced
    float start_time = 0.f, end_time = 0.f;
    
    // Motion data
    using KeyEvent = std::shared_ptr<anim::EventBase>;
    std::vector<KeyEvent> events;

    // Applied to fill events data for a scene context
    std::function<void(ApplicationInfo &, Scene &)> init_events;
  };

  class Application {
    using RenderType = PathRenderPrimitive;
  
    // Handles
    ResourceHandle m_scene_handle;
    ResourceHandle m_window_handle;
    
    // Objects
    ApplicationInfo m_info;
    LinearScheduler m_scheduler;
    Sensor          m_sensor;
    Image           m_image;

  private: // Private functions
    // void init_events_cube_with_dot() {
    //   met_trace();
      
    //   auto &scene = m_scene_handle.getw<Scene>();

    //   // Interpolate constraint color
    //   auto &cvert = scene.components.upliftings[0]->verts[0];
    //   add_twokey<Uplifting::Vertex>({
    //     .handle = cvert,
    //     .values = { cvert.get_mismatch_position(), Colr { 0.110, 0.108, 0.105 } },
    //     .times  = { 0.f, 2.f },
    //     .fps    = m_info.fps
    //   });

    //   // Interpolate emitter position
    //   auto &emitter = scene.components.emitters[0].value;
    //   emitter.transform.position = { 96.f, 280.f, 96.f };
    //   add_twokey<eig::Vector3f>({
    //     .handle = emitter.transform.position,
    //     .values = { emitter.transform.position, eig::Vector3f { 16.f, 360.f, 16.f }},
    //     .times  = { 0.f, 2.f },
    //     .fps    = m_info.fps
    //   });
    // }

    // void init_events_scene_0() {
    //   met_trace();
      
    //   auto &scene = m_scene_handle.getw<Scene>();

    //   auto &cube1 = scene.components.objects("Cube 1").value;
    //   auto &cube2 = scene.components.objects("Cube 2").value;

    //   // Move cubes, left to right
    //   add_twokey<float>({
    //     .handle = cube1.transform.position[0],
    //     .values = {0.825f, -0.5f},
    //     .times  = { 2.f, 4.f },
    //     .fps    = m_info.fps
    //   });
    //   add_twokey<float>({
    //     .handle = cube2.transform.position[0],
    //     .values = {0.5, -0.825f},
    //     .times  = { 2.f, 4.f },
    //     .fps    = m_info.fps
    //   });

    //   // Rotate cubes, some degrees
    //   float angle = 1.571f - (2.f - 1.571f);
    //   add_twokey<float>({
    //     .handle = cube1.transform.rotation[0],
    //     .values = { 2.f, angle },
    //     .times  = { 2.f, 4.f },
    //     .fps    = m_info.fps
    //   });
    //   add_twokey<float>({
    //     .handle = cube1.transform.rotation[0],
    //     .values = { 2.f, angle },
    //     .times  = { 2.f, 4.f },
    //     .fps    = m_info.fps
    //   });

    //   m_end_time = 6.f;
    // }

    // void init_events() {
    //   met_trace()

    //   auto &scene = m_scene_handle.getw<Scene>();
    //   auto &cvert = scene.components.upliftings("Default uplifting")->verts[1];

    //   add_twokey<Uplifting::Vertex>({
    //     .handle = cvert,
    //     .values = { cvert.get_mismatch_position(), Colr { 0.090, 0.089, 0.068 } },
    //     .times  = { 0.f, 2.f },
    //     .fps    = m_info.fps
    //   });
      

    //   // cvert

    //   // auto &gnome = scene.components.objects("Gnome").value;
    //   // auto &emitter1 = scene.components.emitters("Emitter1").value;
    //   // auto &emitter2 = scene.components.emitters("Emitter2").value;

    //   // gnome.transform.scaling = 0.5f;

    //   /* add_twokey<eig::Vector3f>({
    //     .handle = gnome.transform.position,
    //     .values = { gnome.transform.position,  (gnome.transform.position + eig::Vector3f(0.8f, 0, 0)).eval() },
    //     .times  = { .5f, 4.5f },
    //     .fps    = m_info.fps
    //   });
      
    //   add_twokey<eig::Vector3f>({
    //     .handle = gnome.transform.scaling,
    //     .values = { gnome.transform.scaling, (gnome.transform.scaling + eig::Vector3f(.5f)).eval() },
    //     .times  = { .5f, 4.5f },
    //     .fps    = m_info.fps
    //   }); */
      
    //   /* // Phase between two emitters
    //   emitter1.is_active        = true;
    //   emitter2.is_active        = true;
    //   emitter1.illuminant_scale = 1;
    //   emitter2.illuminant_scale = 0; */

    //  /*  add_twokey<float>({
    //     .handle = emitter1.illuminant_scale,
    //     .values = { 1.f, 0.f },
    //     .times  = { .5f, 4.5f },
    //     .fps    = m_info.fps
    //   }); */
    //   /* add_onekey<bool>({
    //     .handle = emitter1.is_active,
    //     .value  = false,
    //     .time   = 4.f
    //   }); */

    //   /* add_onekey<bool>({
    //     .handle = emitter2.is_active,
    //     .value  = true,
    //     .time   = .5f
    //   }); */
    //   /* add_twokey<float>({
    //     .handle = emitter2.illuminant_scale,
    //     .values = { 0.f, 1.f },
    //     .times  = { .5f, 4.5f },
    //     .fps    = m_info.fps
    //   }); */

    //   m_start_time = 0.f;
    //   m_end_time = 5.f;
    // }

    bool run_events(uint frame) {
      // If maximum time is specified and exceeded, we kill the run; otherwise, we keep going
      bool pass_time = m_info.end_time > 0.f && anim::time_to_frame(m_info.end_time, m_info.fps) > frame;

      // Exhaust motion data; return false if no motion is active anymore
      bool pass_events = rng::fold_left(m_info.events, false, 
        [frame](bool left, auto &f) { return f->eval(frame) <= 0 || left; });
    
      // Keep running while one is true
      return pass_time || pass_events;
    }

  public:
    // Application constructor
    Application(ApplicationInfo &&_info) 
    : m_info(std::move(_info)) {
      met_trace();

      // Initialize window (OpenGL context), as a resource owned by the scheduler
      m_window_handle = m_scheduler.global("window").init<gl::Window>({ 
        .swap_interval = 0, /* .flags = gl::WindowFlags::eDebug */
      });
      
      // Initialize program cache as resource ownedd by the scheduler;
      // load from file if a path is specified
      if (!m_info.shader_path.empty() && fs::exists(m_info.shader_path)) {
        m_scheduler.global("cache").set<gl::detail::ProgramCache>(m_info.shader_path);
      } else {
        m_scheduler.global("cache").set<gl::detail::ProgramCache>({ });
      }

      // Initialize scene as a resource owned by the scheduler
      m_scene_handle = m_scheduler.global("scene").set<Scene>({ /* ... */ });
      
      // Load scene data from path and push to gl
      debug::check_expr(fs::exists(m_info.scene_path));
      auto &scene = m_scene_handle.getw<Scene>();
      scene.load(m_info.scene_path);

      // We use the scheduler to ensure scene data and spectral constraints are all handled properly
      m_scheduler.task("scene_handler").init<LambdaTask>(
        [](auto &info) { 
          met_trace();
          info.global("scene").getw<Scene>().update(); 
      });
      m_scheduler.task("gen_upliftings").init<GenUpliftingsTask>(256); // build many, not few
      m_scheduler.task("gen_objects").init<GenObjectsTask>();
      m_scheduler.task("render").init<LambdaTask>([&](auto &info) {
        met_trace();

        const auto &scene = info.global("scene").getr<Scene>();
        auto &renderer = info.global("renderer").getw<RenderType>();

        // Reset renderer internal film
        renderer.reset(m_sensor, scene);

        // Render frame over several iterations
        for (uint i = 0; i < m_info.spp; i += m_info.spp_per_step)
          renderer.render(m_sensor, scene);
        
        // Get frame data
        renderer.film().get(cast_span<float>(m_image.data()));

        // Clip HDR output
        std::for_each(std::execution::par_unseq,
          range_iter(cast_span<float>(m_image.data())),
          [](float &f) { f = std::clamp(f, 0.f, 1.f); });
      });

      // Initialize sensor from scene view
      {
        auto &view = scene.components.views(m_info.view_name).value;
      
        eig::Affine3f trf_rot = eig::Affine3f::Identity();
        trf_rot *= eig::AngleAxisf(view.camera_trf.rotation.x(), eig::Vector3f::UnitY());
        trf_rot *= eig::AngleAxisf(view.camera_trf.rotation.y(), eig::Vector3f::UnitX());
        trf_rot *= eig::AngleAxisf(view.camera_trf.rotation.z(), eig::Vector3f::UnitZ());

        auto dir = (trf_rot * eig::Vector3f(0, 0, 1)).normalized().eval();
        auto eye = -dir; 
        auto cen = (view.camera_trf.position + dir).eval();

        detail::Arcball arcball = {{
          .fov_y    = view.camera_fov_y * std::numbers::pi_v<float> / 180.f,
          .aspect   = static_cast<float>(view.film_size.x()) / static_cast<float>(view.film_size.y()),
          .dist     = 1,
          .e_eye    = eye,
          .e_center = cen,
          .e_up     = { 0, -1, 0 } // flip for video output
        }};
        
        m_sensor.film_size =( view.film_size.cast<float>() * m_info.view_scale).cast<uint>().eval();
        m_sensor.proj_trf  = arcball.proj().matrix();
        m_sensor.view_trf  = arcball.view().matrix();
        m_sensor.flush();
      }

      // Initialize renderer and output buffer
      m_scheduler.global("renderer").init<RenderType>({
        .spp_per_iter = m_info.spp_per_step,
        .cache_handle = m_scheduler.global("cache")
      });
      m_image = {{
        .pixel_frmt = Image::PixelFormat::eRGBA,
        .pixel_type = Image::PixelType::eFloat,
        .color_frmt = Image::ColorFormat::eLRGB,
        .size       = m_sensor.film_size
      }};

      // Instantiate motions for scene animation
      m_info.init_events(m_info, m_scene_handle.getw<Scene>());
    }

    void run() {
      met_trace();

      auto &scene = m_scene_handle.getw<Scene>();
      auto &window = m_window_handle.getw<gl::Window>();

      // Begin video output
      VideoOutputStream os(m_info.out_path.string(), m_sensor.film_size, m_info.fps);
      
      for (uint frame = anim::time_to_frame(m_info.start_time, m_info.fps); ; ++frame) {
        fmt::print("\tGenerating ({}): s={}, f={}\n", m_info.scene_path.filename().string(), frame / m_info.fps, frame);

        // Evaluate motion; exit loop if no more animations are left
        guard_break(run_events(frame));

        // Perform render step
        m_scheduler.run();
          
        // Convert and write to stream
        auto rgb8 = m_image.convert({ 
          .pixel_frmt = Image::PixelFormat::eRGB,
          .pixel_type = Image::PixelType::eUChar,
          .color_frmt = Image::ColorFormat::eSRGB
        });
        os.write(rgb8);

        // Handle window events every full second
        if (frame % m_info.fps == 0) {
          window.swap_buffers();
          window.poll_events();
          met_trace_frame()
        }
      }

      // End video output
      os.close();
    }
  };
} // namespace met

template <typename Ty>
void add_twokey(auto &events, typename met::anim::TwoKeyEvent<Ty>::InfoType &&data) {
  events.push_back(std::make_shared<met::anim::TwoKeyEvent<Ty>>(std::move(data)));
}

template <typename Ty>
void add_onekey(auto &events, typename met::anim::OneKeyEvent<Ty>::InfoType &&data) {
  events.push_back(std::make_shared<met::anim::OneKeyEvent<Ty>>(std::move(data)));
}

// Application entry point
int main() {
  try {
    met_trace();

    using namespace met;

    ApplicationInfo scene_0_info = {
      .scene_path   = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_0.json",
      .out_path     = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_0.mp4",
      .view_name    = "FFW view",
      .view_scale   = 0.25f,
      .fps          = 30u,
      .spp          = 4u,
      .spp_per_step = 4u,
      .start_time   = 0.f,
      .end_time     = 6.f,
      .init_events  = [](auto &info, Scene &scene) {
        met_trace();
        
        auto &cube1 = scene.components.objects("Cube 1").value;
        auto &cube2 = scene.components.objects("Cube 2").value;

        float move_start_time = 1.f, move_end_time = 3.5f;

        // Move cubes, left to right
        add_twokey<float>(info.events, {
          .handle = cube1.transform.position[0],
          .values = {0.825f, -0.5f},
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });
        add_twokey<float>(info.events, {
          .handle = cube2.transform.position[0],
          .values = {0.5, -0.825f},
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });

        // Rotate cubes, some degrees
        float angle = 1.571f - (2.f - 1.571f);
        add_twokey<float>(info.events, {
          .handle = cube1.transform.rotation[0],
          .values = { 2.f, angle },
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });
        add_twokey<float>(info.events, {
          .handle = cube2.transform.rotation[0],
          .values = { 2.f, angle },
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });
      }
    };

    ApplicationInfo scene_1a_info = {
      .scene_path   = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1a.json",
      .out_path     = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1a.mp4",
      .view_name    = "FFW view",
      .view_scale   = 0.5f,
      .fps          = 30u,
      .spp          = 4u,
      .spp_per_step = 4u,
      .start_time   = 0.f,
      .end_time     = 6.f,
      .init_events  = [](auto &info, Scene &scene) {
        met_trace();

        auto &cvert = scene.components.upliftings[0]->verts[0];
        auto &light = scene.components.emitters[0].value;
        
        float move_start_time = 1.f, move_end_time = 4.f;
        
        // Rotate light around
        add_twokey<eig::Vector3f>(info.events,{
          .handle = light.transform.position,
          .values = { light.transform.position, eig::Vector3f { 128, 200, 128 } },
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });
      }
    };

    ApplicationInfo scene_1b_info = {
      .scene_path   = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1b.json",
      .out_path     = "C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Fast forward/Scenes/scene_1b.mp4",
      .view_name    = "FFW view",
      .view_scale   = 0.5f,
      .fps          = 30u,
      .spp          = 4u,
      .spp_per_step = 4u,
      .start_time   = 0.f,
      .end_time     = 6.f,
      .init_events  = [](auto &info, Scene &scene) {
        met_trace();

        auto &cvert = scene.components.upliftings[0]->verts[0];
        auto &light = scene.components.emitters[0].value;
        
        float move_start_time = 1.f, move_end_time = 3.5f;
        
        // Rotate light around
        add_twokey<eig::Vector3f>(info.events,{
          .handle = light.transform.position,
          .values = { light.transform.position, eig::Vector3f { 128, 200, 128 } },
          .times  = { move_start_time, move_end_time },
          .fps    = info.fps
        });
      }
    };
    
    // Queue processes all moved info objects
    std::queue<ApplicationInfo> queue;
    queue.push(std::move(scene_0_info));
    queue.push(std::move(scene_1a_info));
    queue.push(std::move(scene_1b_info));

    // Exhaust input
    while (!queue.empty()) {
      auto task = queue.front();
      queue.pop();

      debug::check_expr(fs::exists(task.scene_path));
      fmt::print("Starting {}\n", task.scene_path.string());

      // Overwrite quality settings for consistenncy
      task.view_scale   = 1.f;
      task.spp          = 256u;
      task.spp_per_step = 4u;
      
      // Application consumes task
      Application app(std::move(task));
      app.run();
    }
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}