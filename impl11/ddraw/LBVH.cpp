#include "common.h"
#include "globals.h"
#include "LBVH.h"
#include <stdio.h>
#include <queue>
#include <algorithm>

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
				if (*root == -1)
					break;
			}
		}
	}
	//log_debug("[DBG] [BVH] root at index: %d", *root);
	return innerNodes;
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

static void PrintTree(std::string level, IGenericTree *T)
{
	if (T == nullptr)
		return;

	int arity = T->GetArity();
	std::vector<IGenericTree *> children = T->GetChildren();
	bool isLeaf = children.size() == 0;

	if (!isLeaf) log_debug("[DBG] [BVH] %s", (level + "   /--\\").c_str());
	for (int i = arity - 1; i >= arity / 2; i--)
		if (i < (int)children.size())
			PrintTree(level + "    ", children[i]);

	log_debug("[DBG] [BVH] %s%s",
		(level + std::to_string(T->GetTriID())).c_str(), isLeaf ? "]" : "");

	for (int i = arity / 2 - 1; i >= 0; i--)
		if (i < (int)children.size())
			PrintTree(level + "    ", children[i]);
	if (!isLeaf) log_debug("[DBG] [BVH] %s", (level + "   \\--/").c_str());
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

LBVH* LBVH::Build(const XwaVector3* vertices, const int numVertices, const int *indices, const int numIndices, const int meshIndex)
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
	log_debug("[DBG] [BVH] numVertices: %d, numIndices: %d, numTris: %d, scene: (%0.3f, %0.3f, %0.3f)-(%0.3f, %0.3f, %0.3f)",
		numVertices, numIndices, numTris,
		sceneBox.min.x, sceneBox.min.y, sceneBox.min.z,
		sceneBox.max.x, sceneBox.max.y, sceneBox.max.z);

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
	log_debug("[DBG] [BVH] FastLBVH finished. Tree built. root: %d", root);

	//char sFileName[80];
	//sprintf_s(sFileName, 80, ".\\BLAS-%d.obj", meshIndex);
	//DumpInnerNodesToOBJ(sFileName, root, innerNodes, leafItems, vertices, indices);

	// Convert to QBVH
	QTreeNode *Q = BinTreeToQTree(root, leafItems.size() == 1, innerNodes, leafItems);
	delete[] innerNodes;

	// Encode the QBVH in a buffer
	void *buffer = EncodeNodes(Q, vertices, indices);

	LBVH *lbvh = new LBVH();
	lbvh->nodes = (BVHNode *)buffer;
	lbvh->numVertices = numVertices;
	lbvh->numIndices = numIndices;
	lbvh->numNodes = Q->numNodes;
	// DEBUG
	//log_debug("[DBG} [BVH] Dumping file: %s", sFileName);
	//lbvh->DumpToOBJ(sFileName);
	//log_debug("[DBG] [BVH] BLAS dumped");
	// DEBUG

	// Tidy up
	DeleteTree(Q);
	//delete[] buffer;
	return lbvh;
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

// Encode a BVH4 node using Embedded Geometry.
// Returns the new offset (in multiples of 4 bytes) that can be written to.
static int EncodeTreeNode4(void *buffer, int startOfs, IGenericTree *T, int32_t parent, const std::vector<int> &children,
	const XwaVector3* Vertices, const int *Indices)
{
	AABB box = T->GetBox();
	int TriID = T->GetTriID();
	int padding = 0;
	int ofs = startOfs;
	uint32_t* ubuffer = (uint32_t *)buffer;
	float* fbuffer = (float *)buffer;

	// This leaf node must have its vertices embedded in the node
	if (TriID != -1)
	{
		int vertofs = TriID * 3;
		XwaVector3 v0 = Vertices[Indices[vertofs]];
		XwaVector3 v1 = Vertices[Indices[vertofs + 1]];
		XwaVector3 v2 = Vertices[Indices[vertofs + 2]];

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
// The tree is expected to be of arity 4.
// The current ray-tracer uses embedded geometry.
uint8_t *EncodeNodes(IGenericTree *root, const XwaVector3* Vertices, const int* Indices)
{
	uint8_t* result = nullptr;
	if (root == nullptr)
		return result;

	int NumNodes = root->GetNumNodes();
	result = new uint8_t[NumNodes * ENCODED_TREE_NODE4_SIZE];
	int startOfs = 0;
	uint32_t arity = root->GetArity();
	log_debug("[DBG] [BVH] Encoding %d BVH nodes", NumNodes);

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
		for (const auto &child : children)
		{
			Q.push(child);
			nextNode++;
		}
	}

	return result;
}
