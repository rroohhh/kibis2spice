#include "reporter.h"

std::ostream & operator<<(std::ostream & stream, const SEVERITY & severity) {
    stream << "\e[0;31m";
    switch(severity) {
    case SEVERITY::RPT_SEVERITY_INFO:
        stream << "INFO";
        break;
    case SEVERITY::RPT_SEVERITY_ERROR:
        stream << "ERROR";
        break;
    case SEVERITY::RPT_SEVERITY_WARNING:
        stream << "WARNING";
        break;
    case SEVERITY::RPT_SEVERITY_ACTION:
        stream << "ACTION";
        break;
    }
    return stream;
}
