#ifndef RECTANGLE_H
#define RECTANGLE_H

#include "aabb.h"
#include "hittable.h"


// This class pretty much copies the AABB hit logic, but acts as a renderable version of it, nothing crazy

class Rectangle : public Hittable {
    public:
        Rectangle(const Point2& min_corner, const Point2& max_corner)
            : min_corner(
                Point2(
                    std::fmin(min_corner.x(), max_corner.x()),
                    std::fmin(min_corner.y(), max_corner.y())
                )
            ),
            max_corner(
                Point2(
                    std::fmax(min_corner.x(), max_corner.x()),
                    std::fmax(min_corner.y(), max_corner.y())
                )
            ),
            bbox(this->min_corner, this->max_corner) {}

        bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
            Interval candidate_t = ray_t;

            for (int axis = 0; axis < 2; ++axis) {
                const Interval& ax = bbox.axis_interval(axis);
                const double adinv = 1.0 / r.direction()[axis];

                auto t0 = (ax.min - r.origin()[axis]) * adinv;
                auto t1 = (ax.max - r.origin()[axis]) * adinv;

                if (t0 < t1) {
                    if (t0 > candidate_t.min) candidate_t.min = t0;
                    if (t1 < candidate_t.max) candidate_t.max = t1;
                } else {
                    if (t1 > candidate_t.min) candidate_t.min = t1;
                    if (t0 < candidate_t.max) candidate_t.max = t0;
                }

                if (candidate_t.max <= candidate_t.min) {
                    return false;
                }
            }

            rec.t = candidate_t.min;
            rec.point = r.at(rec.t);
            return true;
        }

        Aabb bounding_box() const override { return bbox; }

        const Point2& get_min_corner() const { return min_corner; }
        const Point2& get_max_corner() const { return max_corner; }

        std::unique_ptr<sf::Shape> to_sf(const Color& color) const override {
            sf::RectangleShape rendered(
                sf::Vector2f(
                    float(max_corner.x() - min_corner.x()),
                    float(max_corner.y() - min_corner.y())
                )
            );
            rendered.setPosition(sf::Vector2f(float(min_corner.x()), float(min_corner.y())));
            rendered.setFillColor(sf::Color(color.ir(), color.ig(), color.ib()));

            return std::make_unique<sf::RectangleShape>(rendered);
        }

    private:
        Point2 min_corner;
        Point2 max_corner;
        Aabb bbox;
};

#endif
