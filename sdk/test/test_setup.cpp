#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include <nabto/webrtc/device.hpp>

#include <gtest/gtest.h>

class Environment : public ::testing::Environment {
 public:
  ~Environment() override {}

  // Override this to define how to set up the environment.
  void SetUp() override {
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init<42>(plog::Severity::debug, &consoleAppender);

    // init logging for NabtoSignaling::core
    plog::init<nabto::signaling::SIGNALING_LOGGER_INSTANCE_ID>(
        plog::Severity::debug, &consoleAppender);
  }

  // Override this to define how to tear down the environment.
  void TearDown() override {}
};

testing::Environment* const foo_env =
    testing::AddGlobalTestEnvironment(new Environment);
