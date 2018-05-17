#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace UAlbertaBot
{
namespace Logger 
{
    void LogAppendToFile(const std::string & logFile, const std::string & msg);
	void LogAppendToFile(const std::string & logFile, const char *fmt, ...);
    void LogOverwriteToFile(const std::string & logFile, const std::string & msg);
};

namespace FileUtils
{
    std::string ReadFile(const std::string & filename);
}
}

class Log
{
public:
	Log();
	virtual ~Log();
	std::ostringstream& Get();
	std::ostringstream& Debug();
protected:
	std::ostringstream os;
	bool debug;
private:
	Log(const Log&);
	Log& operator =(const Log&);
};