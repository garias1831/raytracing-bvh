#ifndef INTERVAL_H
#define INTERVAL_H

class Interval {
    public:
        double min, max;

        Interval() : min(+infinity), max(-infinity) {}

        Interval(double min, double max) : min(min), max(max) {}

        double size() const {
            return max - min;
        }

        bool contains(double x) {
            return min <= x && x <= max;
        }

        bool surrounds(double x) {
            return min < x && x < max;
        }

    static const Interval empty, universe;
};

const Interval Interval::empty = Interval(+infinity, -infinity);
const Interval Interval::universe = Interval(-infinity, +infinity);

#endif