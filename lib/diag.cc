/*
 * diag.cc
 *
 *  Created on: Jan 21, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <cstdarg>
#include <cstdio>
#include <cstring>

extern "C" {
#   include <simple.h>
}

#include "include/diag.h"

namespace diag {

    /// report an error to SUIF
    void error_(const char *file, int line, const char *message, ...) throw() {
        char error_buffer[1024] = {'\0'};
        char file_name_buffer[1024] = {'\0'};

        va_list args;
        va_start(args, message);
        vsprintf(error_buffer, message, args);
        va_end(args);

        strncpy(file_name_buffer, file, 1023);
        _simple_error(error_buffer, file_name_buffer, line);
    }

    /// report a warning to SUIF
    void warning_(const char *file, int line, const char *message, ...) throw() {
        char error_buffer[1024] = {'\0'};
        char file_name_buffer[1024] = {'\0'};

        va_list args;
        va_start(args, message);
        vsprintf(error_buffer, message, args);
        va_end(args);

        strncpy(file_name_buffer, file, 1023);
        _simple_warning(error_buffer, file_name_buffer, line);
    }

}
