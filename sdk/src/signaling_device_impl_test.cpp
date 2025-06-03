#include "signaling_device_impl.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(signaling_device_impl, parseIceServersTest) {
  std::string iceServers = R"(
    {
        "iceServers": [{
            "urls": ["a"],
            "username": "b",
            "credential": "c"
        }]
    }
    )";
  std::vector<nabto::signaling::IceServer> result =
      nabto::signaling::SignalingDeviceImpl::parseIceServers(iceServers);
  ASSERT_EQ(result.size(), 1);
  auto s = result[0];
  ASSERT_EQ(s.credential, "c");
  ASSERT_EQ(s.username, "b");
  {
    std::vector<std::string> expected = {"a"};
    ASSERT_EQ(s.urls, expected);
  }
}

TEST(signaling_device_impl, parseIceServersExtraData) {
  std::string iceServers = R"(
    {
    "iceServers": [
      {
        "urls": ["stun:stun.nabto.net"]
      },
      {
        "urls": [],
        "bar": "baz"
      }
    ]
    }
    )";
  std::vector<nabto::signaling::IceServer> result =
      nabto::signaling::SignalingDeviceImpl::parseIceServers(iceServers);
  ASSERT_EQ(result.size(), 2);
}
