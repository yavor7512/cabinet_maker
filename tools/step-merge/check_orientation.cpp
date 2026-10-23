#define main merge_main
#include "step_merge.cpp"
#undef main
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Pln.hxx>
int main(int argc,char** argv) {
  try {
    if(argc==3 && std::string(argv[1])=="--write-roles") {
      Doc doc=new TDocStd_Document("BinXCAF");
      auto tool=XCAFDoc_DocumentTool::ShapeTool(doc->Main());
      auto layers=XCAFDoc_DocumentTool::LayerTool(doc->Main());
      auto root=tool->NewShape();TDataStd_Name::Set(root,TCollection_ExtendedString("Role fixture"));
      auto board=tool->AddShape(BRepPrimAPI_MakeBox(200,100,18).Shape(),false);
      auto hardware=tool->AddShape(BRepPrimAPI_MakeCylinder(5,30).Shape(),false);
      TDataStd_Name::Set(board,TCollection_ExtendedString("Screw tray"));
      TDataStd_Name::Set(hardware,TCollection_ExtendedString("Unnamed round part"));
      layers->SetLayer(board,TCollection_ExtendedString(part_role::boardLayer));
      layers->SetLayer(hardware,TCollection_ExtendedString(part_role::hardwareLayer));
      tool->AddComponent(root,board,TopLoc_Location());
      tool->AddComponent(root,hardware,TopLoc_Location());
      auto group=tool->NewShape();TDataStd_Name::Set(group,TCollection_ExtendedString("Hardware group"));
      auto untyped=tool->AddShape(BRepPrimAPI_MakeBox(80,40,10).Shape(),false);
      TDataStd_Name::Set(untyped,TCollection_ExtendedString("Nested rectangular child"));
      layers->SetLayer(untyped,TCollection_ExtendedString(part_role::hardwareLayer));
      auto conflicting=tool->AddShape(BRepPrimAPI_MakeBox(90,40,10).Shape(),false);
      TDataStd_Name::Set(conflicting,TCollection_ExtendedString("Explicit board child"));
      layers->SetLayer(conflicting,TCollection_ExtendedString(part_role::boardLayer));
      tool->AddComponent(group,untyped,TopLoc_Location());
      tool->AddComponent(group,conflicting,TopLoc_Location());
      tool->AddComponent(root,group,TopLoc_Location());
      tool->UpdateAssemblies();
      STEPCAFControl_Writer writer;writer.SetLayerMode(true);
      if(!writer.Transfer(doc,STEPControl_AsIs) || writer.Write(argv[2])!=IFSelect_RetDone)
        throw std::runtime_error("Could not write role STEP fixture");
      return 0;
    }
    if(argc==3 && std::string(argv[1])=="--check-roles") {
      auto records=snapshot(read(argv[2]));
      int boards=0,hardware=0,nestedHardware=0,nestedBoard=0;
      for(const auto& record:records) if(record.solids) {
        if(record.path.find("Screw tray")!=std::string::npos && record.role==1) ++boards;
        else if(record.path.find("Unnamed round part")!=std::string::npos && record.role==2) ++hardware;
        else if(record.path.find("Nested rectangular child")!=std::string::npos && record.role==2) ++nestedHardware;
        else if(record.path.find("Explicit board child")!=std::string::npos && record.role==1) ++nestedBoard;
        else throw std::runtime_error("Part role changed or lost after merge: "+record.path+" (role="+std::to_string(record.role)+")");
      }
      if(boards!=1 || hardware!=1 || nestedHardware!=1 || nestedBoard!=1)
        throw std::runtime_error("Part role counts changed after merge");
      return 0;
    }
    if(argc==3 && std::string(argv[1])=="--write-face") {
      Doc doc=new TDocStd_Document("BinXCAF");
      auto tool=XCAFDoc_DocumentTool::ShapeTool(doc->Main());
      auto face=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),0,100,0,100).Shape();
      tool->AddShape(face,false);
      STEPCAFControl_Writer writer;
      if(!writer.Transfer(doc,STEPControl_AsIs) || writer.Write(argv[2])!=IFSelect_RetDone)
        throw std::runtime_error("Could not write face-only STEP fixture");
      return 0;
    }
    if(argc!=3) throw std::runtime_error("Expected normal and rotated files");
    auto normal=snapshot(read(argv[1])), rotated=snapshot(read(argv[2]));
    if(normal.size()!=rotated.size()) throw std::runtime_error("Part count changed");
    for(std::size_t i=0;i<normal.size();++i) {
      auto a=normal[i],b=rotated[i];
      if(a.path!=b.path || a.solids!=b.solids) throw std::runtime_error("Names/hierarchy changed");
      const auto& x=a.bounds;
      const std::array<double,6> expected{x[0],x[2],-x[4],x[3],x[5],-x[1]};
      for(int k=0;k<6;++k) if(std::abs(expected[k]-b.bounds[k])>1e-5) throw std::runtime_error("Incorrect axis mapping");
      if(std::abs(a.volume-b.volume)>std::max(1e-5,std::abs(a.volume)*1e-7)) throw std::runtime_error("Volume changed");
    }
    std::cout<<"Verified X,Y,Z -> X,Z,-Y for every part; names and hierarchy preserved.\n";
  } catch(const std::exception&e) {std::cerr<<e.what()<<'\n';return 1;}
}
