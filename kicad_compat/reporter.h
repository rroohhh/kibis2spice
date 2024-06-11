#pragma once
#include <iostream>

enum class SEVERITY {
  RPT_SEVERITY_ERROR = 0,
  RPT_SEVERITY_WARNING = 1,
  RPT_SEVERITY_INFO = 2,
  RPT_SEVERITY_ACTION = 3
};

using SEVERITY::RPT_SEVERITY_INFO;
using SEVERITY::RPT_SEVERITY_ERROR;
using SEVERITY::RPT_SEVERITY_WARNING;
using SEVERITY::RPT_SEVERITY_ACTION;

std::ostream & operator<<(std::ostream & stream, const SEVERITY & severity)  ;

struct REPORTER {
    void Report(std::string aMsg, SEVERITY aSeverity = RPT_SEVERITY_INFO) {
        std::cerr << aSeverity << " " << aMsg << "\e[0m" << std::endl;
    }
};
