#pragma once
#include <filesystem>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <limits>

inline void printHelp(Dimensions& d, DrawerFace& face, Hardware& hardware) {
  std::cout << R"HELP(NAME
    drawer - generate a parametric drawer assembly as a millimetre STEP file

SYNOPSIS
    drawer [OPTIONS]
    drawer --project FILE --assembly ID --output FILE [--set NAME=EXPRESSION ...]
    drawer --project FILE --describe [--assembly ID] [--set NAME=EXPRESSION ...]
    drawer --help

DESCRIPTION
    Project mode reads board sizes, named-surface relationships, machining and
    hardware locations from an expanded JSON file. The Makefile expands
    designs/cabinet.jsonnet first; see designs/README.md. Project parameters are
    overridden with --set; project mode does not accept the legacy flags below.

    Legacy standalone mode (without --project):
    Creates four dadoed walls, a captured bottom, pocket screws, and an
    optional applied face. Width, depth and height describe usable interior
    space. Groove width follows bottom thickness. The face is centered on
    the exterior box, then translated by the face offsets.

    Coordinates: X = width/right, Y = depth/back, Z = height/up.
    Locally the box bottom corner is (0,0,0); the face is in front of Y=0.
    --face-offset 0 centers vertically; positive raises it, negative lowers it.
    Face offsets do not move the box, bottom, grooves or corner screws.
    --position-x/y/z translate the ENTIRE assembly into shared world coordinates.
    Defaults are zero; negative translations are allowed. Import multiple STEP
    files with original coordinates to preserve their relative placement.

OPTIONS
    Values may use '--option VALUE' or '--option=VALUE'. All dimensions are
    in millimetres unless noted. Repeated numeric options use the last value.
    Defaults below come from the same constants used by the model.

)HELP";
  for (const auto& option : options(d,face,hardware)) {
    std::cout << "    " << option.flag << " VALUE\n        " << option.description
              << "\n        Default: " << option.initial << " " << option.unit << ".\n\n";
  }
  std::cout << "    --output PATH\n        STEP output path. Default: " << defaults::output
            << ". Parent directories are created; existing files are overwritten.\n\n"
            << "    --face / --no-face\n        Include or omit the applied face; last flag wins. Default: "
            << (defaults::face_enabled ? "included" : "omitted") << ".\n\n"
            << R"HELP(    --model drawer|carcass
        Default: drawer. Carcass creates sides, top, bottom, two full-height
        stiles, with an open back and no front rails. Single side panels align
        with opening edges; stiles extend outward.
        Local origin: left edge of clear opening, front of stiles, opening bottom.
        Drawer face, dado and screw options do not affect carcass geometry.
        Stile width must be at least carcass thickness; stiles overhang outward.
        Cabinet depth includes stile thickness and must exceed it.

    --assembly-name NAME
        Root drawer assembly name. Default: output filename without extension.
        Boards and screws remain separate named parts beneath this root.

    -h, --help
        Print this manual and exit without generating a model.

CONSTRAINTS
    Numeric values must be finite. Positive lengths must be at least
    --min-feature and at most --max-dimension. Offsets may be zero or signed;
    their magnitude is limited by --max-dimension. Clearances may be zero.
    Dado depth must leave at least --min-feature of wall material.
    Pocket angle must be strictly between 0 and 90 degrees. Pockets and
    screw envelopes must clear the bottom, rim and exterior wall surfaces.
    An enabled face must retain some overlap with the front of the box.
    Changes to drawer height may require changing screw heights as well.

VALIDATION
    Advanced tolerance options control checks, not machining allowances.
    Increasing them relaxes validation; defaults are recommended. The model
    checks CAD validity, panel volumes, assembly intersections and bounds,
    then reads the STEP file back to verify solid count, volumes and bounds.

EXAMPLES
    drawer --output drawer.step
    drawer --position-z 35 --output lower.step
    drawer --no-face --position-y 12 --position-z 619 --output upper.step
    drawer --face-offset 10 --output raised-face.step
    drawer --face-offset=-10 --face-height 135 --face-width 375
    drawer --face-offset-x 5 --face-thickness 18
    drawer --no-face --output box-only.step
    drawer --bottom-thickness 6 --dado-depth 4 --output thin-bottom.step

FILES
    src/settings.hpp holds all user-adjustable numeric defaults.
    The output contains one named assembly with numbered board parts and screws.

LIMITATIONS
    Screw heads, shafts, tips and pockets are simplified assembly geometry,
    not certified manufacturer geometry or a jig/CNC machining template.
    Screw threads, drive recesses, face attachment screws, handle and slides
    are not modeled. Groove fit is exact, with no manufacturing clearance.

EXIT STATUS
    0   Model exported and verified, or help displayed.
    1   Invalid arguments, invalid geometry, or file/export failure.
)HELP";
}
inline double parseNumber(const std::string& text, const std::string& flag) {
  std::size_t used = 0;
  double value;
  try { value = std::stod(text,&used); }
  catch (const std::exception&) { throw std::runtime_error(flag + ": expected a finite number"); }
  if (used != text.size() || !std::isfinite(value))
    throw std::runtime_error(flag + ": expected a finite number without a unit suffix");
  return value;
}
inline bool parseArguments(int argc, char** argv, Dimensions& d, DrawerFace& face,
                           Hardware& hardware, std::filesystem::path& output) {
  // Help always reports defaults and does no geometry work.
  for (int i=1;i<argc;++i)
    if (std::string(argv[i])=="--help" || std::string(argv[i])=="-h") {
      printHelp(d,face,hardware); return false;
    }
  const auto registry = options(d,face,hardware);
  for (int i=1;i<argc;++i) {
    std::string flag = argv[i], value;
    const auto equals = flag.find('=');
    const bool inlineValue = equals != std::string::npos;
    if (inlineValue) { value = flag.substr(equals+1); flag.resize(equals); }
    if (flag=="--face" || flag=="--no-face") {
      if (inlineValue) throw std::runtime_error(flag + " takes no value");
      face.enabled = flag=="--face";
      continue;
    }
    const auto found = std::find_if(registry.begin(),registry.end(),
                                    [&](const Option& o){return flag==o.flag;});
    if (flag!="--model" && flag!="--output" && flag!="--assembly-name" && found==registry.end()) throw std::runtime_error("Unknown option: " + flag);
    if (!inlineValue) {
      if (++i==argc) throw std::runtime_error("Missing value for " + flag);
      value = argv[i];
    }
    if (flag=="--model") {
      if (value!="drawer" && value!="carcass") throw std::runtime_error("--model expects drawer or carcass");
      cabinet.model=value; continue;
    }
    if (flag=="--output") { output=value; continue; }
    if (flag=="--assembly-name") {
      if (value.empty() || value.find_first_of("\r\n")!=std::string::npos)
        throw std::runtime_error("Assembly name must be nonempty and single-line");
      exportSettings.assemblyName=value; continue;
    }
    *found->value = parseNumber(value,flag);
  }
  for (const auto& o : registry) {
    const double v=*o.value;
    if ((o.kind==ValueKind::Positive && v<=0) || (o.kind==ValueKind::Nonnegative && v<0))
      throw std::runtime_error(std::string(o.flag)+": value has an invalid sign");
  }
  if (std::floor(exportSettings.boardStart)!=exportSettings.boardStart
      || exportSettings.boardStart>std::numeric_limits<int>::max()-8)
    throw std::runtime_error("--board-start must be a positive integer allowing room for eight boards");
  if (checks.minFeature>=checks.maxDimension || checks.distance>=checks.minFeature
      || checks.relative>=1)
    throw std::runtime_error("Require distance-tolerance < min-feature < max-dimension and relative-tolerance < 1");
  for (const auto& o : registry) {
    const std::string flag=o.flag;
    const bool numerical = flag=="--distance-tolerance" || flag=="--volume-tolerance"
      || flag=="--relative-tolerance" || flag=="--min-feature" || flag=="--max-dimension";
    if (!numerical && std::string(o.unit)=="mm") {
      const double v=*o.value;
      if (std::abs(v)>checks.maxDimension || (o.kind==ValueKind::Positive && v<checks.minFeature))
        throw std::runtime_error(flag+": outside configured feature/dimension limits");
    }
  }
  if (cabinet.model=="drawer" && face.enabled && (face.x(d)>=d.outerWidth() || face.x(d)+face.width<=0
      || face.z(d)>=d.outerHeight() || face.z(d)+face.height<=0))
    throw std::runtime_error("Face offsets leave no overlap with drawer box front");
  return true;
}
