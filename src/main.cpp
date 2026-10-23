#include "geometry.hpp"
#include "cli.hpp"
void verify(const TopoDS_Shape& shape, const Dimensions& d) {
  if (shape.IsNull() || !BRepCheck_Analyzer(shape).IsValid())
    throw std::runtime_error("Drawer is not a valid CAD shape");
  int solids = 0;
  for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next()) ++solids;
  if (solids != 5) throw std::runtime_error("Expected four walls and one bottom panel");
  GProp_GProps props;
  BRepGProp::VolumeProperties(shape, props);
  const double expected = (d.outerWidth() * d.outerDepth() - d.width * d.depth)
    * d.outerHeight() + d.width * d.depth * d.bottomThickness;
  // Check each component volume: this detects lost grooves or a wrongly sized bottom.
  const double panelWidth = d.width + 2 * d.dadoDepth;
  const double panelDepth = d.depth + 2 * d.dadoDepth;
  const double sideVolume = d.thickness * d.outerDepth() * d.outerHeight()
    - d.dadoDepth * panelDepth * d.bottomThickness;
  const double endVolume = d.width * d.thickness * d.outerHeight()
    - d.width * d.dadoDepth * d.bottomThickness;
  std::vector<double> wanted{sideVolume, sideVolume, endVolume, endVolume,
                             panelWidth * panelDepth * d.bottomThickness};
  std::vector<double> actual;
  for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next()) {
    GProp_GProps part;
    BRepGProp::VolumeProperties(it.Current(), part);
    actual.push_back(part.Mass());
  }
  std::sort(wanted.begin(), wanted.end());
  std::sort(actual.begin(), actual.end());
  for (std::size_t i = 0; i < wanted.size(); ++i)
    if (std::abs(actual[i] - wanted[i]) > std::max(checks.volume, wanted[i] * checks.relative))
      throw std::runtime_error("Panel or dado volume failed verification");
  if (std::abs(props.Mass() - expected) > std::max(checks.volume, expected * checks.relative))
    throw std::runtime_error("Material volume does not match requested dimensions");
  Bnd_Box bounds;
  BRepBndLib::AddOptimal(shape, bounds, false, false);
  double x0, y0, z0, x1, y1, z1;
  bounds.Get(x0, y0, z0, x1, y1, z1);
  const double tolerance = checks.distance;
  if (std::abs(x0) > tolerance || std::abs(y0) > tolerance || std::abs(z0) > tolerance
      || std::abs(x1 - d.outerWidth()) > tolerance
      || std::abs(y1 - d.outerDepth()) > tolerance
      || std::abs(z1 - d.outerHeight()) > tolerance)
    throw std::runtime_error("Outer dimensions failed verification");
}
#include "carcass.hpp"
#include "project_cli.hpp"


// Front of the box is Y=0; the applied face extends toward negative Y.
// Center on the exterior box, not on the usable interior height.
void verifyAssemblyBounds(const TopoDS_Shape& shape, const Dimensions& d, const DrawerFace& f, bool positioned = false) {
  Bnd_Box box;
  BRepBndLib::AddOptimal(shape, box, false, false);
  double x0,y0,z0,x1,y1,z1;
  box.Get(x0,y0,z0,x1,y1,z1);
  if (positioned) {
    x0-=exportSettings.positionX; x1-=exportSettings.positionX;
    y0-=exportSettings.positionY; y1-=exportSettings.positionY;
    z0-=exportSettings.positionZ; z1-=exportSettings.positionZ;
  }
  const double minX = f.enabled ? std::min(0.0,f.x(d)) : 0;
  const double maxX = f.enabled ? std::max(d.outerWidth(),f.x(d)+f.width) : d.outerWidth();
  const double minZ = f.enabled ? std::min(0.0,f.z(d)) : 0;
  const double maxZ = f.enabled ? std::max(d.outerHeight(),f.z(d)+f.height) : d.outerHeight();
  const double frontY = f.enabled ? -f.thickness : 0;
  if (std::abs(x0-minX)>checks.distance || std::abs(x1-maxX)>checks.distance
      || std::abs(y0-frontY)>checks.distance || std::abs(y1-d.outerDepth())>checks.distance
      || std::abs(z0-minZ)>checks.distance || std::abs(z1-maxZ)>checks.distance)
    throw std::runtime_error("Drawer face / assembly bounds failed verification");
}

int main(int argc, char** argv) {
  try {
    if (design::isProjectCommand(argc, argv)) return design::projectCommand(argc, argv);
    Dimensions d;
    Hardware hardware;
    DrawerFace face;
    std::filesystem::path output = defaults::output;
    if (!parseArguments(argc,argv,d,face,hardware,output)) return 0;
    if (output.empty()) throw std::runtime_error("Output path cannot be empty");
    if (cabinet.model=="carcass") return generateCarcass(output);
    if (d.thickness - d.dadoDepth < checks.minFeature)
      throw std::runtime_error("Dado depth must leave at least --min-feature of wall material");
    // X = width, Y = depth, Z = height. Offset is measured to the bottom underside.
    // Exact-fit panel: its thickness defines the groove width, with no clearance.
    const auto bottom = BRepPrimAPI_MakeBox(
      gp_Pnt(d.thickness - d.dadoDepth, d.thickness - d.dadoDepth, d.bottomOffset),
      d.width + 2 * d.dadoDepth, d.depth + 2 * d.dadoDepth, d.bottomThickness).Shape();
    const std::vector<TopoDS_Shape> blanks{
      BRepPrimAPI_MakeBox(d.thickness, d.outerDepth(), d.outerHeight()).Shape(),
      BRepPrimAPI_MakeBox(gp_Pnt(d.thickness + d.width, 0, 0),
                         d.thickness, d.outerDepth(), d.outerHeight()).Shape(),
      BRepPrimAPI_MakeBox(gp_Pnt(d.thickness, 0, 0),
                         d.width, d.thickness, d.outerHeight()).Shape(),
      BRepPrimAPI_MakeBox(gp_Pnt(d.thickness, d.thickness + d.depth, 0),
                         d.width, d.thickness, d.outerHeight()).Shape()
    };
    std::vector<TopoDS_Shape> parts;
    BRep_Builder builder;
    TopoDS_Compound drawer;
    builder.MakeCompound(drawer);
    for (const auto& blank : blanks) {
      BRepAlgoAPI_Cut groove(blank, bottom);
      if (!groove.IsDone()) throw std::runtime_error("OpenCascade dado cut failed");
      parts.push_back(groove.Shape());
      builder.Add(drawer, groove.Shape());
    }
    builder.Add(drawer, bottom);
    verify(drawer, d);
    parts.push_back(bottom);
    addHardware(parts,d,hardware);
    if (face.enabled) {
      const auto panel = BRepPrimAPI_MakeBox(
        gp_Pnt(face.x(d), -face.thickness, face.z(d)), face.width,face.thickness,face.height).Shape();
      const double expected = face.width*face.height*face.thickness;
      if (std::abs(volume(panel)-expected)>std::max(checks.volume,expected*checks.relative))
        throw std::runtime_error("Drawer face volume failed verification");
      parts.push_back(panel);
    }
    drawer = compound(parts);
    // Final assembly must have no overlapping component volumes.
    for (std::size_t i=0;i<parts.size();++i)
      for (std::size_t j=i+1;j<parts.size();++j)
        if (overlap(parts[i],parts[j]) > checks.volume)
          throw std::runtime_error("Assembly components overlap");
    const auto expectedVolumes = solidVolumes(drawer);
    const std::size_t expectedCount = blanks.size() + 1 + 2 * 2 * static_cast<std::size_t>(hardware.count) + (face.enabled ? 1 : 0);
    verifyHardware(drawer,expectedVolumes,expectedCount);
    verifyAssemblyBounds(drawer,d,face);
    // Translate all geometry together before export; internal fit is unchanged.
    gp_Trsf placement;
    placement.SetTranslation(gp_Vec(exportSettings.positionX,exportSettings.positionY,exportSettings.positionZ));
    for (auto& part : parts) part=BRepBuilderAPI_Transform(part,placement,true).Shape();
    verifyAssemblyBounds(compound(parts),d,face,true);
    if (output.has_parent_path()) std::filesystem::create_directories(output.parent_path());
    const auto imported = writeNamedAssembly(output,parts,partNames(hardware,face));
    verifyHardware(imported, expectedVolumes,expectedCount);
    verifyAssemblyBounds(imported,d,face,true);
    std::cout << "Saved and verified: " << std::filesystem::absolute(output) << "\n"
              << "Interior W x D x H: " << d.width << " x " << d.depth << " x " << d.height << " mm\n"
              << "Box exterior W x D x H: " << d.outerWidth() << " x " << d.outerDepth() << " x " << d.outerHeight() << " mm\n"
              << "Wall thickness: " << d.thickness << " mm; bottom thickness / groove width: "
              << d.bottomThickness << " mm\n"
              << "Dado depth: " << d.dadoDepth << " mm; bottom offset: " << d.bottomOffset << " mm\n";
    if (face.enabled)
      std::cout << "Applied face W x H x thickness: " << face.width << " x " << face.height
                << " x " << face.thickness << " mm; face offset X/Z: " << face.offsetX << "/" << face.offset << " mm\n";
    std::cout << "Assembly position X/Y/Z: " << exportSettings.positionX << " / "
              << exportSettings.positionY << " / " << exportSettings.positionZ << " mm\n";
    return 0;
  } catch (const Standard_Failure& error) {
    std::cerr << "OpenCascade error: " << error.GetMessageString() << '\n';
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
  }
  return 1;
}
