#pragma once

#undef internal
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#define internal static

internal bool
AreVerticesIdentical(vertex V0, vertex V1)
{
	return(V0.P.x == V1.P.x &&
		V0.P.y == V1.P.y &&
		V0.P.z == V1.P.z &&
		V0.N.x == V1.N.x &&
		V0.N.y == V1.N.y &&
		V0.N.z == V1.N.z);
}

#define VERTEX_NOT_PRESENT 0xffffffff
internal u32
FindVertexIndex(u32 VertexCount, vertex* Vertices, vertex V)
{
	Assert(VertexCount != VERTEX_NOT_PRESENT);

	u32 Result = VERTEX_NOT_PRESENT;
	for(u32 VertexIndex = 0; (Result == VERTEX_NOT_PRESENT) && (VertexIndex < VertexCount); ++VertexIndex)
	{
		vertex TestV = Vertices[VertexIndex];
		if(AreVerticesIdentical(V, TestV))
		{
			Result = VertexIndex;
		}
	}

	return(Result);
}

#define ToImplement Assert(!"To Implement")

internal bool
KdTreeEndBuild(kdtree* Tree)
{
	// TODO(hugo): Improve this
	// with a better heuristic
	return(Tree->TriangleCount < 10);
}

// NOTE(hugo): A plane is a v4 of equation
// <x, n> = 0
// ax + by + cz + d = 0
// So we store n = (a, b, c, d)
typedef v4 plane;

internal plane
FindSeparatingPlane(kdtree* Tree, u32 CurrentDepth)
{
	// TODO(hugo): Improve this
	// See 'On building fast kd-Trees for Ray Tracing,
	// and on doing that in O(N log N)'
	// Basically : Surface Area Heuristic (SAH)

	u32 AxisIndex = CurrentDepth % 3;
	plane Result = {};
	switch(AxisIndex)
	{
		case 0:
			{
				// NOTE(hugo): X Axis
				float Midpoint = -0.5f * (Tree->BoundingBox.Max.x + Tree->BoundingBox.Min.x);
				Result = V4(1.0f, 0.0f, 0.0f, Midpoint);
			} break;
		case 1:
			{
				// NOTE(hugo): Y Axis
				float Midpoint = -0.5f * (Tree->BoundingBox.Max.y + Tree->BoundingBox.Min.y);
				Result = V4(0.0f, 1.0f, 0.0f, Midpoint);
			} break;
		case 2:
			{
				// NOTE(hugo): Z Axis
				float Midpoint = -0.5f * (Tree->BoundingBox.Max.z + Tree->BoundingBox.Min.z);
				Result = V4(0.0f, 0.0f, 1.0f, Midpoint);
			} break;
		InvalidDefaultCase;
	}
	return(Result);
}

struct triangle_plane_separation_result
{
	u32 LeftTriangleCount;
	u32 RightTriangleCount;
	triangle* LeftTrianglePointer;
	triangle* RightTrianglePointer;
};

internal triangle_plane_separation_result
TrianglePlaneSeparation(kdtree* Tree, plane P, render_state* RenderState)
{
	ToImplement;
	triangle_plane_separation_result Result = {};
	return(Result);
}

internal void
ComputeBoundingBox(kdtree* Tree, render_state* RenderState)
{
	Tree->BoundingBox = {V3(MAX_REAL, MAX_REAL, MAX_REAL),
		V3(MIN_REAL, MIN_REAL, MIN_REAL)};
	for(u32 TriangleIndex = 0; TriangleIndex < Tree->TriangleCount; ++TriangleIndex)
	{
		triangle* T = Tree->Triangles + TriangleIndex;
		for(u32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
		{
			v3 P = RenderState->Vertices[T->Indices[VertexIndex]].P;
			if(P.x > Tree->BoundingBox.Max.x)
			{
				Tree->BoundingBox.Max.x = P.x;
			}
			if(P.y > Tree->BoundingBox.Max.y)
			{
				Tree->BoundingBox.Max.y = P.y;
			}
			if(P.z > Tree->BoundingBox.Max.z)
			{
				Tree->BoundingBox.Max.z = P.z;
			}
			if(P.x < Tree->BoundingBox.Min.x)
			{
				Tree->BoundingBox.Min.x = P.x;
			}
			if(P.y < Tree->BoundingBox.Min.y)
			{
				Tree->BoundingBox.Min.y = P.y;
			}
			if(P.z < Tree->BoundingBox.Min.z)
			{
				Tree->BoundingBox.Min.z = P.z;
			}
		}
	}
}

internal void
BuildKdTree(kdtree* Tree, u32 CurrentDepth, render_state* RenderState)
{
	if(KdTreeEndBuild(Tree))
	{
		return;
	}
	plane P = FindSeparatingPlane(Tree, CurrentDepth);
	triangle_plane_separation_result Separation = TrianglePlaneSeparation(Tree, P, RenderState);
	Assert(Separation.LeftTriangleCount + Separation.RightTriangleCount == Tree->TriangleCount);

	// TODO(hugo): Not optimal to allocate this way.
	// I guess I should get them from a pool to have the nodes
	// tightly packed
	kdtree* SubTrees = AllocateArray(kdtree, 2);
	Tree->Left = SubTrees + 0;
	Tree->Right = SubTrees + 1;

	Tree->Left->TriangleCount = Separation.LeftTriangleCount;
	Tree->Left->Triangles = Separation.LeftTrianglePointer;
	ComputeBoundingBox(Tree->Left, RenderState);
	BuildKdTree(Tree->Left, CurrentDepth + 1, RenderState);

	Tree->Right->TriangleCount = Separation.RightTriangleCount;
	Tree->Right->Triangles = Separation.RightTrianglePointer;
	ComputeBoundingBox(Tree->Right, RenderState);
	BuildKdTree(Tree->Right, CurrentDepth + 1, RenderState);

	// NOTE(hugo): Let's say that this node
	// does not contain any triangles.
	Tree->TriangleCount = 0;
}

internal kdtree
LoadKDTreeFromFile(char* Filename, render_state* RenderState)
{
	tinyobj::attrib_t Attributes = {};
	std::vector<tinyobj::shape_t> Shapes = {};
	std::vector<tinyobj::material_t> Materials = {};
	bool LoadObjResult = LoadObj(&Attributes, &Shapes, &Materials,
			0, Filename, 0, true);
	Assert(LoadObjResult);

	kdtree Result = {};

	for(u32 ShapeIndex = 0; ShapeIndex < Shapes.size(); ++ShapeIndex)
	{
		tinyobj::mesh_t Mesh = Shapes[ShapeIndex].mesh;

		u32 MaxTriangleCount = (u32)(Mesh.indices.size()) / 3;

		Result.TriangleCount = 0;
		Result.Triangles = AllocateArray(triangle, MaxTriangleCount);

		u32 CurrentVertexPoolSize = 16;
		RenderState->VertexCount = 0;
		RenderState->Vertices = AllocateArray(vertex, CurrentVertexPoolSize);

		for(u32 TriangleIndex = 0; TriangleIndex < MaxTriangleCount; ++TriangleIndex)
		{
			++Result.TriangleCount;
			triangle* Triangle = Result.Triangles + TriangleIndex;
			for(u32 VIndex = 0; VIndex < 3; ++VIndex)
			{
				tinyobj::index_t AttributeIndex = Mesh.indices[3 * TriangleIndex + VIndex];
				s32 PosIndex = AttributeIndex.vertex_index;
				Assert(PosIndex != -1);
				vertex V = {};
				V.P = V3(Attributes.vertices[3 * PosIndex + 0],
						Attributes.vertices[3 * PosIndex + 1],
						Attributes.vertices[3 * PosIndex + 2]);

				s32 NormalIndex = AttributeIndex.normal_index;
				Assert(NormalIndex != -1);
				V.N = V3(Attributes.normals[3 * NormalIndex + 0],
						Attributes.normals[3 * NormalIndex + 1],
						Attributes.normals[3 * NormalIndex + 2]);

				u32 VertexIndex = FindVertexIndex(RenderState->VertexCount, RenderState->Vertices, V);
				if(VertexIndex == VERTEX_NOT_PRESENT)
				{
					if(RenderState->VertexCount == CurrentVertexPoolSize)
					{
						RenderState->Vertices = ReAllocateArray(RenderState->Vertices, vertex, 2 * CurrentVertexPoolSize);
						CurrentVertexPoolSize *= 2;
					}

					RenderState->Vertices[RenderState->VertexCount] = V;
					VertexIndex = RenderState->VertexCount;
					++RenderState->VertexCount;
				}
				Triangle->Indices[VIndex] = VertexIndex;
			}
		}
	}

	// NOTE(hugo): Compute bounding box
	Result.BoundingBox = {V3(MAX_REAL, MAX_REAL, MAX_REAL),
		V3(MIN_REAL, MIN_REAL, MIN_REAL)};
	for(u32 VertexIndex = 0; VertexIndex < RenderState->VertexCount; ++VertexIndex)
	{
		v3 P = RenderState->Vertices[VertexIndex].P;
		if(P.x > Result.BoundingBox.Max.x)
		{
			Result.BoundingBox.Max.x = P.x;
		}
		if(P.y > Result.BoundingBox.Max.y)
		{
			Result.BoundingBox.Max.y = P.y;
		}
		if(P.z > Result.BoundingBox.Max.z)
		{
			Result.BoundingBox.Max.z = P.z;
		}
		if(P.x < Result.BoundingBox.Min.x)
		{
			Result.BoundingBox.Min.x = P.x;
		}
		if(P.y < Result.BoundingBox.Min.y)
		{
			Result.BoundingBox.Min.y = P.y;
		}
		if(P.z < Result.BoundingBox.Min.z)
		{
			Result.BoundingBox.Min.z = P.z;
		}
	}

	printf("Building the KD Tree...\n");
	BuildKdTree(&Result, 0, RenderState);
	printf("KD Tree built !\n");

	return(Result);
};

internal void
RayKdTreeIntersection(ray Ray, kdtree* Node, render_state* RenderState, hit_record* ClosestHitRecord)
{
	rect3 SceneBoundingBox = RenderState->Scene.BoundingBox;
	if(RayHitBoundingBox(Ray, SceneBoundingBox))
	{
		if(Node->Left)
		{
			RayKdTreeIntersection(Ray, Node->Left, RenderState, ClosestHitRecord);
		}
		if(Node->Right)
		{
			RayKdTreeIntersection(Ray, Node->Right, RenderState, ClosestHitRecord);
		}

		for(u32 TriangleIndex = 0; TriangleIndex < Node->TriangleCount; ++TriangleIndex)
		{
			triangle T = Node->Triangles[TriangleIndex];
			RayTriangleIntersection(Ray, T, RenderState->Vertices, ClosestHitRecord);
		}
	}
}
