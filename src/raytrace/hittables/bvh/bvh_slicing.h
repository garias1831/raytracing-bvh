#ifndef BVH_SLICING_H
#define BVH_SLICING_H

#include <vector>

#include "raytrace/hittables/aabb.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"

struct SlicingBvhArrayNode {
    Aabb bbox;
    int left;
    int right;
};

struct SlicingBvhMetrics {
    int levels = 0;
    double initial_leaf_write_ms = 0.0;
    double total_sort_ms = 0.0;
    double total_merge_ms = 0.0;
    double total_write_ms = 0.0;
    double total_copy_sync_ms = 0.0;
    std::vector<int> boxes_per_level;
    std::vector<double> level_total_ms;
    std::vector<double> level_sort_ms;
    std::vector<double> level_merge_ms;
    std::vector<double> level_write_ms;
    std::vector<double> level_copy_sync_ms;
};

class BvhNodeSlicing : public Hittable {
    public:
        BvhNodeSlicing(HittableList list);

        BvhNodeSlicing(std::vector<shared_ptr<Hittable>> objects, size_t start, size_t end);

        bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;

        Aabb bounding_box() const override;

        const std::vector<shared_ptr<Hittable>>& get_objects() const;
        const std::vector<SlicingBvhArrayNode>& get_bvh() const;

    private:
        std::vector<shared_ptr<Hittable>> objects;
        std::vector<SlicingBvhArrayNode> bvh;

        bool hit_bvh(int i, const Ray& r, Interval ray_t, HitRecord& rec) const;
};

const SlicingBvhMetrics& get_slicing_bvh_metrics();

#endif
