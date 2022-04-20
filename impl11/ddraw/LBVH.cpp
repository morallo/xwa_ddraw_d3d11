#include "common.h"
#include "LBVH.h"
#include <stdio.h>

LBVH *LBVH::LoadLBVH(char *sFileName) {
	FILE *file;
	errno_t error = 0;
	LBVH *lbvh = nullptr;

	try {
		error = fopen_s(&file, sFileName, "rb");
	}
	catch (...) {
		log_debug("[DBG] Could not load [%s]", sFileName);
		return lbvh;
	}

	if (error != 0) {
		log_debug("[DBG] Error %d when loading [%s]", error, sFileName);
		return lbvh;
	}

	// Read the Magic Word and the version
	{
		char magic[9];
		fread(magic, 1, 8, file);
		magic[8] = 0;
		if (strcmp(magic, "BVH2-1.0") != 0)
		{
			log_debug("[DBG] Unknown BVH version. Got: [%s]", magic);
			return lbvh;
		}
	}

	lbvh = new LBVH();

	// Read the vertices
	{
		int32_t NumVertices = 0;
		fread(&NumVertices, sizeof(int32_t), 1, file);
		lbvh->vertices = new float3[NumVertices];
		lbvh->numVertices = NumVertices;
		int NumItems = fread(lbvh->vertices, sizeof(float3), NumVertices, file);
		log_debug("[DBG] Read %d vertices from BVH file", NumItems);
	}

	// Read the indices
	{
		int32_t NumIndices = 0;
		fread(&NumIndices, sizeof(int32_t), 1, file);
		lbvh->indices = new int32_t[NumIndices];
		lbvh->numIndices = NumIndices;
		int NumItems = fread(lbvh->indices, sizeof(float3), NumIndices, file);
		log_debug("[DBG] Read %d indices from BVH file", NumItems);
	}

	// Read the BVH nodes
	{
		int32_t NumTreeNodes = 0;
		fread(&NumTreeNodes, sizeof(int32_t), 1, file);
		lbvh->numTreeNodes = NumTreeNodes;
		lbvh->nodes = new BVHNode[NumTreeNodes];
		int NumItems = fread(lbvh->nodes, sizeof(BVHNode), NumTreeNodes, file);
		log_debug("[DBG] Read %d BVH nodes from BVH file", NumItems);
	}

	fclose(file);
	return lbvh;
}