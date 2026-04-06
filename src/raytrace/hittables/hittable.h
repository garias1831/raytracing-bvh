#ifndef HITTABLE_H
#define HITTABLE_H

/// @brief Stores information about a successful hit.
class HitRecord {
    public:
        Point2 point;
        double t;
};

class Hittable {
    public:
        virtual ~Hittable() = default;

        virtual bool hit(const Ray& r, double ray_tmin, double ray_tmax, HitRecord& rec) const = 0;
};

#endif