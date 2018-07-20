#pragma once

internal bool
RayHitBoundingBox(ray Ray, rect3 BoundingBox)
{
	// TODO(hugo): Better perf for this !

	// NOTE(hugo): Checking BACK FACE of AABB
	{
		// NOTE(hugo): Equation of normal is z = BoundingBox.Min.z
		if(Ray.Dir.z != 0.0f)
		{
			float t = (BoundingBox.Min.z - Ray.Start.z) / Ray.Dir.z;
			v3 HitPoint = Ray.Start + t * Ray.Dir;
			bool Hit = (t >= 0.0f && HitPoint.x >= BoundingBox.Min.x && HitPoint.x < BoundingBox.Max.x &&
					HitPoint.y >= BoundingBox.Min.y && HitPoint.y < BoundingBox.Max.y);
			if(Hit)
			{
				return(true);
			}
		}
		else
		{
			//Assert(Ray.Start.z != BoundingBox.Min.z);
		}
	}

	// NOTE(hugo): Checking FRONT FACE of AABB
	{
		// NOTE(hugo): Equation of normal is z = BoundingBox.Max.z
		if(Ray.Dir.z != 0.0f)
		{
			float t = (BoundingBox.Max.z - Ray.Start.z) / Ray.Dir.z;
			v3 HitPoint = Ray.Start + t * Ray.Dir;
			bool Hit = (t >= 0.0f && HitPoint.x >= BoundingBox.Min.x && HitPoint.x < BoundingBox.Max.x &&
					HitPoint.y >= BoundingBox.Min.y && HitPoint.y < BoundingBox.Max.y);
			if(Hit)
			{
				return(true);
			}
		}
		else
		{
			//Assert(Ray.Start.z != BoundingBox.Max.z);
		}
	}

	// NOTE(hugo): Checking LEFT FACE of AABB
	{
		// NOTE(hugo): Equation of normal is x = BoundingBox.Min.x
		if(Ray.Dir.x != 0.0f)
		{
			float t = (BoundingBox.Min.x - Ray.Start.x) / Ray.Dir.x;
			v3 HitPoint = Ray.Start + t * Ray.Dir;
			bool Hit = (t >= 0.0f && HitPoint.y >= BoundingBox.Min.y && HitPoint.y < BoundingBox.Max.y &&
					HitPoint.z >= BoundingBox.Min.z && HitPoint.z < BoundingBox.Max.z);
			if(Hit)
			{
				return(true);
			}
		}
		else
		{
			//Assert(Ray.Start.x != BoundingBox.Min.x);
		}
	}

	// NOTE(hugo): Checking RIGHT FACE of AABB
	{
		// NOTE(hugo): Equation of normal is x = BoundingBox.Max.x
		if(Ray.Dir.x != 0.0f)
		{
			float t = (BoundingBox.Max.x - Ray.Start.x) / Ray.Dir.x;
			v3 HitPoint = Ray.Start + t * Ray.Dir;
			bool Hit = (t >= 0.0f && HitPoint.y >= BoundingBox.Min.y && HitPoint.y < BoundingBox.Max.y &&
					HitPoint.z >= BoundingBox.Min.z && HitPoint.z < BoundingBox.Max.z);
			if(Hit)
			{
				return(true);
			}
		}
		else
		{
			//Assert(Ray.Start.x != BoundingBox.Max.x);
		}
	}

	// NOTE(hugo): Checking BOTTOM FACE of AABB
	{
		// NOTE(hugo): Equation of normal is y = BoundingBox.Min.y
		if(Ray.Dir.y != 0.0f)
		{
			float t = (BoundingBox.Min.y - Ray.Start.y) / Ray.Dir.y;
			v3 HitPoint = Ray.Start + t * Ray.Dir;
			bool Hit = (t >= 0.0f && HitPoint.x >= BoundingBox.Min.x && HitPoint.x < BoundingBox.Max.x &&
					HitPoint.z >= BoundingBox.Min.z && HitPoint.z < BoundingBox.Max.z);
			if(Hit)
			{
				return(true);
			}
		}
		else
		{
			//Assert(Ray.Start.y != BoundingBox.Min.y);
		}
	}

	// NOTE(hugo): Checking TOP FACE of AABB
	{
		// NOTE(hugo): Equation of normal is y = BoundingBox.Max.y
		if(Ray.Dir.y != 0.0f)
		{
			float t = (BoundingBox.Max.y - Ray.Start.y) / Ray.Dir.y;
			v3 HitPoint = Ray.Start + t * Ray.Dir;
			bool Hit = (t >= 0.0f && HitPoint.x >= BoundingBox.Min.x && HitPoint.x < BoundingBox.Max.x &&
					HitPoint.z >= BoundingBox.Min.z && HitPoint.z < BoundingBox.Max.z);
			if(Hit)
			{
				return(true);
			}
		}
		else
		{
			//Assert(Ray.Start.y != BoundingBox.Max.y);
		}
	}

	return(false);
}

internal void
RayTriangleIntersection(ray Ray, triangle T, vertex* Vertices, hit_record* ClosestHitRecord)
{
	v3 v0 = Vertices[T.Indices[0]].P;
	v3 v1 = Vertices[T.Indices[1]].P;
	v3 v2 = Vertices[T.Indices[2]].P;
	v3 e1 = v1 - v0;
	v3 e2 = v2 - v0;
	v3 TriangleNormal = Normalized(Cross(e1, e2));

	v3 q = Cross(Ray.Dir, e2);
	float a = Dot(q, e1);

	// NOTE(hugo): Is the triangle backface or parallel ?
	if(Dot(TriangleNormal, Ray.Dir) >= 0 || a == 0.0f)
	{
		return;
	}

	v3 s = (Ray.Start - v0) / a;
	v3 r = Cross(s, e1);

	// NOTE(hugo): Compute barycentric coordinates
	v3 BarycentricCoords = {};
	BarycentricCoords.x = Dot(q, s);
	BarycentricCoords.y = Dot(r, Ray.Dir);
	BarycentricCoords.z = 1.0f - BarycentricCoords.x - BarycentricCoords.y;

	// NOTE(hugo): Is the hit inside the triangle ?
	if(BarycentricCoords.x < 0.0f ||
			BarycentricCoords.y < 0.0f ||
			BarycentricCoords.z < 0.0f)
	{
		return;
	}

	float t = Dot(e2, r);
	if(t < 0.0f)
	{
		return;
	}

	// NOTE(hugo): We hit !
	if(t < ClosestHitRecord->t)
	{
		ClosestHitRecord->t = t;
		ClosestHitRecord->P = Ray.Start + t * Ray.Dir;
		v3 n0 = Vertices[T.Indices[0]].N;
		v3 n1 = Vertices[T.Indices[1]].N;
		v3 n2 = Vertices[T.Indices[2]].N;
		v3 N = Normalized(BarycentricCoords.x * n0 + BarycentricCoords.y * n1 + BarycentricCoords.z * n2);
		ClosestHitRecord->N = N;

		// TODO(hugo): Implement material
		ClosestHitRecord->MaterialIndex = T.MatIndex;
	}
}

internal void
RaySphereIntersection(sphere* S, ray Ray, hit_record* ClosestHitRecord)
{
	float A = LengthSqr(Ray.Dir);
	float B = 2.0f * Dot(Ray.Start - S->P, Ray.Dir);
	float C = LengthSqr(Ray.Start - S->P) - S->Radius * S->Radius;
	// NOTE(hugo): The hit equation then becomes
	// A * t * t + B * t + C = 0
	float Delta = B * B - 4.0f * A * C;

	if(Delta < 0)
	{
	}
	else if(Delta == 0.0f)
	{
		float t0 = - B / (2.0f * A);
		if(t0 > 0)
		{
			if(t0 < ClosestHitRecord->t)
			{
				// NOTE(hugo): We hit something forward
				ClosestHitRecord->t = t0;
				ClosestHitRecord->P = Ray.Start + t0 * Ray.Dir;
				// TODO(hugo): @Optim : we know that the norm is radius ?
				ClosestHitRecord->N = Normalized(ClosestHitRecord->P - S->P);
				ClosestHitRecord->MaterialIndex = S->MaterialIndex;
			}
		}
	}
	else // Delta > 0
	{
		// NOTE(hugo): Since A > 0 (it's a norm), we know
		// that t1 < t2
		float t1 = (- B - SquareRoot(Delta)) / (2.0f * A);
		float t2 = (- B + SquareRoot(Delta)) / (2.0f * A);

		if(t1 > 0)
		{
			if(t1 < ClosestHitRecord->t)
			{
				ClosestHitRecord->t = t1;
				ClosestHitRecord->P = Ray.Start + t1 * Ray.Dir;
				// TODO(hugo): @Optim : we know that the norm is radius ?
				ClosestHitRecord->N = Normalized(ClosestHitRecord->P - S->P);
				ClosestHitRecord->MaterialIndex = S->MaterialIndex;
			}
		}
		else if(t2 > 0)
		{
			// NOTE(hugo): We are inside the sphere ??
		}
	}
}

internal kdtree* GetKDTreeFromPool(u32 Index, render_state* RenderState);
internal u32 CreateKDTree(render_state* RenderState);

internal void
RayKdTreeIntersection(ray Ray, kdtree* Node, render_state* RenderState, hit_record* ClosestHitRecord)
{
	rect3 NodeBoundingBox = Node->BoundingBox;
	if(RayHitBoundingBox(Ray, NodeBoundingBox))
	{
		kdtree* Left = GetKDTreeFromPool(Node->LeftIndex, RenderState);
		if(Left)
		{
			RayKdTreeIntersection(Ray, Left, RenderState, ClosestHitRecord);
		}
		kdtree* Right = GetKDTreeFromPool(Node->RightIndex, RenderState);
		if(Right)
		{
			RayKdTreeIntersection(Ray, Right, RenderState, ClosestHitRecord);
		}

		for(u32 TriangleIndex = 0; TriangleIndex < Node->TriangleCount; ++TriangleIndex)
		{
			triangle T = Node->Triangles[TriangleIndex];
			RayTriangleIntersection(Ray, T, RenderState->Vertices, ClosestHitRecord);
		}
	}
}

