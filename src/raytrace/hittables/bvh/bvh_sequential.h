#ifndef BVH_SEQUENTIAL_H
#define BVH_SEQUENTIAL_H

#include "raytrace/hittables/aabb.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"
#include "split_heuristic.h"

#include <algorithm>

inline double sequential_bbox_cost(const Aabb& bbox) {
    return bbox.x.size() * bbox.y.size();
}

inline int sequential_choose_widest_axis(
    const std::vector<shared_ptr<Hittable>>& objects,
    size_t start,
    size_t end
) {
    Aabb bbox = objects[start]->bounding_box();
    for (size_t i = start + 1; i < end; ++i) {
        bbox = Aabb(bbox, objects[i]->bounding_box());
    }

    return bbox.x.size() >= bbox.y.size() ? 0 : 1;
}

inline int sequential_choose_sah_split_index(
    const std::vector<shared_ptr<Hittable>>& objects,
    size_t start,
    size_t end
) {
    const size_t object_span = end - start;
    if (object_span <= 2) {
        return int(start + object_span / 2);
    }

    std::vector<Aabb> prefix(object_span);
    std::vector<Aabb> suffix(object_span);

    prefix[0] = objects[start]->bounding_box();
    for (size_t i = 1; i < object_span; ++i) {
        prefix[i] = Aabb(prefix[i - 1], objects[start + i]->bounding_box());
    }

    suffix[object_span - 1] = objects[end - 1]->bounding_box();
    for (size_t i = object_span - 1; i > 0; --i) {
        suffix[i - 1] = Aabb(suffix[i], objects[start + i - 1]->bounding_box());
    }

    double best_cost = infinity;
    size_t best_offset = object_span / 2;
    for (size_t left_count = 1; left_count < object_span; ++left_count) {
        const size_t right_count = object_span - left_count;
        const double cost =
            sequential_bbox_cost(prefix[left_count - 1]) * double(left_count) +
            sequential_bbox_cost(suffix[left_count]) * double(right_count);
        if (cost < best_cost) {
            best_cost = cost;
            best_offset = left_count;
        }
    }

    return int(start + best_offset);
}

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
            if (get_bvh_split_heuristic() == BvhSplitHeuristic::Sah && end > start) {
                axis = sequential_choose_widest_axis(objects, start, end);
            }

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
                if (get_bvh_split_heuristic() == BvhSplitHeuristic::Sah) {
                    mid = size_t(sequential_choose_sah_split_index(objects, start, end));
                }
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
