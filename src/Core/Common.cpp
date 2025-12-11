#include "Core/Common.h"

#include <windows.h>
#include <DebugApi.h>

#include <iostream>

namespace core
{
    void handleAssert(int _line, const char* _file, const char* _msg)
    {
        char buffer[128];
        if (_msg)
            sprintf_s(buffer, "%s(%d): Assertion failed: %s\n", _file, _line, _msg);
        else
            sprintf_s(buffer, "%s(%d): Assertion failed.\n", _file, _line);
        
        if (IsDebuggerPresent())
            OutputDebugString(buffer);

        std::cerr << buffer << std::endl;
        
        if (IsDebuggerPresent())
            __debugbreak();
        else
            abort();
    }
}
