#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <XCAFDoc_Editor.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TDF_LabelSequence.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <Interface_Static.hxx>
#include <Standard_Failure.hxx>
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <Quantity_Color.hxx>

#include <TDF_ChildIterator.hxx>
#include <optional>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace fs=std::filesystem;
using Doc=Handle(TDocStd_Document);
Doc read(const fs::path& path) {
  Doc doc=new TDocStd_Document("BinXCAF");
  STEPCAFControl_Reader reader;
  if(reader.ReadFile(path.string().c_str())!=IFSelect_RetDone || !reader.Transfer(doc))
    throw std::runtime_error("Cannot read STEP: "+path.string());
  return doc;
}
std::string name(const TDF_Label& l) {
  Handle(TDataStd_Name) n;
  if(!l.FindAttribute(TDataStd_Name::GetID(),n)) return {};
  std::vector<char> buffer(n->Get().LengthOfCString()+1);
  Standard_PCharacter p=buffer.data(); n->Get().ToUTF8CString(p); return p;
}
struct Record {std::string path; std::array<double,6> bounds{}; double volume=0; int solids=0;};
void inspect(const TDF_Label& label,const TopLoc_Location& parent,const std::string& path,std::vector<Record>& out) {
  TDF_Label def=label;
  TopLoc_Location location=parent;
  if(XCAFDoc_ShapeTool::IsReference(label)) {
    XCAFDoc_ShapeTool::GetReferredShape(label,def);
    location=parent*XCAFDoc_ShapeTool::GetLocation(label);
  }
  const auto key=path+"/"+name(label)+"["+name(def)+"]";
  TDF_LabelSequence children;
  if(XCAFDoc_ShapeTool::GetComponents(def,children,false)) {
    out.push_back({key});
    for(int i=1;i<=children.Length();++i) inspect(children.Value(i),location,key,out);
  } else {
    auto shape=XCAFDoc_ShapeTool::GetShape(def).Moved(location);
    Record r; r.path=key;
    Bnd_Box b; BRepBndLib::AddOptimal(shape,b,false,false);
    if(b.IsVoid()) throw std::runtime_error("Empty geometry: "+key);
    b.Get(r.bounds[0],r.bounds[1],r.bounds[2],r.bounds[3],r.bounds[4],r.bounds[5]);
    GProp_GProps props; BRepGProp::VolumeProperties(shape,props); r.volume=props.Mass();
    for(TopExp_Explorer it(shape,TopAbs_SOLID);it.More();it.Next()) ++r.solids;
    out.push_back(r);
  }
}
std::vector<Record> snapshot(const Doc& doc) {
  TDF_LabelSequence roots; XCAFDoc_DocumentTool::ShapeTool(doc->Main())->GetFreeShapes(roots);
  std::vector<Record> result;
  for(int i=1;i<=roots.Length();++i) inspect(roots.Value(i),TopLoc_Location(),"",result);
  std::sort(result.begin(),result.end(),[](const Record&a,const Record&b){return a.path<b.path || (a.path==b.path && a.bounds<b.bounds);});
  return result;
}
Quantity_Color parseColor(std::string hex) {
  if(!hex.empty() && hex[0]=='#') hex.erase(0,1);
  if(hex.size()!=6 || !std::all_of(hex.begin(),hex.end(),[](unsigned char c){return std::isxdigit(c);}))
    throw std::runtime_error("--color expects #RRGGBB (quote the # in a shell)");
  const auto value=std::stoul(hex,nullptr,16);
  return Quantity_Color(((value>>16)&255)/255.0,((value>>8)&255)/255.0,(value&255)/255.0,Quantity_TOC_sRGB);
}
void paint(const Doc& doc,const Quantity_Color& color) {
  auto shapes=XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  auto colors=XCAFDoc_DocumentTool::ColorTool(doc->Main());
  for(TDF_ChildIterator it(shapes->Label(),true);it.More();it.Next()) {
    const auto label=it.Value();
    if(!XCAFDoc_ShapeTool::IsShape(label)) continue;
    colors->SetColorByLayer(label,false);
    for(auto type:{XCAFDoc_ColorGen,XCAFDoc_ColorSurf,XCAFDoc_ColorCurv}) colors->SetColor(label,color,type);
  }
}
void verifyColor(const TDF_Label& label,const Quantity_Color& expected) {
  TDF_Label def=label;
  if(XCAFDoc_ShapeTool::IsReference(label)) XCAFDoc_ShapeTool::GetReferredShape(label,def);
  TDF_LabelSequence children;
  if(XCAFDoc_ShapeTool::GetComponents(def,children,false)) {
    for(int i=1;i<=children.Length();++i) verifyColor(children.Value(i),expected);
  } else {
    Quantity_Color actual;
    if(!XCAFDoc_ColorTool::GetColor(def,XCAFDoc_ColorSurf,actual)
        && !XCAFDoc_ColorTool::GetColor(def,XCAFDoc_ColorGen,actual))
      throw std::runtime_error("Part color missing after STEP export");
    if(actual.Distance(expected)>1e-5) throw std::runtime_error("Part color changed after STEP export");
  }
}

int main(int argc,char** argv) {
  try {
    std::optional<Quantity_Color> color;
    fs::path output; std::vector<fs::path> inputs; bool force=false, positional=false, autocad=false;
    for(int i=1;i<argc;++i) {
      std::string a=argv[i];
      if(!positional && (a=="--help" || a=="-h")) {
        std::cout<<R"(NAME
    step-merge - combine STEP files without fusing geometry
SYNOPSIS
    step-merge --output merged.step [--force] [--autocad] [--] input.step ...
DESCRIPTION
    Accepts one or more inputs. Each input gets a separate named assembly
    beneath the output assembly. Parts, nested assemblies, names and original
    positions remain separate. No Boolean union, alignment or collision removal.
    OpenCascade imports source units and exports millimetres. Standard XCAF
    metadata is copied; unsupported STEP annotations may not survive conversion.
OPTIONS
    -o, --output PATH   Required output filename.
    --material NAME    dark-wood: solid brown #80552F, applied to ALL objects.
    --color HEX        Override ALL object colors using quoted '#RRGGBB'.
                       Last material/color flag wins. Default preserves colors.
                       STEP surface color only; no wood grain/bitmap texture.
    --autocad          Rotate -90 degrees around world X: (X,Y,Z) -> (X,Z,-Y).
                       Maps Z-up to Y-up for the target viewer; apply once.
    --force            Replace an existing output; never overwrite an input.
    --                 Treat all remaining arguments as input paths.
    -h, --help         Show this manual.
VALIDATION
    Reimports the output and checks hierarchy, names, per-part world bounds,
    volumes and solid counts. Invalid or empty inputs abort the operation.
EXIT STATUS
    0 success; 1 invalid arguments, import, export or verification failure.
)"; return 0;
      }
      if(!positional && a=="--") {positional=true;continue;}
      if(!positional && (a=="--color" || a=="--material")) {
        if(++i==argc) throw std::runtime_error("Missing value for "+a);
        std::string value=argv[i];
        if(a=="--material") {
          if(value!="dark-wood") throw std::runtime_error("Supported material preset: dark-wood");
          value="80552F";
        }
        color=parseColor(value); continue;
      }
      if(!positional && a=="--autocad") {autocad=true;continue;}
      if(!positional && a=="--force") {force=true;continue;}
      if(!positional && (a=="--output" || a=="-o")) {
        if(++i==argc) throw std::runtime_error("Missing output path");
        output=argv[i];continue;
      }
      if(!positional && a.size() && a[0]=='-') throw std::runtime_error("Unknown option: "+a);
      inputs.emplace_back(a);
    }
    if(output.empty() || inputs.empty()) throw std::runtime_error("Require --output PATH and at least one input; see --help");
    if(fs::exists(output) && !force) throw std::runtime_error("Output exists; use --force to replace it");
    for(const auto& input:inputs) {
      if(!fs::is_regular_file(input)) throw std::runtime_error("Input is not a file: "+input.string());
      if(fs::weakly_canonical(input)==fs::weakly_canonical(output) || (fs::exists(output)&&fs::equivalent(input,output)))
        throw std::runtime_error("Output must not overwrite an input");
    }
    Doc doc=new TDocStd_Document("BinXCAF");
    auto tool=XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    auto root=tool->NewShape();
    TDataStd_Name::Set(root,TCollection_ExtendedString(output.stem().string().c_str(),true));
    gp_Trsf orientation;
    if(autocad) orientation.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,0,0)),-std::acos(-1.0)/2);
    const TopLoc_Location placement(orientation);
    int index=0;
    for(const auto& input:inputs) {
      auto source=read(input); TDF_LabelSequence roots;
      XCAFDoc_DocumentTool::ShapeTool(source->Main())->GetFreeShapes(roots);
      if(roots.IsEmpty()) throw std::runtime_error("No shapes in "+input.string());
      auto group=tool->NewShape();
      const auto groupName=std::to_string(++index)+" - "+input.stem().string();
      if(!XCAFDoc_Editor::Extract(roots,group)) throw std::runtime_error("Cannot copy "+input.string());
      TDataStd_Name::Set(group,TCollection_ExtendedString(groupName.c_str(),true));
      auto instance=tool->AddComponent(root,group,placement);
      TDataStd_Name::Set(instance,TCollection_ExtendedString(groupName.c_str(),true));
    }
    tool->UpdateAssemblies();
    if(color) paint(doc,*color);
    const auto before=snapshot(doc);
    if(output.has_parent_path()) fs::create_directories(output.parent_path());
    // Write to a temporary sibling and publish only after successful verification.
    fs::path temp=output; temp += ".merging.step";
    if(fs::exists(temp)) throw std::runtime_error("Temporary path already exists: "+temp.string());
    try {
      STEPCAFControl_Writer writer; Interface_Static::SetCVal("write.step.unit","MM");
      if(!writer.Transfer(doc,STEPControl_AsIs) || writer.Write(temp.string().c_str())!=IFSelect_RetDone)
        throw std::runtime_error("STEP export failed");
      const auto imported=read(temp);
      const auto after=snapshot(imported);
      if(color) {
        TDF_LabelSequence roots;
        XCAFDoc_DocumentTool::ShapeTool(imported->Main())->GetFreeShapes(roots);
        for(int i=1;i<=roots.Length();++i) verifyColor(roots.Value(i),*color);
      }

      if(before.size()!=after.size()) throw std::runtime_error("Assembly/part count changed");
      for(std::size_t i=0;i<before.size();++i) {
        const auto&a=before[i]; const auto&b=after[i];
        if(a.path!=b.path || a.solids!=b.solids) throw std::runtime_error("Hierarchy, names or solid counts changed");
        for(int k=0;k<6;++k) if(std::abs(a.bounds[k]-b.bounds[k])>1e-5) throw std::runtime_error("Part position changed");
        if(std::abs(a.volume-b.volume)>std::max(1e-5,std::abs(a.volume)*1e-7)) throw std::runtime_error("Part volume changed");
      }
      fs::rename(temp,output);
    } catch(...) {fs::remove(temp);throw;}
    std::cout<<"Merged and verified "<<inputs.size()<<" files: "<<fs::absolute(output)<<"\n";
    return 0;
  } catch(const Standard_Failure& e) {std::cerr<<e.GetMessageString()<<'\n';return 1;}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
