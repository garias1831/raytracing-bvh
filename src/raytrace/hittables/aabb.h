#ifndef AABB_H
#define AABB_H

#include "util/raytrace.h"
#include "raytrace/interval.h"


// 2D axis-aligned bounding box, used for BVH construction and ray intersection tests.
class aabb {
  public:
    Interval x, y;

    aabb() {} // The default AABB is empty, since intervals are empty by default.

    aabb(const Interval& x, const Interval& y)
      : x(x), y(y) {}

    aabb(const Point2& a, const Point2& b) {
        // Treat the two points a and b as extrema for the bounding box, so we don't require a
        // particular minimum/maximum coordinate order.

        x = (a[0] <= b[0]) ? Interval(a[0], b[0]) : Interval(b[0], a[0]);
        y = (a[1] <= b[1]) ? Interval(a[1], b[1]) : Interval(b[1], a[1]);

    }

    const Interval& axis_interval(int n) const {
        if (n == 1) return y;
        return x;
    }

    bool hit(const Ray& r, Interval ray_t) const {
        const Point2& ray_orig = r.origin();
        const Vec2&   ray_dir  = r.direction();

        for (int axis = 0; axis < 2; axis++) {
            const Interval& ax = axis_interval(axis);
            const double adinv = 1.0 / ray_dir[axis];

            auto t0 = (ax.min - ray_orig[axis]) * adinv;
            auto t1 = (ax.max - ray_orig[axis]) * adinv;

            if (t0 < t1) {
                if (t0 > ray_t.min) ray_t.min = t0;
                if (t1 < ray_t.max) ray_t.max = t1;
            } else {
                if (t1 > ray_t.min) ray_t.min = t1;
                if (t0 < ray_t.max) ray_t.max = t0;
            }

            if (ray_t.max <= ray_t.min)
                return false;
        }
        return true;
    }
};

#endif