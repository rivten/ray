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
};

struct kdtree
{
	u32 LeftIndex;
	u32 RightIndex;
	u32 TriangleCount;
	triangle* Triangles;
	rect3 BoundingBox;
};

