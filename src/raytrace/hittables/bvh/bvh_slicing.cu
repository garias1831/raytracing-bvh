#include "bvh_slicing.h"

#include <chrono>
#include <vector>

#include <thrust/execution_policy.h>
#include <thrust/sort.h>

namespace {
struct GpuSlicingBox {
    double min_x;
    double max_x;
    double min_y;
    double max_y;
};

struct GpuSlicingBvhNode {
    GpuSlicingBox bbox;
    int left;
    int right;
};

SlicingBvhMetrics g_slicing_bvh_metrics;
}

__global__
void slicing_compute_level_bounds_kernel(const GpuSlicingBox* boxes, int count, GpuSlicingBox* bounds_out) {
    __shared__ double min_x[128];
    __shared__ double max_x[128];
    __shared__ double min_y[128];
    __shared__ double max_y[128];

    double local_min_x = DBL_MAX;
    double local_max_x = -DBL_MAX;
    double local_min_y = DBL_MAX;
    double local_max_y = -DBL_MAX;

    for (int i = threadIdx.x; i < count; i += blockDim.x) {
        const GpuSlicingBox box = boxes[i];
        local_min_x = box.min_x < local_min_x ? box.min_x : local_min_x;
        local_max_x = box.max_x > local_max_x ? box.max_x : local_max_x;
        local_min_y = box.min_y < local_min_y ? box.min_y : local_min_y;
        local_max_y = box.max_y > local_max_y ? box.max_y : local_max_y;
    }

    min_x[threadIdx.x] = local_min_x;
    max_x[threadIdx.x] = local_max_x;
    min_y[threadIdx.x] = local_min_y;
    max_y[threadIdx.x] = local_max_y;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride /= 2) {
        if (threadIdx.x < stride) {
            min_x[threadIdx.x] = min_x[threadIdx.x] < min_x[threadIdx.x + stride]
                ? min_x[threadIdx.x] : min_x[threadIdx.x + stride];
            max_x[threadIdx.x] = max_x[threadIdx.x] > max_x[threadIdx.x + stride]
                ? max_x[threadIdx.x] : max_x[threadIdx.x + stride];
            min_y[threadIdx.x] = min_y[threadIdx.x] < min_y[threadIdx.x + stride]
                ? min_y[threadIdx.x] : min_y[threadIdx.x + stride];
            max_y[threadIdx.x] = max_y[threadIdx.x] > max_y[threadIdx.x + stride]
                ? max_y[threadIdx.x] : max_y[threadIdx.x + stride];
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        bounds_out[0] = {min_x[0], max_x[0], min_y[0], max_y[0]};
    }
}

__host__ __device__
int slicing_next_pow2(int k) {
    k--;
    k |= k >> 1;
    k |= k >> 2;
    k |= k >> 4;
    k |= k >> 8;
    k |= k >> 16;
    k++;
    return k;
}

__global__
void slicing_write_lastlevel_keys(int* keys, int n_lastlevel) {
    int index = blockDim.x * blockIdx.x + threadIdx.x;
    if (index >= n_lastlevel) return;
    keys[index] = index;
}

__global__
void slicing_write_bvh_kernel(
    GpuSlicingBvhNode* bvh,
    GpuSlicingBox* newlevel,
    int* keys_lastlevel_sorted,
    int d,
    int n_newlevel,
    int n_lastlevel,
    bool writing_leaves = false
) {
    int index = blockDim.x * blockIdx.x + threadIdx.x;
    if (index >= n_newlevel) return;

    int istart = (1 << (d - 1)) - 1;
    int ibvh = istart + index;
    bvh[ibvh].bbox = newlevel[index];

    if (writing_leaves) {
        bvh[ibvh].left = -1;
        bvh[ibvh].right = -1;
        return;
    }

    int i2 = 2 * index;
    int inext = (1 << d) - 1;
    bvh[ibvh].left = inext + keys_lastlevel_sorted[i2];

    if (i2 + 1 >= n_lastlevel) {
        bvh[ibvh].right = -1;
        return;
    }

    bvh[ibvh].right = inext + keys_lastlevel_sorted[i2 + 1];
}

__global__
void slicing_merge_bboxes_kernel(GpuSlicingBox* lastlevel, GpuSlicingBox* nextlevel, int n_lastlevel) {
    int index = blockDim.x * blockIdx.x + threadIdx.x;
    int i2 = 2 * index;
    if (i2 >= n_lastlevel) return;

    if (i2 == (n_lastlevel - 1)) {
        nextlevel[index] = lastlevel[i2];
        return;
    }

    GpuSlicingBox first = lastlevel[i2];
    GpuSlicingBox second = lastlevel[i2 + 1];
    nextlevel[index] = {
        first.min_x < second.min_x ? first.min_x : second.min_x,
        first.max_x > second.max_x ? first.max_x : second.max_x,
        first.min_y < second.min_y ? first.min_y : second.min_y,
        first.max_y > second.max_y ? first.max_y : second.max_y
    };
}

struct SlicingBoxCompare {
    int axis;

    __host__ __device__
    explicit SlicingBoxCompare(int axis) : axis(axis) {}

    __host__ __device__
    bool operator()(const GpuSlicingBox& a, const GpuSlicingBox& b) const {
        double a_min = axis == 0 ? a.min_x : a.min_y;
        double b_min = axis == 0 ? b.min_x : b.min_y;
        return a_min < b_min;
    }
};

void make_slicing_bvh(std::vector<shared_ptr<Hittable>> objects, SlicingBvhArrayNode* bvh_host, size_t n) {
    g_slicing_bvh_metrics = SlicingBvhMetrics{};

    std::vector<GpuSlicingBox> bboxes_host;
    bboxes_host.reserve(n);
    for (const auto& ptr : objects) {
        Aabb bbox = ptr->bounding_box();
        bboxes_host.push_back({bbox.x.min, bbox.x.max, bbox.y.min, bbox.y.max});
    }

    GpuSlicingBox* bboxes_device = nullptr;
    cudaMalloc(&bboxes_device, n * sizeof(GpuSlicingBox));
    cudaMemcpy(bboxes_device, bboxes_host.data(), n * sizeof(GpuSlicingBox), cudaMemcpyHostToDevice);

    GpuSlicingBox* bboxes_lastlevel_device = bboxes_device;
    GpuSlicingBox* bboxes_nextlevel_device = nullptr;
    cudaMalloc(&bboxes_nextlevel_device, n * sizeof(GpuSlicingBox));
    GpuSlicingBox* level_bounds_device = nullptr;
    cudaMalloc(&level_bounds_device, sizeof(GpuSlicingBox));

    int* lastlevel_keys_device = nullptr;
    cudaMalloc(&lastlevel_keys_device, n * sizeof(int));

    GpuSlicingBvhNode* bvh_device = nullptr;
    int bvh_node_count = 2 * slicing_next_pow2(int(n)) - 1;
    cudaMalloc(&bvh_device, bvh_node_count * sizeof(GpuSlicingBvhNode));

    int n_bboxes = int(n);
    int depth = int(log2(double(slicing_next_pow2(int(n))))) + 1;

    int blockDim = 128;
    int gridDim = (n_bboxes + blockDim - 1) / blockDim;
    auto stage_start = std::chrono::steady_clock::now();
    slicing_write_bvh_kernel<<<gridDim, blockDim>>>(
        bvh_device,
        bboxes_lastlevel_device,
        lastlevel_keys_device,
        depth,
        n_bboxes,
        n_bboxes,
        true
    );
    cudaDeviceSynchronize();
    auto stage_end = std::chrono::steady_clock::now();
    g_slicing_bvh_metrics.initial_leaf_write_ms = std::chrono::duration<double, std::milli>(stage_end - stage_start).count();

    depth--;
    while (n_bboxes > 1) {
        const auto level_start = std::chrono::steady_clock::now();
        g_slicing_bvh_metrics.levels++;
        g_slicing_bvh_metrics.boxes_per_level.push_back(n_bboxes);
        double copy_sync_ms = 0.0;

        blockDim = 128;
        gridDim = (n_bboxes + blockDim - 1) / blockDim;
        stage_start = std::chrono::steady_clock::now();
        slicing_write_lastlevel_keys<<<gridDim, blockDim>>>(lastlevel_keys_device, n_bboxes);
        cudaDeviceSynchronize();
        stage_end = std::chrono::steady_clock::now();
        double write_ms = std::chrono::duration<double, std::milli>(stage_end - stage_start).count();

        stage_start = std::chrono::steady_clock::now();
        slicing_compute_level_bounds_kernel<<<1, 128>>>(bboxes_lastlevel_device, n_bboxes, level_bounds_device);
        cudaDeviceSynchronize();
        stage_end = std::chrono::steady_clock::now();
        copy_sync_ms += std::chrono::duration<double, std::milli>(stage_end - stage_start).count();

        GpuSlicingBox level_bounds{};
        stage_start = std::chrono::steady_clock::now();
        cudaMemcpy(&level_bounds, level_bounds_device, sizeof(GpuSlicingBox), cudaMemcpyDeviceToHost);
        stage_end = std::chrono::steady_clock::now();
        copy_sync_ms += std::chrono::duration<double, std::milli>(stage_end - stage_start).count();
        int axis = (level_bounds.max_x - level_bounds.min_x) >= (level_bounds.max_y - level_bounds.min_y) ? 0 : 1;

        stage_start = std::chrono::steady_clock::now();
        thrust::sort_by_key(
            thrust::device,
            bboxes_lastlevel_device,
            bboxes_lastlevel_device + n_bboxes,
            lastlevel_keys_device,
            SlicingBoxCompare(axis)
        );
        cudaDeviceSynchronize();
        stage_end = std::chrono::steady_clock::now();
        double sort_ms = std::chrono::duration<double, std::milli>(stage_end - stage_start).count();

        int num_nextlevel_bboxes = (n_bboxes % 2 == 0) ? (n_bboxes / 2) : (n_bboxes / 2) + 1;

        blockDim = 128;
        gridDim = (num_nextlevel_bboxes + blockDim - 1) / blockDim;
        stage_start = std::chrono::steady_clock::now();
        slicing_merge_bboxes_kernel<<<gridDim, blockDim>>>(
            bboxes_lastlevel_device,
            bboxes_nextlevel_device,
            n_bboxes
        );
        cudaDeviceSynchronize();
        stage_end = std::chrono::steady_clock::now();
        double merge_ms = std::chrono::duration<double, std::milli>(stage_end - stage_start).count();

        stage_start = std::chrono::steady_clock::now();
        slicing_write_bvh_kernel<<<gridDim, blockDim>>>(
            bvh_device,
            bboxes_nextlevel_device,
            lastlevel_keys_device,
            depth,
            num_nextlevel_bboxes,
            n_bboxes
        );
        cudaDeviceSynchronize();
        stage_end = std::chrono::steady_clock::now();
        write_ms += std::chrono::duration<double, std::milli>(stage_end - stage_start).count();

        GpuSlicingBox* tmp = bboxes_lastlevel_device;
        bboxes_lastlevel_device = bboxes_nextlevel_device;
        bboxes_nextlevel_device = tmp;

        n_bboxes = num_nextlevel_bboxes;
        depth--;

        const auto level_end = std::chrono::steady_clock::now();
        const double level_ms = std::chrono::duration<double, std::milli>(level_end - level_start).count();

        g_slicing_bvh_metrics.total_sort_ms += sort_ms;
        g_slicing_bvh_metrics.total_merge_ms += merge_ms;
        g_slicing_bvh_metrics.total_write_ms += write_ms;
        g_slicing_bvh_metrics.total_copy_sync_ms += copy_sync_ms;
        g_slicing_bvh_metrics.level_total_ms.push_back(level_ms);
        g_slicing_bvh_metrics.level_sort_ms.push_back(sort_ms);
        g_slicing_bvh_metrics.level_merge_ms.push_back(merge_ms);
        g_slicing_bvh_metrics.level_write_ms.push_back(write_ms);
        g_slicing_bvh_metrics.level_copy_sync_ms.push_back(copy_sync_ms);
    }

    std::vector<GpuSlicingBvhNode> bvh_nodes_host(bvh_node_count);
    auto copy_start = std::chrono::steady_clock::now();
    cudaMemcpy(
        bvh_nodes_host.data(),
        bvh_device,
        bvh_node_count * sizeof(GpuSlicingBvhNode),
        cudaMemcpyDeviceToHost
    );
    auto copy_end = std::chrono::steady_clock::now();
    g_slicing_bvh_metrics.total_copy_sync_ms += std::chrono::duration<double, std::milli>(copy_end - copy_start).count();

    for (int i = 0; i < bvh_node_count; ++i) {
        const auto& node = bvh_nodes_host[i];
        bvh_host[i].bbox = Aabb(
            Interval(node.bbox.min_x, node.bbox.max_x),
            Interval(node.bbox.min_y, node.bbox.max_y)
        );
        bvh_host[i].left = node.left;
        bvh_host[i].right = node.right;
    }

    cudaFree(bboxes_lastlevel_device);
    cudaFree(bboxes_nextlevel_device);
    cudaFree(level_bounds_device);
    cudaFree(lastlevel_keys_device);
    cudaFree(bvh_device);
}

const SlicingBvhMetrics& get_slicing_bvh_metrics() {
    return g_slicing_bvh_metrics;
}
