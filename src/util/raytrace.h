#ifndef RAYTRACE_H
#define RAYTRACE_H

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

// C++ Std Usings

using std::make_shared;
using std::shared_ptr;

// Constants

const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.1415926535897932385;

// Utility Functions

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

// Common Headers

#include "raytrace/color.h"
#include "raytrace/ray.h"
#include "raytrace/vec2.h"
#include "raytrace/interval.h"

#endif