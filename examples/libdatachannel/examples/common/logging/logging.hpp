#pragma once

#include <plog/Init.h>
#include <plog/Log.h>

#define NABTO_LOG_ID 42

#define NPLOGV PLOGV_(NABTO_LOG_ID)
#define NPLOGD PLOGD_(NABTO_LOG_ID)
#define NPLOGI PLOGI_(NABTO_LOG_ID)
#define NPLOGW PLOGW_(NABTO_LOG_ID)
#define NPLOGE PLOGE_(NABTO_LOG_ID)
#define NPLOGF PLOGF_(NABTO_LOG_ID)
#define NPLOGN PLOGN_(NABTO_LOG_ID)

namespace nabto {
namespace example {
static void initLogger(enum plog::Severity severity = plog::Severity::none, plog::IAppender* appender = NULL) {
    plog::init<NABTO_LOG_ID>(severity, appender);
    return;
}
}  // namespace example
}  // namespace nabto
