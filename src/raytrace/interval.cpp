#include "interval.h"

Interval::Interval()
    : min(+std::numeric_limits<double>::infinity()),
      max(-std::numeric_limits<double>::infinity()) {}

Interval::Interval(double min, double max) : min(min), max(max) {}

const Interval Interval::empty = Interval(
    +std::numeric_limits<double>::infinity(),
    -std::numeric_limits<double>::infinity()
);
const Interval Interval::universe = Interval(
    -std::numeric_limits<double>::infinity(),
    +std::numeric_limits<double>::infinity()
);
