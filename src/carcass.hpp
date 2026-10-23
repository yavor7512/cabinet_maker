#pragma once
// Open-backed cabinet; origin at left opening edge / frame front / opening bottom.
inline int generateCarcass(const std::filesystem::path& output) {
  const auto& c=cabinet;
  const double t=c.thickness, s=c.stileWidth, f=c.stileThickness;
  const double depth=c.depth-f, height=c.height+2*t;
  if(depth<checks.minFeature || s<t)
    throw std::runtime_error("Require cabinet depth > stile thickness and stile width >= carcass thickness");
  std::vector<TopoDS_Shape> parts;
  std::vector<std::string> names;
  std::vector<double> expected;
  auto board=[&](const char* name,double x,double y,double z,double w,double d,double h) {
    parts.push_back(BRepPrimAPI_MakeBox(gp_Pnt(x,y,z),w,d,h).Shape());
    names.push_back(numbered("B",static_cast<int>(exportSettings.boardStart)+static_cast<int>(names.size()),name));
    expected.push_back(w*d*h);
  };
  board("Left carcass side",-t,f,-t,t,depth,height);
  board("Right carcass side",c.width,f,-t,t,depth,height);
  board("Carcass bottom",0,f,-t,c.width,depth,t);
  board("Carcass top",0,f,c.height,c.width,depth,t);
  board("Left face-frame stile",-s,0,-t,s,f,height);
  board("Right face-frame stile",c.width,0,-t,s,f,height);

  std::sort(expected.begin(),expected.end());
  verifyHardware(compound(parts),expected,parts.size());
  for(std::size_t i=0;i<parts.size();++i)
    for(std::size_t j=i+1;j<parts.size();++j)
      if(overlap(parts[i],parts[j])>checks.volume) throw std::runtime_error("Carcass boards overlap");
  gp_Trsf placement;
  placement.SetTranslation(gp_Vec(exportSettings.positionX,exportSettings.positionY,exportSettings.positionZ));
  for(auto& part:parts) part=BRepBuilderAPI_Transform(part,placement,true).Shape();
  if(output.has_parent_path()) std::filesystem::create_directories(output.parent_path());
  const auto imported=writeNamedAssembly(output,parts,names);
  verifyHardware(imported,expected,parts.size());
  std::cout<<"Saved and verified: "<<std::filesystem::absolute(output)<<"\n"
    <<"Overall including face frame W x D x H: "<<c.width+2*s<<" x "<<c.depth<<" x "<<height<<" mm\n"
    <<"Clear opening W x H: "<<c.width<<" x "<<c.height<<" mm; single side thickness: "<<t<<" mm\n"
    <<"Assembly position X/Y/Z: "<<exportSettings.positionX<<" / "<<exportSettings.positionY<<" / "<<exportSettings.positionZ<<" mm\n";
  return 0;
}
