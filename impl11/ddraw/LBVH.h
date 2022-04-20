#pragma once
#include "EffectsCommon.h"

constexpr int ENCODED_TREE_NODE_SIZE = 48;

#pragma pack(push, 1)
struct BVHNode {
	int ref; // -1 for internal nodes, Triangle index for leaves
	int left; // Offset for the left child
	int right; // Offset for the right child
	int padding;
	float min[4];
	float max[4];
};
#pragma pack(pop)

static_assert(sizeof(BVHNode) == ENCODED_TREE_NODE_SIZE, "BVHNodes must be ENCODED_TREE_SIZE bytes");

class LBVH {
public:
	float3 *vertices;
	int32_t *indices;
	BVHNode *nodes;
	int numVertices, numIndices, numTreeNodes;

	LBVH() {
		this->vertices = nullptr;
		this->indices = nullptr;
		this->nodes = nullptr;
		this->numVertices = 0;
		this->numIndices = 0;
		this->numTreeNodes = 0;
	}
	
	~LBVH() {
		if (vertices != nullptr)
			delete[] vertices;

		if (indices != nullptr)
			delete[] indices;

		if (nodes != nullptr)
			delete[] nodes;
	}

	static LBVH *LoadLBVH(char *sFileName);
};
