#pragma once

#include <cstdio>
#include <cstdarg>

namespace stbsp {
    inline int sprintf(char* buffer, const char* format, ...) {
        va_list args;
        va_start(args, format);
        int result = vsprintf(buffer, format, args);
        va_end(args);
        return result;
    }
    
    inline int snprintf(char* buffer, size_t count, const char* format, ...) {
        va_list args;
        va_start(args, format);
        int result = vsnprintf(buffer, count, format, args);
        va_end(args);
        return result;
    }
    
    inline int vsnprintf(char* buffer, size_t count, const char* format, va_list args) {
        return ::vsnprintf(buffer, count, format, args);
    }
    
    inline int _vsnprintf(char* buffer, size_t count, const char* format, va_list args) {
        return ::vsnprintf(buffer, count, format, args);
    }
}

#define stbsp_sprintf stbsp::sprintf
#define stbsp_snprintf stbsp::snprintf
#define stbsp_vsnprintf stbsp::vsnprintf
#define stbsp__vsnprintf stbsp::vsnprintf
