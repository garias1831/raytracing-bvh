#ifndef INTERVAL_H
#define INTERVAL_H

class Interval {
    public:
        double min, max;

        Interval();

        Interval(double min, double max) ;

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