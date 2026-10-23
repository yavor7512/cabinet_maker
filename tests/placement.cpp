#include <STEPCAFControl_Reader.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDataStd_Name.hxx>
#include <BRepBndLib.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <cmath>
#include <iostream>
#include <stdexcept>

TopoDS_Shape read(const char* path, const char* board, const double expected[6]) {
  Handle(TDocStd_Document) doc=new TDocStd_Document("BinXCAF");
  STEPCAFControl_Reader reader;
  if(reader.ReadFile(path)!=IFSelect_RetDone || !reader.Transfer(doc)) throw std::runtime_error("Import failed");
  auto tool=XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  TDF_LabelSequence roots,components;
  tool->GetFreeShapes(roots);
  if(roots.Length()!=1) throw std::runtime_error("Expected one assembly");
  tool->GetComponents(roots.Value(1),components,false);
  if(std::string(board)=="B12 - Left carcass side" && components.Length()!=6)
    throw std::runtime_error("Expected exactly six carcass boards without spacers");
  bool found=false;
  for(int i=1;i<=components.Length();++i) {
    TDF_Label definition;
    tool->GetReferredShape(components.Value(i),definition);
    Handle(TDataStd_Name) name;
    if(!definition.FindAttribute(TDataStd_Name::GetID(),name)) continue;
    if(TCollection_AsciiString(name->Get())!=board) continue;
    found=true;
    Bnd_Box box;
    BRepBndLib::AddOptimal(tool->GetShape(components.Value(i)),box,false,false);
    double actual[6]; box.Get(actual[0],actual[1],actual[2],actual[3],actual[4],actual[5]);
    for(int j=0;j<6;++j)
      if(std::abs(actual[j]-expected[j])>1e-5) throw std::runtime_error("Script drawer position differs from expected drawing coordinates");
  }
  if(!found) throw std::runtime_error("Named left side missing");
  return tool->GetShape(roots.Value(1));
}
int main(int argc,char** argv) {
  try {
    if(argc!=4) throw std::runtime_error("Expected upper, lower and carcass STEP paths");
    // Independent reference coordinates from the drawing and script dimensions.
    const double upperBounds[]{0,12,619,12,535,721};
    const double lowerBounds[]{0,0,35,12,523,238};
    const auto upper=read(argv[1],"B01 - Left side",upperBounds);
    const auto lower=read(argv[2],"B06 - Left side",lowerBounds);
    const double faceBounds[]{-22,-12,-16,499,0,746};
    read(argv[2],"B11 - Applied drawer face",faceBounds);
    const double carcassBounds[]{-26.5,21.5,-18,-8.5,600.5,755};
    const auto carcass=read(argv[3],"B12 - Left carcass side",carcassBounds);
    const double bottomBounds[]{-8.5,21.5,-18,485.5,600.5,0};
    read(argv[3],"B14 - Carcass bottom",bottomBounds);
    const double rightBounds[]{485.5,21.5,-18,503.5,600.5,755};
    read(argv[3],"B13 - Right carcass side",rightBounds);
    const TopoDS_Shape models[]{upper,lower,carcass};
    for(int i=0;i<3;++i) for(int j=i+1;j<3;++j) {
    BRepAlgoAPI_Common intersection(models[i],models[j]);
    if(!intersection.IsDone()) throw std::runtime_error("Intersection failed");
    GProp_GProps props; BRepGProp::VolumeProperties(intersection.Shape(),props);
    if(std::abs(props.Mass())>1e-5) throw std::runtime_error("Drawer assemblies intersect");
    }
    std::cout<<"Verified 584 mm relative elevation, 12 mm recess, and no overlap.\n";
  } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
