#include "test_instance.hpp"

#include <nabto/webrtc/device.hpp>

#include <gtest/gtest.h>

TEST(connect, create_destroy) {
  auto ti = nabto::test::TestInstance::create();

  ASSERT_STRCASEEQ(ti->epUrl_.c_str(), "http://127.0.0.1:13745");
}

TEST(connect, ok) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createDevice();
  std::promise<void> promise;
  std::promise<void> closeProm;
  std::vector<nabto::webrtc::SignalingDeviceState> states;

  dev->addStateChangeListener([&promise, &closeProm, &states](
                                  nabto::webrtc::SignalingDeviceState state) {
    states.push_back(state);
    if (state == nabto::webrtc::SignalingDeviceState::CONNECTED) {
      promise.set_value();
    }
    if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
      closeProm.set_value();
    }
  });
  dev->start();
  std::future<void> f = promise.get_future();
  f.get();

  dev->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();

  ASSERT_EQ(states.size(), 3);
  ASSERT_EQ(states[0], nabto::webrtc::SignalingDeviceState::CONNECTING);
  ASSERT_EQ(states[1], nabto::webrtc::SignalingDeviceState::CONNECTED);
  ASSERT_EQ(states[2], nabto::webrtc::SignalingDeviceState::CLOSED);
}

TEST(connect, http_fail) {
  auto ti = nabto::test::TestInstance::create(false, true, false);

  auto dev = ti->createDevice();
  std::promise<void> promise;
  std::promise<void> closeProm;
  std::vector<nabto::webrtc::SignalingDeviceState> states;

  dev->addStateChangeListener([&promise, &closeProm, &states](
                                  nabto::webrtc::SignalingDeviceState state) {
    states.push_back(state);
    if (state == nabto::webrtc::SignalingDeviceState::WAIT_RETRY) {
      promise.set_value();
    }
    if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
      closeProm.set_value();
    }
  });
  dev->start();
  std::future<void> f = promise.get_future();
  f.get();

  dev->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();

  ASSERT_EQ(states.size(), 3);
  ASSERT_EQ(states[0], nabto::webrtc::SignalingDeviceState::CONNECTING);
  ASSERT_EQ(states[1], nabto::webrtc::SignalingDeviceState::WAIT_RETRY);
  ASSERT_EQ(states[2], nabto::webrtc::SignalingDeviceState::CLOSED);
}

TEST(connect, ws_fail) {
  auto ti = nabto::test::TestInstance::create(false, false, true);

  auto dev = ti->createDevice();
  std::promise<void> promise;
  std::promise<void> closeProm;
  std::vector<nabto::webrtc::SignalingDeviceState> states;

  dev->addStateChangeListener([&promise, &closeProm, &states](
                                  nabto::webrtc::SignalingDeviceState state) {
    states.push_back(state);
    if (state == nabto::webrtc::SignalingDeviceState::WAIT_RETRY) {
      promise.set_value();
    }
    if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
      closeProm.set_value();
    }
  });
  dev->start();
  std::future<void> f = promise.get_future();
  f.get();

  dev->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();

  ASSERT_EQ(states.size(), 3);
  ASSERT_EQ(states[0], nabto::webrtc::SignalingDeviceState::CONNECTING);
  ASSERT_EQ(states[1], nabto::webrtc::SignalingDeviceState::WAIT_RETRY);
  ASSERT_EQ(states[2], nabto::webrtc::SignalingDeviceState::CLOSED);
}

TEST(connect, reconnect_on_server_close) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createDevice();
  std::promise<void> promise;
  std::promise<void> waitPromise;
  std::promise<void> closeProm;
  std::vector<nabto::webrtc::SignalingDeviceState> states;

  dev->addStateChangeListener([&promise, &closeProm, &waitPromise, &states](
                                  nabto::webrtc::SignalingDeviceState state) {
    states.push_back(state);
    if (state == nabto::webrtc::SignalingDeviceState::CONNECTED) {
      promise.set_value();
    }
    if (state == nabto::webrtc::SignalingDeviceState::WAIT_RETRY) {
      waitPromise.set_value();
    }
    if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
      closeProm.set_value();
    }
  });
  dev->start();
  std::future<void> f = promise.get_future();
  f.get();

  ti->disconnectDevice();

  auto fw = waitPromise.get_future();
  fw.get();

  dev->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();

  ASSERT_EQ(states.size(), 4);
  ASSERT_EQ(states[0], nabto::webrtc::SignalingDeviceState::CONNECTING);
  ASSERT_EQ(states[1], nabto::webrtc::SignalingDeviceState::CONNECTED);
  ASSERT_EQ(states[2], nabto::webrtc::SignalingDeviceState::WAIT_RETRY);
  ASSERT_EQ(states[3], nabto::webrtc::SignalingDeviceState::CLOSED);
}
TEST(connect, connect_a_client) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createDevice();

  std::promise<void> connProm;
  std::promise<void> cliProm;
  std::promise<void> closeProm;
  std::vector<nabto::webrtc::SignalingDeviceState> states;

  dev->addNewChannelListener(
      [&cliProm](nabto::webrtc::SignalingChannelPtr conn, bool authorized) {
        cliProm.set_value();
      });

  dev->addStateChangeListener([&connProm, &closeProm, &states](
                                  nabto::webrtc::SignalingDeviceState state) {
    states.push_back(state);
    if (state == nabto::webrtc::SignalingDeviceState::CONNECTED) {
      connProm.set_value();
    }
    if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
      closeProm.set_value();
    }
  });

  dev->start();

  std::future<void> f = connProm.get_future();
  f.get();

  auto cliId = ti->createClient();

  ASSERT_TRUE(ti->connectClient(cliId));
  ASSERT_TRUE(ti->clientSendMessage(cliId, "test_message"));

  std::future<void> f2 = cliProm.get_future();
  f2.get();

  dev->close();

  std::future<void> f3 = closeProm.get_future();
  f3.get();
}

TEST(connect, connect_without_channel_handler) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createDevice();

  std::promise<void> connProm;
  std::promise<void> closeProm;
  std::vector<nabto::webrtc::SignalingDeviceState> states;

  dev->addStateChangeListener([&connProm, &closeProm, &states](
                                  nabto::webrtc::SignalingDeviceState state) {
    states.push_back(state);
    if (state == nabto::webrtc::SignalingDeviceState::CONNECTED) {
      connProm.set_value();
    }
    if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
      closeProm.set_value();
    }
  });

  dev->start();

  std::future<void> f = connProm.get_future();
  f.get();

  auto cliId = ti->createClient();

  ASSERT_TRUE(ti->connectClient(cliId));
  bool exceptionCaught = false;
  try {
    nabto::test::ProtocolError err = ti->clientWaitForErrorReceived(cliId);
  } catch (std::exception& e) {
    exceptionCaught = true;
  }
  ASSERT_TRUE(exceptionCaught);
  dev->close();

  std::future<void> f3 = closeProm.get_future();
  f3.get();
}

TEST(connect, connect_multiple_clients) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createConnectedDevice();

  std::promise<void> cliProm;
  std::promise<void> closeProm;
  std::vector<nabto::webrtc::SignalingDeviceState> states;

  size_t numberOfClients = 10;
  size_t n = 0;

  dev->addNewChannelListener(
      [&cliProm, &n, numberOfClients](nabto::webrtc::SignalingChannelPtr conn,
                                      bool authorized) {
        n++;
        if (n == numberOfClients) {
          cliProm.set_value();
        }
      });

  dev->addStateChangeListener(
      [&closeProm, &states](nabto::webrtc::SignalingDeviceState state) {
        states.push_back(state);
        if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
          closeProm.set_value();
        }
      });

  for (size_t i = 0; i < numberOfClients; i++) {
    auto cliId = ti->createClient();

    ASSERT_TRUE(ti->connectClient(cliId));
    ASSERT_TRUE(ti->clientSendMessage(cliId, "test_message"));
  }
  std::future<void> f2 = cliProm.get_future();
  f2.get();

  dev->close();

  std::future<void> f3 = closeProm.get_future();
  f3.get();
}

TEST(connect, client_disconnect_after_connect) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createConnectedDevice();

  std::promise<void> cliProm;
  std::promise<void> closeProm;
  std::vector<nabto::webrtc::SignalingDeviceState> states;

  nabto::webrtc::SignalingChannelPtr cliConn = nullptr;

  dev->addNewChannelListener(
      [&cliProm, &cliConn](nabto::webrtc::SignalingChannelPtr conn,
                           bool authorized) {
        cliConn = conn;
        cliProm.set_value();
      });

  dev->addStateChangeListener(
      [&closeProm, &states](nabto::webrtc::SignalingDeviceState state) {
        states.push_back(state);
        if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
          closeProm.set_value();
        }
      });

  auto cliId = ti->createClient();
  ASSERT_TRUE(ti->connectClient(cliId));
  ASSERT_TRUE(ti->clientSendMessage(cliId, "test_message"));

  std::future<void> f2 = cliProm.get_future();
  f2.get();

  std::promise<void> connEventProm;

  ASSERT_TRUE(cliConn != nullptr);
  cliConn->addStateChangeListener(
      [&connEventProm](nabto::webrtc::SignalingChannelState event) {
        if (event == nabto::webrtc::SignalingChannelState::OFFLINE) {
          connEventProm.set_value();
        }
      });

  ASSERT_TRUE(ti->disconnectClient(cliId));

  cliConn->sendMessage("test_message");

  std::future<void> f3 = connEventProm.get_future();
  f3.get();

  dev->close();

  std::future<void> fc = closeProm.get_future();
  fc.get();
}

TEST(connect, device_send_error) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createDeviceWithConnectedCli();
  std::promise<void> closeProm;

  dev.device->addStateChangeListener(
      [&closeProm](nabto::webrtc::SignalingDeviceState state) {
        if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
          closeProm.set_value();
        }
      });

  dev.channel->sendError(
      nabto::webrtc::SignalingError("ERROR_1", "test error"));

  nabto::test::ProtocolError err = ti->clientWaitForErrorReceived(dev.id);

  ASSERT_TRUE(err.code.compare("ERROR_1") == 0);
  ASSERT_TRUE(err.message.compare("test error") == 0);

  dev.device->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();
}

TEST(connect, device_send_error_no_msg) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createDeviceWithConnectedCli();
  std::promise<void> closeProm;

  dev.device->addStateChangeListener(
      [&closeProm](nabto::webrtc::SignalingDeviceState state) {
        if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
          closeProm.set_value();
        }
      });

  dev.channel->sendError(nabto::webrtc::SignalingError("ERROR_1", ""));

  nabto::test::ProtocolError err = ti->clientWaitForErrorReceived(dev.id);

  ASSERT_TRUE(err.code.compare("ERROR_1") == 0);
  ASSERT_TRUE(err.message.compare("") == 0);

  dev.device->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();
}

TEST(connect, device_receive_error) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createDeviceWithConnectedCli();
  std::promise<void> errorProm;
  std::promise<void> closeProm;

  dev.device->addStateChangeListener(
      [&closeProm](nabto::webrtc::SignalingDeviceState state) {
        if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
          closeProm.set_value();
        }
      });

  dev.channel->addErrorListener(
      [&errorProm](const nabto::webrtc::SignalingError& error) {
        ASSERT_TRUE(error.errorCode().compare("ERROR_1") == 0);
        ASSERT_TRUE(error.errorMessage().compare("test error") == 0);
        errorProm.set_value();
      });

  ti->clientSendError(dev.id, "ERROR_1", "test error");

  auto fe = errorProm.get_future();
  fe.get();

  dev.device->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();
}

TEST(connect, device_receive_error_no_msg) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createDeviceWithConnectedCli();
  std::promise<void> errorProm;
  std::promise<void> closeProm;

  dev.device->addStateChangeListener(
      [&closeProm](nabto::webrtc::SignalingDeviceState state) {
        if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
          closeProm.set_value();
        }
      });

  dev.channel->addErrorListener(
      [&errorProm](const nabto::webrtc::SignalingError& error) {
        ASSERT_TRUE(error.errorCode().compare("ERROR_1") == 0);
        ASSERT_TRUE(error.errorMessage().compare("") == 0);
        errorProm.set_value();
      });

  ti->clientSendError(dev.id, "ERROR_1", "");

  auto fe = errorProm.get_future();
  fe.get();

  dev.device->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();
}

TEST(data_formats, extra_device_connect_response_data) {
  auto ti = nabto::test::TestInstance::create(true, false, false);

  auto dev = ti->createDevice();
  std::promise<void> promise;
  std::promise<void> closeProm;
  std::vector<nabto::webrtc::SignalingDeviceState> states;

  dev->addStateChangeListener([&promise, &closeProm, &states](
                                  nabto::webrtc::SignalingDeviceState state) {
    states.push_back(state);
    if (state == nabto::webrtc::SignalingDeviceState::CONNECTED) {
      promise.set_value();
    }
    if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
      closeProm.set_value();
    }
  });
  dev->start();
  std::future<void> f = promise.get_future();
  f.get();

  dev->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();

  ASSERT_EQ(states.size(), 3);
  ASSERT_EQ(states[0], nabto::webrtc::SignalingDeviceState::CONNECTING);
  ASSERT_EQ(states[1], nabto::webrtc::SignalingDeviceState::CONNECTED);
  ASSERT_EQ(states[2], nabto::webrtc::SignalingDeviceState::CLOSED);
}

TEST(data_formats, ws_can_handle_unknown_message_types) {
  auto ti = nabto::test::TestInstance::create();

  auto dev = ti->createConnectedDevice();
  std::promise<void> closeProm;

  dev->addStateChangeListener(
      [&closeProm](nabto::webrtc::SignalingDeviceState state) {
        if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
          closeProm.set_value();
        }
      });

  ti->sendNewMessageType();

  dev->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();
}

TEST(reliablity, client_receives_all_messages) {
  auto ti = nabto::test::TestInstance::create();
  std::vector<std::string> messages = {"1", "2", "3"};

  auto dev = ti->createDeviceWithConnectedCli();
  std::promise<void> closeProm;

  dev.device->addStateChangeListener(
      [&closeProm](nabto::webrtc::SignalingDeviceState state) {
        if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
          closeProm.set_value();
        }
      });

  for (auto m : messages) {
    dev.channel->sendMessage(m);
  }

  std::vector<std::string> resp =
      ti->clientWaitForMessagesIsReceived(dev.id, messages);

  ASSERT_EQ(resp.size(), messages.size());
  for (size_t i = 0; i < resp.size(); i++) {
    ASSERT_STREQ(resp[i].c_str(), messages[i].c_str());
  }

  dev.device->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();
}

TEST(reliablity, device_receives_all_messages) {
  auto ti = nabto::test::TestInstance::create();
  std::vector<std::string> messages = {"1", "2", "3"};

  auto dev = ti->createDeviceWithConnectedCli();
  std::promise<void> closeProm;

  dev.device->addStateChangeListener(
      [&closeProm](nabto::webrtc::SignalingDeviceState state) {
        if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
          closeProm.set_value();
        }
      });

  std::promise<void> doneProm;
  size_t received = 0;
  dev.channel->addMessageListener(
      [&received, messages, &doneProm](const std::string& msg) {
        if (msg == "hello") {
          // Ignore initial hello message
          return;
        }
        if (received < messages.size()) {
          ASSERT_STREQ(msg.c_str(), messages[received].c_str());
        }
        received++;
        if (received == messages.size()) {
          doneProm.set_value();
        }
      });

  for (auto m : messages) {
    ti->clientSendMessage(dev.id, m);
  }

  auto f = doneProm.get_future();
  f.get();

  dev.device->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();
}

TEST(reliablity, client_receives_all_messages_after_reconnect) {
  auto ti = nabto::test::TestInstance::create();
  std::vector<std::string> messages = {"1", "2", "3"};

  auto dev = ti->createDeviceWithConnectedCli();
  std::promise<void> closeProm;

  dev.device->addStateChangeListener(
      [&closeProm](nabto::webrtc::SignalingDeviceState state) {
        if (state == nabto::webrtc::SignalingDeviceState::CLOSED) {
          closeProm.set_value();
        }
      });

  for (auto m : messages) {
    dev.channel->sendMessage(m);
  }

  std::vector<std::string> resp =
      ti->clientWaitForMessagesIsReceived(dev.id, messages);

  ASSERT_EQ(resp.size(), messages.size());
  for (size_t i = 0; i < resp.size(); i++) {
    ASSERT_STREQ(resp[i].c_str(), messages[i].c_str());
  }

  ti->disconnectDevice();

  for (auto m : messages) {
    dev.channel->sendMessage(m);
  }

  std::vector<std::string> resp2 =
      ti->clientWaitForMessagesIsReceived(dev.id, messages);

  ASSERT_EQ(resp2.size(), messages.size());
  for (size_t i = 0; i < resp2.size(); i++) {
    ASSERT_STREQ(resp2[i].c_str(), messages[i].c_str());
  }

  dev.device->close();
  std::future<void> f2 = closeProm.get_future();
  f2.get();
}

/** UTIL FUNCTIONS */

enum plog::Severity plogSeverity(std::string& logLevel) {
  if (logLevel == "trace") {
    return plog::Severity::debug;
  } else if (logLevel == "warn") {
    return plog::Severity::warning;
  } else if (logLevel == "info") {
    return plog::Severity::info;
  } else if (logLevel == "error") {
    return plog::Severity::error;
  }
  return plog::Severity::none;
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;

  char* envLvl = std::getenv("NABTO_LOG_LEVEL");
  std::string strLvl = "";
  if (envLvl) {
    strLvl = std::string(envLvl);
  }

  // init logging for Nabtonabto::webrtc::core
  plog::init<nabto::webrtc::SIGNALING_LOGGER_INSTANCE_ID>(plogSeverity(strLvl),
                                                          &consoleAppender);

  return RUN_ALL_TESTS();
}
