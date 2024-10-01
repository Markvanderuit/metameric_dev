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
  constexpr static uint video_output_w   = 256;
  constexpr static uint video_output_h   = 256;
  constexpr static uint video_output_fps = 24;

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
    uint time_to_frame(float time, uint fps = 24) {
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

    // Type of motion; linear or smoothstep (so almost sigmoidal)
    enum class MotionType { eLinear, eSmooth };

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
        Ty &handle; // Handle to affected value that is updated on motion
        Ty value;   // The then-set value
        float time; // Time of set
      } m_data;

    public:
      // Constructor accepts aggregate type
      OneKeyEvent(InfoType &&data) : m_data(data) {}

      // Operator performs motion between values in timeframe
      int eval(uint frame) override {
        uint frame_a = time_to_frame(m_data.time);

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
        Ty &handle;                                   // Handle to affected value that is updated on motion
        std::array<Ty, 2> values;                     // A/B values between times
        std::array<float, 2> times;                   // A/B times, rounded down to frames
        MotionType motion_type = MotionType::eSmooth; // Linear or smoothstep
      } m_data;
      
    public:
      // Constructor accepts aggregate type
      TwoKeyEvent(InfoType &&data) : m_data(data) {}
      
      // Operator performs motion between values in timeframe
      int eval(uint frame) override {
        // Compute frame interval
        uint frame_a = time_to_frame(m_data.times[0]), 
             frame_b = time_to_frame(m_data.times[1]);
        
        if (frame < frame_a) {
          return -1;
        } else if (frame > frame_b) {
          return 1;
        } else {
          // Compute interpolation
          float x = (static_cast<float>(frame)   - static_cast<float>(frame_a))
                  / (static_cast<float>(frame_b) - static_cast<float>(frame_a));
          float y = m_data.motion_type == MotionType::eLinear 
                  ? f_linear(x) : f_smooth(x);

          // Apply interpolation
          m_data.handle = m_data.values[0] + (m_data.values[1] - m_data.values[0]) * y;
          
          return 0;
        }
      }
    };
  } // namespace anim

  struct Application {
    struct CreateInfo {
      // Direct load scene path
      fs::path scene_path = "";

      // Shader cache path
      fs::path shader_path = "resources/shaders/shaders.bin";

      // General output settings
      uint fps; // Framerate of video
      uint spp; // Sample count per frame

      // Samples taken per render call
      uint spp_per_step = 4;
    };

  private: // Private members
    using RenderType = PathRenderPrimitive;
  
    // Handles
    ResourceHandle m_scene_handle;
    ResourceHandle m_window_handle;
    ResourceHandle m_render_handle;
    
    // Objects
    CreateInfo      m_info;
    LinearScheduler m_scheduler;
    Sensor          m_sensor;
    Image           m_image;

    // Motion data
    using KeyEvent = std::unique_ptr<anim::EventBase>;
    std::vector<KeyEvent> m_events;
    float                 m_maximum_time = 0.f;

  private: // Private functions
    template <typename Ty>
    void add_twokey(anim::TwoKeyEvent<Ty>::InfoType &&data) {
      m_events.push_back(std::make_unique<anim::TwoKeyEvent<Ty>>(std::move(data)));
    }

    template <typename Ty>
    void add_onekey(anim::OneKeyEvent<Ty>::InfoType &&data) {
      m_events.push_back(std::make_unique<anim::OneKeyEvent<Ty>>(std::move(data)));
    }

    void init_events() {
      met_trace()

      auto &scene = m_scene_handle.getw<Scene>();
      auto &gnome = scene.components.objects("Gnome").value;
      auto &emitter1 = scene.components.emitters("Emitter1").value;
      auto &emitter2 = scene.components.emitters("Emitter2").value;

      gnome.transform.scaling = 0.5f;

      add_twokey<eig::Vector3f>({
        .handle = gnome.transform.position,
        .values = { gnome.transform.position,  (gnome.transform.position + eig::Vector3f(0.8f, 0, 0)).eval() },
        .times  = { .5f, 4.5f }
      });
      
      add_twokey<eig::Vector3f>({
        .handle = gnome.transform.scaling,
        .values = { gnome.transform.scaling, (gnome.transform.scaling + eig::Vector3f(.5f)).eval() },
        .times  = { .5f, 4.5f }
      });
      
      // Phase between two emitters
      emitter1.is_active        = true;
      emitter2.is_active        = true;
      emitter1.illuminant_scale = 1;
      emitter2.illuminant_scale = 0;

      add_twokey<float>({
        .handle = emitter1.illuminant_scale,
        .values = { 1.f, 0.f },
        .times  = { .5f, 4.5f }
      });
      /* add_onekey<bool>({
        .handle = emitter1.is_active,
        .value  = false,
        .time   = 4.f
      }); */

      /* add_onekey<bool>({
        .handle = emitter2.is_active,
        .value  = true,
        .time   = .5f
      }); */
      add_twokey<float>({
        .handle = emitter2.illuminant_scale,
        .values = { 0.f, 1.f },
        .times  = { .5f, 4.5f }
      });

      m_maximum_time = 5.0;
    }

    bool run_events(uint frame) {
      // If maximum time is specified and exceeded, we kill the run; otherwise, we keep going
      bool pass_time = m_maximum_time > 0.f && anim::time_to_frame(m_maximum_time) > frame;

      // Exhaust motion data; return false if no motion is active anymore
      bool pass_events = rng::fold_left(m_events, false, 
        [frame](bool left, auto &f) { return f->eval(frame) <= 0 || left; });
    
      // Keep running while one is true
      return pass_time || pass_events;
    }

    void render() {
      met_trace_full();

      // Push scene data to gl
      auto &scene = m_scene_handle.getw<Scene>();
      scene.update();

      // Reset renderer internal film
      auto &renderer = m_render_handle.getw<RenderType>();
      renderer.reset(m_sensor, scene);

      // Render a few times
      for (uint i = 0; i < m_info.spp; i += m_info.spp_per_step)
        renderer.render(m_sensor, scene);
      
      // Get frame data
      renderer.film().get(cast_span<float>(m_image.data()));

      // Clamp HDR float data to prevent weird clipping
      std::for_each(std::execution::par_unseq,
        range_iter(cast_span<float>(m_image.data())),
        [](float &f) { f = std::clamp(f, 0.f, 1.f); });
    }

  public:
    // Application constructor
    Application(CreateInfo info) 
    : m_info(info) {
      met_trace();

      // Initialize window (OpenGL context), as a resource owned by the scheduler
      m_window_handle = m_scheduler.global("window").init<gl::Window>({ 
        .swap_interval = 0, /* .flags = gl::WindowFlags::eDebug */
      });
      
      // Initialize program cache as resource ownedd by the scheduler;
      // load from file if a path is specified
      if (!info.shader_path.empty() && fs::exists(info.shader_path)) {
        m_scheduler.global("cache").set<gl::detail::ProgramCache>(info.shader_path);
      } else {
        m_scheduler.global("cache").set<gl::detail::ProgramCache>({ });
      }

      // Initialize scene as a resource owned by the scheduler
      m_scene_handle = m_scheduler.global("scene").set<Scene>({ /* ... */ });
      
      // Load scene data from path and push to gl
      debug::check_expr(fs::exists(info.scene_path));
      auto &scene = m_scene_handle.getw<Scene>();
      scene.load(info.scene_path);
      scene.update();

      // We use the scheduler to ensure spectral constraints are all handled properly;
      // so run these two tasks a fair few times
      m_scheduler.task("gen_upliftings").init<GenUpliftingsTask>(256);
      m_scheduler.task("gen_objects").init<GenObjectsTask>();
      m_scheduler.run();
      scene.update();

      // Initialize sensor from scene view
      {
        auto &view = scene.components.views("View").value;
      
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
        
        m_sensor.film_size = view.film_size;
        m_sensor.proj_trf  = arcball.proj().matrix();
        m_sensor.view_trf  = arcball.view().matrix();
        m_sensor.flush();
      }

      // Initialize renderer and output buffer
      m_render_handle = m_scheduler.global("render").init<RenderType>({
        .spp_per_iter = info.spp_per_step,
        .max_depth    = PathRecord::path_max_depth,
        .cache_handle = m_scheduler.global("cache")
      });
      m_image = {{
        .pixel_frmt = Image::PixelFormat::eRGBA,
        .pixel_type = Image::PixelType::eFloat,
        .color_frmt = Image::ColorFormat::eLRGB,
        .size       = m_sensor.film_size
      }};

      // Instantiate motions for scene animation
      init_events();
    }

    void run() {
      met_trace();

      auto &scene = m_scene_handle.getw<Scene>();
      auto &window = m_window_handle.getw<gl::Window>();

      // Begin video output
      VideoOutputStream os("output.mp4", m_sensor.film_size, m_info.fps);

      for (uint frame = 0;;++frame) {
        fmt::print("Next: {}/{}\n", frame / m_info.fps, frame);

        // Evaluate motion; exit loop if no more animations are left
        guard_break(run_events(frame));

        // Perform render step
        render();
          
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

  // Application create settings
  struct RunInfo {
    // Direct load scene path
    fs::path scene_path = "";

    // Shader cache path
    fs::path shader_path = "resources/shaders/shaders.bin";
  };

  // Application setup function
  void run(std::string_view scene_path) {
    met_trace();

    fmt::print(
      "Starting...\n  range   : {}-{} nm\n  samples : {}\n  bases   : {}\n  loading : {}\n",
      wavelength_min, wavelength_max, wavelength_samples, wavelength_bases, 
      scene_path);

    Application app = {{
      .scene_path   = scene_path,
      .fps          = 24u,
      .spp          = 32u,
      .spp_per_step = 4u      
    }};

    app.run();
  }
} // namespace met

// Application entry point
int main() {
  try {
    met::run("C:/Users/markv/Documents/Drive/TU Delft/Projects/Indirect uplifting/Metameric Scenes/animated_gnome/animated_gnome.json");
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}