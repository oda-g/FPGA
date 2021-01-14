/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2021 Itsuro Oda
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>

/* Note: This program is based on OpenCL 1.2.
 * clCreateCommandQueue and clEnqueueTask are deprecated in the
 * later version.
 */
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/opencl.h>

#define XCLBIN "../../bin/vadd.xclbin"

#define ALIGNMENT 4096
#define DATA_SIZE 4096

#define LOG printf

int main(void)
{
	cl_int err;
	cl_platform_id platform_id;
	char platform_vendor[100];
	cl_device_id device_id;
	cl_context context;
	cl_command_queue cmd_queue;
	int fd;
	struct stat st;
	size_t xclbin_size;
	unsigned char *xclbin;
	cl_program program;
	cl_kernel kernel;
	cl_int *data_in1, *data_in2, *data_out, *cpu_out;
	cl_mem bo_in1, bo_in2, bo_out;
	cl_int size = DATA_SIZE;
	int i;

	/* NOTE: Resouces are not freed explicitly in case of failure
	 * because the program will exit immediately in case of
	 * failure and resources will be freed implicitly at that time.
	 */

	cpu_out = (cl_int *)malloc(sizeof(cl_int) * DATA_SIZE);
	if (cpu_out == NULL) {
		perror("malloc");
		return 1;
	}

	/* I know there is one platform. */
	LOG("clGetPlatformIDs\n");
	err = clGetPlatformIDs(1, &platform_id, NULL);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't get platform ids: %d\n", err);
		return 1;
	}

	/* I know vender is xilinx, but confirm it just to make sure. */
	LOG("clGetPlatformInfo\n");
	err = clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, 100,
		       (void *)platform_vendor, NULL);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't get platform info: %d\n", err);
		return 1;
	}
	if (strcmp("Xilinx", platform_vendor)) {
		fprintf(stderr, "vender is not xilinx.\n");
		return 1;
	}

	/* I know there is one device. */
	LOG("clGetDeviceIDs\n");
	err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ACCELERATOR,
		       1, &device_id, NULL);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't get device ids: %d\n", err);
		return 1;
	}

	LOG("clCreateContext\n");
	context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &err);
	if (context == NULL) {
		fprintf(stderr, "can't create context: %d\n", err);
		return 1;
	}

	LOG("clCreateCommandQueue\n");
	cmd_queue = clCreateCommandQueue(context, device_id,
		       CL_QUEUE_PROFILING_ENABLE, &err);
	if (cmd_queue == NULL) {
		fprintf(stderr, "can't create command queue: %d\n", err);
		return 1;
	}

	/* reading xclbin */
	fd = open(XCLBIN, O_RDONLY);
	if (fd < 0) {
		perror("open xclbin");
		return 1;
	}
	if (fstat(fd, &st) == -1) {
		perror("stat xclbin");
		return 1;
	}
	xclbin_size = st.st_size;

	xclbin = (unsigned char *)mmap(NULL, xclbin_size, PROT_READ,
			MAP_PRIVATE, fd, 0);
	if (xclbin == NULL) {
		perror("mmap xclbin");
		return 1;
	}

	LOG("clCreateProgramWithBinary\n");
	program = clCreateProgramWithBinary(context, 1, &device_id,
		      &xclbin_size, (const unsigned char **)&xclbin,
		      NULL, &err);
	if (program == NULL) {
		fprintf(stderr, "can't create program: %d\n", err);
		return 1;
	}

	if (munmap(xclbin, xclbin_size) != 0) {
		perror("munmap");
		return 1;
	}
	close(fd);

	LOG("clCreateKernel\n");
	kernel = clCreateKernel(program, "vadd", &err);
	if (kernel == NULL) {
		fprintf(stderr, "can't create kernel: %d\n", err);
		return 1;
	}

	/* prepare memory */
	data_in1 = (cl_int *)aligned_alloc(ALIGNMENT, sizeof(cl_int) * DATA_SIZE);
	if (data_in1 == NULL) {
		perror("aligned_alloc data_in1");
		return 1;
	}
	data_in2 = (cl_int *)aligned_alloc(ALIGNMENT, sizeof(cl_int) * DATA_SIZE);
	if (data_in2 == NULL) {
		perror("aligned_alloc data_in2");
		return 1;
	}
	data_out = (cl_int *)aligned_alloc(ALIGNMENT, sizeof(cl_int) * DATA_SIZE);
	if (data_out == NULL) {
		perror("aligned_alloc data_out");
		return 1;
	}

	srand((unsigned int)time(NULL));
	for (i = 0; i < DATA_SIZE; i++) {
		data_in1[i] = rand();
		data_in2[i] = rand();
		cpu_out[i] = data_in1[i] + data_in2[i];
		data_out[i] = 0;
	}

	LOG("clCreateBuffer in1\n");
	bo_in1 = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
			sizeof(cl_int) * DATA_SIZE, data_in1, &err);
	if (bo_in1 == NULL) {
		fprintf(stderr, "can't create buffer in1: %d\n", err);
		return 1;
	}
	LOG("clCreateBuffer in2\n");
	bo_in2 = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
			sizeof(cl_int) * DATA_SIZE, data_in2, &err);
	if (bo_in2 == NULL) {
		fprintf(stderr, "can't create buffer in2: %d\n", err);
		return 1;
	}
	LOG("clCreateBuffer out\n");
	bo_out = clCreateBuffer(context,  CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
			sizeof(cl_int) * DATA_SIZE, data_out, &err);
	if (bo_out == NULL) {
		fprintf(stderr, "can't create buffer out: %d\n", err);
		return 1;
	}

	LOG("clSetKernelArg 0\n");
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bo_in1);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't set kernel arg 0: %d\n", err);
		return 1;
	}
	LOG("clSetKernelArg 1\n");
	err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bo_in2);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't set kernel arg 1: %d\n", err);
		return 1;
	}
	LOG("clSetKernelArg 2\n");
	err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &bo_out);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't set kernel arg 2: %d\n", err);
		return 1;
	}
	LOG("clSetKernelArg 3\n");
	err = clSetKernelArg(kernel, 3, sizeof(cl_int), &size);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't set kernel arg 3: %d\n", err);
		return 1;
	}

	LOG("clEnqueueMigrateMemObjects in\n");
	err = clEnqueueMigrateMemObjects(cmd_queue, 2, (cl_mem []){bo_in1, bo_in2},
				0, 0, NULL, NULL);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't migrate mem object in: %d\n", err);
		return 1;
	}

	LOG("clEnqueueTask\n");
	err = clEnqueueTask(cmd_queue, kernel, 0, NULL, NULL);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't enqueue kernel: %d\n", err);
		return 1;
	}

	LOG("clEnqueueMigrateMemObjects out\n");
	err = clEnqueueMigrateMemObjects(cmd_queue, 1, &bo_out,
		       CL_MIGRATE_MEM_OBJECT_HOST, 0, NULL, NULL);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't migrate mem object out: %d\n", err);
		return 1;
	}

	LOG("clFinish\n");
	err = clFinish(cmd_queue);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "can't finish: %d\n", err);
		return 1;
	}

	for (i = 0; i < DATA_SIZE; i++) {
		if (cpu_out[i] != data_out[i]) {
			printf("data different !! (%d)\n", i);
			break;
		}
	}
	if (i == DATA_SIZE) {
		printf("OK.\n");
	}

	/* Freeing resouces to show examples. */
	LOG("clReleaseMemObject in1\n");
	clReleaseMemObject(bo_in1);
	LOG("clReleaseMemObject in2\n");
	clReleaseMemObject(bo_in2);
	LOG("clReleaseMemObject out\n");
	clReleaseMemObject(bo_out);

	free(data_in1);
	free(data_in2);
	free(data_out);

	LOG("clReleaseKernel\n");
	clReleaseKernel(kernel);
	LOG("clReleaseProgram\n");
	clReleaseProgram(program);
	LOG("clReleaseCommandQueue\n");
	clReleaseCommandQueue(cmd_queue);
	LOG("clReleaseContext\n");
	clReleaseContext(context);

	return 0;
}
