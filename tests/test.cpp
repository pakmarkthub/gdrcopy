#include <stdlib.h>
#include <getopt.h>
#include <memory.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#include <cuda.h>

using namespace std;

#include "gdrapi.h"
#include "common.hpp"

using namespace gdrcopy::test;

int main(int argc, char *argv[])
{
    ASSERTDRV(cuInit(0));

    CUdevice dev;
    ASSERTDRV(cuDeviceGet(&dev, 0));

    CUcontext dev_ctx;
    ASSERTDRV(cuDevicePrimaryCtxRetain(&dev_ctx, dev));
    ASSERTDRV(cuCtxSetCurrent(dev_ctx));

    CUdeviceptr A[2];

    size_t size = 3 * GPU_PAGE_SIZE;
    /*for (int i = 0; i < 2; ++i) {
        ASSERTDRV(cuMemAlloc(&A[i], size));

        if (A[i] != A[i] & GPU_PAGE_MASK) {
            fprintf(stderr, "A[%i]=0x%llx is not GPU page aligned.\n", i, A[i]);
            return EXIT_FAILURE;
        }
        printf("A[%d].start=0x%llx, A[%d].end=0x%llx\n", i, A[i], i, A[i] + size - 1);
    }*/

    int i = 0;
    ASSERTDRV(cuMemAlloc(&A[i], size));

    if (A[i] != A[i] & GPU_PAGE_MASK) {
        fprintf(stderr, "A[%i]=0x%llx is not GPU page aligned.\n", i, A[i]);
        return EXIT_FAILURE;
    }
    printf("A[%d].start=0x%llx, A[%d].end=0x%llx\n", i, A[i], i, A[i] + size - 1);

    i = 1;
    size = GPU_PAGE_SIZE;
    ASSERTDRV(cuMemAlloc(&A[i], size));

    if (A[i] != A[i] & GPU_PAGE_MASK) {
        fprintf(stderr, "A[%i]=0x%llx is not GPU page aligned.\n", i, A[i]);
        return EXIT_FAILURE;
    }
    printf("A[%d].start=0x%llx, A[%d].end=0x%llx\n", i, A[i], i, A[i] + size - 1);

    
    gdr_mh_t mh_A[2];

    gdr_t g = gdr_open_safe();
    /*for (int i = 0; i < 2; ++i) {
        ASSERT_EQ(gdr_pin_buffer(g, A[i], size, 0, 0, &mh_A[i]), 0);
    }*/

    i = 0;
    size = 3 * GPU_PAGE_SIZE;
    ASSERT_EQ(gdr_pin_buffer(g, A[i], size, 0, 0, &mh_A[i]), 0);

    i = 1;
    size = GPU_PAGE_SIZE;
    ASSERT_EQ(gdr_pin_buffer(g, A[i], size, 0, 0, &mh_A[i]), 0);


    gdr_mh_t mh;
    ASSERT_EQ(gdr_pin_buffer(g, A[0], 4 * GPU_PAGE_SIZE, 0, 0, &mh), 0);

    void *map_d_ptr  = NULL;
    ASSERT_EQ(gdr_map(g, mh, &map_d_ptr, 4 * GPU_PAGE_SIZE), 0);

    ASSERT_EQ(gdr_close(g), 0);
    printf("Done\n");

    return 0;
}

