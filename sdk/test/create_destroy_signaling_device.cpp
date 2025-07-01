#include <nabto/webrtc/device.hpp>

#include <gtest/gtest.h>

TEST(create_destroy, create_destroy) {
  nabto::webrtc::SignalingDeviceConfig conf;
  auto ptr = nabto::webrtc::SignalingDeviceFactory::create(conf);
  auto version = ptr->version();
  ASSERT_TRUE(version.size() > 0);
}
