#pragma once
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <gp_Ax2.hxx>
#include <gp_Vec.hxx>

// Nominal visualization dimensions, NOT a manufacturer drill template.
// Angle and screw length follow Kreg guidance; head/shaft/seat dimensions
// must be calibrated against the purchased screw and a drilled test joint.
inline double volume(const TopoDS_Shape& shape) {
  GProp_GProps p;
  BRepGProp::VolumeProperties(shape, p);
  return p.Mass();
}
inline TopoDS_Shape difference(const TopoDS_Shape& a, const TopoDS_Shape& b) {
  BRepAlgoAPI_Cut op(a,b);
  if (!op.IsDone()) throw std::runtime_error("Hardware cut failed");
  return op.Shape();
}
inline TopoDS_Shape unite(const TopoDS_Shape& a, const TopoDS_Shape& b) {
  BRepAlgoAPI_Fuse op(a,b);
  if (!op.IsDone()) throw std::runtime_error("Screw union failed");
  return op.Shape();
}
inline double overlap(const TopoDS_Shape& a, const TopoDS_Shape& b) {
  BRepAlgoAPI_Common op(a,b);
  if (!op.IsDone()) throw std::runtime_error("Hardware intersection check failed");
  return volume(op.Shape());
}
inline gp_Pnt along(const gp_Pnt& p, const gp_Dir& direction, double distance) {
  return p.Translated(gp_Vec(direction) * distance);
}
inline void addHardware(std::vector<TopoDS_Shape>& parts, const Dimensions& d, const Hardware& h) {
  if (h.angle <= 0 || h.angle >= 90 || h.length <= h.seatDistance + h.tipLength
      || h.pocketDiameter <= h.headDiameter || h.headDiameter <= h.diameter
      || h.headHeight >= h.seatDistance || h.tipLength >= h.length)
    throw std::runtime_error("Invalid screw/pocket proportions or angle (must be between 0 and 90 degrees)");
  const double radius = h.pocketDiameter / 2;
  if (std::floor(h.count)!=h.count || h.count<1)
    throw std::runtime_error("--screws-per-corner must be a positive integer");
  const double lastHeight = h.count==1 ? h.lower : h.upper;
  if (h.lower - radius <= d.bottomOffset + d.bottomThickness + h.edgeClearance
      || lastHeight + radius >= d.outerHeight() - h.edgeClearance
      || (h.count>1 && (h.upper-h.lower)/(h.count-1) <= h.pocketDiameter + h.edgeClearance))
    throw std::runtime_error("Pocket heights must clear dado, rim and each other by --pocket-clearance");
  const double a = h.angle * std::acos(-1.0) / 180.0;
  const double c = std::cos(a), s = std::sin(a);
  if ((h.length - h.seatDistance) * c + h.diameter / 2 >= d.thickness - h.tipClearance)
    throw std::runtime_error("Screw would break through side wall; reduce screw length or increase seat distance");
  const auto outer = BRepPrimAPI_MakeBox(d.outerWidth(), d.outerDepth(), d.outerHeight()).Shape();
  const auto interior = BRepPrimAPI_MakeBox(gp_Pnt(d.thickness,d.thickness,0),
                                          d.width,d.depth,d.outerHeight()).Shape();
  const auto wallEnvelope = difference(outer, interior);
  const auto bottom = parts[4];
  for (int end = 0; end < 2; ++end) {
    for (int side = 0; side < 2; ++side) {
      for (std::size_t index=0; index<h.count; ++index) {
        const double z = h.count==1 ? h.lower : h.lower + index*(h.upper-h.lower)/(h.count-1);
        const gp_Dir axis(side == 0 ? -c : c, end == 0 ? s : -s, 0);
        const gp_Pnt joint(side == 0 ? d.thickness : d.thickness + d.width,
                           end == 0 ? d.thickness/2 : d.outerDepth()-d.thickness/2, z);
        const auto seat = along(joint,axis,-h.seatDistance);
        const auto headBase = along(seat,axis,-h.headHeight);
        const auto head = BRepPrimAPI_MakeCylinder(gp_Ax2(headBase,axis),h.headDiameter/2,h.headHeight).Shape();
        const auto shaft = BRepPrimAPI_MakeCylinder(gp_Ax2(seat,axis),h.diameter/2,h.length-h.tipLength).Shape();
        const auto tip = BRepPrimAPI_MakeCone(gp_Ax2(along(seat,axis,h.length-h.tipLength),axis),
                                            h.diameter/2,0,h.tipLength).Shape();
        const auto screw = unite(unite(head,shaft),tip);
        if (!BRepCheck_Analyzer(screw).IsValid()) throw std::runtime_error("Invalid screw solid");
        if (volume(difference(screw,wallEnvelope)) > checks.volume || overlap(screw,bottom) > checks.volume)
          throw std::runtime_error("Screw protrudes beyond wall envelope or hits bottom");
        const double boreLength = d.thickness / s + h.seatDistance + h.headHeight;
        const auto pocket = BRepPrimAPI_MakeCylinder(gp_Ax2(along(seat,axis,-boreLength),axis),
                                                    radius,boreLength).Shape();
        const auto pilot = BRepPrimAPI_MakeCylinder(gp_Ax2(seat,axis),h.diameter/2,h.length).Shape();
        if (overlap(pocket,bottom) > checks.volume) throw std::runtime_error("Pocket intersects bottom");
        // Cut only the pocketed end board. Receiver gets an installed screw envelope,
        // not a drill instruction: real SPS screws self-tap the receiving side.
        auto& board = parts[2+end];
        const double before = volume(board);
        board = difference(difference(board,pocket),pilot);
        if (before-volume(board) <= checks.volume) throw std::runtime_error("Pocket did not intersect its board");
        if (overlap(screw,board) > checks.volume) throw std::runtime_error("Screw head fails to seat in pocket");
        parts[side] = difference(parts[side],screw);
        parts.push_back(screw);
      }
    }
  }
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
inline void verifyHardware(const TopoDS_Shape& shape, const std::vector<double>& expected,
                           std::size_t expectedCount) {
  const auto actual = solidVolumes(shape);
  if (actual.size()!=expectedCount || expected.size()!=expectedCount)
    throw std::runtime_error("Unexpected assembly solid count");
  for (std::size_t i=0;i<actual.size();++i)
    if (std::abs(actual[i]-expected[i])>std::max(checks.volume,expected[i]*checks.relative))
      throw std::runtime_error("STEP component volume changed");
}
