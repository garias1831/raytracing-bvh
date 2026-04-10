#ifndef HITTABLE_H
#define HITTABLE_H

#include <SFML/Graphics.hpp>
#include "util/raytrace.h"
#include "aabb.h"

/// @brief Stores information about a successful hit.
class HitRecord {
    public:
        Point2 point;
        double t;
};

class Hittable {
    public:
        virtual ~Hittable() = default;

        virtual bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const = 0;

        virtual Aabb bounding_box() const = 0;

        virtual std::unique_ptr<sf::Shape> to_sf(const Color& color) const {
            // Including this instead of just making it an abstract mthd
            // Because for the HittableList, this is annoying as signature
            // Requires a pointer to a single shape, which would mean
            // We want to return a vector / compound type.
            // So stick with a failing default for simplicity
            assert(false && "to_sf() undefined for derived class");
            return NULL;
        }
};

#endif