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
};

#endif