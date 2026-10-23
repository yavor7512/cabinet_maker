#pragma once
#include "expressions.hpp"
#include "validation.hpp"
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>

namespace design {
using Json = rapidjson::Value;
inline std::string str(const Json& v, const std::string& where) {
  if(!v.IsString() || v.GetStringLength()==0) throw std::runtime_error(where+": expected a nonempty string");
  std::string s(v.GetString(), v.GetStringLength());
  if(s.find_first_of("\r\n")!=std::string::npos || s.find('\0')!=std::string::npos) throw std::runtime_error(where+": expected a single-line string");
  return s;
}
inline std::string expr(const Json& v, const std::string& where) {
  if(v.IsString()) return str(v,where);
  if(!v.IsNumber() || !std::isfinite(v.GetDouble())) throw std::runtime_error(where+": expected a number or arithmetic expression");
  std::ostringstream out; out<<std::setprecision(17)<<v.GetDouble(); return out.str();
}
inline void identifier(const std::string& id, bool dotted=true) {
  bool start=true;
  for(unsigned char c:id) {
    if(c=='.' && dotted && !start) { start=true; continue; }
    if((start && !std::isalpha(c) && c!='_') || (!start && !std::isalnum(c) && c!='_')) throw std::runtime_error("Invalid identifier: "+id);
    start=false;
  }
  if(start) throw std::runtime_error("Invalid identifier: "+id);
}
inline void keys(const Json& value, std::initializer_list<const char*> allowed, const std::string& where) {
  if(!value.IsObject()) throw std::runtime_error(where+": expected an object");
  for(auto it=value.MemberBegin(); it!=value.MemberEnd(); ++it) {
    auto name=str(it->name,where);
    if(std::none_of(allowed.begin(),allowed.end(),[&](const char* key){return name==key;})) throw std::runtime_error(where+": unknown field '"+name+"'");
  }
}
inline const Json& required(const Json& object, const char* key, const std::string& where) {
  if(!object.IsObject() || !object.HasMember(key)) throw std::runtime_error(where+": missing '"+key+"'");
  return object[key];
}
inline void object(const Json& v,const std::string& where) { if(!v.IsObject()) throw std::runtime_error(where+": expected an object"); }
inline void array(const Json& v,const std::string& where) { if(!v.IsArray()) throw std::runtime_error(where+": expected an array"); }
inline void uniqueKeys(const Json& v, const std::string& path="$", int depth=0) {
  if(depth>64) throw std::runtime_error("Project nesting exceeds 64 levels");
  if(v.IsObject()) {
    std::set<std::string> seen;
    for(auto it=v.MemberBegin();it!=v.MemberEnd();++it) {
      auto name=str(it->name,path);
      if(!seen.insert(name).second) throw std::runtime_error(path+": duplicate key '"+name+"'");
      uniqueKeys(it->value,path+"."+name,depth+1);
    }
  } else if(v.IsArray()) for(const auto& item:v.GetArray()) uniqueKeys(item,path+"[]",depth+1);
}
struct Box {
  std::array<double,3> low{},size{};
  double high(int axis) const { return low[axis]+size[axis]; }
};
struct Body {
  std::string id,assembly,name;
  int number=0;
  bool board=false;
  Box box;
};
struct Assembly { std::string id,name,filename; };

// Every position and dimension is a graph node. Named surfaces always refer to
// the original rectangular blank, so machining cannot unexpectedly move a datum.
class Project {
  rapidjson::Document document;
  Graph graph;
  std::map<std::string,Body> bodies;
  std::vector<std::string> boardOrder;
  std::map<std::string,Assembly> assemblies;
  std::vector<std::string> assemblyOrder;
  std::set<std::string> parameters;
  static constexpr const char* sizes[3] = {"width","depth","height"};
  static constexpr const char* anchors[3][3] = {{"left_x","center_x","right_x"},{"front_y","center_y","rear_y"},{"bottom_z","center_z","top_z"}};
  void addBody(const std::string& id,const Json& value,bool board) {
    identifier(id);
    if(board) keys(value,{"assembly","name","number","size","align","between"},id);
    else keys(value,{"size","align","between"},id);
    Body body; body.id=id; body.board=board;
    if(board) {
      body.assembly=str(required(value,"assembly",id),id+".assembly");
      if(!assemblies.count(body.assembly)) throw std::runtime_error(id+": unknown assembly '"+body.assembly+"'");
      body.name=str(required(value,"name",id),id+".name");
      graph.add(id+".number",expr(required(value,"number",id),id+".number"));
      boardOrder.push_back(id);
    }
    if(!bodies.emplace(id,body).second) throw std::runtime_error("Duplicate body: "+id);
    if(value.HasMember("size")) keys(value["size"],{"x","y","z"},id+".size");
    if(value.HasMember("between")) keys(value["between"],{"x","y","z"},id+".between");
    if(value.HasMember("align")) keys(value["align"],{"left_x","center_x","right_x","front_y","center_y","rear_y","bottom_z","center_z","top_z"},id+".align");
    for(int axis=0;axis<3;++axis) {
      const char* key=axis==0?"x":axis==1?"y":"z";
      auto dim=id+"."+sizes[axis], low=id+"."+anchors[axis][0];
      int count=0,chosen=-1;
      if(value.HasMember("align")) for(int a=0;a<3;++a) if(value["align"].HasMember(anchors[axis][a])) { ++count; chosen=a; }
      bool sized=value.HasMember("size") && value["size"].HasMember(key);
      bool between=value.HasMember("between") && value["between"].HasMember(key);
      if(between) {
        const auto& endpoints=value["between"][key];
        if(sized || count || !endpoints.IsArray() || endpoints.Size()!=2) throw std::runtime_error(id+"."+key+": between requires exactly two endpoints and no size/alignment on that axis");
        auto a=expr(endpoints[0],id), b=expr(endpoints[1],id);
        auto high=id+"."+anchors[axis][2];
        graph.add(low,a); graph.add(high,b);
        graph.add(dim,high+"-"+low);
        graph.add(id+"."+anchors[axis][1],"("+low+"+"+high+")/2");
      } else {
        if(!sized || count!=1) throw std::runtime_error(id+"."+key+": specify size and exactly one alignment, or between");
        graph.add(dim,expr(value["size"][key],dim));
        auto at=expr(value["align"][anchors[axis][chosen]],id);
        auto anchor=id+"."+anchors[axis][chosen];
        graph.add(anchor,at);
        for(int a=0;a<3;++a) if(a!=chosen)
          graph.add(id+"."+anchors[axis][a],anchor+"+("+std::to_string(a-chosen)+")*"+dim+"/2");
      }
    }
  }
public:
  explicit Project(const std::filesystem::path& path,const std::map<std::string,std::string>& overrides={}) {
    std::ifstream file(path,std::ios::binary);
    if(!file) throw std::runtime_error("Cannot read project: "+path.string());
    file.seekg(0,std::ios::end);
    if(file.tellg()>4*1024*1024) throw std::runtime_error("Project exceeds 4 MiB");
    file.seekg(0);
    std::string contents((std::istreambuf_iterator<char>(file)),{});
    document.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(contents.data(),contents.size());
    if(document.HasParseError()) throw std::runtime_error(path.string()+": JSON error at byte "+std::to_string(document.GetErrorOffset())+": "+rapidjson::GetParseError_En(document.GetParseError()));
    uniqueKeys(document);
    keys(document,{"schema_version","parameters","parameter_aliases","assemblies","assembly_order","frames","boards","cuts","hardware_profiles","hardware","checks"},"project");
    auto& version=required(document,"schema_version","project");
    if(!version.IsInt() || version.GetInt()!=1) throw std::runtime_error("Unsupported project schema_version; expected 1");
    const auto& params=required(document,"parameters","project"); object(params,"parameters");
    for(auto it=params.MemberBegin();it!=params.MemberEnd();++it) {
      auto name=str(it->name,"parameter"); identifier(name,false); parameters.insert(name);
    }
    // Aliases provide compatibility names for one or more instance parameters.
    // Their graph value is the first target; only explicit overrides fan out.
    // A directly named parameter override takes precedence over an alias.
    auto resolvedOverrides=overrides;
    if(document.HasMember("parameter_aliases")) {
      const auto& aliases=document["parameter_aliases"]; object(aliases,"parameter_aliases");
      std::set<std::string> targets;
      for(auto it=aliases.MemberBegin();it!=aliases.MemberEnd();++it) {
        auto name=str(it->name,"parameter alias"); identifier(name,false);
        if(params.HasMember(name.c_str())) throw std::runtime_error("Parameter alias duplicates parameter: "+name);
        const auto& list=it->value; array(list,name);
        if(list.Empty()) throw std::runtime_error("Parameter alias requires at least one target: "+name);
        for(const auto& value:list.GetArray()) {
          auto target=str(value,name);
          if(!params.HasMember(target.c_str())) throw std::runtime_error("Unknown parameter alias target: "+target);
          if(!targets.insert(target).second) throw std::runtime_error("Duplicate parameter alias target: "+target);
          auto override=overrides.find(name);
          if(override!=overrides.end() && !overrides.count(target)) resolvedOverrides[target]=override->second;
        }
        parameters.insert(name);
        graph.add(name,str(list[0],name));
      }
    }
    for(auto it=params.MemberBegin();it!=params.MemberEnd();++it) {
      auto name=str(it->name,"parameter");
      auto override=resolvedOverrides.find(name);
      graph.add(name,override==resolvedOverrides.end()?expr(it->value,name):override->second);
    }
    for(const auto& override:overrides) if(!parameters.count(override.first)) throw std::runtime_error("Unknown parameter override: "+override.first);
    const auto& list=required(document,"assemblies","project"); object(list,"assemblies");
    std::set<std::string> filenames;
    for(auto it=list.MemberBegin();it!=list.MemberEnd();++it) {
      auto id=str(it->name,"assembly"); identifier(id,false); keys(it->value,{"name","part_order","filename"},id);
      auto filename=it->value.HasMember("filename")?str(it->value["filename"],id+".filename"):id+".step";
      if(filename.find_first_of("/\\")!=std::string::npos || std::filesystem::path(filename).extension()!=".step")
        throw std::runtime_error(id+": filename must be a plain .step filename without directories");
      auto folded=filename;
      std::transform(folded.begin(),folded.end(),folded.begin(),[](unsigned char c){return std::tolower(c);});
      if(!filenames.insert(folded).second) throw std::runtime_error("Duplicate assembly filename: "+filename);
      assemblies.emplace(id,Assembly{id,str(required(it->value,"name",id),id),filename});
    }
    if(assemblies.empty()) throw std::runtime_error("Project requires at least one assembly");
    if(document.HasMember("assembly_order")) {
      const auto& order=document["assembly_order"]; array(order,"assembly_order");
      std::set<std::string> seen;
      for(const auto& entry:order.GetArray()) {
        auto id=str(entry,"assembly_order");
        if(!assemblies.count(id) || !seen.insert(id).second) throw std::runtime_error("assembly_order requires distinct configured assembly IDs: "+id);
        assemblyOrder.push_back(id);
      }
      if(assemblyOrder.size()!=assemblies.size()) throw std::runtime_error("assembly_order must include every assembly exactly once");
    } else for(const auto& entry:assemblies) assemblyOrder.push_back(entry.first);
    for(const auto& section:{"frames","boards"}) {
      if(!document.HasMember(section)) { if(std::string(section)=="boards") throw std::runtime_error("Project requires boards"); else continue; }
      const auto& list=document[section]; object(list,section);
      for(auto it=list.MemberBegin();it!=list.MemberEnd();++it) addBody(str(it->name,section),it->value,std::string(section)=="boards");
    }
    graph.validate();
    std::set<int> numbers;
    for(auto& pair:bodies) {
      auto& body=pair.second;
      for(int axis=0;axis<3;++axis) {
        body.box.low[axis]=graph.value(body.id+"."+anchors[axis][0]);
        body.box.size[axis]=graph.value(body.id+"."+sizes[axis]);
        if(body.box.size[axis]<checks.minFeature || body.box.size[axis]>checks.maxDimension || std::abs(body.box.low[axis])>checks.maxDimension || std::abs(body.box.high(axis))>checks.maxDimension)
          throw std::runtime_error(body.id+": dimensions must be 0.001..1000000 mm and coordinates within +/-1000000 mm");
      }
      if(body.board) {
        body.number=integer(graph.value(body.id+".number"),body.id+".number",1,1000000);
        if(!numbers.insert(body.number).second) throw std::runtime_error("Duplicate board number: "+std::to_string(body.number));
      }
    }
    // JSON object key order is not a construction/export order. Jsonnet sorts
    // keys, so use the explicit board numbers for stable STEP and cut-list IDs.
    std::sort(boardOrder.begin(),boardOrder.end(),[&](const std::string& a,const std::string& b){return bodies.at(a).number<bodies.at(b).number;});
    for(const auto& pair:assemblies) if(std::none_of(boardOrder.begin(),boardOrder.end(),[&](const std::string& id){return bodies.at(id).assembly==pair.first;})) throw std::runtime_error("Assembly has no boards: "+pair.first);
    if(document.HasMember("checks")) {
      array(document["checks"],"checks");
      for(const auto& check:document["checks"].GetArray()) {
        keys(check,{"value","min","max","message"},"check");
        if(!check.HasMember("min") && !check.HasMember("max")) throw std::runtime_error("Check requires min or max");
        if(check.HasMember("min") && check.HasMember("max") && eval(check["min"])>eval(check["max"])) throw std::runtime_error("Check min exceeds max");
        auto message=str(required(check,"message","check"),"check.message");
        double v=eval(required(check,"value","check"));
        if((check.HasMember("min") && v<eval(check["min"])) || (check.HasMember("max") && v>eval(check["max"]))) throw std::runtime_error(message);
      }
    }
  }
  static int integer(double n,const std::string& where,int min=1,int max=1000) {
    if(!std::isfinite(n) || n!=std::floor(n) || n<min || n>max) throw std::runtime_error(where+": expected integer "+std::to_string(min)+".."+std::to_string(max));
    return static_cast<int>(n);
  }
  double eval(const Json& expression) { return graph.evaluate(expr(expression,"expression")); }
  double value(const std::string& reference) { return graph.value(reference); }
  std::array<double,3> vector(const Json& v,const std::string& where) {
    if(!v.IsArray() || v.Size()!=3) throw std::runtime_error(where+": expected three expressions [x,y,z]");
    return {eval(v[0]),eval(v[1]),eval(v[2])};
  }
  const Body& body(const std::string& id) const {
    auto it=bodies.find(id); if(it==bodies.end()) throw std::runtime_error("Unknown body: "+id); return it->second;
  }
  const Assembly& assembly(const std::string& id) const {
    auto it=assemblies.find(id); if(it==assemblies.end()) throw std::runtime_error("Unknown assembly: "+id); return it->second;
  }
  const auto& boards() const { return boardOrder; }
  const auto& assemblyIds() const { return assemblyOrder; }
  const Json& data() const { return document; }
};
}
