PLATFORM := xilinx_u200_xdma_201830_2

vadd.xo: vadd.c
	v++ -t hw --platform $(PLATFORM) -c -k vadd -o vadd.xo vadd.c

vadd.xclbin: vadd.xo
	v++ -t hw --platform $(PLATFORM) --config design.cfg -l -o vadd.xclbin vadd.xo
