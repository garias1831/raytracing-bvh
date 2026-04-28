#ifndef BVH_SLICING_H
#define BVH_SLICING_H

#include "raytrace/hittables/aabb.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"

struct BvhArrayNode {
    Aabb bbox;
    int left;
    int right;
};

class BvhNodeSlicing : public Hittable {
    public:
        BvhNodeSlicing(HittableList list);

        BvhNodeSlicing(std::vector<shared_ptr<Hittable>> objects, size_t start, size_t end);

        bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;
        
        Aabb bounding_box() const override;

        std::unique_ptr<sf::Drawable> to_sf(const Color& color) const override;
        std::vector<std::unique_ptr<sf::Drawable>> to_sf_collection(const Color& color) const override;
    
    private:
        std::vector<shared_ptr<Hittable>> objects;
        std::vector<BvhArrayNode> bvh;

        bool hit_bvh(int i, const Ray&r, Interval ray_t, HitRecord& rec) const;
        std::vector<std::unique_ptr<sf::Drawable>> to_sf_collection_internal(int i, const Color& color) const;
};

#endif