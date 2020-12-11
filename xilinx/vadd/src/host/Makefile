vadd_host: vadd_host.c
	gcc -o vadd_host -I/opt/xilinx/xrt/include vadd_host.c -L/opt/xilinx/xrt/lib -lxrt_core -lxrt_coreutil

vadd_cl: vadd_cl.c
	gcc -o vadd_cl -I/opt/xilinx/xrt/include vadd_cl.c -L/opt/xilinx/xrt/lib -lxrt_core -lxrt_coreutil -lOpenCL -lpthread
