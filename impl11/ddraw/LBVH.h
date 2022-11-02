#pragma once
#include "common.h"
#include <vector>

#include "EffectsCommon.h"
#include "xwa_structures.h"

constexpr int ENCODED_TREE_NODE2_SIZE = 48; // BVH2 node size
constexpr int ENCODED_TREE_NODE4_SIZE = 64; // BVH4 node size

struct Vector3;
struct Vector4;

#pragma pack(push, 1)

// BVH2 node format, deprecated since the BVH4 is more better
#ifdef DISABLED
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

static_assert(sizeof(BVHNode) == ENCODED_TREE_NODE2_SIZE, "BVHNodes (2) must be ENCODED_TREE_SIZE bytes");
#endif

// QBVH inner node
struct BVHNode {
	int ref; // TriID: -1 for internal nodes, Triangle index for leaves
	int parent; // Not used at this point
	int padding[2];
	// 16 bytes
	float min[4];
	// 32 bytes
	float max[4];
	// 48 bytes
	int children[4];
	// 64 bytes
};

// QBVH leaf node
struct BVHPrimNode {
	int ref;
	int parent;
	int padding[2];
	// 16 bytes
	float v0[4];
	// 32 bytes
	float v1[4];
	// 48 bytes
	float v2[4];
	// 64 bytes
};

static_assert(sizeof(BVHNode) == ENCODED_TREE_NODE4_SIZE, "BVHNode (4) must be ENCODED_TREE_NODE4_SIZE bytes");
static_assert(sizeof(BVHPrimNode) == ENCODED_TREE_NODE4_SIZE, "BVHPrimNode (4) must be ENCODED_TREE_NODE4_SIZE bytes");

struct MinMax {
	Vector3 min;
	Vector3 max;
};

#pragma pack(pop)

class AABB
{
public:
	Vector3 min;
	Vector3 max;
	// Limits are computed from min, max. Do not update Limits directly. Set min,max and
	// call UpdateLimits() instead.
	std::vector<Vector4> Limits;

	AABB() {
		SetInfinity();
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

	inline XwaVector3 GetCentroid()
	{
		return XwaVector3(
			(min.x + max.x) / 2.0f,
			(min.y + max.y) / 2.0f,
			(min.z + max.z) / 2.0f);
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

// Classes and data used for the Fast LBVH build.
using MortonCode_t = uint32_t;
// Morton Code, Bounding Box, TriID
using LeafItem = std::tuple<MortonCode_t, AABB, int>;
struct InnerNode
{
	int parent;
	// Children
	int left;
	int right;
	bool leftIsLeaf;
	bool rightIsLeaf;
	// Range
	int first;
	int last;
	AABB aabb;
	// Processing
	uint8_t readyCount;
	bool processed;
};

struct InnerNode4
{
	int parent;
	// Children
	int children[4];
	bool isLeaf[4];
	int numChildren;
	int totalNodes;
	// Range
	int first;
	int last;
	AABB aabb;
	// Processing
	uint8_t readyCount;
	bool processed;
};

bool leafSorter(const LeafItem& i, const LeafItem& j);

class IGenericTree
{
public:
	virtual int GetArity() = 0;
	virtual bool IsLeaf() = 0;
	virtual AABB GetBox() = 0;
	virtual int GetTriID() = 0;
	virtual std::vector<IGenericTree *> GetChildren() = 0;
	virtual IGenericTree *GetParent() = 0;
	virtual void SetNumNodes(int numNodes) = 0;
	virtual int GetNumNodes() = 0;
};

class TreeNode : public IGenericTree
{
public:
	int TriID, numNodes;
	TreeNode *left, *right, *parent;
	AABB box;
	XwaVector3 centroid;
	MortonCode_t code;
	bool red; // For Red-Black tree implementation

	void InitNode()
	{
		TriID = -1;
		left = right = parent = nullptr;
		box.SetInfinity();
		code = 0;
		numNodes = 0;
		code = 0xFFFFFFFF;
		// All new nodes are red by default
		red = true;
		// Invalid centroid:
		centroid.x = NAN;
		centroid.y = NAN;
		centroid.z = NAN;
	}

	TreeNode()
	{
		InitNode();
	}

	TreeNode(int TriID)
	{
		InitNode();
		this->TriID = TriID;
	}

	TreeNode(int TriID, MortonCode_t code)
	{
		InitNode();
		this->TriID = TriID;
		this->code = code;
	}

	TreeNode(int TriID, MortonCode_t code, const AABB &box)
	{
		InitNode();
		this->TriID = TriID;
		this->code = code;
		this->box.Expand(box);
	}

	TreeNode(int TriID, TreeNode *left, TreeNode *right)
	{
		InitNode();
		this->TriID = TriID;
		this->left = left;
		this->right = right;
	}

	TreeNode(int TriID, const AABB& box)
	{
		InitNode();
		this->TriID = TriID;
		this->box.Expand(box);
	}

	/*
	TreeNode(int TriID, const AABB &box, const XwaVector3 &centroid)
	{
		InitNode();
		this->TriID = TriID;
		this->box.Expand(box);
		this->centroid = centroid;
	}
	*/

	TreeNode(int TriID, const AABB &box, TreeNode *left, TreeNode *right)
	{
		InitNode();
		this->TriID = TriID;
		this->left = left;
		this->right = right;
		this->box.Expand(box);
	}

	TreeNode(int TriID, TreeNode *left, TreeNode *right, TreeNode *parent)
	{
		InitNode();
		this->TriID = TriID;
		this->left = left;
		this->right = right;
		this->parent = parent;
	}

	virtual int GetArity()
	{
		return 2;
	}

	virtual AABB GetBox()
	{
		return this->box;
	}

	virtual int GetTriID()
	{
		return this->TriID;
	}

	virtual bool IsLeaf()
	{
		return this->left == nullptr && this->right == nullptr;
	}

	virtual std::vector<IGenericTree*> GetChildren()
	{
		std::vector<IGenericTree*> List;
		if (this->left != nullptr)
			List.push_back(this->left);
		if (this->right != nullptr)
			List.push_back(this->right);
		return List;
	}

	virtual IGenericTree *GetParent()
	{
		return this->parent;
	}

	virtual void SetNumNodes(int numNodes)
	{
		this->numNodes = numNodes;
	}

	virtual int GetNumNodes()
	{
		return this->numNodes;
	}
};

class QTreeNode : public IGenericTree
{
public:
	int TriID, numNodes;
	QTreeNode* parent;
	QTreeNode* children[4];
	AABB box;

	void SetNullChildren()
	{
		for (int i = 0; i < 4; i++)
			this->children[i] = nullptr;
	}

	void SetChildren(QTreeNode* children[])
	{
		for (int i = 0; i < 4; i++) {
			this->children[i] = children[i];
			if (children[i] != nullptr)
				this->numNodes += children[i]->numNodes;
		}
	}

	QTreeNode(int TriID)
	{
		this->TriID = TriID;
		this->box.SetInfinity();
		this->numNodes = 0;
		this->parent = nullptr;
		SetNullChildren();
	}

	QTreeNode(int TriID, AABB box)
	{
		this->TriID = TriID;
		this->box = box;
		this->numNodes = 1;
		this->parent = nullptr;
		SetNullChildren();
	}

	QTreeNode(int TriID, QTreeNode *children[])
	{
		this->TriID = TriID;
		this->box.SetInfinity();
		this->numNodes = 1;
		this->parent = nullptr;
		SetChildren(children);
	}

	QTreeNode(int TriID, AABB box, QTreeNode *children[], QTreeNode* parent)
	{
		this->TriID = TriID;
		this->box = box;
		this->parent = parent;
		this->numNodes = 1;
		SetChildren(children);
	}

	virtual int GetArity()
	{
		return 4;
	}

	virtual AABB GetBox()
	{
		return this->box;
	}

	virtual int GetTriID()
	{
		return this->TriID;
	}

	virtual bool IsLeaf()
	{
		for (int i = 0; i < 4; i++)
			if (children[i] != nullptr)
				return false;
		return true;
	}

	virtual std::vector<IGenericTree*> GetChildren()
	{
		std::vector<IGenericTree*> result;
		for (int i = 0; i < 4; i++)
			if (children[i] != nullptr)
				result.push_back(children[i]);
		return result;
	}

	virtual IGenericTree* GetParent()
	{
		return parent;
	}

	virtual void SetNumNodes(int numNodes)
	{
		this->numNodes = numNodes;
	}

	virtual int GetNumNodes()
	{
		return this->numNodes;
	}
};

class LBVH {
public:
	float3 *vertices;
	int32_t *indices;
	BVHNode *nodes;
	uint32_t *vertexCounts;
	int numVertices, numIndices, numNodes;
	float scale;
	bool scaleComputed;

	LBVH() {
		this->vertices = nullptr;
		this->indices = nullptr;
		this->nodes = nullptr;
		this->scale = 1.0f;
		this->scaleComputed = false;

		this->numVertices = 0;
		this->numIndices = 0;
		this->numNodes = 0;
		this->vertexCounts = nullptr;
	}
	
	~LBVH() {
		if (vertices != nullptr)
			delete[] vertices;

		if (indices != nullptr)
			delete[] indices;

		if (nodes != nullptr)
			delete[] nodes;
	}

	static LBVH *LoadLBVH(char *sFileName, bool EmbeddedVerts=false, bool verbose=false);
	static LBVH *Build(const XwaVector3* vertices, const int numVertices, const int* indices, const int numIndices);
	static LBVH *BuildQBVH(const XwaVector3* vertices, const int numVertices, const int* indices, const int numIndices);

	void PrintTree(std::string level, int curnode);
	void DumpToOBJ(char *sFileName);
};

// Red-Black balanced insertion
TreeNode* insertRB(TreeNode* T, int TriID, MortonCode_t code, const AABB& box);

QTreeNode* BinTreeToQTree(int curNode, bool curNodeIsLeaf, const InnerNode* innerNodes, const std::vector<LeafItem>& leafItems);
void DeleteTree(QTreeNode* Q);

uint8_t* EncodeNodes(IGenericTree* root, const XwaVector3* Vertices, const int* Indices);
