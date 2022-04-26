// Copied and adapted from GPU Pro 3, the code is in the public domain, so that's
// OK. I should know, since I'm one of the co-authors!

#define MAX_RT_STACK 512

// RTConstantsBuffer
cbuffer ConstantBuffer : register(b10) {
	matrix RTTransformWorldView;
	// 64 bytes
	matrix RTTransformWorldViewInv;
	// 128 bytes
	int g_NumVertices, g_NumIndices, g_NumTriangles, RTunused0;
	// 144 bytes
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

// BVH, slot 14
StructuredBuffer<BVHNode> g_BVH : register(t14);
// Vertices, slot 15
Buffer<float3> g_Vertices : register(t15);
// Indices, slot 16
Buffer<int> g_Indices : register(t16);

// ------------------------------------------
// Checks if the ray hits the node's AABB
// ------------------------------------------
// Returns: T[0] the min distance to the box
//          T[1] the max distance to the box
// if T[1] >= T[0], then there's an intersection.
// See also: https://tavianator.com/2011/ray_box.html
float2 BVHIntersectBox(float3 start, float3 inv_dir, int node)
{
	float2 T;
	float3 diff_max = g_BVH[node].max.xyz - start;
	float3 diff_min = g_BVH[node].min.xyz - start;
	diff_max *= inv_dir;
	diff_min *= inv_dir;

	T[0] = min(diff_min.x, diff_max.x);
	T[1] = max(diff_min.x, diff_max.x);

	T[0] = max(T[0], min(diff_min.y, diff_max.y));
	T[1] = min(T[1], max(diff_min.y, diff_max.y));

	T[0] = max(T[0], min(diff_min.z, diff_max.z));
	T[1] = min(T[1], max(diff_min.z, diff_max.z));

	// empty interval
	if (T[0] > T[1])
	{
		T[0] = T[1] = -1.0f;
	}

	return T;
}

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

		// Check if the ray intersects the current box
		float2 T = BVHIntersectBox(ray.origin, inv_dir, curnode);

		// Ray intersects the box
		if (T[1] >= T[0])
		{
			int TriID = g_BVH[curnode].ref;
			// Inner node: push the children of this node on the stack
			if (TriID == -1) {
				if (stack_top + 2 < MAX_RT_STACK) {
					// All the inner nodes of a BVH-2 should be complete, so we can just
					// push both children on the stack
					stack[stack_top++] = g_BVH[curnode].left;
					stack[stack_top++] = g_BVH[curnode].right;
				}
			}
			// Leaf node: do the ray-triangle intersection test
			else {
				// Get the triangle data
				int ofs = TriID * 3;
				float3 A = g_Vertices[g_Indices[ofs]];
				float3 B = g_Vertices[g_Indices[ofs + 1]];
				float3 C = g_Vertices[g_Indices[ofs + 2]];
				
				Intersection inters = getIntersection(ray, A, B, C);
				if (RayTriangleTest(inters)) {
					inters.TriID = TriID;
					best_inters = inters;
					return best_inters;
				}
			}
		}
	}

	return best_inters;
}

// Trace a ray and return as soon as we hit geometry
Intersection TraceRaySimpleHit(Ray ray) {
	float3 pos3D = ray.origin;
	// ray.origin is in the pos3D frame, we need to invert it into OPT coords
	pos3D.y = -pos3D.y;
	pos3D *= 40.96f;
	pos3D = mul(float4(pos3D, 1.0f), RTTransformWorldViewInv).xyz;
	//pos3D = mul(float4(pos3D, 1.0f), MeshTransformInv);

	float3 dir = ray.dir;
	// ray.dir is in the pos3D frame, we need to transform it into OPT coords
	// Here the transform rule is slightly different because I hate myself and
	// I altered how lights work in the Tech Room. XWA fixes the lights to the
	// object, but I wanted them fixed in space so that we could really see the
	// shading. See D3dRenderer::UpdateConstantBuffer for more details
	dir.yz = -dir.yz;
	dir = mul(float4(dir, 0), RTTransformWorldViewInv).xyz;
	//dir = mul(float4(dir, 0), MeshTransformInv);

	// pos3D and dir are  now in OPT coords. We can cast the ray
	ray.origin = pos3D * 0.325; // I have no idea why I have to multiply by this constant!
	ray.dir = dir;
	ray.max_dist *= 40.96f;
	return _TraceRaySimpleHit(ray);
	//return _NaiveSimpleHit(ray);
}
