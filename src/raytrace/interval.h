#ifndef INTERVAL_H
#define INTERVAL_H

#include "util/cuda_callable.h"
#include "util/constants.h"

class Interval {
    public:
        double min, max;

        CUDA_CALLABLE_MEMBER
        Interval() : min(infinity), max(infinity) {}

        CUDA_CALLABLE_MEMBER
        Interval(double min, double max) : min(min), max(max) {};

        CUDA_CALLABLE_MEMBER
        Interval(const Interval& a, const Interval& b) {
            // Create interval tightly enclosing both inputs (union but made continuous)
            min = a.min <= b.min ? a.min : b.min;
            max = a.max >= b.max ? a.max : b.max;
        }

        CUDA_CALLABLE_MEMBER
        inline double size() const {
            return max - min;
        }

        CUDA_CALLABLE_MEMBER
        inline bool contains(double x) const {
            return min <= x && x <= max;
        }

        CUDA_CALLABLE_MEMBER
        inline bool surrounds(double x) const {
            return min < x && x < max;
        }
    
    // ! Note: these static members cannot be used on the device.
    // ! May want to refactor this in the future. 
    static const Interval empty, universe;
};

#endif