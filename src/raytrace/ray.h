#ifndef RAY_H
#define RAY_H

#include "vec2.h"

/// @brief A point on a ray is of the form P(t) = A + tb, for origin A and direction b.
class Ray {
    public:
        Ray() {}
        
        Ray(Point2& origin, Vec2& direction) : orig(origin), dir(direction) {} 

        const Point2& origin() const {return orig;}
        const Vec2& direction() const {return dir;}


        Point2 at(double t) const {
            return orig + (dir * t);
        }

    private:
        Point2 orig;
        Vec2 dir;

};

#endif