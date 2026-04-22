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
/// @brief Write level d to the bvh array.
/// @param bvh 
/// @param newlevel 
/// @param d Depth of the level to write
/// @param n_newlevel 
void write_bvh_kernel(Aabb *bvh, Aabb *newlevel, int d, int n_newlevel) {
    int index = blockDim.x * blockIdx.x + threadIdx.x;

    if (index >= n_newlevel) {
        return;
    }

    int istart = pow(2, d - 1) - 1;
    bvh[istart + index] = newlevel[index];
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
void make_slicing_bvh(std::vector<shared_ptr<Hittable>> objects, Aabb *bvh_host, size_t n) {
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

    //  * We store the BVH as an implicit tree over a backing array.
    // This has a few benefits. Firstly, it makes copying the resulting
    // structure back to the host much easier because we don't have to deal with
    // fine-grained transfer of device -> host pointers.
    // Secondly, our indexing ends up having better memory coalescing
    // Because we store the leaves (depth d) at the right portion of the array,
    // followed by the depth d-1 in their own contiguous chunk, and etc.
    // The main downside of this repr is that we might waste some memory,
    // expecially for large n slightly larger than a power of 2. 

    Aabb *bvh_device;
    // If n is a power of 2, the max number of nodes
    // in the complete bintree is 2n - 1 (from geometric series). 
    cudaMalloc(&bvh_device, (2 * next_pow2(n) - 1) * sizeof(Aabb));

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
    write_bvh_kernel<<<gridDim, blockDim>>>(bvh_device, bboxes_lastlevel_device, depth, n_bboxes);
    
    depth--;
    while (n_bboxes > 1) {
        // Sort the level d + 1 bboxes along an axis

        // ! Unlike the sequential code, here we pick 1 axis (x in this case) to sort.
        // This is because alternating axes leads to an invalid tree construction;
        // e.g if the leaves were sorted along x, and the d - 1 nodes are sorted along y...
        // While this still should lead to a relatively balanced BVH, 
        // we should definitely measure the impact on render time to see if
        // it's an issue.
        //
        // A possible benefit of a fixed axis, however, is that we might be
        // able to get away with a single sort for the whole construction,
        // as the nextlevel bboxes will be in the same relative order
        // as their predecessors after the merge.

        int axis = 0;
        thrust::sort(
            thrust::device, 
            bboxes_lastlevel_device, 
            bboxes_lastlevel_device + n_bboxes,
            BoxCompare(axis)
        );

        // Merge the level d + 1 bboxes to generate the current level
        int num_nextlevel_bboxes = (n_bboxes % 2) == 0 ? (n_bboxes/2) : (n_bboxes/2) + 1;


        blockDim = 128;
        gridDim = (num_nextlevel_bboxes + blockDim - 1) / blockDim;
        merge_bboxes_kernel<<<gridDim, blockDim>>>(bboxes_lastlevel_device, bboxes_nextlevel_device, n_bboxes);

        // Write the new level to the bvh 
        blockDim = 128;
        gridDim = (num_nextlevel_bboxes + blockDim - 1) / blockDim;
        write_bvh_kernel<<<gridDim, blockDim>>>(bvh_device, bboxes_nextlevel_device, depth, num_nextlevel_bboxes);
        
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
    cudaFree(bvh_device);
}