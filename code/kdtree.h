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
	rect3 BoundingBox;

	// TODO(hugo): Do I need to set some conventions
	// here like "if Left or Right is not null, then
	// the node contains no triangles" ? Does it matter ?
	u32 TriangleCount;
	triangle* Triangles;

	kdtree* Left;
	kdtree* Right;
};

