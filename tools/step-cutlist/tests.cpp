#define main cutlist_cli_main
#include "main.cpp"
#undef main
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <gp_Ax1.hxx>
#include <cstdlib>
void require(bool b,const char*m){if(!b)throw std::runtime_error(m);}
void reject(const TopoDS_Shape&s){bool rejected=false;try{classify(s,"bad",GeometrySettings{0.01,50.8,0.5,true});}catch(const std::exception&){rejected=true;}require(rejected,"unsupported solid accepted");}
int main(int argc,char**argv){try{
  auto box=BRepPrimAPI_MakeBox(773,579,18).Shape();auto p=classify(box,"board",{});
  require(std::abs(p.w-773)<eps&&std::abs(p.h-579)<eps&&std::abs(p.t-18)<eps,"board dimensions");
  gp_Trsf rotation;rotation.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,2,3)),0.731);
  gp_Trsf move;move.SetTranslation(gp_Vec(123,-456,789));
  auto tilted=BRepBuilderAPI_Transform(box,move*rotation,true).Shape();p=classify(tilted,"tilted",{});
  require(std::abs(p.w-773)<eps&&std::abs(p.h-579)<eps&&std::abs(p.t-18)<eps,"rotated board dimensions");
  auto dado=BRepAlgoAPI_Cut(box,BRepPrimAPI_MakeBox(gp_Pnt(10,0,0),20,579,10).Shape()).Shape();
  auto blank=classify(dado,"dado",{});
  require(blank.machined && std::abs(blank.w-773)<eps && std::abs(blank.h-579)<eps && std::abs(blank.t-18)<eps,"dado blank not recovered");
  auto tiltedDado=BRepBuilderAPI_Transform(dado,move*rotation,true).Shape();
  blank=classify(tiltedDado,"rotated dado",{});
  require(blank.machined && std::abs(blank.w-773)<eps && std::abs(blank.h-579)<eps,"rotated dado blank");
  auto drilled=BRepAlgoAPI_Cut(box,BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(100,100,0),gp_Dir(0,0,1)),5,18).Shape()).Shape();
  require(classify(drilled,"drilled",{}).machined,"drilled board omitted");
  bool hardwareRejected=false;try{classify(BRepPrimAPI_MakeCylinder(10,50).Shape(),"unnamed",{});}catch(...){hardwareRejected=true;}require(hardwareRejected,"cylinder accepted as blank");
  reject(BRepPrimAPI_MakeCylinder(10,50).Shape());
  reject(BRepAlgoAPI_Cut(box,BRepPrimAPI_MakeBox(gp_Pnt(10,10,0),20,20,10).Shape()).Shape());
  reject(BRepAlgoAPI_Cut(box,BRepPrimAPI_MakeBox(gp_Pnt(0,0,0),20,20,18).Shape()).Shape());
  reject(BRepPrimAPI_MakeBox(100,100,100).Shape());
  {
    // Geometry and explicit STEP role, never the display name, decide inventory.
    Handle(TDocStd_Document) doc=new TDocStd_Document("BinXCAF");
    auto tool=XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    auto layers=XCAFDoc_DocumentTool::LayerTool(doc->Main());
    auto root=tool->NewShape();TDataStd_Name::Set(root,TCollection_ExtendedString("Named parts"));
    auto board=tool->AddShape(box,false);TDataStd_Name::Set(board,TCollection_ExtendedString("Screw tray panel"));
    auto machined=tool->AddShape(dado,false);
    TDataStd_Name::Set(machined,TCollection_ExtendedString("Screw tray dado panel"));
    auto screw=tool->AddShape(BRepPrimAPI_MakeCylinder(5,25).Shape(),false);
    TDataStd_Name::Set(screw,TCollection_ExtendedString("Shelf support"));
    auto untyped=tool->AddShape(BRepPrimAPI_MakeCylinder(6,30).Shape(),false);
    TDataStd_Name::Set(untyped,TCollection_ExtendedString("Pocket screw"));
    layers->SetLayer(board,TCollection_ExtendedString(part_role::boardLayer));
    layers->SetLayer(machined,TCollection_ExtendedString(part_role::boardLayer));
    layers->SetLayer(screw,TCollection_ExtendedString(part_role::hardwareLayer));
    tool->AddComponent(root,board,TopLoc_Location());
    tool->AddComponent(root,machined,TopLoc_Location());
    tool->AddComponent(root,screw,TopLoc_Location());
    tool->AddComponent(root,untyped,TopLoc_Location());
    tool->UpdateAssemblies();
    std::vector<Part> namedParts;std::vector<Rejected> namedRejected;
    walk(root,TopLoc_Location(),"test",{},layers,namedParts,namedRejected);
    require(namedParts.size()==2&&namedParts[0].name.find("Screw tray panel")!=std::string::npos,
      "rectangular board with a hardware word was excluded");
    require(namedRejected.size()==2&&namedRejected[0].excluded&&!namedRejected[1].excluded,
      "role-tagged hardware or untyped unsupported geometry was misclassified");
    TDataStd_Name::Set(board,TCollection_ExtendedString("Plain panel"));
    TDataStd_Name::Set(screw,TCollection_ExtendedString("Screw"));
    TDataStd_Name::Set(untyped,TCollection_ExtendedString("Ordinary part"));
    namedParts.clear();namedRejected.clear();
    walk(root,TopLoc_Location(),"test",{},layers,namedParts,namedRejected);
    require(namedParts.size()==2&&namedRejected.size()==2&&namedRejected[0].excluded&&!namedRejected[1].excluded,
      "renaming changed board or hardware classification");
    namedParts.clear();namedRejected.clear();
    GeometrySettings strict;strict.strict=true;
    walk(root,TopLoc_Location(),"test",strict,layers,namedParts,namedRejected);
    require(namedParts.size()==1&&namedRejected.size()==3,
      "strict mode did not reject the machined board");
    require(!namedRejected[0].excluded&&namedRejected[1].excluded&&!namedRejected[2].excluded,
      "strict mode misclassified a machined board or untyped solid");
  }
  {
    Handle(TDocStd_Document) doc=new TDocStd_Document("BinXCAF");
    auto tool=XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    auto layers=XCAFDoc_DocumentTool::LayerTool(doc->Main());
    auto root=tool->NewShape();
    layers->SetLayer(root,TCollection_ExtendedString(part_role::hardwareLayer));
    auto untyped=tool->AddShape(box,false);
    auto explicitBoard=tool->AddShape(BRepPrimAPI_MakeBox(200,100,18).Shape(),false);
    layers->SetLayer(explicitBoard,TCollection_ExtendedString(part_role::boardLayer));
    tool->AddComponent(root,untyped,TopLoc_Location());
    tool->AddComponent(root,explicitBoard,TopLoc_Location());
    tool->UpdateAssemblies();
    std::vector<Part> accepted;std::vector<Rejected> rejected;
    walk(root,TopLoc_Location(),"inherited",{},layers,accepted,rejected);
    require(accepted.empty()&&rejected.size()==2&&rejected[0].excluded&&!rejected[1].excluded,
      "assembly role did not exclude untyped child or flag explicitly tagged board conflict");
    require(rejected[1].reason=="conflicting board and hardware roles", "role conflict reason missing");
  }
  Settings cfg;cfg.kerf=0;std::vector<Part>parts{{"a",60,40,10},{"b",60,40,10}};
  auto r=optimize(parts,{{"square",100,100}},cfg);require(r.exact&&r.sheets.size()==1,"two parts one sheet");
  parts={{"a",50,100,10},{"b",50,100,10}};r=optimize(parts,{{"square",100,100}},cfg);require(r.sheets.size()==1,"zero kerf exact fit");
  cfg.kerf=1;r=optimize(parts,{{"square",100,100}},cfg);require(r.exact&&r.sheets.size()==2,"kerf must require extra sheet");
  cfg.kerf=0;parts={{"a",80,40,10}};bool failed=false;try{optimize(parts,{{"narrow",50,100}},cfg);}catch(...){failed=true;}require(failed,"grain lock ignored");
  cfg.rotate=true;r=optimize(parts,{{"narrow",50,100}},cfg);require(r.exact&&r.sheets.size()==1,"rotation should fit");
  cfg.rotate=false;r=optimize(parts,{{"large",100,100},{"small",80,50}},cfg);require(r.sheets[0].stock.name=="small","area objective");
  cfg.maxCandidates=1;r=optimize(parts,{{"large",100,100}},cfg);require(!r.exact,"budget must report heuristic");
  cfg=Settings{};cfg.exactLimit=0;r=optimize({{"a",773,579,18},{"b",773,579,18},{"c",579,494,18},{"d",579,494,18},{"e",773,38,18},{"f",773,38,18}},{{"8x4",2438.4,1219.2}},cfg);
  std::vector<Placement> placements;std::vector<SawCut> materialCuts;int index=0;
  std::vector<Part>ps{{"a",773,579,18},{"b",773,579,18},{"c",579,494,18},{"d",579,494,18},{"e",773,38,18},{"f",773,38,18}};
  for(auto&s:r.sheets)shopLayout(s.tree,++index,0,0,s.stock.w,s.stock.h,cfg.kerf,placements,materialCuts,0,0,1);
  labelSawSequence(materialCuts,r.sheets,cfg.kerf,placements);validate(placements,ps,r.sheets,cfg.kerf);
  cfg=Settings{};
  auto shop=optimizeShop(ps,{{"8x4",2438.4,1219.2},{"4x4",1219.2,1219.2},{"5x5",1524,1524}},cfg,0,32);
  require(shop.sheets.size()==1&&shop.sheets[0].stock.name=="5x5","shop increased sheet area");
  require(shopCuts(shop)==8,"carcass should need eight kept-edge saw cuts");
  std::vector<Placement>placed;std::vector<SawCut>saw;int si=0;
  for(auto&s:shop.sheets)shopLayout(s.tree,++si,0,0,s.stock.w,s.stock.h,cfg.kerf,placed,saw,0,0,planMode(s.tree,s.stock.w,s.stock.h,cfg.kerf));
  std::stable_sort(saw.begin(),saw.end(),[](auto&a,auto&b){return a.sheet!=b.sheet?a.sheet<b.sheet:a.stage<b.stage;});
  validate(placed,ps,shop.sheets,cfg.kerf);labelSawSequence(saw,shop.sheets,cfg.kerf,placed);
  require(saw.size()==8,"cut score/emission mismatch");
  // Size both full-height columns before crosscutting either one. This is
  // independent of the precise kerf and avoids trimming the width twice.
  require(saw[0].axis=='V'&&std::abs(saw[0].at-saw[0].x-773)<eps,"first column width");
  require(saw[1].axis=='V'&&saw[1].source=="C1R"&&std::abs(saw[1].at-saw[1].x-579)<eps,"second column width");
  require(std::all_of(saw.begin()+2,saw.end(),[](const SawCut&c){return c.axis=='H'&&c.stage==2;}),"columns should need only independent crosscuts");
  for(double actual:{0.0,1.2,3.2,4.9,5.0})traceSawSequence(saw,shop.sheets,5,&placed,actual);
  auto invalid=saw;std::swap(invalid[0],invalid.back());bool badSequence=false;
  try{validateSawSequence(invalid,shop.sheets,cfg.kerf,&placed);}catch(...){badSequence=true;}require(badSequence,"invalid saw order accepted");
  // Mixed sizes/rotation exercise every trim policy and ensure saw cuts really
  // produce the placed blanks, including when exact optimization is unavailable.
  for(int trial=0;trial<12;++trial){
    std::vector<Part>randomParts;for(int i=0;i<7;++i)randomParts.push_back({"test",double(20+(trial*17+i*11)%60),double(15+(trial*7+i*13)%40),5});
    cfg.exactLimit=0;cfg.rotate=trial%2;
    auto candidate=optimizeShop(randomParts,{{"square",100,100},{"large",150,100}},cfg,0,8);
    std::vector<Placement>pp;std::vector<SawCut>cc;si=0;
    for(auto&s:candidate.sheets)shopLayout(s.tree,++si,0,0,s.stock.w,s.stock.h,cfg.kerf,pp,cc,0,0,planMode(s.tree,s.stock.w,s.stock.h,cfg.kerf));
    std::stable_sort(cc.begin(),cc.end(),[](auto&a,auto&b){return a.sheet!=b.sheet?a.sheet<b.sheet:a.stage<b.stage;});
    validate(pp,randomParts,candidate.sheets,cfg.kerf);labelSawSequence(cc,candidate.sheets,cfg.kerf,pp);
    for(double actual:{0.0,1.2,3.2,4.9,5.0})traceSawSequence(cc,candidate.sheets,5,&pp,actual);
    require((int)cc.size()==shopCuts(candidate),"shop cut count wrong");
  }

  // The actual two-drawer inventory should also start with two columns and
  // retain the original 16-cut count with the smaller front and 5 mm allowance.
  {
    std::vector<Part> blanks{{"side",523,102,12},{"side",523,102,12},
      {"end",453,102,12},{"end",453,102,12},{"bottom",511,465,12},
      {"side",523,203,12},{"side",523,203,12},{"end",453,203,12},
      {"end",453,203,12},{"bottom",511,465,12},{"face",711.2,469.9,12}};
    Settings settings;
    auto plan=optimizeShop(blanks,{{"5x5",1524,1524}},settings,35,64);
    require(plan.sheets.size()==1&&shopCuts(plan)==16,"drawer sheet should retain sixteen cuts");
    std::vector<Placement> pp;std::vector<SawCut> cc;int index=0;
    for(const auto&s:plan.sheets)shopLayout(s.tree,++index,0,0,s.stock.w,s.stock.h,5,pp,cc,0,0,planMode(s.tree,s.stock.w,s.stock.h,5));
    std::stable_sort(cc.begin(),cc.end(),[](const SawCut&a,const SawCut&b){return a.sheet!=b.sheet?a.sheet<b.sheet:a.stage<b.stage;});
    labelSawSequence(cc,plan.sheets,5,pp);validate(pp,blanks,plan.sheets,5);
    require(cc.size()==16&&cc[0].axis=='V'&&cc[1].axis=='V'&&cc[1].source=="C1R","drawer sheet should start with two independent columns");
    for(double actual:{0.0,1.2,3.2,4.9,5.0})traceSawSequence(cc,plan.sheets,5,&pp,actual);
  }

  // Exactly packed siblings expose the old error: the last right remainder
  // grows when the real blade is thinner than the planning allowance.
  for(char axis:{'V','H'})for(int mode=0;mode<4;++mode){
    std::vector<Part> blanks{{"first",50,40,12},{"last",50,40,12}};
    auto tree=join(leaf(blanks[0],0),leaf(blanks[1],1),axis,5);
    std::vector<Sheet> sheets{{{"exact",tree->w,tree->h},tree}};
    std::vector<Placement> pp;std::vector<SawCut> cc;
    shopLayout(tree,1,0,0,tree->w,tree->h,5,pp,cc,0,0,mode);
    labelSawSequence(cc,sheets,5,pp);
    require(cc.size()==2,"exact-fit remainder needs a finishing pass");
    require(cc[0].source=="S1"&&cc[1].source=="C1R","remainder reference is wrong");
    require(pp[0].source=="C1L"&&pp[1].source=="C2L","finished pieces must be on the left");
    for(double actual:{0.0,1.2,3.2,4.9,5.0})traceSawSequence(cc,sheets,5,&pp,actual);
    auto unsafe=cc;unsafe.pop_back();validateSawSequence(unsafe,sheets,5,&pp);
    bool detected=false;try{traceSawSequence(unsafe,sheets,5,&pp,3.2);}catch(const std::exception&){detected=true;}
    require(detected,"missing finishing pass accepted for a thinner blade");
    bool tooWide=false;try{traceSawSequence(cc,sheets,5,&pp,5.1);}catch(const std::exception&){tooWide=true;}
    require(tooWide,"blade wider than allowance accepted");
  }

  // Scoring and emission must agree for every trim policy on a mixed tree
  // with shared width, a kerf-dependent remainder, and extra stock at both edges.
  {
    std::vector<Part> blanks{{"first",50,30,12},{"second",50,40,12},{"third",35,75,12}};
    auto tree=join(join(leaf(blanks[0],0),leaf(blanks[1],1),'H',5),leaf(blanks[2],2),'V',5);
    std::vector<Sheet> sheets{{{"mixed",100,85},tree}};
    for(int mode=0;mode<4;++mode){
      std::vector<Placement> pp;std::vector<SawCut> cc;
      shopLayout(tree,1,0,0,100,85,5,pp,cc,0,0,mode);
      require((int)cc.size()==plannedCuts(tree,100,85,5,mode),"trim policy score differs from emitted cuts");
      validate(pp,blanks,sheets,5);
      labelSawSequence(cc,sheets,5,pp);
      for(double actual:{0.0,2.5,5.0})traceSawSequence(cc,sheets,5,&pp,actual);
    }
  }

  if(argc>1){
    // A named assembly with repeated, located board occurrences for CLI integration.
    Handle(TDocStd_Document)doc=new TDocStd_Document("BinXCAF");auto tool=XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    auto root=tool->NewShape();TDataStd_Name::Set(root,TCollection_ExtendedString("Fixture"));
    auto def=tool->AddShape(box,false);TDataStd_Name::Set(def,TCollection_ExtendedString("Panel"));
    tool->AddComponent(root,def,TopLoc_Location());tool->AddComponent(root,def,TopLoc_Location(move*rotation));tool->UpdateAssemblies();
    STEPCAFControl_Writer writer;if(!writer.Transfer(doc,STEPControl_AsIs)||writer.Write(argv[1])!=IFSelect_RetDone)throw std::runtime_error("fixture export failed");
    std::vector<Part> got;std::vector<Rejected>bad;load(argv[1],{},got,bad);require(got.size()==2&&bad.empty(),"repeated located instances lost");
  }
  std::cout<<"Passed geometry, exact search, kerf, grain, budget, layout and assembly tests.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
