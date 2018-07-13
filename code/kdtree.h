#pragma once

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
	u32 LeftIndex;
	u32 RightIndex;
	u32 TriangleCount;
	triangle* Triangles;
	rect3 BoundingBox;
};

