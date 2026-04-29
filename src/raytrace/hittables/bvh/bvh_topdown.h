#ifndef BVH_TOPDOWN_H
#define BVH_TOPDOWN_H

#include "raytrace/hittables/aabb.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"

struct BvhArrayNode {
    Aabb bbox;
    int left;
    int right;
    int object_start;
    int object_count;
};

class BvhNodeTopDown : public Hittable {
    public:
        BvhNodeTopDown(HittableList list);

        BvhNodeTopDown(std::vector<shared_ptr<Hittable>> objects, size_t start, size_t end);

        bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;

        Aabb bounding_box() const override;

        const std::vector<shared_ptr<Hittable>>& get_objects() const;
        const std::vector<BvhArrayNode>& get_bvh() const;

    private:
        std::vector<shared_ptr<Hittable>> objects;
        std::vector<BvhArrayNode> bvh;

        bool hit_bvh(int i, const Ray& r, Interval ray_t, HitRecord& rec) const;
};

#endif
