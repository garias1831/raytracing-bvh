#include "bvh_slicing.h"

#include <algorithm>

// CUDA forward declarations
int slicing_next_pow2(int k);
void make_slicing_bvh(std::vector<shared_ptr<Hittable>> objects, SlicingBvhArrayNode* bvh_host, size_t n);

namespace {
bool slicing_box_compare(
    const shared_ptr<Hittable> a,
    const shared_ptr<Hittable> b,
    int axis_index
) {
    auto a_axis_interval = a->bounding_box().axis_interval(axis_index);
    auto b_axis_interval = b->bounding_box().axis_interval(axis_index);
    return a_axis_interval.min < b_axis_interval.min;
}

bool slicing_box_x_compare(const shared_ptr<Hittable> a, const shared_ptr<Hittable> b) {
    return slicing_box_compare(a, b, 0);
}

bool slicing_box_y_compare(const shared_ptr<Hittable> a, const shared_ptr<Hittable> b) {
    return slicing_box_compare(a, b, 1);
}
}

BvhNodeSlicing::BvhNodeSlicing(HittableList list)
    : BvhNodeSlicing(list.get_objects(), 0, list.get_objects().size()) {
}

BvhNodeSlicing::BvhNodeSlicing(std::vector<shared_ptr<Hittable>> objects, size_t start, size_t end) {
    size_t n = end - start;

    if (n == 0) {
        bvh = std::vector<SlicingBvhArrayNode>(1);
        bvh[0].bbox = Aabb();
        return;
    }

    std::vector<shared_ptr<Hittable>> range_objects(
        objects.begin() + start,
        objects.begin() + end
    );

    int axis = random_int(0, 1);
    auto comparator = axis == 0 ? slicing_box_x_compare : slicing_box_y_compare;
    std::sort(range_objects.begin(), range_objects.end(), comparator);

    std::vector<SlicingBvhArrayNode> bvh_arr(2 * slicing_next_pow2(int(n)) - 1);
    make_slicing_bvh(range_objects, bvh_arr.data(), n);
    bvh = bvh_arr;
    this->objects = range_objects;
}

bool BvhNodeSlicing::hit_bvh(int i, const Ray& r, Interval ray_t, HitRecord& rec) const {
    if (i >= int(bvh.size())) return false;
    if (i < 0) return false;

    Aabb bbox = bvh[i].bbox;
    if (!bbox.hit(r, ray_t)) return false;

    int left = bvh[i].left;
    int right = bvh[i].right;

    int n = int(objects.size());
    int leaf_range_start = slicing_next_pow2(n) - 1;

    if (leaf_range_start <= i) {
        int object_index = i - leaf_range_start;
        if (object_index < 0 || object_index >= n) {
            return false;
        }
        return objects[object_index]->hit(r, ray_t, rec);
    }

    bool hit_left = hit_bvh(left, r, ray_t, rec);
    Interval adjusted_left = Interval(ray_t.min, hit_left ? rec.t : ray_t.max);
    bool hit_right = hit_bvh(right, r, adjusted_left, rec);

    return hit_left || hit_right;
}

bool BvhNodeSlicing::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {
    return hit_bvh(0, r, ray_t, rec);
}

Aabb BvhNodeSlicing::bounding_box() const {
    return bvh[0].bbox;
}

const std::vector<shared_ptr<Hittable>>& BvhNodeSlicing::get_objects() const {
    return objects;
}

const std::vector<SlicingBvhArrayNode>& BvhNodeSlicing::get_bvh() const {
    return bvh;
}
