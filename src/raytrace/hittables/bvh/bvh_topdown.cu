#include "bvh_topdown.h"

#include <cfloat>
#include <thrust/execution_policy.h>
#include <thrust/sort.h>

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

__host__ __device__
int next_pow2(int k) {
    k--;
    k |= k >> 1;
    k |= k >> 2;
    k |= k >> 4;
    k |= k >> 8;
    k |= k >> 16;
    k++;
    return k;
}

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

    __shared__ double min_x[128];
    __shared__ double max_x[128];
    __shared__ double min_y[128];
    __shared__ double max_y[128];

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

void make_topdown_bvh(
    std::vector<shared_ptr<Hittable>> objects,
    BvhArrayNode* bvh_host,
    int* ordered_indices_host,
    size_t n
) {
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

    int blockDim = 128;
    int gridDim = (int(n) + blockDim - 1) / blockDim;
    initialize_indices_kernel<<<gridDim, blockDim>>>(primitive_indices_device, int(n));
    cudaDeviceSynchronize();

    int bvh_size = 2 * next_pow2(int(n)) - 1;
    for (int i = 0; i < bvh_size; ++i) {
        bvh_host[i].left = -1;
        bvh_host[i].right = -1;
        bvh_host[i].object_start = -1;
        bvh_host[i].object_count = 0;
    }

    std::vector<BuildTask> frontier;
    frontier.push_back({0, int(n), 0, 0, 0});

    while (!frontier.empty()) {
        int task_count = int(frontier.size());
        cudaMemcpy(tasks_device, frontier.data(), task_count * sizeof(BuildTask), cudaMemcpyHostToDevice);

        compute_task_bboxes_kernel<<<task_count, blockDim>>>(
            tasks_device,
            task_count,
            primitive_indices_device,
            bboxes_device,
            task_bboxes_device
        );
        cudaDeviceSynchronize();

        std::vector<Aabb> task_bboxes(task_count);
        cudaMemcpy(task_bboxes.data(), task_bboxes_device, task_count * sizeof(Aabb), cudaMemcpyDeviceToHost);

        std::vector<BuildTask> sort_tasks = frontier;
        std::vector<BuildTask> next_frontier;

        for (int i = 0; i < task_count; ++i) {
            const auto& task = frontier[i];
            int object_span = task.end - task.start;

            bvh_host[task.node_index].bbox = task_bboxes[i];

            if (object_span <= 2) {
                bvh_host[task.node_index].object_start = task.start;
                bvh_host[task.node_index].object_count = object_span;
                sort_tasks[i].axis = 0;
                sort_tasks[i].split = 0;
                continue;
            }

            sort_tasks[i].axis = choose_split_axis(task_bboxes[i]);
            sort_tasks[i].split = 1;

            int mid = task.start + object_span / 2;
            int left = 2 * task.node_index + 1;
            int right = 2 * task.node_index + 2;

            bvh_host[task.node_index].left = left;
            bvh_host[task.node_index].right = right;

            next_frontier.push_back({task.start, mid, left, 0, 0});
            next_frontier.push_back({mid, task.end, right, 0, 0});
        }

        write_sort_keys_kernel<<<task_count, blockDim>>>(
            tasks_device,
            task_count,
            primitive_indices_device,
            bboxes_device,
            sort_keys_device
        );

        thrust::sort_by_key(
            thrust::device,
            sort_keys_device,
            sort_keys_device + n,
            primitive_indices_device,
            PrimitiveSortKeyCompare()
        );
        cudaDeviceSynchronize();

        frontier = next_frontier;
    }

    cudaMemcpy(ordered_indices_host, primitive_indices_device, n * sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(task_bboxes_device);
    cudaFree(tasks_device);
    cudaFree(sort_keys_device);
    cudaFree(primitive_indices_device);
    cudaFree(bboxes_device);
}
