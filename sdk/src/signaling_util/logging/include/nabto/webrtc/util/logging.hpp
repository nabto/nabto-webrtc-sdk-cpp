#pragma once

#include <plog/Init.h>
#include <plog/Log.h>
namespace nabto {
namespace example {
enum nabto_log_id : std::uint8_t { NABTO_LOG_ID = 42 };
}  // namespace example
}  // namespace nabto

#define NPLOGV PLOGV_(nabto::example::NABTO_LOG_ID)
#define NPLOGD PLOGD_(nabto::example::NABTO_LOG_ID)
#define NPLOGI PLOGI_(nabto::example::NABTO_LOG_ID)
#define NPLOGW PLOGW_(nabto::example::NABTO_LOG_ID)
#define NPLOGE PLOGE_(nabto::example::NABTO_LOG_ID)
#define NPLOGF PLOGF_(nabto::example::NABTO_LOG_ID)
#define NPLOGN PLOGN_(nabto::example::NABTO_LOG_ID)

namespace nabto {
namespace example {
static void initLogger(enum plog::Severity severity,
                       plog::IAppender* appender) {
  plog::init<NABTO_LOG_ID>(severity, appender);
}
}  // namespace example
}  // namespace nabto
