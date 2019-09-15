#include "UABAssert.h"
#include "Config.h"

using namespace UAlbertaBot;

namespace UAlbertaBot
{
namespace Assert
{
    std::string lastErrorMessage;

    const std::string currentDateTime() 
    {
        time_t     now = time(0);
        struct tm  tstruct;
        char       buf[80];
        //tstruct = *localtime(&now);
		localtime_s(&tstruct, &now);
        strftime(buf, sizeof(buf), "%Y-%m-%d_%X", &tstruct);

        return buf;
    }

    void ReportFailure(const char * file, int line, const char * msg, ...)
    {
        char messageBuffer[1024] = "";
        if (msg != nullptr)
        {
            va_list args;
            va_start(args, msg);
            //vsprintf(messageBuffer, msg, args);
			vsnprintf_s(messageBuffer, 1024, msg, args);
            va_end(args);
        }

        std::stringstream ss;
        ss                                              << std::endl;
        ss << "File:      " << file                     << std::endl;
        ss << "Message:   " << messageBuffer            << std::endl;
        ss << "Line:      " << line                     << std::endl;
        ss << "Time:      " << currentDateTime()        << std::endl;
        
        lastErrorMessage = messageBuffer;

        std::cerr << ss.str();
        BWAPI::Broodwar->printf("%s", ss.str().c_str());

        if (Config::IO::LogAssertToErrorFile)
        {
            Logger::LogAppendToFile(Config::IO::ErrorLogFilename, ss.str());
        }
    }
}
}
