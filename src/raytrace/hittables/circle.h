#ifndef CIRCLE_H
#define CIRCLE_H 

#include "hittable.h"
#include "aabb.h"

class Circle : public Hittable {
    public:
        Circle(const Point2& center, double radius) : center(center), radius(std::fmax(0, radius)) {
            auto rvec = Vec2(radius, radius);
            bbox = Aabb(center - rvec, center + rvec);
        }

        bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
            Vec2 cq = center - r.origin();

            auto a = r.direction().length_squared();
            auto h = dot(r.direction(), cq);
            auto c = cq.length_squared() - radius*radius;
            auto discriminant = h*h - a*c;

            if (discriminant < 0) {
                return false;
            }

            auto sqrtd = std::sqrt(discriminant);
            
            double root = (h - sqrtd) / a;
            if (!ray_t.surrounds(root)) {
                root = (h + sqrtd) / a;
                if (!ray_t.surrounds(root)) {
                    return false;
                }
            }

            rec.point = r.at(root);
            rec.t = root;
            return true;
        }

        Aabb bounding_box() const override { return bbox; }

        std::unique_ptr<sf::Shape> to_sf(const Color& color) const override {
            sf::CircleShape rendered(radius);
            rendered.setOrigin(sf::Vector2f(radius, radius));
            rendered.setPosition(sf::Vector2f(center.x(), center.y()));
            rendered.setFillColor(sf::Color(color.ir(), color.ig(), color.ib()));

            return std::make_unique<sf::CircleShape>(rendered);
        }

    private:
        Point2 center;
        double radius;
        Aabb bbox;
};

#endif