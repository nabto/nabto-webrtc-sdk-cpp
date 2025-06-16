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
  auto s1 = result[0];
  ASSERT_EQ(s1.urls.size(), 1);
  auto u1 = s1.urls[0];
  ASSERT_EQ(u1, "stun:stun.nabto.net");
  ASSERT_TRUE(s1.username.empty());
  ASSERT_TRUE(s1.credential.empty());
  auto s2 = result[1];
  ASSERT_EQ(s2.urls.size(), 0);
  ASSERT_TRUE(s2.username.empty());
  ASSERT_TRUE(s2.credential.empty());
}
