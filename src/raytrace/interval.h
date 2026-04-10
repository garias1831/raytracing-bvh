#ifndef INTERVAL_H
#define INTERVAL_H

class Interval {
    public:
        double min, max;

        Interval();

        Interval(double min, double max);

        Interval(const Interval& a, const Interval& b) {
            // Create interval tightly enclosing both inputs (union but made continuous)
            min = a.min <= b.min ? a.min : b.min;
            max = a.max >= b.max ? a.max : b.max;

        }

        inline double size() const {
            return max - min;
        }

        inline bool contains(double x) const {
            return min <= x && x <= max;
        }

        inline bool surrounds(double x) const {
            return min < x && x < max;
        }

    static const Interval empty, universe;
};

#endif