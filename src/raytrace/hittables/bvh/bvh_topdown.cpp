#include "bvh_topdown.h"

// Cuda forward declarations
void make_topdown_bvh(
    std::vector<shared_ptr<Hittable>> objects,
    BvhArrayNode* bvh_host,
    int* ordered_indices_host,
    size_t n
);


BvhNodeTopDown::BvhNodeTopDown(HittableList list)
    : BvhNodeTopDown(list.get_objects(), 0, list.get_objects().size()) {
    // See the note in the sequential implementation
}

BvhNodeTopDown::BvhNodeTopDown(std::vector<shared_ptr<Hittable>> objects, size_t start, size_t end) {
    size_t n = end - start;

    if (n == 0) {
        bvh = std::vector<BvhArrayNode>(1);
        bvh[0].bbox = Aabb();
        return;
    }

    std::vector<shared_ptr<Hittable>> range_objects(
        objects.begin() + start,
        objects.begin() + end
    );

    std::vector<BvhArrayNode> bvh_arr(2 * int(n) - 1);
    std::vector<int> ordered_indices(n);

    make_topdown_bvh(range_objects, bvh_arr.data(), ordered_indices.data(), n);

    this->objects.reserve(n);
    for (int index : ordered_indices) {
        this->objects.push_back(range_objects[index]);
    }

    bvh = bvh_arr;
}

bool BvhNodeTopDown::hit_bvh(int i, const Ray& r, Interval ray_t, HitRecord& rec) const {
    if (i >= int(bvh.size())) return false;
    if (i < 0) return false;

    Aabb bbox = bvh[i].bbox;
    if (!bbox.hit(r, ray_t)) return false;

    int object_count = bvh[i].object_count;
    if (object_count > 0) {
        bool hit_anything = false;
        auto closest_so_far = ray_t.max;
        HitRecord temp_rec;

        for (int j = 0; j < object_count; ++j) {
            if (objects[bvh[i].object_start + j]->hit(r, Interval(ray_t.min, closest_so_far), temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }

    bool hit_left = hit_bvh(bvh[i].left, r, ray_t, rec);
    Interval adjusted_left = Interval(ray_t.min, hit_left ? rec.t : ray_t.max);
    bool hit_right = hit_bvh(bvh[i].right, r, adjusted_left, rec);

    return hit_left || hit_right;
}

bool BvhNodeTopDown::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {
    return hit_bvh(0, r, ray_t, rec);
}

Aabb BvhNodeTopDown::bounding_box() const { return bvh[0].bbox; }

const std::vector<shared_ptr<Hittable>>& BvhNodeTopDown::get_objects() const { return objects; }

const std::vector<BvhArrayNode>& BvhNodeTopDown::get_bvh() const { return bvh; }
