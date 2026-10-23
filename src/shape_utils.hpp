#pragma once
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <gp_Ax2.hxx>
#include <gp_Vec.hxx>

// Shape operations shared by the configuration-driven geometry engine.
inline double volume(const TopoDS_Shape& shape) {
  GProp_GProps p;
  BRepGProp::VolumeProperties(shape, p);
  return p.Mass();
}
inline TopoDS_Shape difference(const TopoDS_Shape& a, const TopoDS_Shape& b) {
  BRepAlgoAPI_Cut op(a,b);
  if (!op.IsDone()) throw std::runtime_error("Boolean cut failed");
  return op.Shape();
}
inline TopoDS_Shape unite(const TopoDS_Shape& a, const TopoDS_Shape& b) {
  BRepAlgoAPI_Fuse op(a,b);
  if (!op.IsDone()) throw std::runtime_error("Screw union failed");
  return op.Shape();
}
inline TopoDS_Shape intersection(const TopoDS_Shape& a, const TopoDS_Shape& b) {
  BRepAlgoAPI_Common op(a,b);
  if (!op.IsDone()) throw std::runtime_error("Boolean intersection check failed");
  return op.Shape();
}
inline double overlap(const TopoDS_Shape& a, const TopoDS_Shape& b) {
  return volume(intersection(a,b));
}
inline gp_Pnt along(const gp_Pnt& p, const gp_Dir& direction, double distance) {
  return p.Translated(gp_Vec(direction) * distance);
}
inline TopoDS_Compound compound(const std::vector<TopoDS_Shape>& parts) {
  BRep_Builder builder;
  TopoDS_Compound result;
  builder.MakeCompound(result);
  for (const auto& p : parts) builder.Add(result,p);
  return result;
}
inline std::vector<double> solidVolumes(const TopoDS_Shape& shape) {
  std::vector<double> result;
  for (TopExp_Explorer it(shape,TopAbs_SOLID); it.More(); it.Next()) {
    if (!BRepCheck_Analyzer(it.Current()).IsValid()) throw std::runtime_error("Invalid assembly solid");
    result.push_back(volume(it.Current()));
  }
  std::sort(result.begin(),result.end());
  return result;
}
inline void verifySolidVolumes(const TopoDS_Shape& shape, const std::vector<double>& expected,
                           std::size_t expectedCount) {
  const auto actual = solidVolumes(shape);
  if (actual.size()!=expectedCount || expected.size()!=expectedCount)
    throw std::runtime_error("Unexpected assembly solid count");
  for (std::size_t i=0;i<actual.size();++i)
    if (std::abs(actual[i]-expected[i])>std::max(checks.volume,expected[i]*checks.relative))
      throw std::runtime_error("STEP component volume changed");
}
