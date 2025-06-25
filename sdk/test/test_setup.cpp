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
    char* envLvl = std::getenv("NABTO_LOG_LEVEL");
    auto lvl = plog::Severity::none;
    if (envLvl) {
      auto strLvl = std::string(envLvl);
      if (strLvl == "trace") {
        lvl = plog::Severity::debug;
      } else if (strLvl == "info") {
        lvl = plog::Severity::info;
      } else if (strLvl == "error") {
        lvl = plog::Severity::error;
      }
    }

    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init<42>(lvl, &consoleAppender);

    // init logging for NabtoSignaling::core
    plog::init<nabto::webrtc::SIGNALING_LOGGER_INSTANCE_ID>(lvl,
                                                            &consoleAppender);
  }

  // Override this to define how to tear down the environment.
  void TearDown() override {}
};

testing::Environment* const foo_env =
    testing::AddGlobalTestEnvironment(new Environment);
