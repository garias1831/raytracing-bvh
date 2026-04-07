#ifndef HITTABLE_LIST_H
#define HITTABLE_LIST_H

#include <vector>

#include "hittable.h"

class HittableList : public Hittable {
    public:
        HittableList() {}
        HittableList(shared_ptr<Hittable> object) { add(object); }

        void clear() { objects.clear(); }

        void add(shared_ptr<Hittable> object) {
            objects.push_back(object);
        }

        const std::vector<shared_ptr<Hittable>>& get_objects() const { return objects; }

        bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override {
            bool hit_anything = false;
            auto closest_so_far = ray_t.max;
            HitRecord temp_rec;
            for (const auto& obj : objects) {
                if (obj->hit(r, Interval(ray_t.min, closest_so_far), temp_rec)) {
                    hit_anything = true;
                    closest_so_far = temp_rec.t;
                    rec = temp_rec;
                }
            }

            return hit_anything;
        }

    private:
        std::vector<shared_ptr<Hittable>> objects;
};

#endif