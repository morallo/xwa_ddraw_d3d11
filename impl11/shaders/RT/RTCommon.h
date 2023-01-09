#ifndef RT_COMMON_H
#define RT_COMMON_H
// Copied and adapted from GPU Pro 3, the code is in the public domain, so that's
// OK. I should know, since I'm one of the co-authors!

#define MAX_RT_STACK 32

// RTConstantsBuffer
cbuffer ConstantBuffer : register(b10) {
	bool bRTEnabled;
	uint3 g_RTUnused;
	// 16 bytes
};

struct Ray				// 28 bytes
{
	float3 origin;		// 12 bytes
	float3 dir;			// 12 bytes
	float max_dist;		//  4 bytes
};

struct Intersection		// 16 bytes
{
	int TriID;			//  4 bytes
	float U, V, T;		// 12 bytes
};

// BVH2 node, deprecated, BVH4 is more better
#ifdef DISABLED
struct BVHNode {
	int ref;   // -1 for internal nodes, Triangle index for leaves
	int left;
	int right;
	int padding;
	// 16 bytes

	// AABB
	float4 min;
	// 32 bytes
	float4 max;
	// 48 bytes
};
#endif

// BVH4 node
struct BVHNode {
	int ref; // TriID: -1 for internal nodes, Triangle index for leaves
	int parent; // Not used at this point
	int rootIdx;
	int numChildren;
	// 16 bytes
	float4 min;
	// 32 bytes
	float4 max;
	// 48 bytes
	int4 children;
	// 64 bytes
};

// TLAS leaves use the following fields for a different purpose:
#define BLASMatrixSlot rootIdx
#define BLASBaseNodeOffset numChildren

// BVH, slot 14
StructuredBuffer<BVHNode> g_BVH : register(t14);
// Matrices, slot 15
StructuredBuffer<matrix> g_Matrices : register(t15);
// TLAS BVH, slot 16
StructuredBuffer<BVHNode> g_TLAS : register(t16);
// * During regular flight, we have multiple BLASes in a single buffer. Thus, multiple roots may exist in a single
//   buffer and each BVH will have its own starting offset to which all other internal offsets are relative to.

// Vertices, slot 15
//Buffer<float3> g_Vertices : register(t15);
// Indices, slot 16
//Buffer<int> g_Indices : register(t16);

// ------------------------------------------
// Checks if the ray hits the node's AABB
// ------------------------------------------
// Returns: T[0] the min distance to the box
//          T[1] the max distance to the box
// if T[1] >= T[0], then there's an intersection.
// See also: https://tavianator.com/2011/ray_box.html
float2 _BVHIntersectBox(in float3 box_min, in float3 box_max, in float3 start, in float3 inv_dir)
{
	const float3 diff_max = (box_max.xyz - start) * inv_dir;
	const float3 diff_min = (box_min.xyz - start) * inv_dir;
	float2 T;

	T[0] = min(diff_min.x, diff_max.x);
	T[1] = max(diff_min.x, diff_max.x);

	T[0] = max(T[0], min(diff_min.y, diff_max.y));
	T[1] = min(T[1], max(diff_min.y, diff_max.y));

	T[0] = max(T[0], min(diff_min.z, diff_max.z));
	T[1] = min(T[1], max(diff_min.z, diff_max.z));

	return T;
}

float2 BVHIntersectBox(in StructuredBuffer<BVHNode> BVH, in float3 start, in float3 inv_dir, in int node)
{
	const float3 box_min = BVH[node].min.xyz;
	const float3 box_max = BVH[node].max.xyz;
	return _BVHIntersectBox(box_min, box_max, start, inv_dir);
}

#ifdef DISABLED
bool InsideBox(in float3 min, in float3 max, in float3 P)
{
	if (min.x <= P.x && P.x <= max.x &&
		min.y <= P.y && P.y <= max.y &&
		min.z <= P.z && P.z <= max.z)
	{
		return true;
	}
	return false;
}

bool InsideBox(StructuredBuffer<BVHNode> BVH, int curnode, in float3 P)
{
	return InsideBox(BVH[curnode].min.xyz, BVH[curnode].max.xyz, P);
}
#endif

// ------------------------------------------
// Gets the current ray intersection
// ------------------------------------------
Intersection getIntersection(Ray ray, float3 A, float3 B, float3 C)
{
	Intersection inters;
	inters.TriID = -1;

	float3 P, T, Q;
	float3 E1 = B - A;
	float3 E2 = C - A;
	P = cross(ray.dir, E2);
	float det = 1.0f / dot(E1, P);
	T = ray.origin - A;
	inters.U = dot(T, P) * det;
	Q = cross(T, E1);
	inters.V = dot(ray.dir, Q) * det;
	inters.T = dot(E2, Q) * det;

	return inters;
}

// ------------------------------------------
// Ray-Triangle intersection test
// ------------------------------------------
bool RayTriangleTest(in Intersection inters)
{
	return (
		(inters.U >= 0.0f) &&
		(inters.V >= 0.0f) &&
		((inters.U + inters.V) <= 1.0f) &&
		(inters.T >= 0.0f)
	);
}

// Iterate over all the primitives until we find a hit. Only useful for debugging
// purposes
/*
Intersection _NaiveSimpleHit(Ray ray) {
	Intersection best_inters;
	best_inters.TriID = -1;
	for (int TriID = 0; TriID < g_NumTriangles; TriID++) {
		int ofs = TriID * 3;
		float3 A = g_Vertices[g_Indices[ofs]];
		float3 B = g_Vertices[g_Indices[ofs + 1]];
		float3 C = g_Vertices[g_Indices[ofs + 2]];

		Intersection inters = getIntersection(ray, A, B, C);
		if (RayTriangleTest(inters)) {
			inters.TriID = TriID;
			best_inters = inters;
			break;
		}
	}
	return best_inters;
}
*/

// Ray traversal, Indexed Geometry version
#ifdef DISABLED
// ray is in OPT coords. We can traverse the BVH now
Intersection _TraceRaySimpleHit(Ray ray) {
	int stack[MAX_RT_STACK];
	int stack_top = 0;
	int curnode = -1;
	float3 inv_dir = 1.0f / ray.dir;
	Intersection best_inters;
	best_inters.TriID = -1;

	stack[stack_top++] = 0; // Push the root on the stack
	while (stack_top > 0) {
		// Pop a node from the stack
		curnode = stack[--stack_top];
		const BVHNode node = g_BVH[curnode];
		const int TriID = node.ref;

		if (TriID == -1) {
			// This node is a box. Do the ray-box intersection test
			const float2 T = BVHIntersectBox(ray.origin, inv_dir, curnode);

			// T[1] >= T[0] is the standard test, but lines are infinite, so it will also intersect
			// boxes behind the ray. To skip those boxes, we need to add T[1] >= 0, to make sure
			// the box is in front of the ray (for rays originating inside a box, we'll have T[0] < 0,
			// so we can't use that test).
			if (T[1] >= 0 && T[1] >= T[0])
			{
				// Ray intersects the box
				// Inner node: push the children of this node on the stack
				if (TriID == -1 && stack_top + 4 < MAX_RT_STACK) {
					// Push the valid children of this node into the stack
					for (int i = 0; i < 4; i++) {
						int child = node.children[i];
						if (child == -1) break;
						stack[stack_top++] = child;
					}
				}
			}
		}
		else {
			// This node is a triangle. Do the ray-triangle intersection test.
			const float3 A = node.min.xyz;
			const float3 B = node.max.xyz;
			const float3 C = float3(
				asfloat(node.children[0]),
				asfloat(node.children[1]),
				asfloat(node.children[2]));

			Intersection inters = getIntersection(ray, A, B, C);
			if (RayTriangleTest(inters)) {
				inters.TriID = TriID;
				best_inters = inters;
				return best_inters;
			}
		}
	}
#endif

// Ray traversal, Embedded Geometry version
Intersection _TraceRaySimpleHit(Ray ray, in int Offset) {
	int stack[MAX_RT_STACK];
	int stack_top = 0;
	int curnode = -1;
	float3 inv_dir = 1.0f / ray.dir;
	Intersection best_inters;
	best_inters.TriID = -1;

	// Read the padding from the first BVHNode. It will contain the location of the root
	//int root = g_BVH[0].rootIdx;
	int root = g_BVH[Offset].rootIdx + Offset;

	stack[stack_top++] = root; // Push the root on the stack
	while (stack_top > 0) {
		// Pop a node from the stack
		curnode = stack[--stack_top];
		const BVHNode node = g_BVH[curnode];
		const int TriID = node.ref;

		if (TriID == -1) {
			// This node is a box. Do the ray-box intersection test
			const float2 T = BVHIntersectBox(g_BVH, ray.origin, inv_dir, curnode);

			// T[1] >= T[0] is the standard test, but lines are infinite, so it will also intersect
			// boxes behind the ray. To skip those boxes, we need to add T[1] >= 0, to make sure
			// the box is in front of the ray (for rays originating inside a box, we'll have T[0] < 0,
			// so we can't use that test).
			if (T[1] >= 0 && T[1] >= T[0])
			{
				// Ray intersects the box
				// Inner node: push the children of this node on the stack
				if (stack_top + 4 < MAX_RT_STACK) {
					// Push the valid children of this node into the stack
					for (int i = 0; i < 4; i++) {
						int child = node.children[i];
						if (child == -1) break;
						stack[stack_top++] = child + Offset;
					}
				}
			}
		}
		else {
			// This node is a triangle. Do the ray-triangle intersection test.
			const float3 A = node.min.xyz;
			const float3 B = node.max.xyz;
			const float3 C = float3(
				asfloat(node.children[0]),
				asfloat(node.children[1]),
				asfloat(node.children[2]));

			Intersection inters = getIntersection(ray, A, B, C);
			if (RayTriangleTest(inters)) {
				inters.TriID = TriID;
				best_inters = inters;
				return best_inters;
			}
		}
	}

	return best_inters;
}

// Trace a ray and return as soon as we hit geometry (Tech Room version)
// This version expects a single matrix in g_Matrices that applies to the
// whole g_BVH tree
Intersection TraceRaySimpleHit(Ray ray) {
	float3 pos3D = ray.origin;
	// ray.origin is in the pos3D frame, we need to invert it into OPT coords
	pos3D.y = -pos3D.y;
	pos3D *= 40.96f;
	pos3D = mul(float4(pos3D, 1.0f), g_Matrices[0]).xyz;

	float3 dir = ray.dir;
	// ray.dir is in the pos3D frame, we need to transform it into OPT coords
	// Here the transform rule is slightly different because I hate myself and
	// I altered how lights work in the Tech Room. XWA fixes the lights to the
	// object, but I wanted them fixed in space so that we could really see the
	// shading. See D3dRenderer::UpdateConstantBuffer for more details
	dir.yz = -dir.yz;
	dir = mul(float4(dir, 0), g_Matrices[0]).xyz;

	// pos3D and dir are  now in OPT coords. We can cast the ray
	ray.origin    = pos3D;
	ray.dir       = dir;
	ray.max_dist *= 40.96f;;
	return _TraceRaySimpleHit(ray, 0);
	//return _NaiveSimpleHit(ray);
}

// TLAS Ray traversal, Embedded Geometry version
Intersection _TLASTraceRaySimpleHit(Ray ray) {
	int stack[MAX_RT_STACK];
	int stack_top = 0;
	int curnode = -1;
	float3 inv_dir = 1.0f / ray.dir;
	Intersection blank_inters;
	blank_inters.TriID = -1;

	// Read the index of the root node
	int root = g_TLAS[0].rootIdx;

	stack[stack_top++] = root; // Push the root on the stack
	while (stack_top > 0)
	{
		// Pop a node from the stack
		curnode = stack[--stack_top];
		const BVHNode node = g_TLAS[curnode];
		const int ID = node.ref;

		if (ID == -1)
		{
			// This is an inner node. Do the ray-box intersection test
			const float2 T = BVHIntersectBox(g_TLAS, ray.origin, inv_dir, curnode);

			// T[1] >= T[0] is the standard test, but lines are infinite, so it will also intersect
			// boxes behind the ray. To skip those boxes, we need to add T[1] >= 0, to make sure
			// the box is in front of the ray (for rays originating inside a box, we'll have T[0] < 0,
			// so we can't use that test).
			if (T[1] >= 0 && T[1] >= T[0])
			{
				// Ray intersects the box
				// Inner node: push the children of this node on the stack
				if (stack_top + 4 < MAX_RT_STACK) {
					// Push the valid children of this node into the stack
					for (int i = 0; i < 4; i++) {
						int child = node.children[i];
						if (child == -1) break;
						stack[stack_top++] = child;
					}
				}
			}
		}
		else
		{
			// This node is a TLAS leaf. Here we need to do two things:
			// Intersect the ray with the AABB.
			// Intersect the ray with the OBB.
			// If both tests pass, then:
			// - Fetch the offset for the corresponding BLAS.
			// - Continue tracing with the proper BLAS (adding the BLAS offset to all the nodes)

			// Intersect the ray with the TLAS leaf's AABB:
			const float2 T = BVHIntersectBox(g_TLAS, ray.origin, inv_dir, curnode);
			if (T[1] >= 0 && T[1] >= T[0])
			{
				// Ray intersects the box, fetch the BLAS entry and continue the search from there
				int BLASOffset = g_TLAS[curnode].BLASBaseNodeOffset;
				int matrixSlot = g_TLAS[curnode].BLASMatrixSlot;
				if (BLASOffset != -1 && matrixSlot != -1)
				{
					matrix Matrix = g_Matrices[matrixSlot];
					float3 obb_min =
						float3(
							g_TLAS[curnode].min[3],
							g_TLAS[curnode].max[3],
							asfloat(g_TLAS[curnode].children[3]));
					float3 obb_max =
						float3(
							asfloat(g_TLAS[curnode].children[0]),
							asfloat(g_TLAS[curnode].children[1]),
							asfloat(g_TLAS[curnode].children[2]));
					// Transform the ray into the same coord sys as the OBB
					Ray new_ray;
					new_ray.origin = mul(float4(ray.origin, 1), Matrix).xyz;
					new_ray.dir    = mul(float4(ray.dir,    0), Matrix).xyz;

					// Before traversing the BLAS, check if the ray intersects the OBB
					const float3 new_ray_inv_dir = 1.0f / new_ray.dir;
					const float2 T2 = _BVHIntersectBox(obb_min, obb_max, new_ray.origin, new_ray_inv_dir);
					// The ray intersects the OBB, check the BLAS for triangle intersections
					if (T2[1] >= 0 && T2[1] >= T2[0])
					{
						Intersection inters = _TraceRaySimpleHit(new_ray, BLASOffset);
						if (inters.TriID != -1)
							return inters;
					}
				}
			}

		}
	}

	return blank_inters;
}

// Trace a ray and return as soon as we hit geometry
Intersection TLASTraceRaySimpleHit(Ray ray) {
	float3 pos3D = ray.origin;
	// ray.origin is in the pos3D frame (Metric, Y+ is up, Z+ is forward)
	// we need to revert it into OPT coords, but the TLAS and the ray are
	// in the same coord sys. There's no need to transform anything
	pos3D.y = -pos3D.y;
	pos3D  *= 40.96f;

	// pos3D and dir are now in WorldView coords. We can cast the ray
	ray.origin    = pos3D;
	// Ray needs to have its direction inverted. Also Y is flipped for the same reason
	// pos3D.y is flipped (Y+ is up)
	ray.max_dist *= 40.96f;
	return _TLASTraceRaySimpleHit(ray);
}

#endif