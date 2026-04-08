#include "interval.h"
#include "util/raytrace.h"

Interval::Interval() : min(+infinity), max(-infinity) {}

Interval::Interval(double min, double max) : min(min), max(max) {}

const Interval Interval::empty = Interval(+infinity, -infinity);
const Interval Interval::universe = Interval(-infinity, +infinity);