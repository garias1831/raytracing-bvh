#include "bvh_slicing.h"

#include <iostream>

__global__
void kernel_helloworld() {
    printf("Hello from Cuda!\n");
}

void hello_gpu() {
    kernel_helloworld<<<1, 10>>>();
}