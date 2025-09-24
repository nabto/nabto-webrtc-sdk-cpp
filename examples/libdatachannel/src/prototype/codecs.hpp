#pragma once

#include <string>

namespace nabto::example {
    enum class Protocol {
        RTP,
        RTSP
    };
    
    enum class VideoCodec {
        NONE,
        H264,
        H265,
        VP8
    };
    
    enum class AudioCodec {
        NONE,
        OPUS,
        PCMU
    };

    inline const char* protocolToString(Protocol p) {
        switch (p) {
            case Protocol::RTP: return "rtp";
            case Protocol::RTSP: return "rtsp";
        }
    }

    inline const char* videoCodecToString(VideoCodec c) {
        switch (c) {
            case VideoCodec::NONE: return "none";
            case VideoCodec::H264: return "h264";
            case VideoCodec::H265: return "h265";
            case VideoCodec::VP8: return "vp8";
        }
    }

    inline const char* audioCodecToString(AudioCodec c) {
        switch (c) {
            case AudioCodec::NONE: return "none";
            case AudioCodec::OPUS: return "opus";
            case AudioCodec::PCMU: return "pcmu";
        }
    }
}

