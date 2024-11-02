#pragma once

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
      m_codec   = av::findEncodingCodec(m_ofmt);
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
} // namespace met