#pragma once

#include <plog/Init.h>
#include <plog/Log.h>

constexpr int NABTO_LOG_ID = 42;

#define NPLOGV PLOGV_(NABTO_LOG_ID)
#define NPLOGD PLOGD_(NABTO_LOG_ID)
#define NPLOGI PLOGI_(NABTO_LOG_ID)
#define NPLOGW PLOGW_(NABTO_LOG_ID)
#define NPLOGE PLOGE_(NABTO_LOG_ID)
#define NPLOGF PLOGF_(NABTO_LOG_ID)
#define NPLOGN PLOGN_(NABTO_LOG_ID)

namespace nabto {
namespace webrtc {
namespace util {

/**
 * Initialize logging for the nabto util components.
 *
 * @param severity The severity level to log at.
 * @param appender The plog appender to use for logging
 */
inline void initLogger(enum plog::Severity severity,
                       plog::IAppender* appender) {
  plog::init<NABTO_LOG_ID>(severity, appender);
}
}  // namespace util
}  // namespace webrtc
}  // namespace nabto
