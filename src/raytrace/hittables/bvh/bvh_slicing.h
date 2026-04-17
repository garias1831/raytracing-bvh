#ifndef BVH_SLICING_H
#define BVH_SLICING_H

#include "raytrace/hittables/aabb.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"

class BvhNodeSlicing : public Hittable {
    public:
        BvhNodeSlicing(HittableList list);

        BvhNodeSlicing(std::vector<shared_ptr<Hittable>> objects, size_t start, size_t end);

        bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;
        
        Aabb bounding_box() const override;
    
    private:
        shared_ptr<Hittable> left;
        shared_ptr<Hittable> right;
        Aabb bbox;
};

#endif