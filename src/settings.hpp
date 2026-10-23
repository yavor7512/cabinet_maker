#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

// Single source for editable defaults and command-line help.
namespace defaults {
inline constexpr double opening_width = 494, opening_height = 737, cabinet_depth = 597;
inline constexpr double carcass_thickness = 18, stile_width = 38, stile_thickness = 18;
inline constexpr const char* model = "drawer";
inline constexpr double position_x = 0;
inline constexpr double position_y = 0;
inline constexpr double position_z = 0;
inline constexpr double d_width = 300;
inline constexpr double d_depth = 505;
inline constexpr double d_height = 70;
inline constexpr double d_thickness = 12;
inline constexpr double d_bottomThickness = 12;
inline constexpr double d_dadoDepth = 6;
inline constexpr double d_bottomOffset = 6;
inline constexpr double face_width = 375;
inline constexpr double face_height = 135;
inline constexpr double face_thickness = 12;
inline constexpr double face_offset = 0;
inline constexpr double face_offsetX = 0;
inline constexpr double hardware_length = 19.05;
inline constexpr double hardware_diameter = 3.5;
inline constexpr double hardware_headDiameter = 5.5;
inline constexpr double hardware_headHeight = 1.8;
inline constexpr double hardware_tipLength = 3;
inline constexpr double hardware_pocketDiameter = 7;
inline constexpr double hardware_angle = 15;
inline constexpr double hardware_seatDistance = 10.5;
inline constexpr double hardware_lower = 35;
inline constexpr double hardware_upper = 65;
inline constexpr double hardware_count = 2;
inline constexpr double hardware_edgeClearance = 2;
inline constexpr double hardware_tipClearance = 0.5;
inline constexpr double checks_distance = 1.0e-05;
inline constexpr double checks_volume = 1.0e-05;
inline constexpr double checks_relative = 1.0e-07;
inline constexpr double checks_minFeature = 0.001;
inline constexpr double checks_maxDimension = 1000000.0;
inline constexpr double board_start = 1;
inline constexpr const char* assembly_name = "";
inline constexpr bool face_enabled = true;
inline constexpr const char* output = "drawer.step";
}
struct Dimensions {
  double width = defaults::d_width;
  double depth = defaults::d_depth;
  double height = defaults::d_height;
  double thickness = defaults::d_thickness;
  double bottomThickness = defaults::d_bottomThickness;
  double dadoDepth = defaults::d_dadoDepth;
  double bottomOffset = defaults::d_bottomOffset;
  double outerWidth() const { return width + 2 * thickness; }
  double outerDepth() const { return depth + 2 * thickness; }
  double outerHeight() const { return height + bottomThickness + bottomOffset; }
};
struct DrawerFace {
  double width = defaults::face_width;
  double height = defaults::face_height;
  double thickness = defaults::face_thickness;
  double offset = defaults::face_offset;
  double offsetX = defaults::face_offsetX;
  bool enabled = defaults::face_enabled;
  double x(const Dimensions& d) const { return (d.outerWidth()-width)/2 + offsetX; }
  double z(const Dimensions& d) const { return (d.outerHeight()-height)/2 + offset; }
};
struct Hardware {
  double count = defaults::hardware_count;
  double length = defaults::hardware_length;
  double diameter = defaults::hardware_diameter;
  double headDiameter = defaults::hardware_headDiameter;
  double headHeight = defaults::hardware_headHeight;
  double tipLength = defaults::hardware_tipLength;
  double pocketDiameter = defaults::hardware_pocketDiameter;
  double angle = defaults::hardware_angle;
  double seatDistance = defaults::hardware_seatDistance;
  double lower = defaults::hardware_lower;
  double upper = defaults::hardware_upper;
  double edgeClearance = defaults::hardware_edgeClearance;
  double tipClearance = defaults::hardware_tipClearance;
};
struct Validation {
  double distance = defaults::checks_distance;
  double volume = defaults::checks_volume;
  double relative = defaults::checks_relative;
  double minFeature = defaults::checks_minFeature;
  double maxDimension = defaults::checks_maxDimension;
};
// Validation is shared by the geometry helpers in this single-threaded CLI.
inline Validation checks;
struct ExportSettings {
  double positionX = defaults::position_x;
  double positionY = defaults::position_y;
  double positionZ = defaults::position_z;
  double boardStart = defaults::board_start;
  std::string assemblyName = defaults::assembly_name;
};
inline ExportSettings exportSettings;
struct CabinetSettings {
  std::string model = defaults::model;
  double width = defaults::opening_width, height = defaults::opening_height;
  double depth = defaults::cabinet_depth, thickness = defaults::carcass_thickness;
  double stileWidth = defaults::stile_width, stileThickness = defaults::stile_thickness;
};
inline CabinetSettings cabinet;
enum class ValueKind { Positive, Nonnegative, Signed };
struct Option { const char* flag; double* value; double initial; const char* unit; const char* description; ValueKind kind; };
inline std::vector<Option> options(Dimensions& d, DrawerFace& face, Hardware& hardware) {
 return {
  {"--opening-width", &cabinet.width, defaults::opening_width, "mm", "Carcass clear opening width.", ValueKind::Positive},
  {"--opening-height", &cabinet.height, defaults::opening_height, "mm", "Carcass clear opening height.", ValueKind::Positive},
  {"--cabinet-depth", &cabinet.depth, defaults::cabinet_depth, "mm", "Overall cabinet depth including face frame.", ValueKind::Positive},
  {"--carcass-thickness", &cabinet.thickness, defaults::carcass_thickness, "mm", "Carcass panel thickness.", ValueKind::Positive},
  {"--stile-width", &cabinet.stileWidth, defaults::stile_width, "mm", "Face-frame stile width; extends outward from the opening edge.", ValueKind::Positive},
  {"--stile-thickness", &cabinet.stileThickness, defaults::stile_thickness, "mm", "Face-frame front-to-back thickness.", ValueKind::Positive},
  {"--position-x", &exportSettings.positionX, defaults::position_x, "mm", "Whole assembly translation: positive right, negative left.", ValueKind::Signed},
  {"--position-y", &exportSettings.positionY, defaults::position_y, "mm", "Whole assembly translation: positive rearward, negative forward.", ValueKind::Signed},
  {"--position-z", &exportSettings.positionZ, defaults::position_z, "mm", "Whole assembly translation: positive up, negative down.", ValueKind::Signed},
  {"--board-start", &exportSettings.boardStart, defaults::board_start, "number", "First unique board number; boards are numbered in left/right/front/back/bottom/face order.", ValueKind::Positive},
  {"--screws-per-corner", &hardware.count, defaults::hardware_count, "count", "Positive integer; equally spaced between lower and upper heights (one uses lower).", ValueKind::Positive},
  {"--width", &d.width, defaults::d_width, "mm", "Usable interior width.", ValueKind::Positive},
  {"--depth", &d.depth, defaults::d_depth, "mm", "Usable interior depth.", ValueKind::Positive},
  {"--height", &d.height, defaults::d_height, "mm", "Usable height above the bottom panel.", ValueKind::Positive},
  {"--thickness", &d.thickness, defaults::d_thickness, "mm", "Wall material thickness.", ValueKind::Positive},
  {"--bottom-thickness", &d.bottomThickness, defaults::d_bottomThickness, "mm", "Bottom thickness; also sets dado width.", ValueKind::Positive},
  {"--dado-depth", &d.dadoDepth, defaults::d_dadoDepth, "mm", "Dado penetration into each wall.", ValueKind::Positive},
  {"--bottom-offset", &d.bottomOffset, defaults::d_bottomOffset, "mm", "Wall bottom to underside of bottom panel.", ValueKind::Positive},
  {"--face-width", &face.width, defaults::face_width, "mm", "Applied face width.", ValueKind::Positive},
  {"--face-height", &face.height, defaults::face_height, "mm", "Applied face height.", ValueKind::Positive},
  {"--face-thickness", &face.thickness, defaults::face_thickness, "mm", "Applied face material thickness.", ValueKind::Positive},
  {"--face-offset", &face.offset, defaults::face_offset, "mm", "Vertical shift from centered: positive up, negative down.", ValueKind::Signed},
  {"--face-offset-x", &face.offsetX, defaults::face_offsetX, "mm", "Horizontal shift from centered: positive right, negative left.", ValueKind::Signed},
  {"--screw-length", &hardware.length, defaults::hardware_length, "mm", "Screw length from head underside to tip.", ValueKind::Positive},
  {"--screw-diameter", &hardware.diameter, defaults::hardware_diameter, "mm", "Simplified thread-envelope diameter.", ValueKind::Positive},
  {"--head-diameter", &hardware.headDiameter, defaults::hardware_headDiameter, "mm", "Pan-head envelope diameter.", ValueKind::Positive},
  {"--head-height", &hardware.headHeight, defaults::hardware_headHeight, "mm", "Axial head thickness.", ValueKind::Positive},
  {"--tip-length", &hardware.tipLength, defaults::hardware_tipLength, "mm", "Conical tip length.", ValueKind::Positive},
  {"--pocket-diameter", &hardware.pocketDiameter, defaults::hardware_pocketDiameter, "mm", "Nominal micro-pocket counterbore diameter.", ValueKind::Positive},
  {"--pocket-angle", &hardware.angle, defaults::hardware_angle, "degrees", "Screw axis angle to outside front/back face (between 0 and 90).", ValueKind::Positive},
  {"--seat-distance", &hardware.seatDistance, defaults::hardware_seatDistance, "mm", "Axial distance from head underside to joint.", ValueKind::Positive},
  {"--screw-lower", &hardware.lower, defaults::hardware_lower, "mm", "Lower screw axis height above box bottom.", ValueKind::Positive},
  {"--screw-upper", &hardware.upper, defaults::hardware_upper, "mm", "Upper screw axis height above box bottom.", ValueKind::Positive},
  {"--pocket-clearance", &hardware.edgeClearance, defaults::hardware_edgeClearance, "mm", "Minimum pocket clearance from dado, rim and other pocket.", ValueKind::Nonnegative},
  {"--screw-tip-clearance", &hardware.tipClearance, defaults::hardware_tipClearance, "mm", "Minimum screw-envelope clearance from outside side face.", ValueKind::Nonnegative},
  {"--distance-tolerance", &checks.distance, defaults::checks_distance, "mm", "Absolute bounds comparison tolerance.", ValueKind::Positive},
  {"--volume-tolerance", &checks.volume, defaults::checks_volume, "mm^3", "Absolute volume/intersection comparison tolerance.", ValueKind::Positive},
  {"--relative-tolerance", &checks.relative, defaults::checks_relative, "ratio", "Relative volume comparison tolerance.", ValueKind::Positive},
  {"--min-feature", &checks.minFeature, defaults::checks_minFeature, "mm", "Minimum positive modeled length and remaining dado wall.", ValueKind::Positive},
  {"--max-dimension", &checks.maxDimension, defaults::checks_maxDimension, "mm", "Maximum magnitude of modeled length or offset.", ValueKind::Positive},
 };
}
