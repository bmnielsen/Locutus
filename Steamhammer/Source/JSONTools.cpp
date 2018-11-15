#include "JSONTools.h"


namespace BlueBlueSky
{
namespace JSONTools
{
    void ReadBool(const char * key, const rapidjson::Value & value, bool & dest)
    {
        if (value.HasMember(key))
        {
            BBS_ASSERT(value[key].IsBool(), "%s should be a bool", key);
            dest = value[key].GetBool();
        }
    }

    void ReadString(const char * key, const rapidjson::Value & value, std::string & dest)
    {
        if (value.HasMember(key))
        {
            BBS_ASSERT(value[key].IsString(), "%s should be a string", key);
            dest = value[key].GetString();
        }
    }
}
}

