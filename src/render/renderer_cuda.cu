#include <cstdint>
#include <cmath>
#include <vector>

#include <cuda_runtime.h>
#include <SFML/Graphics/Color.hpp>

#include "render/renderer.h"
#include "raytrace/hittables/bvh/bvh_slicing.h"
#include "raytrace/hittables/bvh/bvh_topdown.h"
#include "raytrace/hittables/circle.h"
#include "raytrace/hittables/rectangle.h"

// Parallelizing the ray tracing part, mainly to make the renderer work faster since it was way too slow when increasing the number of objects.


namespace {
    enum GpuPrimitiveType {
        GpuPrimitiveCircle = 0,
        GpuPrimitiveRectangle = 1
    };

    struct GpuPrimitive {
        int type;
        double a;
        double b;
        double c;
        double d;
    };

    struct GpuBvhNode {
        double min_x;
        double max_x;
        double min_y;
        double max_y;
        int left;
        int right;
        int object_start;
        int object_count;
    };

    struct GpuColor {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
        std::uint8_t a;
    };

    __device__ bool bbox_hit(const GpuBvhNode& bbox, double ox, double oy, double dx, double dy, double t_min, double t_max) {
        for (int axis = 0; axis < 2; ++axis) {
            double bmin = axis == 0 ? bbox.min_x : bbox.min_y;
            double bmax = axis == 0 ? bbox.max_x : bbox.max_y;
            double o = axis == 0 ? ox : oy;
            double d = axis == 0 ? dx : dy;
            double adinv = 1.0 / d;

            double t0 = (bmin - o) * adinv;
            double t1 = (bmax - o) * adinv;

            if (t0 < t1) {
                if (t0 > t_min) t_min = t0;
                if (t1 < t_max) t_max = t1;
            } else {
                if (t1 > t_min) t_min = t1;
                if (t0 < t_max) t_max = t0;
            }

            if (t_max <= t_min) return false;
        }

        return true;
    }

    __device__ bool circle_hit(const GpuPrimitive& circle, double ox, double oy, double dx, double dy, double t_min, double t_max) {
        double cqx = circle.a - ox;
        double cqy = circle.b - oy;

        double a = dx * dx + dy * dy;
        double h = dx * cqx + dy * cqy;
        double c = cqx * cqx + cqy * cqy - circle.c * circle.c;
        double discriminant = h * h - a * c;

        if (discriminant < 0.0) return false;

        double sqrtd = sqrt(discriminant);
        double root = (h - sqrtd) / a;
        if (!(t_min < root && root < t_max)) {
            root = (h + sqrtd) / a;
            if (!(t_min < root && root < t_max)) {
                return false;
            }
        }

        return true;
    }

    __device__ bool rectangle_hit(const GpuPrimitive& rect, double ox, double oy, double dx, double dy, double t_min, double t_max) {
        GpuBvhNode bbox = {rect.a, rect.c, rect.b, rect.d, -1, -1, -1, 0};
        return bbox_hit(bbox, ox, oy, dx, dy, t_min, t_max);
    }

    __device__ bool primitive_hit(const GpuPrimitive& primitive, double ox, double oy, double dx, double dy, double t_min, double t_max) {
        if (primitive.type == GpuPrimitiveCircle) {
            return circle_hit(primitive, ox, oy, dx, dy, t_min, t_max);
        }

        if (primitive.type == GpuPrimitiveRectangle) {
            return rectangle_hit(primitive, ox, oy, dx, dy, t_min, t_max);
        }

        return false;
    }

    __device__ bool world_hit(
        const GpuBvhNode* nodes,
        int node_count,
        const GpuPrimitive* primitives,
        double ox,
        double oy,
        double dx,
        double dy
    ) {
        int stack[128];
        int stack_size = 0;
        stack[stack_size++] = 0;

        while (stack_size > 0) {
            int node_index = stack[--stack_size];
            if (node_index < 0 || node_index >= node_count) continue;

            const GpuBvhNode node = nodes[node_index];
            if (!bbox_hit(node, ox, oy, dx, dy, 0.0, 1.0)) continue;

            if (node.object_count > 0) {
                for (int i = 0; i < node.object_count; ++i) {
                    if (primitive_hit(primitives[node.object_start + i], ox, oy, dx, dy, 0.0, 1.0)) {
                        return true;
                    }
                }
                continue;
            }

            if (node.right >= 0 && stack_size < 128) stack[stack_size++] = node.right;
            if (node.left >= 0 && stack_size < 128) stack[stack_size++] = node.left;
        }

        return false;
    }

    __device__ double attenuation(double distance) {
        double r = 100.0;
        double ratio = distance / r;
        return 1.0 / (1.0 + ratio * ratio);
    }

    __global__ void render_pixels_kernel(
        const GpuBvhNode* nodes,
        int node_count,
        const GpuPrimitive* primitives,
        int width,
        int height,
        double pixel00_x,
        double pixel00_y,
        double source_x,
        double source_y,
        GpuColor* colors
    ) {
        int index = blockIdx.x * blockDim.x + threadIdx.x;
        int pixel_count = width * height;
        if (index >= pixel_count) return;

        int i = index % width;
        int j = index / width;

        double ox = pixel00_x + i;
        double oy = pixel00_y + j;
        double dx = source_x - ox;
        double dy = source_y - oy;

        if (world_hit(nodes, node_count, primitives, ox, oy, dx, dy)) {
            colors[index] = {0, 0, 0, 255};
            return;
        }

        double distance = sqrt(dx * dx + dy * dy);
        double factor = attenuation(distance);
        colors[index] = {
            static_cast<std::uint8_t>(factor * 93.0),
            static_cast<std::uint8_t>(factor * 12.0),
            static_cast<std::uint8_t>(factor * 237.0),
            255
        };
    }

    bool build_gpu_primitives(
        const std::vector<shared_ptr<Hittable>>& objects,
        std::vector<GpuPrimitive>& primitives_host
    ) {
        primitives_host.clear();
        primitives_host.reserve(objects.size());

        for (const auto& object : objects) {
            auto circle = std::dynamic_pointer_cast<Circle>(object);
            if (circle) {
                primitives_host.push_back({
                    GpuPrimitiveCircle,
                    circle->get_center().x(),
                    circle->get_center().y(),
                    circle->get_radius(),
                    0.0
                });
                continue;
            }

            auto rectangle = std::dynamic_pointer_cast<Rectangle>(object);
            if (rectangle) {
                primitives_host.push_back({
                    GpuPrimitiveRectangle,
                    rectangle->get_min_corner().x(),
                    rectangle->get_min_corner().y(),
                    rectangle->get_max_corner().x(),
                    rectangle->get_max_corner().y()
                });
                continue;
            }

            return false;
        }

        return true;
    }

    bool render_pixel_map_cuda_impl(
        const std::vector<GpuPrimitive>& primitives_host,
        const std::vector<GpuBvhNode>& nodes_host,
        uint window_width,
        uint window_height,
        Point2 pixel00_loc,
        Point2 source_loc,
        std::vector<sf::Color>& colors_out
    ) {
        GpuPrimitive* primitives_device = nullptr;
        GpuBvhNode* nodes_device = nullptr;
        GpuColor* colors_device = nullptr;

        int pixel_count = int(window_width * window_height);
        colors_out.resize(pixel_count);

        cudaError_t status = cudaSuccess;
        status = cudaMalloc(&primitives_device, primitives_host.size() * sizeof(GpuPrimitive));
        if (status != cudaSuccess) return false;
        status = cudaMalloc(&nodes_device, nodes_host.size() * sizeof(GpuBvhNode));
        if (status != cudaSuccess) {
            cudaFree(primitives_device);
            return false;
        }
        status = cudaMalloc(&colors_device, pixel_count * sizeof(GpuColor));
        if (status != cudaSuccess) {
            cudaFree(nodes_device);
            cudaFree(primitives_device);
            return false;
        }

        cudaMemcpy(primitives_device, primitives_host.data(), primitives_host.size() * sizeof(GpuPrimitive), cudaMemcpyHostToDevice);
        cudaMemcpy(nodes_device, nodes_host.data(), nodes_host.size() * sizeof(GpuBvhNode), cudaMemcpyHostToDevice);

        int block_dim = 256;
        int grid_dim = (pixel_count + block_dim - 1) / block_dim;
        render_pixels_kernel<<<grid_dim, block_dim>>>(
            nodes_device,
            int(nodes_host.size()),
            primitives_device,
            int(window_width),
            int(window_height),
            pixel00_loc.x(),
            pixel00_loc.y(),
            source_loc.x(),
            source_loc.y(),
            colors_device
        );

        status = cudaDeviceSynchronize();
        if (status != cudaSuccess) {
            cudaFree(colors_device);
            cudaFree(nodes_device);
            cudaFree(primitives_device);
            return false;
        }

        std::vector<GpuColor> raw_colors(pixel_count);
        status = cudaMemcpy(raw_colors.data(), colors_device, pixel_count * sizeof(GpuColor), cudaMemcpyDeviceToHost);

        cudaFree(colors_device);
        cudaFree(nodes_device);
        cudaFree(primitives_device);

        if (status != cudaSuccess) {
            return false;
        }

        for (int i = 0; i < pixel_count; ++i) {
            colors_out[i] = sf::Color(raw_colors[i].r, raw_colors[i].g, raw_colors[i].b, raw_colors[i].a);
        }

        return true;
    }
}

bool render_pixel_map_cuda(
    const BvhNodeTopDown& world,
    uint window_width,
    uint window_height,
    Point2 pixel00_loc,
    Point2 source_loc,
    std::vector<sf::Color>& colors_out
) {
    std::vector<GpuPrimitive> primitives_host;
    if (!build_gpu_primitives(world.get_objects(), primitives_host)) {
        return false;
    }

    std::vector<GpuBvhNode> nodes_host;
    nodes_host.reserve(world.get_bvh().size());
    for (const auto& node : world.get_bvh()) {
        nodes_host.push_back({
            node.bbox.x.min,
            node.bbox.x.max,
            node.bbox.y.min,
            node.bbox.y.max,
            node.left,
            node.right,
            node.object_start,
            node.object_count
        });
    }

    return render_pixel_map_cuda_impl(
        primitives_host,
        nodes_host,
        window_width,
        window_height,
        pixel00_loc,
        source_loc,
        colors_out
    );
}

bool render_pixel_map_cuda(
    const BvhNodeSlicing& world,
    uint window_width,
    uint window_height,
    Point2 pixel00_loc,
    Point2 source_loc,
    std::vector<sf::Color>& colors_out
) {
    std::vector<GpuPrimitive> primitives_host;
    if (!build_gpu_primitives(world.get_objects(), primitives_host)) {
        return false;
    }

    std::vector<GpuBvhNode> nodes_host;
    nodes_host.reserve(world.get_bvh().size());

    const int object_count = int(world.get_objects().size());
    int pow2_leaf_count = 1;
    while (pow2_leaf_count < object_count) {
        pow2_leaf_count <<= 1;
    }
    const int slicing_leaf_start = pow2_leaf_count - 1;

    for (int i = 0; i < int(world.get_bvh().size()); ++i) {
        const auto& node = world.get_bvh()[i];
        const bool is_leaf = i >= slicing_leaf_start;
        const int primitive_index = i - slicing_leaf_start;
        const bool valid_leaf = is_leaf && primitive_index >= 0 && primitive_index < object_count;

        nodes_host.push_back({
            node.bbox.x.min,
            node.bbox.x.max,
            node.bbox.y.min,
            node.bbox.y.max,
            valid_leaf ? -1 : node.left,
            valid_leaf ? -1 : node.right,
            valid_leaf ? primitive_index : -1,
            valid_leaf ? 1 : 0
        });
    }

    return render_pixel_map_cuda_impl(
        primitives_host,
        nodes_host,
        window_width,
        window_height,
        pixel00_loc,
        source_loc,
        colors_out
    );
}
