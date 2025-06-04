#include <nabto/webrtc/device.hpp>

#include <gtest/gtest.h>

TEST(create_destroy, create_destroy) {
  nabto::signaling::SignalingDeviceConfig conf;
  auto ptr = nabto::signaling::SignalingDeviceFactory::create(conf);
}
