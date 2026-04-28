#include "bvh_slicing.h"

#include <cmath>
#include <iostream>
#include <thrust/sort.h>
#include <thrust/execution_policy.h>

__host__ __device__
// Return the next power of 2 larger than k
inline int next_pow2(int k) {
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
void write_lastlevel_keys(int *keys, int n_lastlevel) {
    int index = blockDim.x * blockIdx.x + threadIdx.x;
    if (index >= n_lastlevel) return;
    keys[index] = index;
}

__global__
/// @brief Write level d to the bvh array.
/// @param bvh 
/// @param newlevel 
/// @param[out] keys Unique ID assigned to each LASTLEVEL bbox,
//                   sorted by the axis of the NEWLEVEL. Used
//                   for linking BVH levels when we switch axes
/// @param d Depth of the level to write
/// @param n_newlevel 
void write_bvh_kernel(BvhArrayNode *bvh, 
                      Aabb *newlevel, 
                      int *keys_lastlevel_sorted, 
                      int d, int n_newlevel, 
                      int n_lastlevel, bool writing_leaves=false) {
    int index = blockDim.x * blockIdx.x + threadIdx.x;

    if (index >= n_newlevel) {
        return;
    }

    int istart = pow(2, d - 1) - 1;
    int ibvh = istart + index;
    bvh[ibvh].bbox = newlevel[index];

    if (writing_leaves) {
        bvh[ibvh].left = -1;
        bvh[ibvh].right = -1;
        return;
    };

    // Write in keys from the lastlevel
    int i2 = 2 * index;
    int inext = pow(2, d) - 1;
    bvh[ibvh].left = inext + keys_lastlevel_sorted[i2];

    if (i2 + 1 >= n_lastlevel) {
        bvh[ibvh].right = -1;
        return;
    };
    
    bvh[ibvh].right = inext + keys_lastlevel_sorted[i2 + 1];
}


__global__
/// @brief 
/// @param[in] lastlevel The d + 1 level bboxes.
/// @param[out] nextlevel The merged level d bboxes.
void merge_bboxes_kernel(Aabb *lastlevel, Aabb *nextlevel, int n_lastlevel) {
    int index = blockDim.x * blockIdx.x + threadIdx.x;

    int i2 = 2 * index;
    if (i2 >= n_lastlevel) return;
    
    // If the bbox is unpaired, just propagate it up a level
    if (i2 == (n_lastlevel - 1)) {       
        nextlevel[index] = lastlevel[i2];
        return;
    }

    // Otherwise, merge the bbox with its nearest neighbor
    Aabb first = lastlevel[i2];
    Aabb second = lastlevel[i2 + 1];
    nextlevel[index] = Aabb(first, second);
}


// Thrust-compatible Aabb comparator
struct BoxCompare {
    int axis;

    __host__ __device__
    BoxCompare(int axis) : axis(axis) {};

    __host__ __device__
    bool operator()(const Aabb& a, const Aabb& b) {
        auto a_axis_interval = a.axis_interval(axis);
        auto b_axis_interval = b.axis_interval(axis);
        return a_axis_interval.min < b_axis_interval.min;
    }
};


/// @brief 
/// @param objects Collection of scene objects. 
/// @param[out] bvh_host Result buffer to store the bvh array. 
/// @param n Number of objects in the scene.
void make_slicing_bvh(std::vector<shared_ptr<Hittable>> objects, BvhArrayNode *bvh_host, size_t n) {
    // ? Will the overhead of this conversion dominate the benefit we get from the GPU?
    // * Slight optimization: we might want to store the bboxes in the HittableList to
    // * avoid having this loop in the bvh construction

    // Copy the bboxes over to the GPU
    std::vector<Aabb> bboxes_host;
    for (auto& ptr : objects) {
        bboxes_host.push_back(ptr->bounding_box());
    }

    Aabb *bboxes_device;
    cudaMalloc(&bboxes_device, n * sizeof(Aabb));
    cudaMemcpy(bboxes_device, bboxes_host.data(), n * sizeof(Aabb), cudaMemcpyHostToDevice);

    Aabb *bboxes_lastlevel_device = bboxes_device;
    Aabb *bboxes_nextlevel_device;
    cudaMalloc(&bboxes_nextlevel_device, n * sizeof(Aabb));

    int *lastlevel_keys_device;
    cudaMalloc(&lastlevel_keys_device, n * sizeof(int));

    //  * We store the BVH as an implicit tree over a backing array.
    // This has a few benefits. Firstly, it makes copying the resulting
    // structure back to the host much easier because we don't have to deal with
    // fine-grained transfer of device -> host pointers.
    // Secondly, our indexing ends up having better memory coalescing
    // Because we store the leaves (depth d) at the right portion of the array,
    // followed by the depth d-1 in their own contiguous chunk, and etc.
    // The main downside of this repr is that we might waste some memory,
    // expecially for large n slightly larger than a power of 2. 

    BvhArrayNode *bvh_device;
    // If n is a power of 2, the max number of nodes
    // in the complete bintree is 2n - 1 (from geometric series). 
    cudaMalloc(&bvh_device, (2 * next_pow2(n) - 1) * sizeof(BvhArrayNode));

    cudaDeviceSynchronize();


    // ? Right now, we keep merging and linking the bboxes from leaves to root
    // An interesting optimization could be to measure the effect
    // of adjusting the granularity size on construction time
    int n_bboxes = int(n);
    int depth = log2(next_pow2(n)) + 1;

    // Write the leaves to the bvh
    // TODO: when benchmarking, play around with the threadsPerBlock    
    int blockDim = 128;
    int gridDim = (n + blockDim - 1) / blockDim;
    write_bvh_kernel<<<gridDim, blockDim>>>(bvh_device, 
        bboxes_lastlevel_device, 
        lastlevel_keys_device,
        depth,
        n_bboxes, 
        n_bboxes, true);
   
    
    depth--;
    while (n_bboxes > 1) {
        // This keys array is used to ensure proper linking of parents to
        // children when the axis per level can change. 
        // At a highlevel, we assign an id to each lastlevel bbox, which we
        // reorder when we do the sort for each one according to the newlevel
        // axis. Then consecutive keys correspond to the left/right children
        // of the merged bbox in newlevel. 
        write_lastlevel_keys<<<gridDim, blockDim>>>(lastlevel_keys_device, n_bboxes);
        
        // Sort the level d + 1 bboxes along an axis
        int axis = random_int(0, 1);
        thrust::sort_by_key(
            thrust::device, 
            bboxes_lastlevel_device, 
            bboxes_lastlevel_device + n_bboxes,
            lastlevel_keys_device,
            BoxCompare(axis)
        );

        // Merge the level d + 1 bboxes to generate the current level
        int num_nextlevel_bboxes = (n_bboxes % 2) == 0 ? (n_bboxes/2) : (n_bboxes/2) + 1;


        blockDim = 128;
        gridDim = (num_nextlevel_bboxes + blockDim - 1) / blockDim;
        merge_bboxes_kernel<<<gridDim, blockDim>>>(bboxes_lastlevel_device, 
            bboxes_nextlevel_device, 
            n_bboxes);

        // Write the new level to the bvh 
        blockDim = 128;
        gridDim = (num_nextlevel_bboxes + blockDim - 1) / blockDim;
        write_bvh_kernel<<<gridDim, blockDim>>>(
            bvh_device, 
            bboxes_nextlevel_device, 
            lastlevel_keys_device,
            depth, 
            num_nextlevel_bboxes,
            n_bboxes
        );

        
        Aabb *tmp = bboxes_lastlevel_device;
        bboxes_lastlevel_device = bboxes_nextlevel_device;
        bboxes_nextlevel_device = tmp; // Reuse the space reserved for the old lastlevel

        n_bboxes = num_nextlevel_bboxes;
        depth--;
    }

    cudaDeviceSynchronize();

    // ? For memcpys, might want to consider looking into unified memory
    // to see if we could cut out a few memcpysd
    cudaMemcpy(bvh_host, bvh_device, (2 * next_pow2(n) - 1) * sizeof(Aabb), cudaMemcpyDeviceToHost);

    cudaDeviceSynchronize();

    cudaFree(bboxes_lastlevel_device);
    cudaFree(bboxes_nextlevel_device);
    cudaFree(lastlevel_keys_device);
    cudaFree(bvh_device);
}