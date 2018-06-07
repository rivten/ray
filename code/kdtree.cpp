#pragma once

#undef internal
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#define internal static

struct vertex
{
	v3 P;
	v3 N;
	// TODO(hugo): add UV ?
};

struct triangle
{
	u32 Indices[3];
	//u32 MaterialIndex;
};

struct mesh
{
	u32 VertexCount;
	vertex* Vertices;
	u32 TriangleCount;
	triangle* Triangles;
};

struct kdtree
{
	rect3 BoundingBox;
	mesh Mesh;
};

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

internal kdtree
LoadKDTreeFromFile(char* Filename)
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

		Result.Mesh.TriangleCount = 0;
		Result.Mesh.Triangles = AllocateArray(triangle, MaxTriangleCount);

		u32 CurrentVertexPoolSize = 16;
		Result.Mesh.VertexCount = 0;
		Result.Mesh.Vertices = AllocateArray(vertex, CurrentVertexPoolSize);

		for(u32 TriangleIndex = 0; TriangleIndex < MaxTriangleCount; ++TriangleIndex)
		{
			++Result.Mesh.TriangleCount;
			triangle* Triangle = Result.Mesh.Triangles + TriangleIndex;
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

				u32 VertexIndex = FindVertexIndex(Result.Mesh.VertexCount, Result.Mesh.Vertices, V);
				if(VertexIndex == VERTEX_NOT_PRESENT)
				{
					if(Result.Mesh.VertexCount == CurrentVertexPoolSize)
					{
						Result.Mesh.Vertices = ReAllocateArray(Result.Mesh.Vertices, vertex, 2 * CurrentVertexPoolSize);
						CurrentVertexPoolSize *= 2;
					}

					Result.Mesh.Vertices[Result.Mesh.VertexCount] = V;
					VertexIndex = Result.Mesh.VertexCount;
					++Result.Mesh.VertexCount;
				}
				Triangle->Indices[VIndex] = VertexIndex;
			}
		}
	}

	// NOTE(hugo): Compute bounding box
	Result.BoundingBox = {V3(MAX_REAL, MAX_REAL, MAX_REAL),
		V3(MIN_REAL, MIN_REAL, MIN_REAL)};
	for(u32 VertexIndex = 0; VertexIndex < Result.Mesh.VertexCount; ++VertexIndex)
	{
		v3 P = Result.Mesh.Vertices[VertexIndex].P;
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

	return(Result);
};
