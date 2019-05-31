/*
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <check.h>

using namespace std;

#include "gdrapi.h"
#include "common.hpp"


START_TEST(invalidation_basic)
{
    int filedes_0[2];
    int filedes_1[2];
    int read_fd;
    int write_fd;
    ck_assert_int_ne(pipe(filedes_0), -1);
    ck_assert_int_ne(pipe(filedes_1), -1);

    srand(time(NULL));

    const size_t _size = 256*1024+16; //32*1024+8;
    const size_t size = (_size + GPU_PAGE_SIZE - 1) & GPU_PAGE_MASK;
    const char *myname;

    pid_t pid = fork();
    ck_assert_int_ge(pid, 0);

    myname = pid == 0 ? "child" : "parent";

    cout << myname << ": Start" << endl;

    if (pid == 0) {
        close(filedes_0[0]);
        close(filedes_1[1]);

        read_fd = filedes_1[0];
        write_fd = filedes_0[1];

        srand(rand());
        int cont = 0;

        do {
            cout << myname << ": waiting for cont signal" << endl;
            read(read_fd, &cont, sizeof(int));
            cout << myname << ": receive cont signal " << cont << endl;
        } while (cont != 1);
    }
    else {
        close(filedes_0[1]);
        close(filedes_1[0]);

        read_fd = filedes_0[0];
        write_fd = filedes_1[1];
    }

    int mydata = (rand() % 1000) + 1;

    void *dummy;
    ASSERTRT(cudaMalloc(&dummy, 0));

    CUdeviceptr d_A;
    ASSERTDRV(cuMemAlloc(&d_A, size));
    ASSERTDRV(cuMemsetD8(d_A, 0x95, size));
    //OUT << "device ptr: " << hex << d_A << dec << endl;

    unsigned int flag = 1;
    ASSERTDRV(cuPointerSetAttribute(&flag, CU_POINTER_ATTRIBUTE_SYNC_MEMOPS, d_A));

    gdr_t g = gdr_open();
    ASSERT_NEQ(g, (void*)0);

    gdr_mh_t mh;
    BEGIN_CHECK {
        CUdeviceptr d_ptr = d_A;

        // tokens are optional in CUDA 6.0
        // wave out the test if GPUDirectRDMA is not enabled
        BREAK_IF_NEQ(gdr_pin_buffer(g, d_ptr, size, 0, 0, &mh), 0);
        ASSERT_NEQ(mh, null_mh);

        void *bar_ptr  = NULL;
        ASSERT_EQ(gdr_map(g, mh, &bar_ptr, size), 0);
        //OUT << "bar_ptr: " << bar_ptr << endl;

        gdr_info_t info;
        ASSERT_EQ(gdr_get_info(g, mh, &info), 0);
        int off = d_ptr - info.va;

        int *buf_ptr = (int *)((char *)bar_ptr + off);

        buf_ptr[0] = mydata;
        cout << myname << ": write buf_ptr[0] with " << buf_ptr[0] << endl;

        if (pid == 0) {
            write(write_fd, &mydata, sizeof(int));

            int cont = 0;
            do {
                read(read_fd, &cont, sizeof(int));
            } while (cont != 1);
        }

        ASSERTDRV(cuMemFree(d_A));

        if (pid > 0) {
            cout << myname << ": read buf_ptr[0] after cuMemFree get " << buf_ptr[0] << endl;
            int msg = 1;
            write(write_fd, &msg, sizeof(int));
            int child_data = 0;
            do {
                read(read_fd, &child_data, sizeof(int));
            } while (child_data == 0);
            int data_from_buf = buf_ptr[0];
            cout << myname << ": read buf_ptr[0] after child write get " << data_from_buf << endl;
            cout << myname << ": child data is " << child_data << endl;
            write(write_fd, &msg, sizeof(int));
            ck_assert_msg(child_data != data_from_buf, "Data from the child process should not be visible on the parent process via bar mapping!!! Security breach!!!");
        }

        printf("unmapping\n");
        ASSERT_EQ(gdr_unmap(g, mh, bar_ptr, size), 0);
        printf("unpinning\n");
        ASSERT_EQ(gdr_unpin_buffer(g, mh), 0);
    } END_CHECK;
    ASSERT_EQ(gdr_close(g), 0);
}
END_TEST

int main(int argc, char *argv[])
{
    Suite *s = suite_create("Invalidation");
    TCase *tc = tcase_create("Invalidation");
    SRunner *sr = srunner_create(s);

    int nf;

    suite_add_tcase(s, tc);
    tcase_add_test(tc, invalidation_basic);

    srunner_run_all(sr, CK_ENV);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    return nf == 0 ? 0 : 1;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
