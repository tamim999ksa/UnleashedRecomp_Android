#include "version_utils.h"
#include <os/logger.h>
#include <stdexcept>

bool parseVersion(const std::string &versionStr, int &major, int &minor, int &revision)
{
    size_t start = 0;
    if (versionStr[0] == 'v')
    {
        start = 1;
    }

    size_t firstDot = versionStr.find('.', start);
    size_t secondDot = versionStr.find('.', firstDot + 1);

    if (firstDot == std::string::npos || secondDot == std::string::npos)
    {
        return false;
    }

    try
    {
        major = std::stoi(versionStr.substr(start, firstDot - start));
        minor = std::stoi(versionStr.substr(firstDot + 1, secondDot - firstDot - 1));
        revision = std::stoi(versionStr.substr(secondDot + 1));
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Error while parsing version: {}.", e.what());
        return false;
    }

    return true;
}
