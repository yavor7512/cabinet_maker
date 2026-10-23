#include "project_geometry.hpp"
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <XCAFDoc_ColorTool.hxx>
#include <Quantity_Color.hxx>

using namespace design;
void require(bool condition,const std::string& message) { if(!condition) throw std::runtime_error(message); }
void near(double a,double b,const std::string& message,double tolerance=1e-5) { require(std::abs(a-b)<=tolerance,message+": "+std::to_string(a)+" vs "+std::to_string(b)); }
std::array<double,6> bounds(const TopoDS_Shape& shape) {
  Bnd_Box b; BRepBndLib::AddOptimal(shape,b,false,false); std::array<double,6> result;
  b.Get(result[0],result[1],result[2],result[3],result[4],result[5]); return result;
}
struct Snapshot {
  std::map<std::string,TopoDS_Shape> parts;
  std::vector<std::string> hierarchy;
  std::map<std::string,std::array<double,3>> colors;
};
Snapshot readParts(const char* path) {
  Handle(TDocStd_Document) doc=new TDocStd_Document("BinXCAF");
  STEPCAFControl_Reader reader;
  require(reader.ReadFile(path)==IFSelect_RetDone && reader.Transfer(doc),"STEP import failed");
  auto tool=XCAFDoc_DocumentTool::ShapeTool(doc->Main()); TDF_LabelSequence roots;
  tool->GetFreeShapes(roots); require(roots.Length()==1,"Expected one assembly");
  Snapshot result;
  std::function<void(const TDF_Label&,const TopLoc_Location&,const std::string&)> visit;
  visit=[&](const TDF_Label& item,const TopLoc_Location& parent,const std::string& prefix) {
    TDF_Label definition=item;
    auto location=parent;
    if(tool->IsReference(item)) { tool->GetReferredShape(item,definition); location=parent*tool->GetLocation(item); }
    auto name=prefix+"/"+labelName(item)+"["+labelName(definition)+"]";
    result.hierarchy.push_back(name);
    TDF_LabelSequence children;
    if(tool->GetComponents(definition,children,false)) {
      for(int i=1;i<=children.Length();++i) visit(children.Value(i),location,name);
    } else {
      require(result.parts.emplace(name,tool->GetShape(definition).Moved(location)).second,"Duplicate part path");
      for(auto type:{XCAFDoc_ColorGen,XCAFDoc_ColorSurf,XCAFDoc_ColorCurv}) {
        Quantity_Color color;
        if(XCAFDoc_ColorTool::GetColor(definition,type,color))
          result.colors[name+"/"+std::to_string(type)]={color.Red(),color.Green(),color.Blue()};
      }
    }
  };
  visit(roots.Value(1),TopLoc_Location(),"");
  return result;
}
void compare(const char* before,const char* after) {
  auto a=readParts(before),b=readParts(after);
  require(a.hierarchy==b.hierarchy,"Assembly hierarchy, names or part order changed");
  require(a.parts.size()==b.parts.size(),"Part count changed");
  require(a.colors.size()==b.colors.size(),"Part color assignments changed");
  for(const auto& pair:a.colors) {
    auto it=b.colors.find(pair.first); require(it!=b.colors.end(),"Missing color: "+pair.first);
    for(int i=0;i<3;++i) near(pair.second[i],it->second[i],pair.first+" color");
  }
  for(const auto& pair:a.parts) {
    auto it=b.parts.find(pair.first); require(it!=b.parts.end(),"Missing part: "+pair.first);
    auto x=bounds(pair.second),y=bounds(it->second);
    for(int i=0;i<6;++i) near(x[i],y[i],pair.first+" bounds");
    double av=volume(pair.second),bv=volume(it->second);
    double tolerance=std::max(1e-5,av*1e-7);
    near(av,bv,pair.first+" volume",tolerance);
    // Bounds and volume alone could miss relocated machining. Compare occupied
    // material as well, including pockets, grooves and every individual screw.
    double common=overlap(pair.second,it->second);
    near(av,common,pair.first+" removed/added material",tolerance);
    near(bv,common,pair.first+" removed/added material",tolerance);
  }
  std::cout<<"Equivalent hierarchy, colors and occupied material ("<<a.parts.size()<<" parts): "<<after<<"\n";
}

int main(int argc,char** argv) {
  try {
    if(argc==4 && std::string(argv[1])=="--compare") { compare(argv[2],argv[3]); return 0; }
    require(argc==3,"Expected project file and temporary test directory");
    const std::filesystem::path source=argv[1],dir=argv[2];
    std::filesystem::create_directories(dir);
    Project initial(source);
    require(initial.assemblyIds()==std::vector<std::string>({"carcass","upper","lower"}),"Configured merge order changed");
    require(initial.assembly("lower").filename=="tall-front-bottom-aligned.step","Configured filename changed");
    require(initial.boards().size()==17,"Expected 17 boards");
    near(initial.body("lower.face").box.low[2],9.8,"Initial face bottom");
    near(initial.body("lower.face").box.high(2),721,"Initial face top");
    near(initial.body("lower.face").box.high(1),initial.body("lower.front").box.low[1],"Face touches drawer front");
    near(initial.body("lower.bottom").box.size[0],465,"Bottom width includes dado penetration");
    near(initial.body("lower.front").box.size[0],453,"Front fits between sides");

    Project raised(source,{{"UPPER_POSITION_Z_MM","644.8"}});
    near(raised.body("lower.face").box.low[2],35.6,"Face follows upper assembly");
    near(raised.body("lower.face").box.high(2),746.8,"Face top follows upper assembly");
    for(const auto& id:initial.boards()) if(id.rfind("lower.",0)==0 && id!="lower.face") {
      require(initial.body(id).box.low==raised.body(id).box.low,"Moving upper moved lower drawer board: "+id);
      require(initial.body(id).box.size==raised.body(id).box.size,"Moving upper resized lower drawer board: "+id);
    }
    Project taller(source,{{"UPPER_HEIGHT_MM","150"}});
    near(taller.body("lower.face").box.high(2),769,"Face follows upper height");
    Project spaced(source,{{"FACE_OFFSET_Y_MM","-4"}});
    near(spaced.body("lower.face").box.high(1),8,"Independent face Y offset");
    near(spaced.body("lower.front").box.low[1],12,"Face Y offset keeps drawer fixed");
    Project moved(source,{{"LOWER_POSITION_Y_MM","22"}});
    near(moved.body("lower.face").box.high(1),22,"Face follows lower front in Y");
    Project widened(source,{{"BOX_WIDTH_MM","500"}});
    near(widened.body("upper_box").box.size[0],500,"Shared alias updates upper drawer");
    near(widened.body("lower_box").box.size[0],500,"Shared alias updates lower drawer");
    Project individual(source,{{"BOX_WIDTH_MM","500"},{"lower_width","480"}});
    near(individual.body("upper_box").box.size[0],500,"Alias applies to other instances");
    near(individual.body("lower_box").box.size[0],480,"Explicit instance parameter takes precedence");
    Project onlyFace(source,{{"FACE_TOP_Z_MM","upper_box.top_z + 25.8"}});
    near(onlyFace.body("lower.face").box.high(2),746.8,"Independent 25.8 mm face lift");
    for(const auto& id:initial.boards()) if(id!="lower.face")
      require(initial.body(id).box.low==onlyFace.body(id).box.low,"Face lift moved another board: "+id);

    auto mutate=[&](const std::string& name,const std::function<void(rapidjson::Document&)>& edit) {
      std::ifstream input(source); std::string content((std::istreambuf_iterator<char>(input)),{});
      rapidjson::Document doc; doc.Parse(content.c_str()); edit(doc);
      rapidjson::StringBuffer buffer; rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); doc.Accept(writer);
      auto path=dir/(name+".json"); std::ofstream(path)<<buffer.GetString(); return path;
    };
    auto set=[](rapidjson::Document& doc,Json& value,const char* text){value.SetString(text,doc.GetAllocator());};
    auto unnamed=mutate("default-export-names",[](rapidjson::Document& doc) {
      doc.RemoveMember("assembly_order");
      for(auto& entry:doc["assemblies"].GetObject()) entry.value.RemoveMember("filename");
    });
    Project defaultExports(unnamed);
    require(defaultExports.assemblyIds()==std::vector<std::string>({"carcass","lower","upper"}),"Default assembly order must be alphabetical");
    require(defaultExports.assembly("lower").filename=="lower.step","Default filename must follow the configured ID");
    auto independent=mutate("independent-instance-defaults",[](rapidjson::Document& doc) {
      doc["parameters"]["lower_width"].SetInt(480);
    });
    Project different(independent);
    near(different.body("upper_box").box.size[0],477,"Alias does not merge instance defaults");
    near(different.body("lower_box").box.size[0],480,"Instance retains its own default");
    auto anchored=mutate("dimension-from-anchor",[&](rapidjson::Document& doc) {
      set(doc,doc["boards"]["lower.face"]["size"]["x"],"2 * (lower.face.center_x - lower_box.left_x)");
    });
    Project fromAnchor(anchored);
    near(fromAnchor.body("lower.face").box.size[0],477,"Size may depend on its explicitly supplied centre anchor");
    auto alternate=mutate("caps-over-sides",[&](rapidjson::Document& doc) {
      for(const auto& cap:{"carcass.bottom","carcass.top"}) {
        auto& ends=doc["boards"][cap]["between"]["x"];
        set(doc,ends[0],"opening.left_x - CARCASS_THICKNESS_MM");
        set(doc,ends[1],"opening.right_x + CARCASS_THICKNESS_MM");
      }
      for(const auto& side:{"carcass.left","carcass.right"}) {
        auto& ends=doc["boards"][side]["between"]["z"];
        set(doc,ends[0],"carcass.bottom.top_z"); set(doc,ends[1],"carcass.top.bottom_z");
      }
    });
    Project changedJoinery(alternate);
    near(changedJoinery.body("carcass.top").box.size[0],530,"Configuration changed cap width");
    near(changedJoinery.body("carcass.left").box.size[2],737,"Configuration changed side height");
    auto changed=generate(changedJoinery,"carcass");
    require(changed.parts.size()==6,"Alternate construction lost boards");
    near(bounds(changed.parts[3])[3]-bounds(changed.parts[3])[0],530,"Changed construction reaches actual geometry");

    auto baseline=generate(initial,"upper");
    Project loose(source,{{"DADO_CLEARANCE_MM","0.2"}}); auto relieved=generate(loose,"upper");
    for(int i=0;i<4;++i) require(volume(relieved.parts[i])<volume(baseline.parts[i])-1,"Dado clearance did not remove extra material");
    near(volume(relieved.parts[4]),volume(baseline.parts[4]),"Dado clearance must not resize bottom");

    int failures=0;
    auto rejects=[&](const std::string& expected,const std::function<void()>& operation) {
      try { operation(); }
      catch(const std::exception& e) { require(std::string(e.what()).find(expected)!=std::string::npos,"Wrong failure; expected '"+expected+"', got '"+e.what()+"'"); ++failures; return; }
      throw std::runtime_error("Accepted invalid project; expected "+expected);
    };
    rejects("Unknown parameter",[&]{Project bad(source,{{"TYPO","42"}});});
    rejects("plain .step filename",[&]{auto path=mutate("outside-output",[&](rapidjson::Document& doc){set(doc,doc["assemblies"]["lower"]["filename"],"../outside.step");});Project bad(path);});
    rejects("Duplicate assembly filename",[&]{auto path=mutate("duplicate-output",[&](rapidjson::Document& doc){set(doc,doc["assemblies"]["lower"]["filename"],"CABINET-CARCASS.step");});Project bad(path);});
    rejects("distinct configured assembly IDs",[&]{auto path=mutate("unknown-export",[&](rapidjson::Document& doc){set(doc,doc["assembly_order"][0],"missing");});Project bad(path);});
    rejects("distinct configured assembly IDs",[&]{auto path=mutate("duplicate-export",[&](rapidjson::Document& doc){set(doc,doc["assembly_order"][0],"lower");});Project bad(path);});
    rejects("include every assembly",[&]{auto path=mutate("omitted-export",[](rapidjson::Document& doc){doc["assembly_order"].PopBack();});Project bad(path);});
    rejects("Unknown parameter alias target",[&]{auto path=mutate("bad-alias",[&](rapidjson::Document& doc){set(doc,doc["parameter_aliases"]["BOX_WIDTH_MM"][0],"missing");});Project bad(path);});
    rejects("Duplicate parameter alias target",[&]{auto path=mutate("duplicate-alias",[&](rapidjson::Document& doc){set(doc,doc["parameter_aliases"]["BOX_WIDTH_MM"][1],"upper_width");});Project bad(path);});
    rejects("requires at least one target",[&]{auto path=mutate("empty-alias",[](rapidjson::Document& doc){doc["parameter_aliases"]["BOX_WIDTH_MM"].Clear();});Project bad(path);});
    rejects("Unknown reference",[&]{Project bad(source,{{"FACE_TOP_Z_MM","missing.top_z"}});});
    rejects("Circular relationship",[&]{Project bad(source,{{"FACE_TOP_Z_MM","lower.face.top_z"}});});
    rejects("Division by zero",[&]{Project bad(source,{{"FACE_WIDTH_MM","1 / 0"}});});
    rejects("dimensions must be",[&]{Project bad(source,{{"FACE_WIDTH_MM","-1"}});});
    rejects("Unknown function",[&]{Project bad(source,{{"FACE_WIDTH_MM","exec(1)"}});});
    rejects("Duplicate board number",[&]{Project bad(source,{{"LOWER_BOARD_START","1"}});});
    rejects("expected integer",[&]{Project bad(source,{{"LOWER_BOARD_START","6.5"}});});
    rejects("JSON error",[&]{auto path=dir/"invalid.json";std::ofstream(path)<<"{ invalid";Project bad(path);});
    rejects("duplicate key",[&]{auto path=dir/"duplicate.json";std::ofstream(path)<<"{\"schema_version\":1,\"schema_version\":1}";Project bad(path);});
    rejects("unknown field",[&]{auto path=mutate("typo",[](rapidjson::Document& doc){doc.AddMember("typo",1,doc.GetAllocator());});Project bad(path);});
    rejects("exactly one alignment",[&]{auto path=mutate("ambiguous",[](rapidjson::Document& doc){doc["boards"]["lower.face"]["align"].AddMember("bottom_z",0,doc.GetAllocator());});Project bad(path);});
    rejects("exactly one alignment",[&]{auto path=mutate("missing-axis",[](rapidjson::Document& doc){doc["boards"]["lower.face"]["align"].RemoveMember("top_z");});Project bad(path);});
    rejects("dimensions must be",[&]{auto path=mutate("negative-span",[&](rapidjson::Document& doc){set(doc,doc["boards"]["lower.front"]["between"]["x"][1],"lower.left.left_x");});Project bad(path);});
    rejects("Unknown cut type",[&]{auto path=mutate("bad-cut",[&](rapidjson::Document& doc){set(doc,doc["cuts"][0]["type"],"mystery");});Project bad(path);generate(bad,"upper");});
    rejects("Unknown body",[&]{auto path=mutate("bad-target",[&](rapidjson::Document& doc){set(doc,doc["cuts"][0]["targets"][0],"missing");});Project bad(path);generate(bad,"upper");});
    rejects("expected integer",[&]{Project bad(source,{{"SCREWS_PER_CORNER","1.5"}});generate(bad,"upper");});
    rejects("screw leaves its joint",[&]{Project bad(source,{{"SCREW_LENGTH_MM","40"}});generate(bad,"upper");});
    rejects("Unknown assembly",[&]{generate(initial,"missing");});
    rejects("Configured parts overlap",[&]{Project bad(source,{{"FACE_OFFSET_Y_MM","1"}});generate(bad,"lower");});
    std::cout<<"Passed configured alignment, alternative construction, machining and "<<failures<<" rejection checks.\n";
    return 0;
  } catch(const Standard_Failure& e) {std::cerr<<e.GetMessageString()<<'\n';}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';}
  return 1;
}
