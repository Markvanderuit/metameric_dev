#pragma once

#include <metameric/core/image.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/primitives_render.hpp>
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

      m_ofmt.setFormat(std::string(), output_path.filename().string());
      m_octx.setFormat(m_ofmt);

      // Specify encoder and codec
      m_codec = av::findEncodingCodec(m_ofmt);
      m_encoder = av::VideoEncoderContext { m_codec };

      // Specify encoder settings
      m_encoder.setWidth(size.x());
      m_encoder.setHeight(size.y());
      m_encoder.setPixelFormat(av::PixelFormat(output_fmt));
      m_encoder.setTimeBase(av::Rational(1, fps));
      // encoder.setBitRate(1000'000);
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

  } // namespace anim

  // Application create settings
  struct RunInfo {
    // Direct load scene path
    fs::path scene_path = "";

    // Shader cache path
    fs::path shader_path = "resources/shaders/shaders.bin";
  };

  // Application setup function
  void init(RunInfo info) {
    met_trace();
    
    fmt::print(
      "Starting...\n  range   : {}-{} nm\n  samples : {}\n  bases   : {}\n  loading : {}\n",
      wavelength_min, wavelength_max, wavelength_samples, wavelength_bases, 
      info.scene_path.string());

    // Scheduler is responsible for handling application tasks, 
    // task resources, and the program runtime loop
    LinearScheduler scheduler;

    // Initialize window (OpenGL context), as a resource owned by the scheduler
    auto &window = scheduler.global("window").init<gl::Window>({ 
      .swap_interval = 0, .flags = gl::WindowFlags::eDebug
    }).getw<gl::Window>();

    // Enable OpenGL debug messages, if requested
    if constexpr (met_enable_debug) {
      gl::debug::enable_messages(gl::DebugMessageSeverity::eLow, gl::DebugMessageTypeFlags::eAll);
      gl::debug::insert_message("OpenGL debug messages are active!", gl::DebugMessageSeverity::eLow);
    }

    // Initialize program cache as resource ownedd by the scheduler;
    // load from file if a path is specified
    if (!info.shader_path.empty() && fs::exists(info.shader_path)) {
      scheduler.global("cache").set<gl::detail::ProgramCache>(info.shader_path);
    } else {
      scheduler.global("cache").set<gl::detail::ProgramCache>({ });
    }

    // Initialize program cache and scene data as resources owned by the scheduler
    // and not a specific schedule task
    scheduler.global("scene").set<Scene>({ });

    {
      VideoOutputStream os("output.mp4", { 512, 512 }, 60);

      Image image = {{
        .pixel_frmt = Image::PixelFormat::eRGB,
        .pixel_type = Image::PixelType::eFloat,
        .color_frmt = Image::ColorFormat::eNone,
        .size       = { 512, 512 }
      }};
      image.clear({ 0.7, 0.2, 0.7, 1.f });

      for (uint s = 0; s < 9; ++s) {
        for (uint i = 0; i < 60; ++i) {
          eig::Array4f v = 0.f;
          v[s % 3] = static_cast<float>(i) / 60.f;
          image.clear(v);

          auto rgb8 = image.convert({ .pixel_type = Image::PixelType::eUChar });
          os.write(rgb8);
        }
      }

      os.close();
    }

    /* // Load scene if a scene path is provided
    if (!info.scene_path.empty())
      scheduler.global("scene").getw<Scene>().load(info.scene_path);

    // Initialize renderer
    PathRenderPrimitive renderer = {{
      .spp_per_iter = 1u,
      .max_depth    = PathRecord::path_max_depth,
      .cache_handle = scheduler.global("cache")
    }}; */

    // {
    //   // ...
    // }

    // Attempt to save shader cache, if exists
    if (!info.shader_path.empty())
      scheduler.global("cache").getr<gl::detail::ProgramCache>().save(info.shader_path);
  }
} // namespace met

// Application entry point
int main() {
  try {
    met::init({ /* .scene_path = "path/to/file.json" */ });
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}