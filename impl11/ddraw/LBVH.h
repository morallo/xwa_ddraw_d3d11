#pragma once

#include <vector>

#include "EffectsCommon.h"
#include "xwa_structures.h"

constexpr int ENCODED_TREE_NODE_SIZE = 48;

struct Vector3;
struct Vector4;

#pragma pack(push, 1)
struct BVHNode {
	int ref; // -1 for internal nodes, Triangle index for leaves
	int left; // Offset for the left child
	int right; // Offset for the right child
	int padding;
	// 16 bytes
	float min[4];
	// 32 bytes
	float max[4];
	// 48 bytes
};

struct MinMax {
	Vector3 min;
	Vector3 max;
};

#pragma pack(pop)

static_assert(sizeof(BVHNode) == ENCODED_TREE_NODE_SIZE, "BVHNodes must be ENCODED_TREE_SIZE bytes");

class AABB
{
public:
	Vector3 min;
	Vector3 max;
	// Limits are computed from min, max. Do not update Limits directly. Set min,max and
	// call UpdateLimits() instead.
	std::vector<Vector4> Limits;

	AABB() {
		min.x = min.y = min.z = 0.0f;
		max.x = max.y = max.z = 0.0f;
	}

	inline void SetInfinity() {
		min.x = min.y = min.z = FLT_MAX;
		max.x = max.y = max.z = -FLT_MAX;
	}

	bool IsInvalid() {
		return (min.x == FLT_MAX || max.x == -FLT_MAX);
	}

	inline void Expand(const XwaVector3 &v) {
		if (v.x < min.x) min.x = v.x;
		if (v.y < min.y) min.y = v.y;
		if (v.z < min.z) min.z = v.z;

		if (v.x > max.x) max.x = v.x;
		if (v.y > max.y) max.y = v.y;
		if (v.z > max.z) max.z = v.z;
	}

	inline void Expand(const Vector4 &v) {
		if (v.x < min.x) min.x = v.x;
		if (v.y < min.y) min.y = v.y;
		if (v.z < min.z) min.z = v.z;

		if (v.x > max.x) max.x = v.x;
		if (v.y > max.y) max.y = v.y;
		if (v.z > max.z) max.z = v.z;
	}

	inline void Expand(const AABB &aabb) {
		if (aabb.min.x < min.x) min.x = aabb.min.x;
		if (aabb.min.y < min.y) min.y = aabb.min.y;
		if (aabb.min.z < min.z) min.z = aabb.min.z;

		if (aabb.max.x > max.x) max.x = aabb.max.x;
		if (aabb.max.y > max.y) max.y = aabb.max.y;
		if (aabb.max.z > max.z) max.z = aabb.max.z;
	}

	inline void Expand(const std::vector<Vector4> &Limits)
	{
		for each (Vector4 v in Limits)
			Expand(v);
	}

	Vector3 GetRange() {
		return max - min;
	}

	void UpdateLimits() {
		Limits.clear();
		Limits.push_back(Vector4(min.x, min.y, min.z, 1.0f));
		Limits.push_back(Vector4(max.x, min.y, min.z, 1.0f));
		Limits.push_back(Vector4(max.x, max.y, min.z, 1.0f));
		Limits.push_back(Vector4(min.x, max.y, min.z, 1.0f));

		Limits.push_back(Vector4(min.x, min.y, max.z, 1.0f));
		Limits.push_back(Vector4(max.x, min.y, max.z, 1.0f));
		Limits.push_back(Vector4(max.x, max.y, max.z, 1.0f));
		Limits.push_back(Vector4(min.x, max.y, max.z, 1.0f));
	}

	void TransformLimits(const Matrix4 &T) {
		for (uint32_t i = 0; i < Limits.size(); i++)
			Limits[i] = T * Limits[i];
	}

	void DumpLimitsToOBJ(FILE *D3DDumpOBJFile, int OBJGroupId, int VerticesCountOffset);
};

class LBVH {
public:
	float3 *vertices;
	int32_t *indices;
	BVHNode *nodes;
	//MinMax *meshMinMaxs;
	uint32_t *vertexCounts;
	int numVertices, numIndices, numNodes;
	//int numMeshMinMaxs, numVertexCounts;
	float scale;
	bool scaleComputed;

	LBVH() {
		this->vertices = nullptr;
		this->indices = nullptr;
		this->nodes = nullptr;
		//this->vertexCounts = nullptr;
		//this->meshMinMaxs = nullptr;
		this->scale = 1.0f;
		this->scaleComputed = false;

		this->numVertices = 0;
		this->numIndices = 0;
		this->numNodes = 0;
		//this->numMeshMinMaxs = 0;
		//this->numVertexCounts = 0;
	}
	
	~LBVH() {
		if (vertices != nullptr)
			delete[] vertices;

		if (indices != nullptr)
			delete[] indices;

		if (nodes != nullptr)
			delete[] nodes;

		//if (meshMinMaxs != nullptr)
		//	delete[] meshMinMaxs;

		//if (vertexCounts != nullptr)
		//	delete[] vertexCounts;
	}

	static LBVH *LoadLBVH(char *sFileName, bool verbose=false);

	void PrintTree(std::string level, int curnode);
};
