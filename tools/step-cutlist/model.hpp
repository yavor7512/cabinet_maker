#pragma once
#include "nesting.hpp"
#include <STEPCAFControl_Reader.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDocStd_Document.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_LabelSequence.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <gp_Pln.hxx>
#include <gp_Ax3.hxx>
#include <filesystem>
#include <regex>
namespace cut {
struct Rejected {std::string name,reason; bool excluded=false;};
struct GeometrySettings {double tolerance=0.01,maxThickness=50.8,maxRatio=0.5; bool strict=false;};
std::string labelName(const TDF_Label&label) {
  Handle(TDataStd_Name)n;if(!label.FindAttribute(TDataStd_Name::GetID(),n))return "unnamed";
  std::vector<char>b(n->Get().LengthOfCString()+1);Standard_PCharacter p=b.data();n->Get().ToUTF8CString(p);return p;
}
Part classifyStrict(const TopoDS_Shape&solid,const std::string&name,const GeometrySettings&cfg) {
  if(!BRepCheck_Analyzer(solid).IsValid())throw std::runtime_error("invalid solid");
  std::vector<gp_Dir> axes;
  for(TopExp_Explorer it(solid,TopAbs_FACE);it.More();it.Next()) {
    BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
    if(surface.GetType()!=GeomAbs_Plane)throw std::runtime_error("nonplanar face: curved geometry or holes");
    auto dir=surface.Plane().Axis().Direction();bool known=false;
    for(auto&a:axes) {
      double dot=std::abs(a.Dot(dir));if(dot>1-1e-10){known=true;break;}
      if(dot>1e-10)throw std::runtime_error("angled face: not a rectangular 90-degree part");
    }
    if(!known)axes.push_back(dir);
  }
  if(axes.size()!=3)throw std::runtime_error("not a rectangular solid");
  gp_Dir z=axes[0].Crossed(axes[1]);gp_Trsf local;local.SetTransformation(gp_Ax3(gp_Pnt(0,0,0),z,axes[0]));
  auto shape=BRepBuilderAPI_Transform(solid,local,true).Shape();
  Bnd_Box b;BRepBndLib::AddOptimal(shape,b,false,false);double x,y,zz,X,Y,Z;b.Get(x,y,zz,X,Y,Z);
  std::vector<double>d{X-x,Y-y,Z-zz};std::sort(d.begin(),d.end());
  if(d[0]<=cfg.tolerance)throw std::runtime_error("zero/near-zero thickness");
  GProp_GProps props;BRepGProp::VolumeProperties(shape,props);double box=d[0]*d[1]*d[2];
  // Tight volume check prevents machining details being silently treated as stock.
  if(std::abs(std::abs(props.Mass())-box)>std::max(1e-5,box*1e-8))throw std::runtime_error("not a full rectangle: dados, pockets, notches or voids");
  if(d[0]>cfg.maxThickness+cfg.tolerance || d[0]/d[1]>cfg.maxRatio)throw std::runtime_error("not sheet-like under thickness/ratio limits");
  return {name,d[2],d[1],d[0]};
}
// Recover rectangular stock from surviving orthogonal exterior datum faces.
// Interior curved/angled faces are machining, not new stock dimensions.
Part classify(const TopoDS_Shape&solid,const std::string&name,const GeometrySettings&cfg) {
  if(cfg.strict)return classifyStrict(solid,name,cfg);
  if(!BRepCheck_Analyzer(solid).IsValid())throw std::runtime_error("invalid solid");
  std::vector<gp_Dir> directions;
  for(TopExp_Explorer it(solid,TopAbs_FACE);it.More();it.Next()) {
    BRepAdaptor_Surface f(TopoDS::Face(it.Current()));if(f.GetType()!=GeomAbs_Plane)continue;
    gp_Dir n=f.Plane().Axis().Direction();
    if(std::none_of(directions.begin(),directions.end(),[&](const gp_Dir&d){return std::abs(d.Dot(n))>1-1e-10;}))directions.push_back(n);
  }
  double best=INFINITY;Part result;bool found=false;
  for(std::size_t i=0;i<directions.size();++i)for(std::size_t j=i+1;j<directions.size();++j) {
    if(std::abs(directions[i].Dot(directions[j]))>1e-10)continue;
    gp_Dir z=directions[i].Crossed(directions[j]);
    if(std::none_of(directions.begin(),directions.end(),[&](const gp_Dir&d){return std::abs(d.Dot(z))>1-1e-10;}))continue;
    gp_Trsf local;local.SetTransformation(gp_Ax3(gp_Pnt(0,0,0),z,directions[i]));
    auto shape=BRepBuilderAPI_Transform(solid,local,true).Shape();
    Bnd_Box box;BRepBndLib::AddOptimal(shape,box,false,false);double lo[3],hi[3];box.Get(lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);
    bool supported[6]{};
    for(TopExp_Explorer it(shape,TopAbs_FACE);it.More();it.Next()) {
      BRepAdaptor_Surface f(TopoDS::Face(it.Current()));if(f.GetType()!=GeomAbs_Plane)continue;
      auto plane=f.Plane();auto n=plane.Axis().Direction();auto point=plane.Location();
      GProp_GProps surface;BRepGProp::SurfaceProperties(it.Current(),surface);
      if(surface.Mass()<cfg.tolerance*cfg.tolerance)continue;
      for(int k=0;k<3;++k)if(std::abs(n.Coord(k+1))>1-1e-10) {
        if(std::abs(point.Coord(k+1)-lo[k])<=cfg.tolerance)supported[2*k]=true;
        if(std::abs(point.Coord(k+1)-hi[k])<=cfg.tolerance)supported[2*k+1]=true;
      }
    }
    if(!std::all_of(std::begin(supported),std::end(supported),[](bool b){return b;}))continue;
    std::vector<double>d{hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2]};std::sort(d.begin(),d.end());
    if(d[0]<=cfg.tolerance||d[0]>cfg.maxThickness+cfg.tolerance||d[0]/d[1]>cfg.maxRatio)continue;
    double volume=d[0]*d[1]*d[2];if(volume>=best)continue;
    GProp_GProps props;BRepGProp::VolumeProperties(shape,props);double removed=volume-std::abs(props.Mass());
    if(removed < -std::max(1e-5,volume*1e-8))continue;
    best=volume;found=true;result={name,d[2],d[1],d[0],removed>std::max(1e-5,volume*1e-8),std::max(0.0,removed)};
  }
  if(!found)throw std::runtime_error("cannot identify sheet blank with six surviving rectangular exterior datum planes");
  return result;
}
void walk(const TDF_Label&label,const TopLoc_Location&parent,const std::string&path,const GeometrySettings&cfg,std::vector<Part>&parts,std::vector<Rejected>&rejected) {
  auto def=label;auto location=parent;
  if(XCAFDoc_ShapeTool::IsReference(label)) {XCAFDoc_ShapeTool::GetReferredShape(label,def);location=parent*XCAFDoc_ShapeTool::GetLocation(label);}
  std::string full=path+"/"+labelName(def);TDF_LabelSequence children;
  if(XCAFDoc_ShapeTool::GetComponents(def,children,false)) {
    for(int i=1;i<=children.Length();++i)walk(children.Value(i),location,full+"["+std::to_string(i)+"]",cfg,parts,rejected);return;
  }
  auto shape=XCAFDoc_ShapeTool::GetShape(def).Moved(location);int i=0;
  for(TopExp_Explorer it(shape,TopAbs_SOLID);it.More();it.Next()) {
    auto id=full+"#"+std::to_string(++i);
    const std::regex hardware(R"(\b(screw|bolt|nut|washer)\b)",std::regex::icase);
    if(std::regex_search(labelName(def),hardware)) {rejected.push_back({id,"named hardware excluded from plywood inventory",true});continue;}
    try {parts.push_back(classify(it.Current(),id,cfg));}catch(const std::exception&e){rejected.push_back({id,e.what()});}
  }
  if(!i)rejected.push_back({full,"no solid: open surfaces/wires are unsupported"});
}
void load(const std::filesystem::path&path,const GeometrySettings&cfg,std::vector<Part>&parts,std::vector<Rejected>&rejected) {
  Handle(TDocStd_Document)doc=new TDocStd_Document("BinXCAF");STEPCAFControl_Reader reader;
  if(reader.ReadFile(path.string().c_str())!=IFSelect_RetDone || !reader.Transfer(doc))throw std::runtime_error("Cannot read STEP: "+path.string());
  TDF_LabelSequence roots;XCAFDoc_DocumentTool::ShapeTool(doc->Main())->GetFreeShapes(roots);
  if(roots.IsEmpty())throw std::runtime_error("No shapes: "+path.string());
  for(int i=1;i<=roots.Length();++i)walk(roots.Value(i),TopLoc_Location(),path.filename().string()+"["+std::to_string(i)+"]",cfg,parts,rejected);
}
}
