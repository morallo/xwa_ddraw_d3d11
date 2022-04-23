// Based on GPU Pro 3's Raytracer from chapter 6.
#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 16

[numthreads(BLOCK_SIZE_X, BLOCK_SIZE_Y, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {

}
