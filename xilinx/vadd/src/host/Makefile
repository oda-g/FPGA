vadd_cl: vadd_cl.c
	gcc -o vadd_cl -I/opt/xilinx/xrt/include vadd_cl.c -L/opt/xilinx/xrt/lib -lxrt_core -lxrt_coreutil -lOpenCL -lpthread

vadd_xrt: vadd_xrt.c
	gcc -o vadd_xrt -I/opt/xilinx/xrt/include vadd_xrt.c -L/opt/xilinx/xrt/lib -lxrt_core -lxrt_coreutil
