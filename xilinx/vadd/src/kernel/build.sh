source /tools/Xilinx/Vitis/2020.1/settings64.sh
mkdir -p tmp
cp vadd.c design.cfg Makefile tmp
cd tmp
make $1
