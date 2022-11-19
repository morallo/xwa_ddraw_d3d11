#include "common.h"
#include "globals.h"
#include "LBVH.h"
#include <stdio.h>
#include <queue>
#include <algorithm>

// This is the global index where the next inner QBVH node will be written.
// It's used by BuildFastQBVH/FastLQBVH (the single-step version) to create
// the QBVH buffer as the BVH2 is built. It's expected that the algorithm
// will be parallelized and multiple threads will have to update this variable
// atomically, so that's why it's a global variable for now.
static int QBVHEncodeNodeIdx = 0;

void PrintTreeBuffer(std::string level, BVHNode* buffer, int curNode);
int BinTreeToBuffer(BVHNode* buffer, int EncodeOfs,
	int curNode, bool curNodeIsLeaf, InnerNode* innerNodes, const std::vector<LeafItem>& leafItems,
	const XwaVector3* vertices, const int* indices);

// Load a BVH2, deprecated since the BVH4 are more better
#ifdef DISABLED
LBVH *LBVH::LoadLBVH(char *sFileName, bool verbose) {
	FILE *file;
	errno_t error = 0;
	LBVH *lbvh = nullptr;

	try {
		error = fopen_s(&file, sFileName, "rb");
	}
	catch (...) {
		if (verbose)
			log_debug("[DBG] [BVH] Could not load [%s]", sFileName);
		return lbvh;
	}

	if (error != 0) {
		if (verbose)
			log_debug("[DBG] [BVH] Error %d when loading [%s]", error, sFileName);
		return lbvh;
	}

	try {
		// Read the Magic Word and the version
		{
			char magic[9];
			fread(magic, 1, 8, file);
			magic[8] = 0;
			if (strcmp(magic, "BVH2-1.0") != 0)
			{
				log_debug("[DBG] [BVH] Unknown BVH version. Got: [%s]", magic);
				return lbvh;
			}
		}

		lbvh = new LBVH();

		// Read the vertices. The vertices are in OPT coords. They should match what we see
		// in XwaOptEditor
		{
			int32_t NumVertices = 0;
			fread(&NumVertices, sizeof(int32_t), 1, file);
			lbvh->numVertices = NumVertices;
			lbvh->vertices = new float3[NumVertices];
			int NumItems = fread(lbvh->vertices, sizeof(float3), NumVertices, file);
			if (verbose)
				log_debug("[DBG] [BVH] Read %d vertices from BVH file", NumItems);
			// DEBUG
			/*
			log_debug("[DBG] [BVH] Vertices BEGIN");
			for (int i = 0; i < lbvh->numVertices; i++) {
				float3 V = lbvh->vertices[i];
				log_debug("[DBG] [BVH] %0.6f, %0.6f, %0.6f",
					V.x, V.y, V.z);
			}
			log_debug("[DBG] [BVH] Vertices END");
			*/
			// DEBUG
		}

		// Read the indices
		{
			int32_t NumIndices = 0;
			fread(&NumIndices, sizeof(int32_t), 1, file);
			lbvh->numIndices = NumIndices;
			lbvh->indices = new int32_t[NumIndices];
			int NumItems = fread(lbvh->indices, sizeof(int32_t), NumIndices, file);
			if (verbose)
				log_debug("[DBG] [BVH] Read %d indices from BVH file", NumItems);
		}

		// Read the BVH nodes
		{
			int32_t NumNodes = 0;
			fread(&NumNodes, sizeof(int32_t), 1, file);
			lbvh->numNodes = NumNodes;
			lbvh->nodes = new BVHNode[NumNodes];
			int NumItems = fread(lbvh->nodes, sizeof(BVHNode), NumNodes, file);
			if (verbose)
				log_debug("[DBG] [BVH] Read %d BVH nodes from BVH file", NumItems);
		}

		// Thanks to Jeremy I no longer need to read the mesh AABBs nor their vertex counts,
		// I can read the OPT scale directly off of XWA's heap. So this block is now unnecessary.
		// See D3dRenderer::UpdateMeshBuffers() for details on how to get the OPT scale.
#ifdef DISABLED
		// Read the mesh AABBs
		{
			int32_t NumMeshMinMaxs = 0;
			fread(&NumMeshMinMaxs, sizeof(int32_t), 1, file);
			lbvh->numMeshMinMaxs = NumMeshMinMaxs;
			lbvh->meshMinMaxs = new MinMax[NumMeshMinMaxs];
			int NumItems = fread(lbvh->meshMinMaxs, sizeof(MinMax), NumMeshMinMaxs, file);
			if (verbose)
				log_debug("[DBG] [BVH] Read %d AABBs from BVH file", NumItems);
		}

		// Read the Vertex Counts
		{
			int32_t NumVertexCounts = 0;
			fread(&NumVertexCounts, sizeof(int32_t), 1, file);
			lbvh->numVertexCounts = NumVertexCounts;
			lbvh->vertexCounts = new uint32_t[NumVertexCounts];
			int NumItems = fread(lbvh->vertexCounts, sizeof(uint32_t), NumVertexCounts, file);
			if (verbose)
				log_debug("[DBG] [BVH] Read %d Vertex Counts from BVH file", NumItems);
		}
#endif

		// DEBUG
		// Check some basic properties of the BVH
		if (false) {
			int minTriID = 2000000, maxTriID = -1;
			bool innerNodeComplete = true;

			for (int i = 0; i < lbvh->numNodes; i++) {
				if (lbvh->nodes[i].ref != -1) {
					minTriID = min(minTriID, lbvh->nodes[i].ref);
					maxTriID = max(maxTriID, lbvh->nodes[i].ref);
				}
				else {
					if (lbvh->nodes[i].left == -1 || lbvh->nodes[i].right == -1)
						innerNodeComplete = false;
				}
			}
			log_debug("[DBG] [BVH] minTriID: %d, maxTriID: %d", minTriID, maxTriID);
			log_debug("[DBG] [BVH] innerNodeComplete: %d", innerNodeComplete);

			// Check that all the indices reference existing vertices
			bool indicesRangeOK = true;
			for (int i = 0; i < lbvh->numIndices; i++) {
				if (lbvh->indices[i] < 0 || lbvh->indices[i] >= lbvh->numVertices) {
					log_debug("[DBG] [BVH] Invalid LBVH index: ", lbvh->indices[i]);
					indicesRangeOK = false;
					break;
				}
			}
			log_debug("[DBG] [BVH] indicesRangeOK: %d", indicesRangeOK);
		}
	}
	catch (...) {
		log_debug("[DBG] [BVH] There were errors while reading [%s]", sFileName);
		if (lbvh != nullptr) {
			delete lbvh;
			lbvh = nullptr;
		}
	}

	fclose(file);

	// DEBUG: PrintTree
	{
		//lbvh->PrintTree("", 0);
	}
	// DEBUG

	return lbvh;
}

void LBVH::PrintTree(std::string level, int curnode)
{
	if (curnode == -1)
		return;
	PrintTree(level + "    ", nodes[curnode].right);
	log_debug("[DBG] [BVH] %s%d", level.c_str(), nodes[curnode].ref);
	PrintTree(level + "    ", nodes[curnode].left);
}
#endif

int CalcNumInnerQBVHNodes(int numPrimitives)
{
	return max(1, (int)((2.0f * numPrimitives) / 3.0f));
}

bool leafSorter(const LeafItem& i, const LeafItem& j)
{
	return std::get<0>(i) < std::get<0>(j);
}

// Load a BVH4
LBVH *LBVH::LoadLBVH(char *sFileName, bool EmbeddedVerts, bool verbose) {
	FILE *file;
	errno_t error = 0;
	LBVH *lbvh = nullptr;

	try {
		error = fopen_s(&file, sFileName, "rb");
	}
	catch (...) {
		if (verbose)
			log_debug("[DBG] [BVH] Could not load [%s]", sFileName);
		return lbvh;
	}

	if (error != 0) {
		if (verbose)
			log_debug("[DBG] [BVH] Error %d when loading [%s]", error, sFileName);
		return lbvh;
	}

	try {
		// Read the Magic Word and the version
		{
			char magic[9];
			fread(magic, 1, 8, file);
			magic[8] = 0;
			if (strcmp(magic, "BVH4-1.0") != 0)
			{
				log_debug("[DBG] [BVH] Unknown BVH version. Got: [%s]", magic);
				return lbvh;
			}
		}

		lbvh = new LBVH();

		// Read Vertices and Indices when the geometry is not embedded
		if (!EmbeddedVerts) {
			// Read the vertices. The vertices are in OPT coords. They should match what we see
			// in XwaOptEditor
			{
				int32_t NumVertices = 0;
				fread(&NumVertices, sizeof(int32_t), 1, file);
				lbvh->numVertices = NumVertices;
				lbvh->vertices = new float3[NumVertices];
				int NumItems = fread(lbvh->vertices, sizeof(float3), NumVertices, file);
				if (verbose)
					log_debug("[DBG] [BVH] Read %d vertices from BVH file", NumItems);
			}

			// Read the indices
			{
				int32_t NumIndices = 0;
				fread(&NumIndices, sizeof(int32_t), 1, file);
				lbvh->numIndices = NumIndices;
				lbvh->indices = new int32_t[NumIndices];
				int NumItems = fread(lbvh->indices, sizeof(int32_t), NumIndices, file);
				if (verbose)
					log_debug("[DBG] [BVH] Read %d indices from BVH file", NumItems);
			}
		}

		// Read the BVH nodes
		{
			int32_t NumNodes = 0;
			fread(&NumNodes, sizeof(int32_t), 1, file);
			lbvh->numNodes = NumNodes;
			lbvh->nodes = new BVHNode[NumNodes];
			int NumItems = fread(lbvh->nodes, sizeof(BVHNode), NumNodes, file);
			if (verbose)
				log_debug("[DBG] [BVH] Read %d BVH nodes from BVH file", NumItems);
		}

		// DEBUG
		// Check some basic properties of the BVH
		if (false) {
			int minTriID = 2000000, maxTriID = -1;
			int minChildren = 10, maxChildren = -1;

			for (int i = 0; i < lbvh->numNodes; i++) {
				if (lbvh->nodes[i].ref != -1) {
					// Leaf node
					minTriID = min(minTriID, lbvh->nodes[i].ref);
					maxTriID = max(maxTriID, lbvh->nodes[i].ref);
				}
				else {
					// Inner node
					int numChildren = 0;
					for (int j = 0; j < 4; j++)
						numChildren += (lbvh->nodes[i].children[j] != -1) ? 1 : 0;
					if (numChildren < minChildren) minChildren = numChildren;
					if (numChildren > maxChildren) maxChildren = numChildren;
				}
			}
			log_debug("[DBG] [BVH] minTriID: %d, maxTriID: %d", minTriID, maxTriID);
			log_debug("[DBG] [BVH] min,maxChildren: %d, %d", minChildren, maxChildren);

			// Check that all the indices reference existing vertices
			if (!EmbeddedVerts) {
				bool indicesRangeOK = true;
				for (int i = 0; i < lbvh->numIndices; i++) {
					if (lbvh->indices[i] < 0 || lbvh->indices[i] >= lbvh->numVertices) {
						log_debug("[DBG] [BVH] Invalid LBVH index: ", lbvh->indices[i]);
						indicesRangeOK = false;
						break;
					}
				}
				log_debug("[DBG] [BVH] indicesRangeOK: %d", indicesRangeOK);
			}
			else {
				// Embedded Verts, dump an OBJ for debugging purposes
				//lbvh->DumpToOBJ("C:\\Temp\\BVHEmbedded.obj");
			}
		}
	}
	catch (...) {
		log_debug("[DBG] [BVH] There were errors while reading [%s]", sFileName);
		if (lbvh != nullptr) {
			delete lbvh;
			lbvh = nullptr;
		}
	}

	fclose(file);
	return lbvh;
}

void LBVH::PrintTree(std::string level, int curnode)
{
	if (curnode == -1)
		return;
	log_debug("[DBG] [BVH] %s", (level + "    --").c_str());
	for (int i = 3; i >= 2; i--)
		if (nodes[curnode].children[i] != -1)
			PrintTree(level + "    ", nodes[curnode].children[i]);

	log_debug("[DBG] [BVH] %s%d", level.c_str(), nodes[curnode].ref);

	for (int i = 1; i >= 0; i--)
		if (nodes[curnode].children[i] != -1)
			PrintTree(level + "    ", nodes[curnode].children[i]);
	log_debug("[DBG] [BVH] %s", (level + "    --").c_str());
}

void LBVH::DumpToOBJ(char *sFileName)
{
	BVHPrimNode *primNodes = (BVHPrimNode *)nodes;
	FILE *file = NULL;
	int index = 1;

	fopen_s(&file, sFileName, "wt");
	if (file == NULL) {
		log_debug("[DBG] [BVH] Could not open file: %s", sFileName);
		return;
	}

	log_debug("[DBG] [BVH] Dumping %d nodes to OBJ", numNodes);
	for (int i = 0; i < numNodes; i++) {
		if (nodes[i].ref != -1) {
			//BVHPrimNode node = primNodes[i];
			BVHNode node = nodes[i];
			// Leaf node, let's dump the embedded vertices
			fprintf(file, "o leaf-%d\n", i);
			/*
			fprintf(file, "v %f %f %f\n",
				node.v0[0] * OPT_TO_METERS,
				node.v0[1] * OPT_TO_METERS,
				node.v0[2] * OPT_TO_METERS);
			fprintf(file, "v %f %f %f\n",
				node.v1[0] * OPT_TO_METERS,
				node.v1[1] * OPT_TO_METERS,
				node.v1[2] * OPT_TO_METERS);
			fprintf(file, "v %f %f %f\n",
				node.v2[0] * OPT_TO_METERS,
				node.v2[1] * OPT_TO_METERS,
				node.v2[2] * OPT_TO_METERS);
			*/
			Vector3 v0, v1, v2;
			v0.x = node.min[0];
			v0.y = node.min[1];
			v0.z = node.min[2];

			v1.x = node.max[0];
			v1.y = node.max[1];
			v1.z = node.max[2];

			v2.x = *(float *)&(node.children[0]);
			v2.y = *(float *)&(node.children[1]);
			v2.z = *(float *)&(node.children[2]);

			fprintf(file, "v %f %f %f\n", v0.x * OPT_TO_METERS, v0.y * OPT_TO_METERS, v0.z * OPT_TO_METERS);
			fprintf(file, "v %f %f %f\n", v1.x * OPT_TO_METERS, v1.y * OPT_TO_METERS, v1.z * OPT_TO_METERS);
			fprintf(file, "v %f %f %f\n", v2.x * OPT_TO_METERS, v2.y * OPT_TO_METERS, v2.z * OPT_TO_METERS);

			fprintf(file, "f %d %d %d\n", index, index + 1, index + 2);
			index += 3;
		}
		else {
			// Inner node, dump the AABB
			BVHNode node = nodes[i];
			fprintf(file, "o aabb-%d\n", i);

			fprintf(file, "v %f %f %f\n",
				node.min[0] * OPT_TO_METERS, node.min[1] * OPT_TO_METERS, node.min[2] * OPT_TO_METERS);
			fprintf(file, "v %f %f %f\n",
				node.max[0] * OPT_TO_METERS, node.min[1] * OPT_TO_METERS, node.min[2] * OPT_TO_METERS);
			fprintf(file, "v %f %f %f\n",
				node.max[0] * OPT_TO_METERS, node.max[1] * OPT_TO_METERS, node.min[2] * OPT_TO_METERS);
			fprintf(file, "v %f %f %f\n",
				node.min[0] * OPT_TO_METERS, node.max[1] * OPT_TO_METERS, node.min[2] * OPT_TO_METERS);

			fprintf(file, "v %f %f %f\n",
				node.min[0] * OPT_TO_METERS, node.min[1] * OPT_TO_METERS, node.max[2] * OPT_TO_METERS);
			fprintf(file, "v %f %f %f\n",
				node.max[0] * OPT_TO_METERS, node.min[1] * OPT_TO_METERS, node.max[2] * OPT_TO_METERS);
			fprintf(file, "v %f %f %f\n",
				node.max[0] * OPT_TO_METERS, node.max[1] * OPT_TO_METERS, node.max[2] * OPT_TO_METERS);
			fprintf(file, "v %f %f %f\n",
				node.min[0] * OPT_TO_METERS, node.max[1] * OPT_TO_METERS, node.max[2] * OPT_TO_METERS);

			fprintf(file, "f %d %d\n", index, index + 1);
			fprintf(file, "f %d %d\n", index + 1, index + 2);
			fprintf(file, "f %d %d\n", index + 2, index + 3);
			fprintf(file, "f %d %d\n", index + 3, index);

			fprintf(file, "f %d %d\n", index + 4, index + 5);
			fprintf(file, "f %d %d\n", index + 5, index + 6);
			fprintf(file, "f %d %d\n", index + 6, index + 7);
			fprintf(file, "f %d %d\n", index + 7, index + 4);

			fprintf(file, "f %d %d\n", index, index + 4);
			fprintf(file, "f %d %d\n", index + 1, index + 5);
			fprintf(file, "f %d %d\n", index + 2, index + 6);
			fprintf(file, "f %d %d\n", index + 3, index + 7);
			index += 8;
		}
	}
	fclose(file);
	log_debug("[DBG] [BVH] BVH Dumped to OBJ");
}

static int firstbithigh(uint32_t X)
{
	int pos = 31;
	uint32_t mask = 0x1;
	while (pos >= 0)
	{
		if ((X & (mask << pos)) != 0x0)
			return pos;
		pos--;
	}
	return pos;
}

static int delta32(uint32_t X, uint32_t Y)
{
	if (X == Y)
		return -1;
	return firstbithigh(X ^ Y);
}

static uint32_t SpreadBits(uint32_t x, int offset)
{
	if ((x < 0) || (x > 1023))
	{
		return -1;
	}

	if ((offset < 0) || (offset > 2))
	{
		return -1;
	}

	x = (x | (x << 10)) & 0x000F801F;
	x = (x | (x << 4)) & 0x00E181C3;
	x = (x | (x << 2)) & 0x03248649;
	x = (x | (x << 2)) & 0x09249249;

	return x << offset;
}

// From https://stackoverflow.com/questions/1024754/how-to-compute-a-3d-morton-number-interleave-the-bits-of-3-ints
MortonCode_t GetMortonCode32(uint32_t x, uint32_t y, uint32_t z)
{
	return SpreadBits(x, 2) | SpreadBits(y, 1) | SpreadBits(z, 0);
}

MortonCode_t GetMortonCode32(const XwaVector3 &V)
{
	uint32_t x = (uint32_t)(V.x * 1023.0f);
	uint32_t y = (uint32_t)(V.y * 1023.0f);
	uint32_t z = (uint32_t)(V.z * 1023.0f);
	return GetMortonCode32(x, y, z);
}

// This is the delta used in Apetrei 2014
static int delta(const std::vector<LeafItem> &leafItems, int i)
{
	MortonCode_t mi = std::get<0>(leafItems[i]);
	MortonCode_t mj = std::get<0>(leafItems[i + 1]);
	return (mi == mj) ? i ^ (i + 1) : mi ^ mj;
}

int ChooseParent(int curNode, bool isLeaf, int numLeaves, const std::vector<LeafItem> &leafItems, InnerNode *innerNodes)
{
	int parent = -1;
	int left, right;
	AABB curAABB;

	if (isLeaf)
	{
		left = curNode;
		right = curNode;
		curAABB = std::get<1>(leafItems[curNode]);
	}
	else
	{
		left = innerNodes[curNode].first;
		right = innerNodes[curNode].last;
		curAABB = innerNodes[curNode].aabb;
	}

	/*
	log_debug("[DBG] [BVH] %s, curNode: %d, left,right: [%d,%d], d(right): %d, d(left - 1): %d",
		isLeaf ? "LEAF" : "INNER", curNode, left, right,
		right < numLeaves - 1 ? delta(leafItems, right) : -1,
		left - 1 >= 0 ? delta(leafItems, left - 1) : -1);
	*/
	if (left == 0 || (right != (numLeaves - 1) && delta(leafItems, right) < delta(leafItems, left - 1)))
	{
		parent = right;
		if (parent == numLeaves - 1)
		{
			// We have found the root
			return curNode;
		}
		innerNodes[parent].left = curNode;
		innerNodes[parent].leftIsLeaf = isLeaf;
		innerNodes[parent].first = left;
		innerNodes[parent].readyCount++;
		innerNodes[parent].aabb.Expand(curAABB);
		//log_debug("[DBG] [BVH]    case 1, parent: %d, parent.left: %d, parent.first: %d, readyCount: %d",
		//	parent, innerNodes[parent].left, innerNodes[parent].first, innerNodes[parent].readyCount);
	}
	else
	{
		parent = left - 1;
		innerNodes[parent].right = curNode;
		innerNodes[parent].rightIsLeaf = isLeaf;
		innerNodes[parent].last = right;
		innerNodes[parent].readyCount++;
		innerNodes[parent].aabb.Expand(curAABB);
		//log_debug("[DBG] [BVH]    case 2, parent: %d, parent.right: %d, parent.last: %d, readyCount: %d",
		//	parent, innerNodes[parent].right, innerNodes[parent].last, innerNodes[parent].readyCount);
	}
	return -1;
}

InnerNode *FastLBVH(const std::vector<LeafItem> &leafItems, int *root)
{
	int numLeaves = leafItems.size();
	*root = -1;
	//log_debug("[DBG] [BVH] numLeaves: %d", numLeaves);
	if (numLeaves <= 1)
		// Nothing to do, the single leaf is the root
		return nullptr;

	// Initialize the inner nodes
	int numInnerNodes = numLeaves - 1;
	int innerNodesProcessed = 0;
	InnerNode* innerNodes = new InnerNode[numInnerNodes];
	for (int i = 0; i < numInnerNodes; i++) {
		innerNodes[i].readyCount = 0;
		innerNodes[i].processed = false;
		innerNodes[i].aabb.SetInfinity();
	}

	// Start the tree by iterating over the leaves
	//log_debug("[DBG] [BVH] Adding leaves to BVH");
	for (int i = 0; i < numLeaves; i++)
		ChooseParent(i, true, numLeaves, leafItems, innerNodes);

	// Build the tree
	while (*root == -1 && innerNodesProcessed < numInnerNodes)
	{
		//log_debug("[DBG] [BVH] ********** Inner node iteration");
		for (int i = 0; i < numInnerNodes; i++) {
			if (!innerNodes[i].processed && innerNodes[i].readyCount == 2)
			{
				*root = ChooseParent(i, false, numLeaves, leafItems, innerNodes);
				innerNodes[i].processed = true;
				innerNodesProcessed++;
				if (*root != -1)
					break;
			}
		}
	}
	//log_debug("[DBG] [BVH] root at index: %d", *root);
	return innerNodes;
}

/// <summary>
/// Implements the Fast LBVH choose parent algorithm for QBVH nodes.
/// </summary>
/// <param name="curNode"></param>
/// <param name="isLeaf"></param>
/// <param name="numLeaves"></param>
/// <param name="leafItems"></param>
/// <param name="innerNodes"></param>
/// <returns>The root index if it was found, or -1 otherwise.</returns>
int ChooseParent4(int curNode, bool isLeaf, int numLeaves, const std::vector<LeafItem>& leafItems, InnerNode4* innerNodes)
{
	int parent = -1;
	int totalNodes = 0;
	int left, right;
	AABB curAABB;

	if (isLeaf)
	{
		left = curNode;
		right = curNode;
		curAABB = std::get<1>(leafItems[curNode]);
		totalNodes = 1;
	}
	else
	{
		left = innerNodes[curNode].first;
		right = innerNodes[curNode].last;
		curAABB = innerNodes[curNode].aabb;
		totalNodes = innerNodes[curNode].totalNodes;
	}

	if (left == 0 || (right != (numLeaves - 1) && delta(leafItems, right) < delta(leafItems, left - 1)))
	{
		parent = right;
		if (parent == numLeaves - 1)
		{
			// We have found the root
			return curNode;
		}
		innerNodes[parent].children[0] = curNode;
		innerNodes[parent].isLeaf[0] = isLeaf;
		innerNodes[parent].first = left;
	}
	else
	{
		parent = left - 1;
		innerNodes[parent].children[1] = curNode;
		innerNodes[parent].isLeaf[1] = isLeaf;
		innerNodes[parent].last = right;
		//log_debug("[DBG] [BVH]    case 2, parent: %d, parent.right: %d, parent.last: %d, readyCount: %d",
		//	parent, innerNodes[parent].right, innerNodes[parent].last, innerNodes[parent].readyCount);
	}
	innerNodes[parent].readyCount++;
	innerNodes[parent].aabb.Expand(curAABB);
	innerNodes[parent].numChildren++;
	innerNodes[parent].totalNodes += totalNodes;
	return -1;
}

/// <summary>
/// Converts a BVH2 node into a BVH4 node.
/// </summary>
/// <param name="innerNodes">The inner node list</param>
/// <param name="i">The current node to convert</param>
void ConvertToBVH4Node(InnerNode4 *innerNodes, int i)
{
	// TODO: Update the AABB of the converted node?
	InnerNode4 node = innerNodes[i];

	int numGrandChildren = 0;
	// Count all the grandchildren
	numGrandChildren += (node.isLeaf[0]) ? 1 : innerNodes[node.children[0]].numChildren;
	numGrandChildren += (node.isLeaf[1]) ? 1 : innerNodes[node.children[1]].numChildren;

	//log_debug("[DBG] [BVH] Attempting BVH4 conversion for node: %d, numChildren: %d, numGrandChildren: %d",
	//	i, node.numChildren, numGrandChildren);

	if (3 <= numGrandChildren && numGrandChildren <= 4)
	{
		// This node can be collapsed
		int numChild = 0;
		int totalNodes = node.totalNodes;
		InnerNode4 tmp = node;
		int arity = node.numChildren;
		//log_debug("[DBG] [BVH] Collapsing node of arity: %d", arity);
		for (int k = 0; k < arity; k++)
		{
			//log_debug("[DBG] [BVH]   child: %d, isLeaf: %d", k, node.isLeaf[k]);
			if (node.isLeaf[k])
			{
				// No grandchildren, copy immediate child
				tmp.children[numChild] = node.children[k];
				tmp.isLeaf[numChild]   = true;
				numChild++;
			}
			else
			{
				// Pull up grandchildren
				const int child = node.children[k];
				const int numGrandChildren = innerNodes[child].numChildren;
				for (int j = 0; j < numGrandChildren; j++)
				{
					//log_debug("[DBG] [BVH]      gchild: %d, node: %d, isLeaf: %d",
					//	j, innerNodes[child].children[j], innerNodes[child].isLeaf[j]);
					tmp.children[numChild] = innerNodes[child].children[j];
					tmp.isLeaf[numChild]   = innerNodes[child].isLeaf[j];
					tmp.QBVHOfs[numChild]  = innerNodes[child].QBVHOfs[j];

					numChild++;
				}
				// Disable the node that is now empty
				innerNodes[child].numChildren = -1;
				totalNodes--;
			}
		}
		tmp.numChildren = numChild;
		tmp.totalNodes = totalNodes;
		// Replace the original node
		innerNodes[i] = tmp;
		//log_debug("[DBG] [BVH] converted node %d to BVH4 node, numChildren: %d",
		//	i, tmp.numChildren);
	}
}

InnerNode4* FastLQBVH(const std::vector<LeafItem>& leafItems, int &root_out)
{
	int numLeaves = leafItems.size();
	root_out = -1;
	if (numLeaves <= 1)
		// Nothing to do, the single leaf is the root
		return nullptr;

	// Initialize the inner nodes
	int numInnerNodes = numLeaves - 1;
	int innerNodesProcessed = 0;
	InnerNode4* innerNodes = new InnerNode4[numInnerNodes];
	for (int i = 0; i < numInnerNodes; i++) {
		innerNodes[i].readyCount = 0;
		innerNodes[i].processed = false;
		innerNodes[i].aabb.SetInfinity();
		innerNodes[i].numChildren = 0;
		innerNodes[i].totalNodes = 1; // Count the current node
	}

	// Start the tree by iterating over the leaves
	//log_debug("[DBG] [BVH] Adding leaves to BVH");
	for (int i = 0; i < numLeaves; i++)
		ChooseParent4(i, true, numLeaves, leafItems, innerNodes);

	// Build the tree
	while (root_out == -1 && innerNodesProcessed < numInnerNodes)
	{
		//log_debug("[DBG] [BVH] ********** Inner node iteration");
		for (int i = 0; i < numInnerNodes; i++)
		{
			if (!innerNodes[i].processed && innerNodes[i].readyCount == 2)
			{
				// This node has its two children and doesn't have a parent yet.
				// Pull-up grandchildren and convert to BVH4 node
				ConvertToBVH4Node(innerNodes, i);
				root_out = ChooseParent4(i, false, numLeaves, leafItems, innerNodes);
				innerNodes[i].processed = true;
				innerNodesProcessed++;
				if (root_out != -1)
					break;
			}
		}
	}
	//log_debug("[DBG] [BVH] root at index: %d", *root);
	return innerNodes;
}

int EncodeInnerNode(BVHNode* buffer, BVHNode *node, int EncodeNodeIndex)
{
	uint32_t* ubuffer = (uint32_t*)buffer;
	float* fbuffer = (float*)buffer;
	// Convert the node index into an int offset:
	int ofs = EncodeNodeIndex * sizeof(BVHNode) / 4;

	// Encode the next inner node.
	ubuffer[ofs++] = -1; // TriID
	ubuffer[ofs++] = -1; // parent;
	ubuffer[ofs++] = 0; // padding;
	ubuffer[ofs++] = 0; // padding;
	// 16 bytes
	fbuffer[ofs++] = node->min[0];
	fbuffer[ofs++] = node->min[1];
	fbuffer[ofs++] = node->min[2];
	fbuffer[ofs++] = 1.0f;
	// 32 bytes
	fbuffer[ofs++] = node->max[0];
	fbuffer[ofs++] = node->max[1];
	fbuffer[ofs++] = node->max[2];
	fbuffer[ofs++] = 1.0f;
	// 48 bytes
	for (int j = 0; j < 4; j++)
		ubuffer[ofs++] = node->children[j];
	// 64 bytes
	return ofs;
}

int EncodeInnerNode(BVHNode* buffer, InnerNode4* innerNodes, int curNode, int EncodeNodeIndex)
{
	uint32_t* ubuffer = (uint32_t*)buffer;
	float* fbuffer = (float*)buffer;
	AABB box = innerNodes[curNode].aabb;
	const int numChildren = innerNodes[curNode].numChildren;
	int ofs = EncodeNodeIndex * sizeof(BVHNode) / 4;

	// Encode the next inner node.
	ubuffer[ofs++] = -1; // TriID
	ubuffer[ofs++] = -1; // parent;
	ubuffer[ofs++] =  0; // padding;
	ubuffer[ofs++] =  0; // padding;
	// 16 bytes
	fbuffer[ofs++] = box.min.x;
	fbuffer[ofs++] = box.min.y;
	fbuffer[ofs++] = box.min.z;
	fbuffer[ofs++] = 1.0f;
	// 32 bytes
	fbuffer[ofs++] = box.max.x;
	fbuffer[ofs++] = box.max.y;
	fbuffer[ofs++] = box.max.z;
	fbuffer[ofs++] = 1.0f;
	// 48 bytes
	for (int j = 0; j < 4; j++)
		ubuffer[ofs++] = (j < numChildren) ? innerNodes[curNode].QBVHOfs[j] : -1;
	// 64 bytes
	return ofs;
}

int EncodeLeafNode(BVHNode* buffer, const std::vector<LeafItem>& leafItems, int leafIdx, int EncodeNodeIdx,
	const XwaVector3 *vertices, const int *indices)
{
	uint32_t* ubuffer = (uint32_t*)buffer;
	float* fbuffer = (float*)buffer;
	int TriID = std::get<2>(leafItems[leafIdx]);
	int idx = TriID * 3;
	XwaVector3 v0, v1, v2;
	int EncodeOfs = EncodeNodeIdx * sizeof(BVHNode) / 4;
	//log_debug("[DBG] [BVH] Encoding leaf %d, TriID: %d, at QBVHOfs: %d",
	//	leafIdx, TriID, (EncodeOfs * 4) / 64);
	//if (vertices != nullptr && indices != nullptr)
	{
		v0 = vertices[indices[idx + 0]];
		v1 = vertices[indices[idx + 1]];
		v2 = vertices[indices[idx + 2]];
	}
	// Encode the current primitive into the QBVH buffer, in the leaf section
	ubuffer[EncodeOfs++] = TriID;
	ubuffer[EncodeOfs++] = -1; // parent
	ubuffer[EncodeOfs++] =  0;
	ubuffer[EncodeOfs++] =  0;
	// 16 bytes
	fbuffer[EncodeOfs++] = v0.x;
	fbuffer[EncodeOfs++] = v0.y;
	fbuffer[EncodeOfs++] = v0.z;
	fbuffer[EncodeOfs++] = 1.0f;
	// 32 bytes
	fbuffer[EncodeOfs++] = v1.x;
	fbuffer[EncodeOfs++] = v1.y;
	fbuffer[EncodeOfs++] = v1.z;
	fbuffer[EncodeOfs++] = 1.0f;
	// 48 bytes
	fbuffer[EncodeOfs++] = v2.x;
	fbuffer[EncodeOfs++] = v2.y;
	fbuffer[EncodeOfs++] = v2.z;
	fbuffer[EncodeOfs++] = 1.0f;
	// 64 bytes
	return EncodeOfs;
}

// Encodes the immediate children of inner node curNode
void EncodeChildren(BVHNode *buffer, int numQBVHInnerNodes, InnerNode4* innerNodes, int curNode, const std::vector<LeafItem>& leafItems)
{
	uint32_t* ubuffer = (uint32_t*)buffer;
	float* fbuffer = (float*)buffer;

	int numChildren = innerNodes[curNode].numChildren;
	for (int i = 0; i < numChildren; i++) {
		int childNode = innerNodes[curNode].children[i];

		// Leaves are already encoded
		if (innerNodes[curNode].isLeaf[i]) {
			// TODO: Write the parent of the leaf
			int TriID = std::get<2>(leafItems[childNode]);
			//innerNodes[curNode].QBVHOfs[i] = TriID + numQBVHInnerNodes;
			innerNodes[curNode].QBVHOfs[i] = childNode + numQBVHInnerNodes; // ?
			//log_debug("[DBG] [BVH] Linking Leaf. curNode: %d, i: %d, childNode: %d, QBVHOfs: %d",
			//	curNode, i, childNode, innerNodes[curNode].QBVHOfs[i]);
		}
		else
		{
			if (!innerNodes[childNode].isEncoded) {
				EncodeInnerNode(buffer, innerNodes, childNode, QBVHEncodeNodeIdx);
				// Update the parent's QBVH offset for this child:
				innerNodes[curNode].QBVHOfs[i] = QBVHEncodeNodeIdx;
				innerNodes[childNode].isEncoded = true;
				//log_debug("[DBG] [BVH] Linking Inner (E). curNode: %d, i: %d, childNode: %d, QBVHOfs: %d",
				//	curNode, i, childNode, innerNodes[curNode].QBVHOfs[i]);
				// Jump to the next available slot (atomic decrement)
				QBVHEncodeNodeIdx--;
			}
			/*else {
				log_debug("[DBG] [BVH] Linking Inner (NE). curNode: %d, i: %d, childNode: %d, QBVHOfs: %d",
					curNode, i, childNode, innerNodes[curNode].QBVHOfs[i]);
			}*/
		}
	}
}

/// <summary>
/// Builds the BVH2, converts it to a QBVH and encodes it into buffer, all at the same time.
/// </summary>
/// <param name="buffer">The BVHNode encoding buffer</param>
/// <param name="leafItems"></param>
/// <returns>The index of the QBVH root</returns>
void FastLQBVH(BVHNode* buffer, int numQBVHInnerNodes, const std::vector<LeafItem>& leafItems, int &root_out /*int& inner_root, bool debug = false*/)
{
	int numLeaves = leafItems.size();
	// QBVHEncodeNodeIdx points to the next available inner node index.
	QBVHEncodeNodeIdx = numQBVHInnerNodes - 1;
	root_out = -1;
	//inner_root = -1;
	if (numLeaves <= 1) {
		// Nothing to do, the single leaf is the root. Return the index of the root
		root_out = numQBVHInnerNodes;
		return;
	}
	//log_debug("[DBG] [BVH] Initial QBVHEncodeNodeIdx: %d", QBVHEncodeNodeIdx);

	// Initialize the inner nodes
	int numInnerNodes = numLeaves - 1;
	int innerNodesProcessed = 0;
	InnerNode4* innerNodes = new InnerNode4[numInnerNodes];
	for (int i = 0; i < numInnerNodes; i++) {
		innerNodes[i].readyCount = 0;
		innerNodes[i].processed = false;
		innerNodes[i].aabb.SetInfinity();
		innerNodes[i].numChildren = 0;
		innerNodes[i].totalNodes = 1; // Count the current node
		innerNodes[i].isEncoded = false;
	}

	// Start the tree by iterating over the leaves
	//log_debug("[DBG] [BVH] Adding leaves to BVH");
	for (int i = 0; i < numLeaves; i++)
		ChooseParent4(i, true, numLeaves, leafItems, innerNodes);

	// Build the tree
	while (root_out == -1 && innerNodesProcessed < numInnerNodes)
	{
		//log_debug("[DBG] [BVH] ********** Inner node iteration");
		for (int i = 0; i < numInnerNodes; i++)
		{
			if (!innerNodes[i].processed && innerNodes[i].readyCount == 2)
			{
				// This node has its two children and doesn't have a parent yet.
				// Pull-up grandchildren and convert to BVH4 node
				ConvertToBVH4Node(innerNodes, i);
				// The children of this node can now be encoded
				EncodeChildren(buffer, numQBVHInnerNodes, innerNodes, i, leafItems);
				root_out = ChooseParent4(i, false, numLeaves, leafItems, innerNodes);
				innerNodes[i].processed = true;
				innerNodesProcessed++;
				if (root_out != -1)
				{
					//log_debug("[DBG] [BVH] Encoding the root (%d), after processing node: %d, at QBVHOfs: %d",
					//	root_out, i, QBVHEncodeNodeIdx);
					// Encode the root
					EncodeInnerNode(buffer, innerNodes, root_out, QBVHEncodeNodeIdx);
					// Replace root with the encoded QBVH root offset
					//inner_root = root_out;
					root_out = QBVHEncodeNodeIdx;
					//log_debug("[DBG] [BVH] inner_root: %d, root_out: %d");
					break;
				}
			}
		}
	}
	//log_debug("[DBG] [BVH] root at index: %d", *root);
	/*
	if (debug)
	{
		return innerNodes;
	}
	else
	{
		return nullptr;
	}
	*/
	delete[] innerNodes;
}

std::string tab(int N)
{
	std::string res = "";
	for (int i = 0; i < N; i++)
		res += " ";
	return res;
}

static void printTree(int N, int curNode, bool isLeaf, InnerNode* innerNodes)
{
	if (curNode == -1 || innerNodes == nullptr)
		return;

	if (isLeaf)
	{
		log_debug("[DBG] [BVH] %s%d]", tab(N).c_str(), curNode);
		return;
	}

	printTree(N + 4, innerNodes[curNode].right, innerNodes[curNode].rightIsLeaf, innerNodes);
	log_debug("[DBG] [BVH] %s%d", tab(N).c_str(), curNode);
	printTree(N + 4, innerNodes[curNode].left, innerNodes[curNode].leftIsLeaf, innerNodes);
}

static void printTree(int N, int curNode, bool isLeaf, InnerNode4* innerNodes)
{
	if (curNode == -1 || innerNodes == nullptr)
		return;

	if (isLeaf)
	{
		log_debug("[DBG] [BVH] %s%d]", tab(N).c_str(), curNode);
		return;
	}

	int arity = innerNodes[curNode].numChildren;
	log_debug("[DBG] [BVH] %s", (tab(N) + "   /--\\").c_str());
	for (int i = arity - 1; i >= arity / 2; i--)
		printTree(N + 4, innerNodes[curNode].children[i], innerNodes[curNode].isLeaf[i], innerNodes);
	log_debug("[DBG] [BVH] %s%d,%d", tab(N).c_str(), curNode, innerNodes[curNode].totalNodes);
	for (int i = arity / 2 - 1; i >= 0; i--)
		printTree(N + 4, innerNodes[curNode].children[i], innerNodes[curNode].isLeaf[i], innerNodes);
	log_debug("[DBG] [BVH] %s", (tab(N) + "   \\--/").c_str());
}

static void PrintTree(std::string level, IGenericTree *T)
{
	if (T == nullptr)
		return;

	int arity = T->GetArity();
	std::vector<IGenericTree *> children = T->GetChildren();
	bool isLeaf = T->IsLeaf();

	if (arity > 2 && !isLeaf) log_debug("[DBG] [BVH] %s", (level + "   /--\\").c_str());
	for (int i = arity - 1; i >= arity / 2; i--)
		if (i < (int)children.size())
			PrintTree(level + "    ", children[i]);

	log_debug("[DBG] [BVH] %s%s",
		(level + std::to_string(T->GetTriID())).c_str(), isLeaf ? "]" : "");

	for (int i = arity / 2 - 1; i >= 0; i--)
		if (i < (int)children.size())
			PrintTree(level + "    ", children[i]);
	if (arity > 2 && !isLeaf) log_debug("[DBG] [BVH] %s", (level + "   \\--/").c_str());
}

void PrintTreeBuffer(std::string level, BVHNode *buffer, int curNode)
{
	if (buffer == nullptr)
		return;

	BVHNode node = buffer[curNode];
	int TriID = node.ref;
	if (TriID != -1) {
		// Leaf
		log_debug("[DBG] [BVH] %s%d,%d]", level.c_str(), curNode, TriID);
		/*
		BVHPrimNode* n = (BVHPrimNode *)&node;
		log_debug("[DBG] [BVH] %sv0: (%0.3f, %0.3f, %0.3f)", (level + "   ").c_str(),
			n->v0[0], n->v0[1], n->v0[2]);
		log_debug("[DBG] [BVH] %sv1: (%0.3f, %0.3f, %0.3f)", (level + "   ").c_str(),
			n->v1[0], n->v1[1], n->v1[2]);
		log_debug("[DBG] [BVH] %sv2: (%0.3f, %0.3f, %0.3f)", (level + "   ").c_str(),
			n->v2[0], n->v2[1], n->v2[2]);
		*/
		return;
	}

	int arity = 4;
	if (arity > 2) log_debug("[DBG] [BVH] %s", (level + "   /----\\").c_str());
	for (int i = arity - 1; i >= arity / 2; i--)
		if (node.children[i] != -1)
			PrintTreeBuffer(level + "    ", buffer, node.children[i]);

	log_debug("[DBG] [BVH] %s%d", level.c_str(), curNode);

	for (int i = arity / 2 - 1; i >= 0; i--)
		if (node.children[i] != -1)
			PrintTreeBuffer(level + "    ", buffer, node.children[i]);
	if (arity > 2) log_debug("[DBG] [BVH] %s", (level + "   \\----/").c_str());
}

void Normalize(XwaVector3 &A, const AABB &sceneBox, const XwaVector3 &range)
{
	A.x -= sceneBox.min.x;
	A.y -= sceneBox.min.y;
	A.z -= sceneBox.min.z;

	A.x /= range.x;
	A.y /= range.y;
	A.z /= range.z;
}

int DumpTriangle(const std::string &name, FILE *file, int OBJindex, const XwaVector3 &v0, const XwaVector3& v1, const XwaVector3& v2)
{
	if (name.size() != 0)
		fprintf(file, "o %s\n", name.c_str());

	fprintf(file, "v %f %f %f\n", v0.x * OPT_TO_METERS, v0.y * OPT_TO_METERS, v0.z * OPT_TO_METERS);
	fprintf(file, "v %f %f %f\n", v1.x * OPT_TO_METERS, v1.y * OPT_TO_METERS, v1.z * OPT_TO_METERS);
	fprintf(file, "v %f %f %f\n", v2.x * OPT_TO_METERS, v2.y * OPT_TO_METERS, v2.z * OPT_TO_METERS);

	fprintf(file, "f %d %d %d\n", OBJindex, OBJindex + 1, OBJindex + 2);
	return OBJindex + 3;
}

int DumpAABB(const std::string &name, FILE* file, int OBJindex, const Vector3 &min, const Vector3 &max)
{
	fprintf(file, "o %s\n", name.c_str());

	fprintf(file, "v %f %f %f\n",
		min[0] * OPT_TO_METERS, min[1] * OPT_TO_METERS, min[2] * OPT_TO_METERS);
	fprintf(file, "v %f %f %f\n",
		max[0] * OPT_TO_METERS, min[1] * OPT_TO_METERS, min[2] * OPT_TO_METERS);
	fprintf(file, "v %f %f %f\n",
		max[0] * OPT_TO_METERS, max[1] * OPT_TO_METERS, min[2] * OPT_TO_METERS);
	fprintf(file, "v %f %f %f\n",
		min[0] * OPT_TO_METERS, max[1] * OPT_TO_METERS, min[2] * OPT_TO_METERS);

	fprintf(file, "v %f %f %f\n",
		min[0] * OPT_TO_METERS, min[1] * OPT_TO_METERS, max[2] * OPT_TO_METERS);
	fprintf(file, "v %f %f %f\n",
		max[0] * OPT_TO_METERS, min[1] * OPT_TO_METERS, max[2] * OPT_TO_METERS);
	fprintf(file, "v %f %f %f\n",
		max[0] * OPT_TO_METERS, max[1] * OPT_TO_METERS, max[2] * OPT_TO_METERS);
	fprintf(file, "v %f %f %f\n",
		min[0] * OPT_TO_METERS, max[1] * OPT_TO_METERS, max[2] * OPT_TO_METERS);

	fprintf(file, "f %d %d\n", OBJindex + 0, OBJindex + 1);
	fprintf(file, "f %d %d\n", OBJindex + 1, OBJindex + 2);
	fprintf(file, "f %d %d\n", OBJindex + 2, OBJindex + 3);
	fprintf(file, "f %d %d\n", OBJindex + 3, OBJindex + 0);

	fprintf(file, "f %d %d\n", OBJindex + 4, OBJindex + 5);
	fprintf(file, "f %d %d\n", OBJindex + 5, OBJindex + 6);
	fprintf(file, "f %d %d\n", OBJindex + 6, OBJindex + 7);
	fprintf(file, "f %d %d\n", OBJindex + 7, OBJindex + 4);

	fprintf(file, "f %d %d\n", OBJindex + 0, OBJindex + 4);
	fprintf(file, "f %d %d\n", OBJindex + 1, OBJindex + 5);
	fprintf(file, "f %d %d\n", OBJindex + 2, OBJindex + 6);
	fprintf(file, "f %d %d\n", OBJindex + 3, OBJindex + 7);
	return OBJindex + 8;
}

void DumpInnerNodesToOBJ(char *sFileName, int rootIdx,
	const InnerNode* innerNodes, const std::vector<LeafItem>& leafItems,
	const XwaVector3* vertices, const int* indices)
{
	int OBJindex = 1;
	int numLeaves = leafItems.size();
	int numInnerNodes = numLeaves - 1;
	std::string name;

	log_debug("[DBG] [BVH] ***** Dumping Fast LBVH to file: %s", sFileName);

	FILE* file = NULL;
	fopen_s(&file, sFileName, "wt");
	if (file == NULL) {
		log_debug("[DBG] [BVH] Could not open file: %s", sFileName);
		return;
	}

	for (int curNode = 0; curNode < numLeaves; curNode++)
	{
		int TriID = std::get<2>(leafItems[curNode]);
		int i = TriID * 3;

		XwaVector3 v0 = vertices[indices[i + 0]];
		XwaVector3 v1 = vertices[indices[i + 1]];
		XwaVector3 v2 = vertices[indices[i + 2]];

		name = "leaf-" + std::to_string(curNode);
		OBJindex = DumpTriangle(name, file, OBJindex, v0, v1, v2);
	}

	for (int curNode = 0; curNode < numInnerNodes; curNode++)
	{
		InnerNode node = innerNodes[curNode];

		name = (curNode == rootIdx) ?
			name = "ROOT-" + std::to_string(curNode) :
			name = "aabb-" + std::to_string(curNode);
		OBJindex = DumpAABB(name, file, OBJindex, node.aabb.min, node.aabb.max);
	}

	fclose(file);
}

// Encode a BVH4 node using Embedded Geometry.
// Returns the new offset (in multiples of 4 bytes) that can be written to.
static int EncodeTreeNode4(void* buffer, int startOfs, IGenericTree* T, int32_t parent, const std::vector<int>& children,
	const XwaVector3* Vertices, const int* Indices)
{
	AABB box = T->GetBox();
	int TriID = T->GetTriID();
	int padding = 0;
	int ofs = startOfs;
	uint32_t* ubuffer = (uint32_t*)buffer;
	float* fbuffer = (float*)buffer;

	// This leaf node must have its vertices embedded in the node
	if (TriID != -1)
	{
		int vertofs = TriID * 3;
		XwaVector3 v0, v1, v2;
		//if (Vertices != nullptr && Indices != nullptr)
		{
			v0 = Vertices[Indices[vertofs]];
			v1 = Vertices[Indices[vertofs + 1]];
			v2 = Vertices[Indices[vertofs + 2]];
		}

		ubuffer[ofs++] = TriID;
		ubuffer[ofs++] = parent;
		ubuffer[ofs++] = padding;
		ubuffer[ofs++] = padding;
		// 16 bytes

		fbuffer[ofs++] = v0.x;
		fbuffer[ofs++] = v0.y;
		fbuffer[ofs++] = v0.z;
		fbuffer[ofs++] = 1.0f;
		// 32 bytes
		fbuffer[ofs++] = v1.x;
		fbuffer[ofs++] = v1.y;
		fbuffer[ofs++] = v1.z;
		fbuffer[ofs++] = 1.0f;
		// 48 bytes
		fbuffer[ofs++] = v2.x;
		fbuffer[ofs++] = v2.y;
		fbuffer[ofs++] = v2.z;
		fbuffer[ofs++] = 1.0f;
		// 64 bytes
	}
	else
	{
		ubuffer[ofs++] = TriID;
		ubuffer[ofs++] = parent;
		ubuffer[ofs++] = padding;
		ubuffer[ofs++] = padding;
		// 16 bytes
		fbuffer[ofs++] = box.min.x;
		fbuffer[ofs++] = box.min.y;
		fbuffer[ofs++] = box.min.z;
		fbuffer[ofs++] = 1.0f;
		// 32 bytes
		fbuffer[ofs++] = box.max.x;
		fbuffer[ofs++] = box.max.y;
		fbuffer[ofs++] = box.max.z;
		fbuffer[ofs++] = 1.0f;
		// 48 bytes
		for (int i = 0; i < 4; i++)
			ubuffer[ofs++] = children[i];
		// 64 bytes
	}

	/*
	if (ofs - startOfs == ENCODED_TREE_NODE4_SIZE)
	{
		log_debug("[DBG] [BVH] TreeNode should be encoded in %d bytes, but got %d instead",
			ENCODED_TREE_NODE4_SIZE, ofs - startOfs);
	}
	*/
	return ofs;
}

// Encode a BVH4 node using Embedded Geometry.
// Returns the new offset (in multiples of 4 bytes) that can be written to.
static int EncodeTreeNode4(void* buffer, int startOfs,
	int curNode, InnerNode4* innerNodes,
	bool isLeaf, const std::vector<LeafItem>& leafItems,
	int32_t parent, const std::vector<int>& children,
	const XwaVector3* Vertices, const int* Indices)
{
	AABB box = isLeaf ? std::get<1>(leafItems[curNode]) : innerNodes[curNode].aabb;
	int TriID = isLeaf ? std::get<2>(leafItems[curNode]) : -1;
	int padding = 0;
	int ofs = startOfs;
	uint32_t* ubuffer = (uint32_t*)buffer;
	float* fbuffer = (float*)buffer;

	// This leaf node must have its vertices embedded in the node
	if (TriID != -1)
	{
		int vertofs = TriID * 3;
		XwaVector3 v0, v1, v2;
		if (Vertices != nullptr && Indices != nullptr)
		{
			v0 = Vertices[Indices[vertofs]];
			v1 = Vertices[Indices[vertofs + 1]];
			v2 = Vertices[Indices[vertofs + 2]];
		}

		ubuffer[ofs++] = TriID;
		ubuffer[ofs++] = parent;
		ubuffer[ofs++] = padding;
		ubuffer[ofs++] = padding;
		// 16 bytes

		fbuffer[ofs++] = v0.x;
		fbuffer[ofs++] = v0.y;
		fbuffer[ofs++] = v0.z;
		fbuffer[ofs++] = 1.0f;
		// 32 bytes
		fbuffer[ofs++] = v1.x;
		fbuffer[ofs++] = v1.y;
		fbuffer[ofs++] = v1.z;
		fbuffer[ofs++] = 1.0f;
		// 48 bytes
		fbuffer[ofs++] = v2.x;
		fbuffer[ofs++] = v2.y;
		fbuffer[ofs++] = v2.z;
		fbuffer[ofs++] = 1.0f;
		// 64 bytes
	}
	else
	{
		ubuffer[ofs++] = TriID;
		ubuffer[ofs++] = parent;
		ubuffer[ofs++] = padding;
		ubuffer[ofs++] = padding;
		// 16 bytes
		fbuffer[ofs++] = box.min.x;
		fbuffer[ofs++] = box.min.y;
		fbuffer[ofs++] = box.min.z;
		fbuffer[ofs++] = 1.0f;
		// 32 bytes
		fbuffer[ofs++] = box.max.x;
		fbuffer[ofs++] = box.max.y;
		fbuffer[ofs++] = box.max.z;
		fbuffer[ofs++] = 1.0f;
		// 48 bytes
		for (int i = 0; i < 4; i++)
			ubuffer[ofs++] = children[i];
		// 64 bytes
	}

	/*
	if (ofs - startOfs == ENCODED_TREE_NODE4_SIZE)
	{
		log_debug("[DBG] [BVH] TreeNode should be encoded in %d bytes, but got %d instead",
			ENCODED_TREE_NODE4_SIZE, ofs - startOfs);
	}
	*/
	return ofs;
}

// Returns a compact buffer containing BVHNode entries that represent the given tree
// The current ray-tracer uses embedded geometry.
uint8_t* EncodeNodes(IGenericTree* root, const XwaVector3* Vertices, const int* Indices)
{
	uint8_t* result = nullptr;
	if (root == nullptr)
		return result;

	int NumNodes = root->GetNumNodes();
	result = new uint8_t[NumNodes * ENCODED_TREE_NODE4_SIZE];
	int startOfs = 0;
	uint32_t arity = root->GetArity();
	//log_debug("[DBG] [BVH] Encoding %d BVH nodes", NumNodes);

	// A breadth-first traversal will ensure that each level of the tree is encoded to the
	// buffer before advancing to the next level. We can thus keep track of the offset in
	// the buffer where the next node will appear.
	std::queue<IGenericTree*> Q;

	// Initialize the queue and the offsets.
	Q.push(root);
	// Since we're going to put this data in an array, it's easier to specify the children
	// offsets as indices into this array.
	int nextNode = 1;
	std::vector<int> childOfs;

	while (Q.size() != 0)
	{
		IGenericTree* T = Q.front();
		Q.pop();
		std::vector<IGenericTree*> children = T->GetChildren();

		// In a breadth-first search, the left child will always be at offset nextNode
		// Add the children offsets
		childOfs.clear();
		for (uint32_t i = 0; i < arity; i++)
			childOfs.push_back(i < children.size() ? nextNode + i : -1);

		startOfs = EncodeTreeNode4(result, startOfs, T, -1 /* parent (TODO) */,
			childOfs, Vertices, Indices);

		// Enqueue the children
		for (const auto& child : children)
		{
			Q.push(child);
			nextNode++;
		}
	}

	return result;
}

// Returns a compact buffer containing BVHNode entries that represent the given tree
// The tree is expected to be of arity 4.
// The current ray-tracer uses embedded geometry.
uint8_t* EncodeNodes(int root, InnerNode4* innerNodes, const std::vector<LeafItem>& leafItems,
	const XwaVector3* Vertices, const int* Indices)
{
	using Item = std::pair<int, bool>;
	uint8_t* result = nullptr;
	if (root == -1)
		return result;

	int NumNodes = innerNodes[root].totalNodes;
	result = new uint8_t[NumNodes * ENCODED_TREE_NODE4_SIZE];
	int startOfs = 0;
	uint32_t arity = 4; // Yeah, hard-coded, this will _definitely_ come back and bite me in the ass later, but whatever.
	//log_debug("[DBG] [BVH] Encoding %d QBVH nodes", NumNodes);

	// A breadth-first traversal will ensure that each level of the tree is encoded to the
	// buffer before advancing to the next level. We can thus keep track of the offset in
	// the buffer where the next node will appear.
	std::queue<Item> Q;

	// Initialize the queue and the offsets.
	Q.push(Item(root, false));
	// Since we're going to put this data in an array, it's easier to specify the children
	// offsets as indices into this array.
	int nextNode = 1;
	std::vector<int> childOfs;

	while (Q.size() != 0)
	{
		Item curItem = Q.front();
		int curNode = curItem.first;
		bool isLeaf = curItem.second;
		Q.pop();
		int* children = isLeaf ? nullptr : innerNodes[curNode].children;
		bool* isLeafArray = isLeaf ? nullptr : innerNodes[curNode].isLeaf;
		uint32_t numChildren = isLeaf ? 0 : innerNodes[curNode].numChildren;

		// In a breadth-first search, the left child will always be at offset nextNode
		// Add the children offsets
		childOfs.clear();
		for (uint32_t i = 0; i < arity; i++)
			childOfs.push_back(i < numChildren ? nextNode + i : -1);

		startOfs = EncodeTreeNode4(result, startOfs,
			curNode, innerNodes,
			isLeaf, leafItems,
			-1 /* parent (TODO) */, childOfs,
			Vertices, Indices);

		// Enqueue the children
		for (uint32_t i = 0; i < numChildren; i++)
		{
			Q.push(Item(children[i], isLeafArray[i]));
			nextNode++;
		}
	}

	return result;
}

// Converts a BVH2 into an encoded QBVH and returns a compact buffer
// The current ray-tracer uses embedded geometry.
BVHNode* EncodeNodesAsQBVH(int root, InnerNode* innerNodes, const std::vector<LeafItem>& leafItems,
	const XwaVector3* Vertices, const int* Indices, int &numQBVHNodes_out)
{
	using Item = std::pair<int, bool>;
	BVHNode* result = nullptr;
	if (root == -1)
		return result;

	int numPrimitives = leafItems.size();
	numQBVHNodes_out = numPrimitives + CalcNumInnerQBVHNodes(numPrimitives);
	result = new BVHNode[numQBVHNodes_out];
	// Initialize the root
	result[0].padding[0] = 0;

	// A breadth-first traversal will ensure that each level of the tree is encoded to the
	// buffer before advancing to the next level. We can thus keep track of the offset in
	// the buffer where the next node will appear.
	std::queue<Item> Q;

	// Initialize the queue and the offsets.
	Q.push(Item(root, false));
	// Since we're going to put this data in an array, it's easier to specify the children
	// offsets as indices into this array.
	int nextNode = 1;
	int EncodeIdx = 0;

	while (Q.size() != 0)
	{
		Item curItem = Q.front();
		int curNode = curItem.first;
		bool isLeaf = curItem.second;
		Q.pop();
		//int* children = isLeaf ? nullptr : innerNodes[curNode].children;
		//bool* isLeafArray = isLeaf ? nullptr : innerNodes[curNode].isLeaf;
		//uint32_t numChildren = isLeaf ? 0 : innerNodes[curNode].numChildren;
		BVHNode node;
		int nextchild = 0;

		if (!isLeaf)
		{
			// Initialize the node
			node.ref    = -1;
			node.parent  = -1;

			node.min[0] = innerNodes[curNode].aabb.min.x;
			node.min[1] = innerNodes[curNode].aabb.min.y;
			node.min[2] = innerNodes[curNode].aabb.min.z;

			node.max[0] = innerNodes[curNode].aabb.max.x;
			node.max[1] = innerNodes[curNode].aabb.max.y;
			node.max[2] = innerNodes[curNode].aabb.max.z;

			for (int i = 0; i < 4; i++)
				node.children[i] = -1;

			// Pull-up the grandchildren, if possible
			if (innerNodes[curNode].leftIsLeaf)
			{
				node.children[nextchild++] = nextNode++;
				Q.push(Item(innerNodes[curNode].left, true));
			}
			else
			{
				int c0 = innerNodes[curNode].left;

				node.children[nextchild++] = nextNode++;
				Q.push(Item(innerNodes[c0].left, innerNodes[c0].leftIsLeaf));
				node.children[nextchild++] = nextNode++;
				Q.push(Item(innerNodes[c0].right, innerNodes[c0].rightIsLeaf));
			}

			if (innerNodes[curNode].rightIsLeaf)
			{
				node.children[nextchild++] = nextNode++;
				Q.push(Item(innerNodes[curNode].right, true));
			}
			else
			{
				int c1 = innerNodes[curNode].right;

				node.children[nextchild++] = nextNode++;
				Q.push(Item(innerNodes[c1].left, innerNodes[c1].leftIsLeaf));
				node.children[nextchild++] = nextNode++;
				Q.push(Item(innerNodes[c1].right, innerNodes[c1].rightIsLeaf));
			}

			// Encode the inner node
			EncodeInnerNode(result, &node, EncodeIdx++);
		}
		else
		{
			// Encode a leaf node
			EncodeLeafNode(result, leafItems, curNode, EncodeIdx++, Vertices, Indices);
		}
	}

	return result;
}

TreeNode* RotLeft(TreeNode* T)
{
	if (T == nullptr) return nullptr;

	TreeNode* L = T->left;
	TreeNode* R = T->right;
	TreeNode* RL = T->right->left;
	TreeNode* RR = T->right->right;
	R->right = RR;
	R->left = T;
	T->left = L;
	T->right = RL;

	return R;
}

TreeNode* RotRight(TreeNode* T)
{
	if (T == nullptr) return nullptr;

	TreeNode* L = T->left;
	TreeNode* R = T->right;
	TreeNode* LL = T->left->left;
	TreeNode* LR = T->left->right;
	L->left = LL;
	L->right = T;
	T->left = LR;
	T->right = R;

	return L;
}

// Red-Black balanced insertion
TreeNode* insertRB(TreeNode* T, int TriID, MortonCode_t code, const AABB &box)
{
	if (T == nullptr)
	{
		return new TreeNode(TriID, code, box);
	}

	if (code <= T->code)
	{
		T->left = insertRB(T->left, TriID, code, box);
		// Rebalance
		if (T->left->left != nullptr && T->left->red && T->left->left->red)
		{
			T = RotRight(T);
			T->left->red = false;
			T->right->red = false;
		}
		else if (T->left->right != nullptr && T->left->red && T->left->right->red)
		{
			T->left = RotLeft(T->left);
			T = RotRight(T);
			T->left->red = false;
			T->right->red = false;
		}
	}
	else
	{
		T->right = insertRB(T->right, TriID, code, box);
		// Rebalance
		if (T->right->right != nullptr && T->right->red && T->right->right->red)
		{
			T = RotLeft(T);
			T->left->red = false;
			T->right->red = false;
		}
		else if (T->right->left != nullptr && T->right->red && T->right->left->red)
		{
			T->right = RotRight(T->right);
			T = RotLeft(T);
			T->left->red = false;
			T->right->red = false;
		}
	}

	return T;
}

LBVH* LBVH::Build(const XwaVector3* vertices, const int numVertices, const int *indices, const int numIndices)
{
	// Get the scene limits
	AABB sceneBox;
	XwaVector3 range;
	for (int i = 0; i < numVertices; i++)
		sceneBox.Expand(vertices[i]);
	range.x = sceneBox.max.x - sceneBox.min.x;
	range.y = sceneBox.max.y - sceneBox.min.y;
	range.z = sceneBox.max.z - sceneBox.min.z;

	int numTris = numIndices / 3;
	/*
	log_debug("[DBG] [BVH] numVertices: %d, numIndices: %d, numTris: %d, scene: (%0.3f, %0.3f, %0.3f)-(%0.3f, %0.3f, %0.3f)",
		numVertices, numIndices, numTris,
		sceneBox.min.x, sceneBox.min.y, sceneBox.min.z,
		sceneBox.max.x, sceneBox.max.y, sceneBox.max.z);
	*/

	// Get the Morton Code and AABB for each triangle.
	std::vector<LeafItem> leafItems;
	for (int i = 0, TriID = 0; i < numIndices; i += 3, TriID++) {
		AABB aabb;
		aabb.Expand(vertices[indices[i + 0]]);
		aabb.Expand(vertices[indices[i + 1]]);
		aabb.Expand(vertices[indices[i + 2]]);

		XwaVector3 centroid = aabb.GetCentroid();
		Normalize(centroid, sceneBox, range);
		MortonCode_t m = GetMortonCode32(centroid);
		leafItems.push_back(std::make_tuple(m, aabb, TriID));
	}

	// Sort the morton codes
	std::sort(leafItems.begin(), leafItems.end(), leafSorter);

	// Build the tree
	int root = -1;
	InnerNode* innerNodes = FastLBVH(leafItems, &root);
	//log_debug("[DBG] [BVH] FastLBVH finished. Tree built. root: %d", root);

	//char sFileName[80];
	//sprintf_s(sFileName, 80, ".\\BLAS-%d.obj", meshIndex);
	//DumpInnerNodesToOBJ(sFileName, root, innerNodes, leafItems, vertices, indices);

	// Convert to QBVH
	//QTreeNode *Q = BinTreeToQTree(root, leafItems.size() == 1, innerNodes, leafItems);
	//delete[] innerNodes;
	// Encode the QBVH in a buffer
	//void *buffer = EncodeNodes(Q, vertices, indices);

	int numNodes = 0;
	BVHNode *buffer = EncodeNodesAsQBVH(root, innerNodes, leafItems, vertices, indices, numNodes);
	delete[] innerNodes;

	LBVH *lbvh = new LBVH();
	lbvh->nodes = (BVHNode *)buffer;
	lbvh->numVertices = numVertices;
	lbvh->numIndices = numIndices;
	//lbvh->numNodes = Q->numNodes;
	lbvh->numNodes = numNodes;
	lbvh->scale = 1.0f;
	lbvh->scaleComputed = true;
	// DEBUG
	//log_debug("[DBG} [BVH] Dumping file: %s", sFileName);
	//lbvh->DumpToOBJ(sFileName);
	//log_debug("[DBG] [BVH] BLAS dumped");
	// DEBUG

	// Tidy up
	//DeleteTree(Q);
	return lbvh;
}

LBVH* LBVH::BuildQBVH(const XwaVector3* vertices, const int numVertices, const int* indices, const int numIndices)
{
	// Get the scene limits
	AABB sceneBox;
	XwaVector3 range;
	for (int i = 0; i < numVertices; i++)
		sceneBox.Expand(vertices[i]);
	range.x = sceneBox.max.x - sceneBox.min.x;
	range.y = sceneBox.max.y - sceneBox.min.y;
	range.z = sceneBox.max.z - sceneBox.min.z;

	int numTris = numIndices / 3;
	//log_debug("[DBG] [BVH] numVertices: %d, numIndices: %d, numTris: %d, scene: (%0.3f, %0.3f, %0.3f)-(%0.3f, %0.3f, %0.3f)",
	//	numVertices, numIndices, numTris,
	//	sceneBox.min.x, sceneBox.min.y, sceneBox.min.z,
	//	sceneBox.max.x, sceneBox.max.y, sceneBox.max.z);

	// Get the Morton Code and AABB for each triangle.
	std::vector<LeafItem> leafItems;
	for (int i = 0, TriID = 0; i < numIndices; i += 3, TriID++) {
		AABB aabb;
		aabb.Expand(vertices[indices[i + 0]]);
		aabb.Expand(vertices[indices[i + 1]]);
		aabb.Expand(vertices[indices[i + 2]]);

		XwaVector3 centroid = aabb.GetCentroid();
		Normalize(centroid, sceneBox, range);
		MortonCode_t m = GetMortonCode32(centroid);
		leafItems.push_back(std::make_tuple(m, aabb, TriID));
	}

	// Sort the morton codes
	std::sort(leafItems.begin(), leafItems.end(), leafSorter);

	// Build the tree
	int root = -1;
	InnerNode4* innerNodes = FastLQBVH(leafItems, root);
	int totalNodes = innerNodes[root].totalNodes;
	//log_debug("[DBG] [BVH] FastLQBVH* finished. QTree built. root: %d, totalNodes: %d", root, totalNodes);
	AABB scene = innerNodes[root].aabb;
	//log_debug("[DBG] [BVH] scene size from the QTree: (%0.3f, %0.3f, %0.3f)-(%0.3f, %0.3f, %0.3f)",
	//	scene.min.x, scene.min.y, scene.min.z,
	//	scene.max.x, scene.max.y, scene.max.z);

	// Encode the QBVH in a buffer
	BVHNode *buffer = (BVHNode * )EncodeNodes(root, innerNodes, leafItems, vertices, indices);
	delete[] innerNodes;
	// Initialize the root
	buffer[0].padding[0] = 0;

	LBVH* lbvh = new LBVH();
	lbvh->nodes = (BVHNode*)buffer;
	lbvh->numVertices = numVertices;
	lbvh->numIndices = numIndices;
	lbvh->numNodes = totalNodes;
	lbvh->scale = 1.0f;
	lbvh->scaleComputed = true;
	// DEBUG
	//log_debug("[DBG} [BVH] Dumping file: %s", sFileName);
	//lbvh->DumpToOBJ(sFileName);
	//log_debug("[DBG] [BVH] BLAS dumped");
	// DEBUG

	// Tidy up
	//delete[] buffer; // We can't delete the buffer here, lbvh->nodes now owns it
	return lbvh;
}

LBVH* LBVH::BuildFastQBVH(const XwaVector3* vertices, const int numVertices, const int* indices, const int numIndices)
{
	// Get the scene limits
	AABB sceneBox;
	XwaVector3 range;
	for (int i = 0; i < numVertices; i++)
		sceneBox.Expand(vertices[i]);
	range.x = sceneBox.max.x - sceneBox.min.x;
	range.y = sceneBox.max.y - sceneBox.min.y;
	range.z = sceneBox.max.z - sceneBox.min.z;

	int numTris = numIndices / 3;
	int numQBVHInnerNodes = CalcNumInnerQBVHNodes(numTris);
	int numQBVHNodes = numTris + numQBVHInnerNodes;
	/*
	log_debug("[DBG] [BVH] numVertices: %d, numIndices: %d, numTris: %d, numQBVHInnerNodes: %d, numQBVHNodes: %d, "
		"scene: (%0.3f, %0.3f, %0.3f)-(%0.3f, %0.3f, %0.3f)",
		numVertices, numIndices, numTris, numQBVHInnerNodes, numQBVHNodes,
		sceneBox.min.x, sceneBox.min.y, sceneBox.min.z,
		sceneBox.max.x, sceneBox.max.y, sceneBox.max.z);
	*/

	// We can reserve the buffer for the QBVH now.
	BVHNode* QBVHBuffer = new BVHNode[numQBVHNodes];
	//log_debug("[DBG] [BVH] sizeof(BVHNode): %d", sizeof(BVHNode));

	// Get the Morton Code and AABB for each triangle.
	std::vector<LeafItem> leafItems;
	for (int i = 0, TriID = 0; i < numIndices; i += 3, TriID++) {
		AABB aabb;
		XwaVector3 v0 = vertices[indices[i + 0]];
		XwaVector3 v1 = vertices[indices[i + 1]];
		XwaVector3 v2 = vertices[indices[i + 2]];
		aabb.Expand(v0);
		aabb.Expand(v1);
		aabb.Expand(v2);

		XwaVector3 centroid = aabb.GetCentroid();
		Normalize(centroid, sceneBox, range);
		MortonCode_t m = GetMortonCode32(centroid);
		leafItems.push_back(std::make_tuple(m, aabb, TriID));
	}

	// Sort the morton codes
	std::sort(leafItems.begin(), leafItems.end(), leafSorter);

	// Encode the sorted leaves
	// TODO: Encode the leaves before sorting, and use TriID as the sort index.
	//int LeafOfs = numQBVHInnerNodes * sizeof(BVHNode) / 4;
	int LeafEncodeIdx = numQBVHInnerNodes;
	for (unsigned int i = 0; i < leafItems.size(); i++)
	{
		EncodeLeafNode(QBVHBuffer, leafItems, i, LeafEncodeIdx++, vertices, indices);
	}

	// Build, convert and encode the QBVH
	int root = -1;
	FastLQBVH(QBVHBuffer, numQBVHInnerNodes, leafItems, root);

	// Initialize the root
	//QBVHBuffer[0].padding[0] = 0;
	//int totalNodes = numQBVHNodes - root;

	// Initialize the root
	QBVHBuffer[0].padding[0] = root;
	int totalNodes = numQBVHNodes;

	//log_debug("[DBG] [BVH] FastLQBVH** finished. QTree built. root: %d, numQBVHNodes: %d, totalNodes: %d",
	//	root, numQBVHNodes, totalNodes);
	/*
	AABB scene;
	scene.min.x = QBVHBuffer[root].min[0];
	scene.min.y = QBVHBuffer[root].min[1];
	scene.min.z = QBVHBuffer[root].min[2];
	scene.max.x = QBVHBuffer[root].max[0];
	scene.max.y = QBVHBuffer[root].max[1];
	scene.max.z = QBVHBuffer[root].max[2];
	log_debug("[DBG] [BVH] scene size from the QTree: (%0.3f, %0.3f, %0.3f)-(%0.3f, %0.3f, %0.3f)",
		scene.min.x, scene.min.y, scene.min.z,
		scene.max.x, scene.max.y, scene.max.z);
	*/
	//log_debug("[DBG] [BVH] ****************************************************************");
	//log_debug("[DBG] [BVH] Printing Tree");
	//printTree(0, inner_root, false, innerNodes);
	//delete[] innerNodes;

	//log_debug("[DBG] [BVH] ****************************************************************");
	//log_debug("[DBG] [BVH] Printing Buffer");
	//PrintTreeBuffer("", QBVHBuffer, root);

	// Re-label all the offsets so that the root is at offset 0. This makes the tree
	// compatible with the current raytracer, but we don't actually need to do this.
	// The alternative is to alter the raytracer so that it can start traversal on
	// a nonzero root index. Let's do that later, though.
	/*
	for (int i = root; i < numQBVHNodes; i++) {
		// This change only applies to inner nodes
		if (QBVHBuffer[i].ref == -1) {
			for (int j = 0; j < 4; j++) {
				if (QBVHBuffer[i].children[j] == -1)
					break;
				QBVHBuffer[i].children[j] -= root;
			}
		}
	}
	*/

	//log_debug("[DBG] [BVH] ****************************************************************");
	//log_debug("[DBG] [BVH] Printing Buffer");
	//PrintTreeBuffer("", QBVHBuffer + root, 0);
	//PrintTreeBuffer("", QBVHBuffer, root);

	LBVH* lbvh = new LBVH();
	//lbvh->rawBuffer = QBVHBuffer;
	//lbvh->nodes = QBVHBuffer + root;
	lbvh->nodes = QBVHBuffer;
	lbvh->numVertices = numVertices;
	lbvh->numIndices = numIndices;
	lbvh->numNodes = totalNodes;
	lbvh->scale = 1.0f;
	lbvh->scaleComputed = true;
	// DEBUG
	//log_debug("[DBG} [BVH] Dumping file: %s", sFileName);
	//lbvh->DumpToOBJ(sFileName);
	//log_debug("[DBG] [BVH] BLAS dumped");
	// DEBUG

	// Tidy up
	//delete[] buffer; // We can't delete the buffer here, lbvh->nodes now owns it
	return lbvh;
}

void DeleteTree(TreeNode* T)
{
	if (T == nullptr) return;

	DeleteTree(T->left);
	DeleteTree(T->right);
	delete T;
}

void DeleteTree(QTreeNode* Q)
{
	if (Q == nullptr)
		return;

	for (int i = 0; i < 4; i++)
		DeleteTree(Q->children[i]);

	delete Q;
}

QTreeNode *BinTreeToQTree(int curNode, bool curNodeIsLeaf, const InnerNode* innerNodes, const std::vector<LeafItem> &leafItems)
{
	QTreeNode *children[] = {nullptr, nullptr, nullptr, nullptr};
	if (curNode == -1) {
		return nullptr;
	}

	if (curNodeIsLeaf) {
		return new QTreeNode(std::get<2>(leafItems[curNode]) /* TriID */, std::get<1>(leafItems[curNode]) /* box */);
	}

	int left = innerNodes[curNode].left;
	int right = innerNodes[curNode].right;
	int nextchild = 0;
	int nodeCounter = 0;

	// if (left != -1) // All inner nodes in the Fast LBVH have 2 children
	{
		if (innerNodes[curNode].leftIsLeaf)
		{
			children[nextchild++] = BinTreeToQTree(left, true, innerNodes, leafItems);
		}
		else
		{
			children[nextchild++] = BinTreeToQTree(innerNodes[left].left, innerNodes[left].leftIsLeaf, innerNodes, leafItems);
			children[nextchild++] = BinTreeToQTree(innerNodes[left].right, innerNodes[left].rightIsLeaf, innerNodes, leafItems);
		}
	}

	// if (right != -1) // All inner nodes in the Fast LBVH have 2 children
	{
		if (innerNodes[curNode].rightIsLeaf)
		{
			children[nextchild++] = BinTreeToQTree(right, true, innerNodes, leafItems);
		}
		else
		{
			children[nextchild++] = BinTreeToQTree(innerNodes[right].left, innerNodes[right].leftIsLeaf, innerNodes, leafItems);
			children[nextchild++] = BinTreeToQTree(innerNodes[right].right, innerNodes[right].rightIsLeaf, innerNodes, leafItems);
		}
	}

	// Compute the AABB for this node
	AABB box;
	for (int i = 0; i < nextchild; i++)
		box.Expand(children[i]->box);

	return new QTreeNode(-1, box, children, nullptr);
}

void TestFastLBVH()
{
	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] TestFastLBVH() START");
	std::vector<LeafItem> leafItems;
	AABB aabb;
	// This is the example from Apetrei 2014.
	// Here the TriID is the same as the morton code for debugging purposes.
	leafItems.push_back(std::make_tuple(4, aabb, 4));
	leafItems.push_back(std::make_tuple(12, aabb, 12));
	leafItems.push_back(std::make_tuple(3, aabb, 3));
	leafItems.push_back(std::make_tuple(13, aabb, 13));
	leafItems.push_back(std::make_tuple(5, aabb, 5));
	leafItems.push_back(std::make_tuple(2, aabb, 2));
	leafItems.push_back(std::make_tuple(15, aabb, 15));
	leafItems.push_back(std::make_tuple(8, aabb, 8));

	// Sort by the morton codes
	std::sort(leafItems.begin(), leafItems.end(), leafSorter);

	int root = -1;
	InnerNode* innerNodes = FastLBVH(leafItems, &root);

	log_debug("[DBG] [BVH] ****************************************************************");
	int numLeaves = leafItems.size();
	int numInnerNodes = numLeaves - 1;
	for (int i = 0; i < numInnerNodes; i++)
	{
		log_debug("[DBG] [BVH] node: %d, left,right: %s%d, %s%d",
			i,
			innerNodes[i].leftIsLeaf ? "(L)" : "", innerNodes[i].left,
			innerNodes[i].rightIsLeaf ? "(L)" : "", innerNodes[i].right);
	}

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] Printing Tree");
	printTree(0, root, false, innerNodes);

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] BVH2 --> QBVH conversion");
	QTreeNode* Q = BinTreeToQTree(root, false, innerNodes, leafItems);
	delete[] innerNodes;

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] Printing QTree");
	PrintTree("", Q);
	DeleteTree(Q);

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] TestFastLBVH() END");
}

void TestFastLQBVH()
{
	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] TestFastLQBVH() START");
	std::vector<LeafItem> leafItems;
	AABB aabb;
	// This is the example from Apetrei 2014.
	// Here the TriID is the same as the morton code for debugging purposes.
	leafItems.push_back(std::make_tuple( 4, aabb, 0));
	leafItems.push_back(std::make_tuple(12, aabb, 1));
	leafItems.push_back(std::make_tuple( 3, aabb, 2));
	leafItems.push_back(std::make_tuple(13, aabb, 3));
	leafItems.push_back(std::make_tuple( 5, aabb, 4));
	leafItems.push_back(std::make_tuple( 2, aabb, 5));
	leafItems.push_back(std::make_tuple(15, aabb, 6));
	leafItems.push_back(std::make_tuple( 8, aabb, 7));

	// Sort by the morton codes
	std::sort(leafItems.begin(), leafItems.end(), leafSorter);

	int root = -1;
	InnerNode4* innerNodes = FastLQBVH(leafItems, root);
	int totalNodes = innerNodes[root].totalNodes;

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] root: %d, totalNodes: %d", root, totalNodes);
	int numLeaves = leafItems.size();
	int numInnerNodes = numLeaves - 1;
	for (int i = 0; i < numInnerNodes; i++)
	{
		std::string msg = "";
		for (int j = 0; j < innerNodes[i].numChildren; j++) {
			msg += (innerNodes[i].isLeaf[j] ? "(L)" : "") +
				   std::to_string(innerNodes[i].children[j]) + ", ";
		}

		log_debug("[DBG] [BVH] node: %d, numChildren: %d| %s",
			i, innerNodes[i].numChildren, msg.c_str());
	}

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] Printing Tree");
	printTree(0, root, false, innerNodes);

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] Encoding Buffer");
	BVHNode* buffer = (BVHNode *)EncodeNodes(root, innerNodes, leafItems, nullptr, nullptr);
	log_debug("[DBG] [BVH] Printing Buffer");
	PrintTreeBuffer("", buffer, 0);
	delete[] buffer;
	delete[] innerNodes;

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] TestFastLQBVH() END");
}

void TestFastLQBVHEncode()
{
	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] TestFastLQBVHEncode() START");
	std::vector<LeafItem> leafItems;
	AABB aabb;
	// This is the example from Apetrei 2014.
	// Here the TriID is the same as the morton code for debugging purposes.
	leafItems.push_back(std::make_tuple( 4, aabb, 0));
	leafItems.push_back(std::make_tuple(12, aabb, 1));
	leafItems.push_back(std::make_tuple( 3, aabb, 2));
	leafItems.push_back(std::make_tuple(13, aabb, 3));
	leafItems.push_back(std::make_tuple( 5, aabb, 4));
	leafItems.push_back(std::make_tuple( 2, aabb, 5));
	leafItems.push_back(std::make_tuple(15, aabb, 6));
	leafItems.push_back(std::make_tuple( 8, aabb, 7));

	// Sort by the morton codes
	std::sort(leafItems.begin(), leafItems.end(), leafSorter);

	int numTris = leafItems.size();
	int numQBVHInnerNodes = CalcNumInnerQBVHNodes(numTris);
	int numQBVHNodes = numTris + numQBVHInnerNodes;
	BVHNode* QBVHBuffer = new BVHNode[numQBVHNodes];

	log_debug("[DBG] [BVH] numTris: %d, numQBVHInnerNodes: %d, numQBVHNodes: %d",
		numTris, numQBVHInnerNodes, numQBVHNodes);

	// Encode the leaves
	//int LeafOfs = numQBVHInnerNodes * sizeof(BVHNode) / 4;
	int LeafEncodeIdx = numQBVHInnerNodes;
	for (unsigned int i = 0; i < leafItems.size(); i++)
	{
		EncodeLeafNode(QBVHBuffer, leafItems, i, LeafEncodeIdx++, nullptr, nullptr);
	}

	int root = -1, inner_root = -1;
	FastLQBVH(QBVHBuffer, numQBVHInnerNodes, leafItems, root);
	int totalNodes = numQBVHNodes - root;

	log_debug("[DBG] [BVH] root: %d, totalNodes: %d", root, totalNodes);
	log_debug("[DBG] [BVH] ****************************************************************");
	/*
	int numLeaves = leafItems.size();
	int numInnerNodes = numLeaves - 1;
	for (int i = 0; i < numInnerNodes; i++)
	{
		std::string msg = "";
		for (int j = 0; j < innerNodes[i].numChildren; j++) {
			msg += (innerNodes[i].isLeaf[j] ? "(L)" : "") +
				std::to_string(innerNodes[i].children[j]) + ", ";
		}

		log_debug("[DBG] [BVH] node: %d, numChildren: %d| %s",
			i, innerNodes[i].numChildren, msg.c_str());
	}
	*/

	//log_debug("[DBG] [BVH] ****************************************************************");
	//log_debug("[DBG] [BVH] Printing Tree");
	//printTree(0, inner_root, false, innerNodes);
	//delete[] innerNodes;

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] Printing Buffer");
	PrintTreeBuffer("", QBVHBuffer, root);
	delete[] QBVHBuffer;
	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] TestFastLQBVHEncode() END");
}

void TestRedBlackBVH()
{
	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] TestRedBlackBVH() START");

	AABB box;
	TreeNode* T = nullptr;

	/*
	T = insertRB(T, 0, 0, box);
	T = insertRB(T, 1, 1, box);
	T = insertRB(T, 2, 2, box);
	T = insertRB(T, 3, 3, box);
	T = insertRB(T, 4, 4, box);
	T = insertRB(T, 5, 5, box);
	T = insertRB(T, 6, 6, box);
	T = insertRB(T, 7, 7, box);
	T = insertRB(T, 8, 8, box);
	T = insertRB(T, 9, 9, box);
	T = insertRB(T, 10, 10, box);
	T = insertRB(T, 11, 11, box);
	T = insertRB(T, 12, 12, box);
	*/

	T = insertRB(T, 4, 4, box);
	T = insertRB(T, 12, 12, box);
	T = insertRB(T, 3, 3, box);
	T = insertRB(T, 13, 13, box);
	T = insertRB(T, 5, 5, box);
	T = insertRB(T, 2, 2, box);
	T = insertRB(T, 15, 15, box);
	T = insertRB(T, 8, 8, box);

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] Printing Tree");
	PrintTree("", T);
	DeleteTree(T);

	log_debug("[DBG] [BVH] ****************************************************************");
	log_debug("[DBG] [BVH] TestRedBlackBVH() END");
}
