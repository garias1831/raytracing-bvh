#include "bvh_slicing.h"

// Cuda forward declarations
void hello_gpu();

BvhNodeSlicing::BvhNodeSlicing(HittableList list) : BvhNodeSlicing(list.get_objects(), 0, list.get_objects().size()) {
    // See the note in the sequential implementation
}

BvhNodeSlicing::BvhNodeSlicing(std::vector<shared_ptr<Hittable>> objects, size_t start, size_t end) {
    // Dummy implementation
    hello_gpu();
}

bool BvhNodeSlicing::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {
    if(!bbox.hit(r, ray_t)) return false;

    bool hit_left = left->hit(r, ray_t, rec);
    Interval adjusted_left = Interval(ray_t.min, hit_left ? rec.t : ray_t.max);
    bool hit_right = right->hit(r, adjusted_left, rec);

    return hit_left || hit_right;
}

Aabb BvhNodeSlicing::bounding_box() const { return bbox; }