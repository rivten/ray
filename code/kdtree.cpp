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
	return(Tree->TriangleCount < 15);
}

enum plane_axis
{
	Plane_X,
	Plane_Y,
	Plane_Z,
};

// NOTE(hugo): This plane
// has equation Axis = k
struct plane
{
	plane_axis Axis;
	float k;
};

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
				float Midpoint = 0.5f * (Tree->BoundingBox.Max.x + Tree->BoundingBox.Min.x);
				Result = {Plane_X, Midpoint};
			} break;
		case 1:
			{
				// NOTE(hugo): Y Axis
				float Midpoint = 0.5f * (Tree->BoundingBox.Max.y + Tree->BoundingBox.Min.y);
				Result = {Plane_Y, Midpoint};
			} break;
		case 2:
			{
				// NOTE(hugo): Z Axis
				float Midpoint = 0.5f * (Tree->BoundingBox.Max.z + Tree->BoundingBox.Min.z);
				Result = {Plane_Z, Midpoint};
			} break;
		InvalidDefaultCase;
	}
	return(Result);
}

struct triangle_plane_separation_result
{
	u32 LeftTriangleCount;
	u32 RightTriangleCount;
};

internal v3
GetIsobarycenter(triangle* T, render_state* RenderState)
{
	v3 Result = {};
	Result += RenderState->Vertices[T->Indices[0]].P;
	Result += RenderState->Vertices[T->Indices[1]].P;
	Result += RenderState->Vertices[T->Indices[2]].P;
	Result /= 3.0f;

	return(Result);
}

internal triangle_plane_separation_result
TrianglePlaneSeparation(kdtree* Tree, plane P, render_state* RenderState)
{
	triangle_plane_separation_result Result = {};
	triangle* CurrentTriangle = Tree->Triangles;
	triangle* ToWriteRight = Tree->Triangles + (Tree->TriangleCount - 1);
	while(CurrentTriangle != (ToWriteRight + 1))
	{
		// TODO(hugo): Is the isobarycenter the right choice here ?
		v3 IsoBar = GetIsobarycenter(CurrentTriangle, RenderState);

		// TODO(hugo): If I _know_ that the planes are one of 
		// three possibilities, why not do three functions
		// TrianglePlaneSeparation_X/Y/Z ?
		float IsoBarComponent = 0.0f;
		switch(P.Axis)
		{
			case Plane_X:
				{
					IsoBarComponent = IsoBar.x;
				} break;
			case Plane_Y:
				{
					IsoBarComponent = IsoBar.y;
				} break;
			case Plane_Z:
				{
					IsoBarComponent = IsoBar.z;
				} break;
			InvalidDefaultCase;
		}

		if(IsoBarComponent <= P.k)
		{
			// NOTE(hugo): The triangle is
			// currently at the right place.
			++CurrentTriangle;
			++Result.LeftTriangleCount;
		}
		else
		{
			triangle TempTriangle = *ToWriteRight;
			*ToWriteRight = *CurrentTriangle;
			*CurrentTriangle = TempTriangle;
			++Result.RightTriangleCount;
			--ToWriteRight;
		}
	}

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

internal kdtree* GetKDTreeFromPool(render_state* RenderState);

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

	Tree->Left = GetKDTreeFromPool(RenderState);
	Tree->Right = GetKDTreeFromPool(RenderState);

	Tree->Left->TriangleCount = Separation.LeftTriangleCount;
	Tree->Left->Triangles = Tree->Triangles;
	ComputeBoundingBox(Tree->Left, RenderState);
	BuildKdTree(Tree->Left, CurrentDepth + 1, RenderState);

	Tree->Right->TriangleCount = Separation.RightTriangleCount;
	Tree->Right->Triangles = Tree->Triangles + Separation.LeftTriangleCount;
	ComputeBoundingBox(Tree->Right, RenderState);
	BuildKdTree(Tree->Right, CurrentDepth + 1, RenderState);

	// NOTE(hugo): Let's say that this node
	// does not contain any triangles.
	Tree->TriangleCount = 0;
}

internal void
DEBUGOutputTreeGraphvizRec(FILE* f, kdtree* Node)
{
	if(Node->Left)
	{
		fprintf(f, "\t\"%p\" -> \"%p\" /* %ld */\n", Node, Node->Left, (char*)Node->Left - (char*)Node);
		DEBUGOutputTreeGraphvizRec(f, Node->Left);
	}
	if(Node->Right)
	{
		fprintf(f, "\t\"%p\" -> \"%p\" /* %ld */\n", Node, Node->Right, (char*)Node->Right - (char*)Node);
		DEBUGOutputTreeGraphvizRec(f, Node->Right);
	}
}

internal void
DEBUGOutputTreeGraphviz(kdtree* Root)
{
	FILE* OutputFile = fopen("kdtree.gv", "w");
	Assert(OutputFile);
	fprintf(OutputFile, "digraph G\n{\n");
	fprintf(OutputFile, "\t/* sizeof(kdtree) = %lu */\n", sizeof(kdtree));
	DEBUGOutputTreeGraphvizRec(OutputFile, Root);
	fprintf(OutputFile, "}");
	fclose(OutputFile);

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

	kdtree* Result = GetKDTreeFromPool(RenderState);

	for(u32 ShapeIndex = 0; ShapeIndex < Shapes.size(); ++ShapeIndex)
	{
		tinyobj::mesh_t Mesh = Shapes[ShapeIndex].mesh;

		u32 MaxTriangleCount = (u32)(Mesh.indices.size()) / 3;

		Result->TriangleCount = 0;
		Result->Triangles = AllocateArray(triangle, MaxTriangleCount);

		u32 CurrentVertexPoolSize = 16;
		RenderState->VertexCount = 0;
		RenderState->Vertices = AllocateArray(vertex, CurrentVertexPoolSize);

		for(u32 TriangleIndex = 0; TriangleIndex < MaxTriangleCount; ++TriangleIndex)
		{
			++Result->TriangleCount;
			triangle* Triangle = Result->Triangles + TriangleIndex;
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
	Result->BoundingBox = {V3(MAX_REAL, MAX_REAL, MAX_REAL),
		V3(MIN_REAL, MIN_REAL, MIN_REAL)};
	for(u32 VertexIndex = 0; VertexIndex < RenderState->VertexCount; ++VertexIndex)
	{
		v3 P = RenderState->Vertices[VertexIndex].P;
		if(P.x > Result->BoundingBox.Max.x)
		{
			Result->BoundingBox.Max.x = P.x;
		}
		if(P.y > Result->BoundingBox.Max.y)
		{
			Result->BoundingBox.Max.y = P.y;
		}
		if(P.z > Result->BoundingBox.Max.z)
		{
			Result->BoundingBox.Max.z = P.z;
		}
		if(P.x < Result->BoundingBox.Min.x)
		{
			Result->BoundingBox.Min.x = P.x;
		}
		if(P.y < Result->BoundingBox.Min.y)
		{
			Result->BoundingBox.Min.y = P.y;
		}
		if(P.z < Result->BoundingBox.Min.z)
		{
			Result->BoundingBox.Min.z = P.z;
		}
	}

	printf("Building the KD Tree...\n");
	BuildKdTree(Result, 0, RenderState);
	printf("KD Tree built !\n");

#if 1
	DEBUGOutputTreeGraphviz(Result);
#endif

	return(*Result);
};

internal void
RayKdTreeIntersection(ray Ray, kdtree* Node, render_state* RenderState, hit_record* ClosestHitRecord)
{
	rect3 NodeBoundingBox = Node->BoundingBox;
	if(RayHitBoundingBox(Ray, NodeBoundingBox))
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

