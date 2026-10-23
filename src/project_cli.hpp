#pragma once
#include "project_geometry.hpp"

namespace design {
inline bool isProjectCommand(int argc,char** argv) {
  for(int i=1;i<argc;++i) {
    std::string flag=argv[i]; flag=flag.substr(0,flag.find('='));
    if(flag=="--project" || flag=="--assembly" || flag=="--set" || flag=="--describe") return true;
  }
  return false;
}
inline int projectCommand(int argc,char** argv) {
  std::filesystem::path path,output;
  std::string assembly;
  std::map<std::string,std::string> overrides;
  bool describe=false;
  for(int i=1;i<argc;++i) {
    std::string flag=argv[i],value;
    auto equals=flag.find('='); bool inlineValue=equals!=std::string::npos;
    if(inlineValue) { value=flag.substr(equals+1); flag.resize(equals); }
    if(flag=="--help" || flag=="-h") {
      std::cout<<"drawer --project FILE --assembly ID --output FILE [--set NAME=EXPRESSION ...]\n"
        <<"drawer --project FILE --describe [--assembly ID] [--set NAME=EXPRESSION ...]\n"
        <<"Project files define boards, named-surface alignments, joinery and hardware.\n"
        <<"--set overrides a declared project parameter. Repeated overrides use the last value.\n"
        <<"--describe resolves board envelopes without writing files or creating geometry.\n";
      return 0;
    }
    if(flag=="--describe") {
      if(inlineValue) throw std::runtime_error("--describe takes no value");
      describe=true; continue;
    }
    if(flag!="--project" && flag!="--assembly" && flag!="--output" && flag!="--set") throw std::runtime_error("Unknown project option: "+flag+" (legacy drawer flags cannot be mixed with --project)");
    if(!inlineValue) { if(++i==argc) throw std::runtime_error("Missing value for "+flag); value=argv[i]; }
    if(value.empty()) throw std::runtime_error(flag+": value must not be empty");
    if(flag=="--project") path=value;
    else if(flag=="--assembly") assembly=value;
    else if(flag=="--output") output=value;
    else {
      auto split=value.find('=');
      if(split==std::string::npos || split==0 || split+1==value.size()) throw std::runtime_error("--set expects NAME=EXPRESSION");
      overrides[value.substr(0,split)]=value.substr(split+1);
    }
  }
  if(path.empty()) throw std::runtime_error("--project FILE is required");
  Project project(path,overrides);
  if(!assembly.empty()) project.assembly(assembly);
  if(describe) {
    if(!output.empty()) throw std::runtime_error("--describe does not accept --output");
    std::cout<<std::setprecision(12)<<"BOARD\tASSEMBLY\tMIN X/Y/Z (mm)\tSIZE X/Y/Z (mm)\n";
    for(const auto& id:project.boards()) {
      const auto& b=project.body(id);
      if(!assembly.empty() && b.assembly!=assembly) continue;
      std::cout<<id<<'\t'<<b.assembly;
      for(double v:b.box.low) std::cout<<'\t'<<v;
      for(double v:b.box.size) std::cout<<'\t'<<v;
      std::cout<<'\n';
    }
    return 0;
  }
  if(assembly.empty() || output.empty()) throw std::runtime_error("Generation requires --assembly ID and --output FILE");
  if(std::filesystem::weakly_canonical(path)==std::filesystem::weakly_canonical(output)) throw std::runtime_error("Output must not overwrite the project configuration");
  exportProject(project,assembly,output);
  return 0;
}
}
