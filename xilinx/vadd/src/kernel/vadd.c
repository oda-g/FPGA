void
vadd(const unsigned int *in1, const unsigned int *in2,
     unsigned int *out, int size)
{
#pragma HLS INTERFACE m_axi port=in1 offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=in2 offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem

	int i;

	for (i = 0; i < size; i++) {
		out[i] = in1[i] + in2[i];
	}
}
