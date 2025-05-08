#include <gtest/gtest.h>
#include <nabto/signaling/signaling.hpp>

TEST(create_destroy, create_destroy) {
    nabto::signaling::SignalingDeviceConfig conf;
    auto ptr = nabto::signaling::SignalingDeviceFactory::create(conf);
}
