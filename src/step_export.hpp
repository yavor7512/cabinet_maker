#pragma once
#include <STEPCAFControl_Writer.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_LabelSequence.hxx>
#include <set>
#include <sstream>
#include <iomanip>

inline std::string numbered(const char* prefix, int number, const std::string& description) {
  std::ostringstream out;
  out << prefix << std::setfill('0') << std::setw(2) << number << " - " << description;
  return out.str();
}
inline std::string labelName(const TDF_Label& label) {
  Handle(TDataStd_Name) attribute;
  if (!label.FindAttribute(TDataStd_Name::GetID(),attribute)) return {};
  const auto& name=attribute->Get();
  std::vector<char> buffer(static_cast<std::size_t>(name.LengthOfCString())+1);
  Standard_PCharacter target=buffer.data();
  name.ToUTF8CString(target);
  return buffer.data();
}
inline TopoDS_Shape writeNamedAssembly(const std::filesystem::path& output,
                                      const std::vector<TopoDS_Shape>& parts,
                                      const std::vector<std::string>& names,
                                      const std::string& assemblyName) {
  if (parts.size()!=names.size()) throw std::runtime_error("Part/name count mismatch");
  if (assemblyName.empty()) throw std::runtime_error("Assembly name cannot be empty");
  Handle(TDocStd_Document) doc = new TDocStd_Document("BinXCAF");
  auto tool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  const TDF_Label root = tool->NewShape();
  TDataStd_Name::Set(root,TCollection_ExtendedString(assemblyName.c_str(),true));
  for (std::size_t i=0;i<parts.size();++i) {
    // Strip incidental compound wrappers left by Boolean cuts so each part
    // imports as a solid beneath the selected assembly.
    TopExp_Explorer it(parts[i],TopAbs_SOLID);
    if (!it.More()) throw std::runtime_error("Named part has no solid");
    const TopoDS_Shape solid=it.Current();
    it.Next();
    if (it.More()) throw std::runtime_error("Named part contains multiple solids");
    const TDF_Label definition=tool->AddShape(solid,false);
    TDataStd_Name::Set(definition,TCollection_ExtendedString(names[i].c_str(),true));
    const TDF_Label instance=tool->AddComponent(root,definition,TopLoc_Location());
    TDataStd_Name::Set(instance,TCollection_ExtendedString(names[i].c_str(),true));
  }
  tool->UpdateAssemblies();
  STEPCAFControl_Writer writer;
  writer.SetNameMode(true);
  Interface_Static::SetCVal("write.step.unit","MM");
  if (!writer.Transfer(doc,STEPControl_AsIs) || writer.Write(output.string().c_str())!=IFSelect_RetDone)
    throw std::runtime_error("Could not write named STEP assembly");

  // Reimport XCAF data, checking hierarchy and names as well as geometry.
  Handle(TDocStd_Document) imported = new TDocStd_Document("BinXCAF");
  STEPCAFControl_Reader reader;
  reader.SetNameMode(true);
  if (reader.ReadFile(output.string().c_str())!=IFSelect_RetDone || !reader.Transfer(imported))
    throw std::runtime_error("Could not reimport named STEP assembly");
  auto importedTool=XCAFDoc_DocumentTool::ShapeTool(imported->Main());
  TDF_LabelSequence roots;
  importedTool->GetFreeShapes(roots);
  if (roots.Length()!=1 || !XCAFDoc_ShapeTool::IsAssembly(roots.Value(1)))
    throw std::runtime_error("STEP must contain one root assembly");
  if (labelName(roots.Value(1))!=assemblyName)
    throw std::runtime_error("STEP assembly name did not survive export");
  TDF_LabelSequence components;
  XCAFDoc_ShapeTool::GetComponents(roots.Value(1),components,false);
  if (components.Length()!=static_cast<int>(names.size()))
    throw std::runtime_error("STEP assembly component count changed");
  std::set<std::string> expected(names.begin(),names.end()), actual;
  for (int i=1;i<=components.Length();++i) {
    TDF_Label definition;
    if (!XCAFDoc_ShapeTool::GetReferredShape(components.Value(i),definition))
      throw std::runtime_error("STEP component lost its definition");
    const std::string name=labelName(definition);
    if (labelName(components.Value(i))!=name || !actual.insert(name).second)
      throw std::runtime_error("STEP instance names missing or duplicated");
    const auto original=std::find(names.begin(),names.end(),name);
    if (original==names.end()) throw std::runtime_error("Unexpected STEP part name");
    const auto& source=parts[static_cast<std::size_t>(original-names.begin())];
    // Check each named instance's world bounds, detecting lost placement.
    Bnd_Box sourceBounds, importedBounds;
    BRepBndLib::AddOptimal(source,sourceBounds,false,false);
    BRepBndLib::AddOptimal(importedTool->GetShape(components.Value(i)),importedBounds,false,false);
    double a[6], b[6];
    sourceBounds.Get(a[0],a[1],a[2],a[3],a[4],a[5]);
    importedBounds.Get(b[0],b[1],b[2],b[3],b[4],b[5]);
    for (int axis=0;axis<6;++axis)
      if (std::abs(a[axis]-b[axis])>checks.distance)
        throw std::runtime_error("Named component position changed in STEP");
    const double expectedVolume=volume(source);
    if (std::abs(volume(importedTool->GetShape(definition))-expectedVolume)>
        std::max(checks.volume,expectedVolume*checks.relative))
      throw std::runtime_error("Named part geometry changed in STEP");
  }
  if (actual!=expected) throw std::runtime_error("STEP part names did not survive export");
  return importedTool->GetShape(roots.Value(1));
}
