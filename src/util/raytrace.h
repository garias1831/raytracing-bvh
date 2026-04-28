#ifndef RAYTRACE_H
#define RAYTRACE_H

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>
#include <random>

#include "constants.h"

// C++ Std Usings
using uint = unsigned int;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;

// Utility Functions

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

inline double random_double() {
    static std::uniform_real_distribution<double> distribution(0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}

inline double random_double(double min, double max) {
    // Returns a random real in [min,max).
    return min + (max-min)*random_double();
}

inline int random_int(int min, int max) {
    // Returns a random integer in [min,max].
    return int(random_double(min, max+1));
}

// Common Headers

#include "raytrace/color.h"
#include "raytrace/ray.h"
#include "raytrace/vec2.h"
#include "raytrace/interval.h"

#endif