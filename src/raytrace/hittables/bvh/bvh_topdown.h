#ifndef BVH_TOPDOWN_H
#define BVH_TOPDOWN_H

#include <vector>

#include "raytrace/hittables/aabb.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"
#include "split_heuristic.h"

struct TopDownBvhMetrics {
    int frontier_levels = 0;
    double total_sort_ms = 0.0;
    double total_copy_sync_ms = 0.0;
    std::vector<int> task_counts_per_level;
    std::vector<double> avg_primitives_per_task_per_level;
    std::vector<int> min_primitives_per_task_per_level;
    std::vector<int> max_primitives_per_task_per_level;
    std::vector<double> level_total_ms;
    std::vector<double> level_sort_ms;
    std::vector<double> level_copy_sync_ms;
};

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

const TopDownBvhMetrics& get_topdown_bvh_metrics();
void set_topdown_leaf_threshold(int threshold);
int get_topdown_leaf_threshold();

#endif
