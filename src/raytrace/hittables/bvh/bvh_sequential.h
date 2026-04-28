#ifndef BVH_SEQUENTIAL_H
#define BVH_SEQUENTIAL_H

#include <SFML/Graphics.hpp>
#include "raytrace/hittables/aabb.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"

#include <algorithm>


/// @brief Sequential BVH implementation from the RTweekend tutorial 
class BvhNodeSequential : public Hittable {
    public:
        BvhNodeSequential(HittableList list) : BvhNodeSequential(list.get_objects(), 0, list.get_objects().size()) {
            // There's a C++ subtlety here. This constructor (without span indices) creates an
            // implicit copy of the hittable list, which we will modify. The lifetime of the copied
            // list only extends until this constructor exits. That's OK, because we only need to
            // persist the resulting bounding volume hierarchy.
        }

        BvhNodeSequential(std::vector<shared_ptr<Hittable>> objects, size_t start, size_t end) {
            int axis = random_int(0, 1);

            auto comparator = (axis == 0) ? box_x_compare : box_y_compare;

            size_t object_span = end - start;

            if (object_span == 1) {
                // Copy the singleton to 2 leaves arbitrarily
                left = right = objects[start];
            } else if (object_span == 2) {
                left = objects[start];
                right = objects[start + 1];
            } else {
                std::sort(std::begin(objects) + start, std::begin(objects) + end, comparator);

                auto mid = start + object_span/2;
                left = make_shared<BvhNodeSequential>(objects, start, mid);
                right = make_shared<BvhNodeSequential>(objects, mid, end);
            }
            bbox = Aabb(left->bounding_box(), right->bounding_box());
        }

        bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
            if(!bbox.hit(r, ray_t)) return false;

            bool hit_left = left->hit(r, ray_t, rec);
            Interval adjusted_left = Interval(ray_t.min, hit_left ? rec.t : ray_t.max);
            bool hit_right = right->hit(r, adjusted_left, rec);

            return hit_left || hit_right;
        }

        Aabb bounding_box() const override { return bbox; }

        std::unique_ptr<sf::Drawable> to_sf(const Color& color) const override {
            return bbox.to_sf(color);
        }

        // Draw the Aabbs of the BVH, but not the underlying objects.
        std::vector<std::unique_ptr<sf::Drawable>> to_sf_collection(const Color& color) const override {
            std::vector<std::unique_ptr<sf::Drawable>> boxes;
            auto boxes_left = left->to_sf_collection(color);
            auto boxes_right = right->to_sf_collection(color);
            
            // If we call to_sf_collection() on the leaves of hte BVH,
            // we re-render the underlying objects using the same color as the
            // bboxes, so skip them. 
            if (boxes_left.size() == 1 && boxes_right.size() == 1) {
                boxes.push_back(to_sf(color));
                
                // Slight hack here -- we detect leaves by checking size == 1,
                // but naively the parents of the leaves would also end up with size 1
                // if we only returned the parent bbox excluding the leaves.
                // So append an invisible dummy to guarantee our size checks
                // only skip the BVH leaves. 
                sf::CircleShape dummy(1);
                dummy.setFillColor(sf::Color::Transparent);
                boxes.push_back(make_unique<sf::CircleShape>(dummy));
                return boxes;
            }
            
            // Combine bbox arrays
            // Need move iters here because unique_ptr doesn't support copying
            boxes.insert(boxes.end(),
                std::make_move_iterator(boxes_left.begin()), 
                std::make_move_iterator(boxes_left.end()));
            
            
            boxes.push_back(to_sf(color));

            boxes.insert(boxes.end(),
                std::make_move_iterator(boxes_right.begin()), 
                std::make_move_iterator(boxes_right.end()));
            return boxes;
        }        

    private:
        shared_ptr<Hittable> left;
        shared_ptr<Hittable> right;
        Aabb bbox;

        static bool box_compare(
            const shared_ptr<Hittable> a, const shared_ptr<Hittable> b, int axis_index
        ) {
            auto a_axis_interval = a->bounding_box().axis_interval(axis_index);
            auto b_axis_interval = b->bounding_box().axis_interval(axis_index);
            return a_axis_interval.min < b_axis_interval.min;
        }

        static bool box_x_compare(const shared_ptr<Hittable> a, const shared_ptr<Hittable> b) {
            return box_compare(a, b, 0);
        }

        static bool box_y_compare(const shared_ptr<Hittable> a, const shared_ptr<Hittable> b) {
            return box_compare(a, b, 1);
        }
};

#endif