source /opt/xilinx/xrt/setup.sh
xclbinutil --dump-section IP_LAYOUT:json:ip_layout.json -i vadd.xclbin
xclbinutil --dump-section MEM_TOPOLOGY:json:mem_topology.json -i vadd.xclbin
xclbinutil --dump-section CONNECTIVITY:json:connectivity.json -i vadd.xclbin
xclbinutil --dump-section EMBEDDED_METADATA:raw:embedded_metadata.xml -i vadd.xclbin
