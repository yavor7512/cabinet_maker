#pragma once

// Numerical policy for the CAD engine, independent of any cabinet design.
struct Validation {
  double distance = 1.0e-5;
  double volume = 1.0e-5;
  double relative = 1.0e-7;
  double minFeature = 0.001;
  double maxDimension = 1000000.0;
};
inline constexpr Validation checks;
