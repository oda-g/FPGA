/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2021 Itsuro Oda
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ert.h"
#include "xrt.h"
#include "xrt_mem.h"

#define DATA_SIZE 1024
#define ARG_BANK 1  /* bank 1 */
#define CMD_SIZE 4096

#define ARG_IN1_OFFSET 0x10
#define ARG_IN2_OFFSET 0x1c
#define ARG_OUT_OFFSET 0x28
#define ARG_SIZE_OFFSET 0x34

#define XCLBIN "../../bin/vadd.xclbin"

#define LOG printf

int main(void)
{
	xclDeviceHandle dev_handle;
	int fd;
	struct stat st;
	size_t xclbin_size;
	struct axlf *xclbin;
	uuid_t xclbin_uuid;
	int ret;
	int i;
	xclBufferHandle bo_in1, bo_in2, bo_out, bo_cmd;
	size_t data_len = sizeof(int) * DATA_SIZE;
	int *data_in1, *data_in2, *data_out, *cpu_out;
	uint64_t p_in1, p_in2, p_out;
	struct xclBOProperties bo_prop;
	struct ert_start_kernel_cmd *start_cmd = NULL;
	struct cu_cmd_state_timestamps *ts;
	long mes_time;

	/* NOTE: Resouces are not freed explicitly in case of failure
	 * because the program will exit immediately in case of
	 * failure and resouces will be freed implicitly at that time.
	 */

	cpu_out = (int *)malloc(sizeof(int) * DATA_SIZE);
	if (cpu_out == NULL) {
		perror("malloc");
		return 1;
	}

	/* I know there is one device. */
	LOG("xclOpen\n");
	dev_handle = xclOpen(0, NULL, 0);
	if (dev_handle == XRT_NULL_HANDLE) {
		perror("xclOpen");
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

	xclbin = (struct axlf *)mmap(NULL, xclbin_size, PROT_READ,
			MAP_PRIVATE, fd, 0);
	if (xclbin == NULL) {
		perror("mmap xclbin");
		return 1;
	}
	memcpy(xclbin_uuid, xclbin->m_header.uuid, sizeof(uuid_t));

	LOG("xclLoadXclBin\n");
	ret = xclLoadXclBin(dev_handle, (const struct axlf *)xclbin);
	if (ret != 0) {
		perror("xclLoadXclBin");
		return 1;
	}

	if (munmap(xclbin, xclbin_size) != 0) {
		perror("munmap");
		return 1;
	}
	close(fd);

	for (i = 0; i < 3; i++) {
		LOG("xclOpenContext %d\n", i);
		ret = xclOpenContext(dev_handle, xclbin_uuid, (unsigned int)i, 1);
		if (ret != 0) {
			perror("xclOpenContext");
			return 1;
		}
	}

	LOG("xclAllocBO in1\n");
	bo_in1 = xclAllocBO(dev_handle, data_len, 0, ARG_BANK);
	if (bo_in1 == XRT_NULL_BO) {
		perror("xclAllocBO in1");
		return 1;
	}
	LOG("xclAllocBO in2\n");
	bo_in2 = xclAllocBO(dev_handle, data_len, 0, ARG_BANK);
	if (bo_in2 == XRT_NULL_BO) {
		perror("xclAllocBO in2");
		return 1;
	}
	LOG("xclAllocBO out\n");
	bo_out = xclAllocBO(dev_handle, data_len, 0, ARG_BANK);
	if (bo_out == XRT_NULL_BO) {
		perror("xclAllocBO out");
		return 1;
	}
	LOG("xclAllocBO cmd\n");
	bo_cmd = xclAllocBO(dev_handle, (size_t)CMD_SIZE, 0, XCL_BO_FLAGS_EXECBUF);
	if (bo_cmd == XRT_NULL_BO) {
		perror("xclAllocBO cmd");
		return 1;
	}

	LOG("xclGetBOProperties in1\n");
	if (xclGetBOProperties(dev_handle, bo_in1, &bo_prop) != 0) {
		perror("xclGetBOProperties in1");
		return 1;
	}
	p_in1 = bo_prop.paddr;

	LOG("xclGetBOProperties in2\n");
	if (xclGetBOProperties(dev_handle, bo_in2, &bo_prop) != 0) {
		perror("xclGetBOProperties in2");
		return 1;
	}
	p_in2 = bo_prop.paddr;

	LOG("xclGetBOProperties out\n");
	if (xclGetBOProperties(dev_handle, bo_out, &bo_prop) != 0) {
		perror("xclGetBOProperties out");
		return 1;
	}
	p_out = bo_prop.paddr;

	LOG("xclMapBO in1\n");
	data_in1 = (int *)xclMapBO(dev_handle, bo_in1, true);
	if (data_in1 == NULL) {
		perror("xclMapBO in1");
		return 1;
	}
	LOG("xclMapBO in2\n");
	data_in2 = (int *)xclMapBO(dev_handle, bo_in2, true);
	if (data_in2 == NULL) {
		perror("xclMapBO in2");
		return 1;
	}
	LOG("xclMapBO out\n");
	data_out = (int *)xclMapBO(dev_handle, bo_out, true);
	if (data_out == NULL) {
		perror("xclMapBO out");
		return 1;
	}
	LOG("xclMapBO cmd\n");
	start_cmd = (struct ert_start_kernel_cmd *)xclMapBO(dev_handle, bo_cmd, true);
	if (start_cmd == NULL) {
		perror("xclMapBO cmd");
		return 1;
	}

	srand((unsigned int)time(NULL));
	for (i = 0; i < DATA_SIZE; i++) {
		data_in1[i] = rand();
		data_in2[i] = rand();
		cpu_out[i] = data_in1[i] + data_in2[i];
	}
	memset(data_out, 0, data_len);

	LOG("xclSyncBO in1\n");
	if (xclSyncBO(dev_handle, bo_in1, XCL_BO_SYNC_BO_TO_DEVICE, data_len, 0) != 0) {
		perror("xclSyncBO in1");
		return 1;
	}
	LOG("xclSyncBO in2\n");
	if (xclSyncBO(dev_handle, bo_in2, XCL_BO_SYNC_BO_TO_DEVICE, data_len, 0) != 0) {
		perror("xclSyncBO in2");
		return 1;
	}

	memset(start_cmd, 0, CMD_SIZE);
	start_cmd->state = ERT_CMD_STATE_NEW;
	start_cmd->opcode = ERT_START_CU;
	start_cmd->stat_enabled = 1;
	start_cmd->count = 1 + (ARG_SIZE_OFFSET/4 + 1);
	start_cmd->cu_mask = 0x7;  /* CU 0, 1, 2 */
	start_cmd->data[ARG_IN1_OFFSET/4] = p_in1 & 0xffffffff; 
	start_cmd->data[ARG_IN1_OFFSET/4 + 1] = p_in1 >> 32;
	start_cmd->data[ARG_IN2_OFFSET/4] = p_in2 & 0xffffffff; 
	start_cmd->data[ARG_IN2_OFFSET/4 + 1] = p_in2 >> 32;
	start_cmd->data[ARG_OUT_OFFSET/4] = p_out & 0xffffffff; 
	start_cmd->data[ARG_OUT_OFFSET/4 + 1] = p_out >> 32;
	start_cmd->data[ARG_SIZE_OFFSET/4] = DATA_SIZE; 

	LOG("xclExecBuf\n");
	if (xclExecBuf(dev_handle, bo_cmd) != 0) {
		perror("xclExecBuf");
		return 1;
	}

	while (1) {
		LOG("xclExecWait\n");
		ret = xclExecWait(dev_handle, 1000);
		if (ret < 0) {
			perror("xclExecWait");
			return 1;
		}
		if (ret > 0) {
			break;
		}
	}

	/* check timestamps */
	ts = ert_start_kernel_timestamps(start_cmd);
	mes_time = ts->skc_timestamps[ERT_CMD_STATE_COMPLETED] \
			- ts->skc_timestamps[ERT_CMD_STATE_RUNNING];
	printf("FPGA kernel execution time: %ld(ns)\n", mes_time);

	LOG("xclSyncBO out\n");
	if (xclSyncBO(dev_handle, bo_out, XCL_BO_SYNC_BO_FROM_DEVICE, data_len, 0) != 0) {
		perror("xclSyncBO out");
		return 1;
	}

	for (i = 0; i < DATA_SIZE; i++) {
		if (cpu_out[i] != data_out[i]) {
			printf("data different !! (%d)\n", i);
			break;
		}
	}
	if (i == DATA_SIZE) {
		printf("### OK ###\n");
	}

	/* Freeing resouces to show examples. */
	LOG("xclUnmaoBO in1\n");
	xclUnmapBO(dev_handle, bo_in1, data_in1);

	LOG("xclUnmaoBO in2\n");
	xclUnmapBO(dev_handle, bo_in2, data_in2);

	LOG("xclUnmaoBO out\n");
	xclUnmapBO(dev_handle, bo_out, data_out);

	LOG("xclUnmaoBO cmd\n");
	xclUnmapBO(dev_handle, bo_cmd, start_cmd);

	LOG("xclFreeBO in1\n");
	xclFreeBO(dev_handle, bo_in1);

	LOG("xclFreeBO in2\n");
	xclFreeBO(dev_handle, bo_in2);

	LOG("xclFreeBO out\n");
	xclFreeBO(dev_handle, bo_out);

	LOG("xclFreeBO cmd\n");
	xclFreeBO(dev_handle, bo_cmd);

	for (i = 0; i < 3; i++) {
		LOG("xclCloseContext\n");
		xclCloseContext(dev_handle, xclbin_uuid, (unsigned int)i);
	}

	LOG("xclClose\n");
	xclClose(dev_handle);

	return 0;
}
