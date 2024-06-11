#pragma once

#include <memory>
#include <iostream>
#include <string>
#include <stdio.h>

class SPICE_SIMULATOR {
  public:
  SPICE_SIMULATOR(std::string name) : m_name(name) {};

  void Init() {};

  void LoadNetlist(std::string netlist) {
    // std::ofstream netlist_file;
    // netlist_file.open ("out.spice");
    // netlist_file << netlist;
    // netlist_file.close();

    auto fp = popen("ngspice -n > /dev/null", "w");
    if (fp == nullptr) {
      std::cout << "\e[0;31m" << "could not run ngspice: " << std::strerror(errno) << "\e[0m" << std::endl;
      return;
    }
    netlist = std::string(".control set ngbehaviour=ltpsa .endc\n") + netlist;
    auto count = std::fwrite(netlist.c_str(), sizeof(char), netlist.size(), fp);
    if (count != netlist.size()) {
      std::cout << "\e[0;31m" << "could not write netlist" << "\e[0m" << std::endl;
    }
    std::string output;
    char c;
    while ((c = std::getc(fp)) != EOF) {
      output.push_back(c);
    }
    auto status = pclose(fp);
    if (status == -1) {
      std::cout << "\e[0;31m" << "error closing ngspice" << "\e[0m" << std::endl;
    } else if (status != 0) {
      std::cout << "\e[0;31m" << "ngspice failed:" << "\e[0m" << std::endl;
      std::cout << output << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }
  private:
  std::string m_name;
};

namespace SIMULATOR {
  std::shared_ptr<SPICE_SIMULATOR> CreateInstance(std::string name) {
    return std::make_shared<SPICE_SIMULATOR>(name);
  }
}
