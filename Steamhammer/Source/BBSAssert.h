#pragma once

#include "Common.h"
#include <cstdio>
#include <cstdarg>
#include "Logger.h"
#include <sstream>
#include <stdarg.h>

#include <ctime>
#include <iomanip>

#define BBS_BREAK

#define BBS_ASSERT_ALL

#ifdef BBS_ASSERT_ALL
    #define BBS_ASSERT(cond, msg, ...) \
        do \
        { \
            if (!(cond)) \
            { \
                BlueBlueSky::Assert::ReportFailure(#cond, __FILE__, __LINE__, (msg), ##__VA_ARGS__); \
                BBS_BREAK \
            } \
        } while(0)

    #define BBS_ASSERT_WARNING(cond, msg, ...) \
        do \
        { \
            if (!(cond)) \
            { \
                BlueBlueSky::Assert::ReportFailure(#cond, __FILE__, __LINE__, (msg), ##__VA_ARGS__); \
            } \
        } while(0)
#else
    #define BBS_ASSERT(cond, msg, ...) 
#endif

namespace BlueBlueSky
{
    namespace Assert
    {
        void ShutDown();

        extern std::string lastErrorMessage;

        const std::string currentDateTime();

        void ReportFailure(const char * condition, const char * file, int line, const char * msg, ...);
    }
}
