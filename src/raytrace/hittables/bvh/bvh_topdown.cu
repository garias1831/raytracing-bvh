#include "bvh_topdown.h"

#include <chrono>
#include <cfloat>
#include <thrust/execution_policy.h>
#include <thrust/sort.h>

namespace {
    TopDownBvhMetrics g_topdown_bvh_metrics;
    int g_topdown_leaf_threshold = 8;
    constexpr int kTopDownThreadsPerBlock = 128;
}

struct BuildTask {
    int start;
    int end;
    int node_index;
    int axis;
    int split;
};

struct PrimitiveSortKey {
    int task_ordinal;
    double coord;
    int original_offset;
};

struct PrimitiveSortKeyCompare {
    __host__ __device__
    bool operator()(const PrimitiveSortKey& a, const PrimitiveSortKey& b) const {
        if (a.task_ordinal != b.task_ordinal) return a.task_ordinal < b.task_ordinal;
        if (a.coord != b.coord) return a.coord < b.coord;
        return a.original_offset < b.original_offset;
    }
};

__device__
double axis_center(const Aabb& bbox, int axis) {
    if (axis == 0) {
        return 0.5 * (bbox.x.min + bbox.x.max);
    }
    return 0.5 * (bbox.y.min + bbox.y.max);
}

__global__
void initialize_indices_kernel(int* indices, int n) {
    int index = blockDim.x * blockIdx.x + threadIdx.x;
    if (index >= n) return;
    indices[index] = index;
}

__global__
void compute_task_bboxes_kernel(
    const BuildTask* tasks,
    int task_count,
    const int* primitive_indices,
    const Aabb* primitive_bboxes,
    Aabb* task_bboxes
) {
    int task_id = blockIdx.x;
    if (task_id >= task_count) return;

    const BuildTask task = tasks[task_id];

    __shared__ double min_x[1024];
    __shared__ double max_x[1024];
    __shared__ double min_y[1024];
    __shared__ double max_y[1024];

    double local_min_x = DBL_MAX;
    double local_max_x = -DBL_MAX;
    double local_min_y = DBL_MAX;
    double local_max_y = -DBL_MAX;

    for (int i = task.start + threadIdx.x; i < task.end; i += blockDim.x) {
        Aabb bbox = primitive_bboxes[primitive_indices[i]];
        local_min_x = bbox.x.min < local_min_x ? bbox.x.min : local_min_x;
        local_max_x = bbox.x.max > local_max_x ? bbox.x.max : local_max_x;
        local_min_y = bbox.y.min < local_min_y ? bbox.y.min : local_min_y;
        local_max_y = bbox.y.max > local_max_y ? bbox.y.max : local_max_y;
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
        task_bboxes[task_id].x.min = min_x[0];
        task_bboxes[task_id].x.max = max_x[0];
        task_bboxes[task_id].y.min = min_y[0];
        task_bboxes[task_id].y.max = max_y[0];
    }
}

__global__
void write_sort_keys_kernel(
    const BuildTask* tasks,
    int task_count,
    const int* primitive_indices,
    const Aabb* primitive_bboxes,
    PrimitiveSortKey* sort_keys
) {
    int task_id = blockIdx.x;
    if (task_id >= task_count) return;

    const BuildTask task = tasks[task_id];
    for (int offset = threadIdx.x; offset < task.end - task.start; offset += blockDim.x) {
        int global_index = task.start + offset;
        int primitive_index = primitive_indices[global_index];

        PrimitiveSortKey key;
        key.task_ordinal = task_id;
        key.original_offset = offset;
        key.coord = task.split ? axis_center(primitive_bboxes[primitive_index], task.axis) : 0.0;
        sort_keys[global_index] = key;
    }
}

int choose_split_axis(const Aabb& bbox) {
    return bbox.x.size() >= bbox.y.size() ? 0 : 1;
}

double bbox_cost(const Aabb& bbox) {
    return bbox.x.size() * bbox.y.size();
}

int choose_sah_split_index(
    const BuildTask& task,
    const std::vector<int>& primitive_indices_host,
    const std::vector<Aabb>& primitive_bboxes_host
) {
    const int object_span = task.end - task.start;
    if (object_span <= 2) {
        return task.start + object_span / 2;
    }

    std::vector<Aabb> prefix(object_span);
    std::vector<Aabb> suffix(object_span);

    prefix[0] = primitive_bboxes_host[primitive_indices_host[task.start]];
    for (int i = 1; i < object_span; ++i) {
        prefix[i] = Aabb(prefix[i - 1], primitive_bboxes_host[primitive_indices_host[task.start + i]]);
    }

    suffix[object_span - 1] = primitive_bboxes_host[primitive_indices_host[task.end - 1]];
    for (int i = object_span - 2; i >= 0; --i) {
        suffix[i] = Aabb(suffix[i + 1], primitive_bboxes_host[primitive_indices_host[task.start + i]]);
    }

    double best_cost = DBL_MAX;
    int best_offset = object_span / 2;
    for (int left_count = 1; left_count < object_span; ++left_count) {
        const int right_count = object_span - left_count;
        const double cost =
            bbox_cost(prefix[left_count - 1]) * double(left_count) +
            bbox_cost(suffix[left_count]) * double(right_count);
        if (cost < best_cost) {
            best_cost = cost;
            best_offset = left_count;
        }
    }

    return task.start + best_offset;
}

void make_topdown_bvh(
    std::vector<shared_ptr<Hittable>> objects,
    BvhArrayNode* bvh_host,
    int* ordered_indices_host,
    size_t n
) {
    g_topdown_bvh_metrics = TopDownBvhMetrics{};

    std::vector<Aabb> bboxes_host;
    for (const auto& ptr : objects) {
        bboxes_host.push_back(ptr->bounding_box());
    }

    Aabb* bboxes_device;
    cudaMalloc(&bboxes_device, n * sizeof(Aabb));
    cudaMemcpy(bboxes_device, bboxes_host.data(), n * sizeof(Aabb), cudaMemcpyHostToDevice);

    int* primitive_indices_device;
    cudaMalloc(&primitive_indices_device, n * sizeof(int));

    PrimitiveSortKey* sort_keys_device;
    cudaMalloc(&sort_keys_device, n * sizeof(PrimitiveSortKey));

    BuildTask* tasks_device;
    cudaMalloc(&tasks_device, n * sizeof(BuildTask));

    Aabb* task_bboxes_device;
    cudaMalloc(&task_bboxes_device, n * sizeof(Aabb));

    int blockDim = kTopDownThreadsPerBlock;
    int gridDim = (int(n) + blockDim - 1) / blockDim;
    initialize_indices_kernel<<<gridDim, blockDim>>>(primitive_indices_device, int(n));
    cudaDeviceSynchronize();

    int bvh_size = 2 * int(n) - 1;
    for (int i = 0; i < bvh_size; ++i) {
        bvh_host[i].left = -1;
        bvh_host[i].right = -1;
        bvh_host[i].object_start = -1;
        bvh_host[i].object_count = 0;
    }

    std::vector<BuildTask> frontier;
    frontier.push_back({0, int(n), 0, 0, 0});
    int next_free_node_index = 1;

    while (!frontier.empty()) {
        const auto level_start = std::chrono::steady_clock::now();
        int task_count = int(frontier.size());
        g_topdown_bvh_metrics.frontier_levels++;
        g_topdown_bvh_metrics.task_counts_per_level.push_back(task_count);

        long long primitives_in_level = 0;
        int min_primitives_in_task = task_count > 0 ? (frontier[0].end - frontier[0].start) : 0;
        int max_primitives_in_task = min_primitives_in_task;
        for (const auto& task : frontier) {
            int task_span = task.end - task.start;
            primitives_in_level += task_span;
            if (task_span < min_primitives_in_task) min_primitives_in_task = task_span;
            if (task_span > max_primitives_in_task) max_primitives_in_task = task_span;
        }
        g_topdown_bvh_metrics.avg_primitives_per_task_per_level.push_back(
            task_count > 0 ? double(primitives_in_level) / double(task_count) : 0.0
        );
        g_topdown_bvh_metrics.min_primitives_per_task_per_level.push_back(min_primitives_in_task);
        g_topdown_bvh_metrics.max_primitives_per_task_per_level.push_back(max_primitives_in_task);

        double copy_sync_ms = 0.0;

        auto copy_sync_start = std::chrono::steady_clock::now();
        cudaMemcpy(tasks_device, frontier.data(), task_count * sizeof(BuildTask), cudaMemcpyHostToDevice);
        auto copy_sync_end = std::chrono::steady_clock::now();
        copy_sync_ms += std::chrono::duration<double, std::milli>(copy_sync_end - copy_sync_start).count();

        compute_task_bboxes_kernel<<<task_count, blockDim>>>(
            tasks_device,
            task_count,
            primitive_indices_device,
            bboxes_device,
            task_bboxes_device
        );
        copy_sync_start = std::chrono::steady_clock::now();
        cudaDeviceSynchronize();
        copy_sync_end = std::chrono::steady_clock::now();
        copy_sync_ms += std::chrono::duration<double, std::milli>(copy_sync_end - copy_sync_start).count();

        std::vector<Aabb> task_bboxes(task_count);
        copy_sync_start = std::chrono::steady_clock::now();
        cudaMemcpy(task_bboxes.data(), task_bboxes_device, task_count * sizeof(Aabb), cudaMemcpyDeviceToHost);
        copy_sync_end = std::chrono::steady_clock::now();
        copy_sync_ms += std::chrono::duration<double, std::milli>(copy_sync_end - copy_sync_start).count();

        std::vector<BuildTask> sort_tasks = frontier;
        std::vector<BuildTask> next_frontier;
        std::vector<int> next_left_indices;
        std::vector<int> next_right_indices;

        for (int i = 0; i < task_count; ++i) {
            const auto& task = frontier[i];
            int object_span = task.end - task.start;

            bvh_host[task.node_index].bbox = task_bboxes[i];

            if (object_span <= g_topdown_leaf_threshold) {
                bvh_host[task.node_index].object_start = task.start;
                bvh_host[task.node_index].object_count = object_span;
                sort_tasks[i].axis = 0;
                sort_tasks[i].split = 0;
                continue;
            }

            sort_tasks[i].axis = choose_split_axis(task_bboxes[i]);
            sort_tasks[i].split = 1;
            int left = next_free_node_index++;
            int right = next_free_node_index++;

            bvh_host[task.node_index].left = left;
            bvh_host[task.node_index].right = right;
            next_left_indices.push_back(left);
            next_right_indices.push_back(right);
        }

        write_sort_keys_kernel<<<task_count, blockDim>>>(
            tasks_device,
            task_count,
            primitive_indices_device,
            bboxes_device,
            sort_keys_device
        );

        const auto sort_start = std::chrono::steady_clock::now();
        thrust::sort_by_key(
            thrust::device,
            sort_keys_device,
            sort_keys_device + n,
            primitive_indices_device,
            PrimitiveSortKeyCompare()
        );
        copy_sync_start = std::chrono::steady_clock::now();
        cudaDeviceSynchronize();
        copy_sync_end = std::chrono::steady_clock::now();
        copy_sync_ms += std::chrono::duration<double, std::milli>(copy_sync_end - copy_sync_start).count();
        const auto sort_end = std::chrono::steady_clock::now();

        std::vector<int> primitive_indices_host;
        if (get_bvh_split_heuristic() == BvhSplitHeuristic::Sah) {
            primitive_indices_host.resize(n);
            copy_sync_start = std::chrono::steady_clock::now();
            cudaMemcpy(
                primitive_indices_host.data(),
                primitive_indices_device,
                n * sizeof(int),
                cudaMemcpyDeviceToHost
            );
            copy_sync_end = std::chrono::steady_clock::now();
            copy_sync_ms += std::chrono::duration<double, std::milli>(copy_sync_end - copy_sync_start).count();
        }

        int split_task_index = 0;
        for (int i = 0; i < task_count; ++i) {
            const auto& task = frontier[i];
            const int object_span = task.end - task.start;
            if (object_span <= g_topdown_leaf_threshold) {
                continue;
            }

            int mid = task.start + object_span / 2;
            if (get_bvh_split_heuristic() == BvhSplitHeuristic::Sah) {
                mid = choose_sah_split_index(task, primitive_indices_host, bboxes_host);
            }

            next_frontier.push_back({task.start, mid, next_left_indices[split_task_index], 0, 0});
            next_frontier.push_back({mid, task.end, next_right_indices[split_task_index], 0, 0});
            split_task_index++;
        }

        const auto level_end = std::chrono::steady_clock::now();
        const double sort_ms = std::chrono::duration<double, std::milli>(sort_end - sort_start).count();
        const double level_ms = std::chrono::duration<double, std::milli>(level_end - level_start).count();

        g_topdown_bvh_metrics.total_sort_ms += sort_ms;
        g_topdown_bvh_metrics.total_copy_sync_ms += copy_sync_ms;
        g_topdown_bvh_metrics.level_sort_ms.push_back(sort_ms);
        g_topdown_bvh_metrics.level_copy_sync_ms.push_back(copy_sync_ms);
        g_topdown_bvh_metrics.level_total_ms.push_back(level_ms);

        frontier = next_frontier;
    }

    const auto final_copy_start = std::chrono::steady_clock::now();
    cudaMemcpy(ordered_indices_host, primitive_indices_device, n * sizeof(int), cudaMemcpyDeviceToHost);
    const auto final_copy_end = std::chrono::steady_clock::now();
    g_topdown_bvh_metrics.total_copy_sync_ms += std::chrono::duration<double, std::milli>(
        final_copy_end - final_copy_start
    ).count();

    cudaFree(task_bboxes_device);
    cudaFree(tasks_device);
    cudaFree(sort_keys_device);
    cudaFree(primitive_indices_device);
    cudaFree(bboxes_device);
}

const TopDownBvhMetrics& get_topdown_bvh_metrics() {
    return g_topdown_bvh_metrics;
}

void set_topdown_leaf_threshold(int threshold) {
    g_topdown_leaf_threshold = threshold < 2 ? 2 : threshold;
}

int get_topdown_leaf_threshold() {
    return g_topdown_leaf_threshold;
}
