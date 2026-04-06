#ifndef CIRCLE_H
#define CIRCLE_H 

#include "hittable.h"

class Circle : public Hittable {
    public:
        Circle(const Point2& center, double radius) : center(center), radius(std::fmax(0, radius)) {}

        bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
            Vec2 cq = center - r.origin();

            auto a = r.direction().length_squared();
            auto h = dot(r.direction(), cq);
            auto c = cq.length_squared() - radius*radius;
            auto discriminant = h*h - a*c;

            if (discriminant < 0) {
                return false;
            }

            auto sqrtd = std::sqrt(discriminant);
            
            double t = (h - sqrtd) / a;
            if (t <= t_min || t_max <= t) {
                t = (h + sqrtd) / a;
                if (t <= t_min || t_max <= t) {
                    return false;
                }
            }

            rec.point = r.at(t);
            rec.t = t;
            return true;
        }

    private:
        Point2 center;
        double radius;
};

#endif