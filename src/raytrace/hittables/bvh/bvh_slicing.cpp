#include "bvh_slicing.h"

#include <algorithm>

// Cuda forward declarations
int next_pow2(int k);
void make_slicing_bvh(std::vector<shared_ptr<Hittable>> objects, BvhArrayNode *bvh_host, size_t n);


bool box_compare(
    const shared_ptr<Hittable> a, const shared_ptr<Hittable> b, int axis_index
) {
    auto a_axis_interval = a->bounding_box().axis_interval(axis_index);
    auto b_axis_interval = b->bounding_box().axis_interval(axis_index);
    return a_axis_interval.min < b_axis_interval.min;
}

bool box_x_compare(const shared_ptr<Hittable> a, const shared_ptr<Hittable> b) {
    return box_compare(a, b, 0);
}

bool box_y_compare(const shared_ptr<Hittable> a, const shared_ptr<Hittable> b) {
    return box_compare(a, b, 1);
}


BvhNodeSlicing::BvhNodeSlicing(HittableList list) : BvhNodeSlicing(list.get_objects(), 0, list.get_objects().size()) {
    // See the note in the sequential implementation
}

BvhNodeSlicing::BvhNodeSlicing(std::vector<shared_ptr<Hittable>> objects, size_t start, size_t end) {
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

    int axis = random_int(0, 1);
    auto comparator = axis == 0 ? box_x_compare : box_y_compare;

    // TODO: we should probably move this sort onto the GPU
    std::sort(range_objects.begin(), range_objects.end(), comparator);
    
    std::vector<BvhArrayNode> bvh_arr(2 * next_pow2(n) - 1);
    make_slicing_bvh(range_objects, bvh_arr.data(), n);
    bvh = bvh_arr;
    this->objects = range_objects;
}

bool BvhNodeSlicing::hit_bvh(int i, const Ray& r, Interval ray_t, HitRecord& rec) const {
    if (i >= bvh.capacity()) return false;
    
    if (i < 0) return false;

    Aabb bbox = bvh[i].bbox;
    if (!bbox.hit(r, ray_t)) return false;

    int left = bvh[i].left;
    int right = bvh[i].right;
    
    // If i is a leaf, use the hit() method of the underlying object
    int n = objects.size();
    int depth = log2(next_pow2(n)) + 1;

    int leaf_range_start = pow(2, depth - 1) - 1;

    if (leaf_range_start <= i) {
        return objects[i - leaf_range_start]->hit(r, ray_t, rec);
    }
   
    // Otherwise, check for hits in the subtrees
    bool hit_left = hit_bvh(left, r, ray_t, rec);
    Interval adjusted_left = Interval(ray_t.min, hit_left ? rec.t : ray_t.max);
    bool hit_right = hit_bvh(right, r, adjusted_left, rec);
    
    return hit_left || hit_right;
}

bool BvhNodeSlicing::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {
    return hit_bvh(0, r, ray_t, rec);
}

Aabb BvhNodeSlicing::bounding_box() const { return bvh[0].bbox; }

std::unique_ptr<sf::Drawable> BvhNodeSlicing::to_sf(const Color& color) const {
    return this->bounding_box().to_sf(color);
}

std::vector<std::unique_ptr<sf::Drawable>> BvhNodeSlicing::to_sf_collection_internal(int i, const Color& color) const {
    std::vector<std::unique_ptr<sf::Drawable>> boxes;

    if (i >= bvh.capacity()) return boxes;
    if (i < 0) return boxes;

    int left = bvh[i].left;
    int right = bvh[i].right;

    // Don't render bboxes for the leaves
    int n = objects.size();
    int depth = log2(next_pow2(n)) + 1;

    int leaf_range_start = pow(2, depth - 1) - 1;
    if (leaf_range_start <= i) return boxes;

    // Render the bbox and recurse
    auto boxes_left = to_sf_collection_internal(left, color);
    auto boxes_right = to_sf_collection_internal(right, color);

    boxes.insert(boxes.end(),
                 std::make_move_iterator(boxes_left.begin()), 
                 std::make_move_iterator(boxes_left.end()));
    boxes.push_back(bvh[i].bbox.to_sf(color));
    boxes.insert(boxes.end(),
                 std::make_move_iterator(boxes_right.begin()), 
                 std::make_move_iterator(boxes_right.end()));
    return boxes;
}

std::vector<std::unique_ptr<sf::Drawable>> BvhNodeSlicing::to_sf_collection(const Color& color) const {
    return to_sf_collection_internal(0, color);
}