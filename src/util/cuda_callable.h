#ifndef CUDA_CALLABLE
#define CUDA_CALLABLE

/**
 * This header exports the following utility macro
 * for declaring that a class method can be used in both host and device code.
 * __CUDACC__ is defined by nvcc when compiling .cu files.
*/

#ifdef __CUDACC__
#define CUDA_CALLABLE_MEMBER __host__ __device__
#else
#define CUDA_CALLABLE_MEMBER
#endif

#endif