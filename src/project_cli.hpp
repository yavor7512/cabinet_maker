#pragma once
#include "project_geometry.hpp"

namespace design {
inline void projectHelp() {
  std::cout << R"HELP(drawer --project FILE --assembly ID --output FILE [--set NAME=EXPRESSION ...]
drawer --project FILE --assembly ID --output-dir DIR
drawer --project FILE --all --output-dir DIR
drawer --project FILE --list-assemblies
drawer --project FILE --list-outputs --output-dir DIR
drawer --project FILE --describe [--assembly ID]

Project files define every assembly, board, alignment, joint and hardware placement.
FILE is expanded JSON; use make project to expand a Jsonnet design.
--assembly selects a configured assembly ID. There are no built-in models.
--all generates every assembly in the configured order.
--output-dir uses configured filenames (default: ID.step).
--list-assemblies lists configured IDs, names, board counts and filenames.
--list-outputs lists export paths in merge order without generating geometry.
--set overrides a declared parameter. Repeated overrides use the last value.
--describe resolves board envelopes without generating geometry or writing files.

Make shortcuts: make assemblies; make carcass; make assembly ASSEMBLY=lower
Old --model/--width/--face-* flags were removed; use the configuration or --set.

EXIT STATUS
  0 success; 1 invalid input, geometry or export failure.
)HELP";
}
inline int projectCommand(int argc,char** argv) {
  std::filesystem::path path,output,directory;
  std::string assembly;
  std::map<std::string,std::string> overrides;
  enum class Mode { Generate, Describe, Assemblies, Outputs };
  Mode mode=Mode::Generate;
  bool all=false;
  auto select=[&](Mode next) {
    if(mode!=Mode::Generate && mode!=next) throw std::runtime_error("Inspection modes cannot be combined");
    mode=next;
  };
  for(int i=1;i<argc;++i) {
    std::string flag=argv[i],value;
    auto equals=flag.find('='); bool inlineValue=equals!=std::string::npos;
    if(inlineValue) { value=flag.substr(equals+1); flag.resize(equals); }
    if(flag=="--help" || flag=="-h" || flag=="--all" || flag=="--describe" || flag=="--list-assemblies" || flag=="--list-outputs") {
      if(inlineValue) throw std::runtime_error(flag+" takes no value");
      if(flag=="--help" || flag=="-h") { projectHelp(); return 0; }
      if(flag=="--all") all=true;
      else select(flag=="--describe"?Mode::Describe:flag=="--list-assemblies"?Mode::Assemblies:Mode::Outputs);
      continue;
    }
    if(flag!="--project" && flag!="--assembly" && flag!="--output" && flag!="--output-dir" && flag!="--set")
      throw std::runtime_error("Unknown option: "+flag+"; construction is configured through --project and --set (see --help)");
    if(!inlineValue) { if(++i==argc) throw std::runtime_error("Missing value for "+flag); value=argv[i]; }
    if(value.empty()) throw std::runtime_error(flag+": value must not be empty");
    if(flag=="--project") path=value;
    else if(flag=="--assembly") assembly=value;
    else if(flag=="--output") output=value;
    else if(flag=="--output-dir") directory=value;
    else {
      auto split=value.find('=');
      if(split==std::string::npos || split==0 || split+1==value.size()) throw std::runtime_error("--set expects NAME=EXPRESSION");
      overrides[value.substr(0,split)]=value.substr(split+1);
    }
  }
  if(path.empty()) throw std::runtime_error("--project FILE is required");
  if(!output.empty() && !directory.empty()) throw std::runtime_error("--output and --output-dir cannot be combined");
  if(directory.string().find_first_of("\r\n")!=std::string::npos) throw std::runtime_error("Output directory must be a single-line path");
  if(all && (mode!=Mode::Generate || !assembly.empty() || !output.empty()))
    throw std::runtime_error("--all cannot be combined with inspection modes, --assembly or --output");
  if((mode==Mode::Assemblies || mode==Mode::Outputs) && (!assembly.empty() || !output.empty()))
    throw std::runtime_error("Listing cannot be combined with --assembly or --output");
  if(mode==Mode::Assemblies && !directory.empty()) throw std::runtime_error("--list-assemblies does not accept --output-dir");
  if(mode==Mode::Describe && (!output.empty() || !directory.empty())) throw std::runtime_error("--describe does not accept --output or --output-dir");
  if((all || mode==Mode::Outputs) && directory.empty()) throw std::runtime_error("--output-dir DIR is required");
  Project project(path,overrides);
  if(!assembly.empty()) project.assembly(assembly);
  if(mode==Mode::Assemblies) {
    std::cout<<"ASSEMBLY\tNAME\tBOARDS\tFILENAME\n";
    for(const auto& id:project.assemblyIds()) {
      auto count=std::count_if(project.boards().begin(),project.boards().end(),[&](const std::string& board){return project.body(board).assembly==id;});
      const auto& a=project.assembly(id);
      std::cout<<id<<'\t'<<a.name<<'\t'<<count<<'\t'<<a.filename<<'\n';
    }
    return 0;
  }
  if(mode==Mode::Outputs) {
    for(const auto& id:project.assemblyIds()) std::cout<<(directory/project.assembly(id).filename).string()<<'\n';
    return 0;
  }
  if(mode==Mode::Describe) {
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
  if(!all && (assembly.empty() || (output.empty() && directory.empty())))
    throw std::runtime_error("Generation requires --assembly ID and --output FILE or --output-dir DIR; use --all to generate every assembly");
  const auto ids=all?project.assemblyIds():std::vector<std::string>{assembly};
  // Validate every destination before starting a multi-assembly export.
  for(const auto& id:ids) {
    const auto destination=output.empty()?directory/project.assembly(id).filename:output;
    if(std::filesystem::weakly_canonical(path)==std::filesystem::weakly_canonical(destination))
      throw std::runtime_error("Output must not overwrite the project configuration");
  }
  for(const auto& id:ids) exportProject(project,id,output.empty()?directory/project.assembly(id).filename:output);
  return 0;
}
}
