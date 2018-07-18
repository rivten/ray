#include <stdio.h>

#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "rivten.h"
#include "rivten_math.h"
#include "random.h"
#include "kdtree.h"

#define USE_ENTROPY 0
#define RAY_COMPUTE_VARIATION 1

#if !USE_ENTROPY
#include <time.h>
#include <stdlib.h>
internal float
RandomUnilateral(void)
{
	float Result = (float)(rand()) / (float)(RAND_MAX);
	return(Result);
}
#endif

#define SDL_CHECK(Op) {s32 Result = (Op); Assert(Result == 0);}
#define MAX_FLOAT32 FLT_MAX

#if 0
global_variable u32 GlobalWindowWidth = 1024;
global_variable u32 GlobalWindowHeight = 1024;
#else
global_variable u32 GlobalWindowWidth = 512;
global_variable u32 GlobalWindowHeight = 512;
#endif
global_variable bool GlobalRunning = true;
global_variable bool GlobalComputed = false;
global_variable u32 GlobalAACount = 20000;

global_variable u32 GlobalChunkWidth = 64;
global_variable u32 GlobalChunkHeight = 64;

#if 1
// NOTE(hugo): DEBUG DATA
global_variable u32 DEBUGRayCount = 0;
global_variable u64 DEBUGCycleCountPass = 0;
#endif

internal u32
RGBToPixel(u8 R, u8 G, u8 B)
{
	u8 A = 0xFF;
	u32 Result = (A << 24) |
			     (R << 16) |
			     (G << 8)  |
			     (B << 0);

	return(Result);
}

internal u32
RGBToPixel(v3 Color)
{
	u8 R = (u8)(u32(255.99f * Color.r) & 0xFF);
	u8 G = (u8)(u32(255.99f * Color.g) & 0xFF);
	u8 B = (u8)(u32(255.99f * Color.b) & 0xFF);
	return(RGBToPixel(R, G, B));
}

internal u32
LinearToPixel(v3 Color)
{
	v3 GammaCorrectedColor = V3(SquareRoot(Color.x),
			SquareRoot(Color.y), SquareRoot(Color.z));
	return(RGBToPixel(GammaCorrectedColor));
}

internal v3
LinearToSRGB(v3 Linear)
{
	v3 GammaCorrectedColor = V3(SquareRoot(Linear.x),
			SquareRoot(Linear.y), SquareRoot(Linear.z));
	return(GammaCorrectedColor);
}

struct camera
{
	v3 P;
	v3 XAxis;
	v3 ZAxis;
};

struct sphere
{
	float Radius;
	v3 P;
	u32 MaterialIndex;
};

struct material
{
	v3 Albedo;
	float Attenuation;
	float Scatter; // NOTE(hugo): 0 : No scatter (full specular), 1 : full diffuse
};

struct persistent_render_value
{
	float ScreenWidth;
	float ScreenHeight;
	v3 CameraYAxis;
};

#include "multithreading.h"

struct render_state;
struct shoot_ray_block_data
{
	v3* BackbufferChunk;
	render_state* RenderState;
	u32 ChunkStartX;
	u32 ChunkStartY;
	u32 RayCount;
};

struct render_state
{
	memory_arena Arena;
	temporary_memory TempMemory;
	u32 SphereCount;
	sphere Spheres[256];

	kdtree* Trees;
	u32 TreeCount;
	u32 TreeMaxPoolCount;

	u32 VertexCount;
	vertex* Vertices;

	u32 MaterialCount;
	material Materials[256];

	camera Camera;
	float FocalLength;
	float FoV;
	float AspectRatio;

	platform_work_queue Queue;

	u32 ShootRayChunkCount;
	shoot_ray_block_data ShootRayChunkPool[256];

	random_series Entropy;

	persistent_render_value PersistentRenderValue;
};

struct ray
{
	v3 Start;
	v3 Dir;
};

struct hit_record
{
	v3 P;
	v3 N;
	float t;
	u32 MaterialIndex;
};

inline v3
Reflect(v3 V, v3 N)
{
	return(V - 2.0f * Dot(V, N) * N);
}

internal void
PushSphere(render_state* RenderState, sphere S)
{
	Assert(RenderState->SphereCount < ArrayCount(RenderState->Spheres));
	RenderState->Spheres[RenderState->SphereCount] = S;
	++RenderState->SphereCount;
}

internal void
PushMaterial(render_state* RenderState, material M)
{
	Assert(RenderState->MaterialCount < ArrayCount(RenderState->Materials));
	RenderState->Materials[RenderState->MaterialCount] = M;
	++RenderState->MaterialCount;
}

inline float
GetDeltaHitSphere(sphere* S, ray* R)
{
	float A = LengthSqr(R->Dir);
	float B = 2.0f * Dot(R->Start - S->P, R->Dir);
	float C = LengthSqr(R->Start - S->P) - S->Radius * S->Radius;
	// NOTE(hugo): The hit equation then becomes
	// A * t * t + B * t + C = 0
	float Result = B * B - 4.0f * A * C;
	return(Result);
}

internal v3
GetRandomPointInUnitSphere(random_series* Entropy)
{
	v3 Result = {};
	do
	{
#if USE_ENTROPY
		float x = RandomUnilateral(Entropy);
		float y = RandomUnilateral(Entropy);
		float z = RandomUnilateral(Entropy);
#else
		float x = RandomUnilateral();
		float y = RandomUnilateral();
		float z = RandomUnilateral();
#endif
		Result = 2.0f * V3(x, y, z) - V3(1.0f, 1.0f, 1.0f);
	} while(LengthSqr(Result) >= 1.0f);
	return(Result);
}

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
		ClosestHitRecord->MaterialIndex = 0;
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

#include "kdtree.cpp"

struct ray_context
{
	u32 Depth;
	u32 RayShot;
	v3 Throughput;
};

// TODO(hugo): Maybe unroll this recursive call ?
internal v3
ShootRay(render_state* RenderState, ray Ray, ray_context* Context)
{
	++Context->RayShot;
	hit_record ClosestHitRecord = {};
	ClosestHitRecord.t = MAX_FLOAT32;

#if 0
	for(u32 SphereIndex = 0; SphereIndex < RenderState->SphereCount; ++SphereIndex)
	{
		sphere* S = RenderState->Spheres + SphereIndex;
		RaySphereIntersection(S, Ray, &ClosestHitRecord);
	}
#endif
	RayKdTreeIntersection(Ray, &RenderState->Trees[0], RenderState, &ClosestHitRecord);

	if(ClosestHitRecord.t < MAX_FLOAT32)
	{
		ray NextRay = {};
		NextRay.Start = ClosestHitRecord.P;
		v3 TargetDiffuse = ClosestHitRecord.N + GetRandomPointInUnitSphere(&RenderState->Entropy);
		v3 TargetSpecular = Reflect(Ray.Dir, ClosestHitRecord.N);
		material* M = RenderState->Materials + ClosestHitRecord.MaterialIndex;

		NextRay.Dir = Normalized(Lerp(TargetSpecular, M->Scatter, TargetDiffuse));

		v3 RayColor = M->Attenuation * M->Albedo;

		// NOTE(hugo): Russian Roulette Path Termination
		// {
		Context->Throughput = Hadamard(Context->Throughput, RayColor);
		float RussianRouletteP = Maxf(Context->Throughput.x, Maxf(Context->Throughput.y, Context->Throughput.y));
		if(Context->Depth > 0)
		{
			if(RandomUnilateral() > RussianRouletteP)
			{
				return(RayColor);
			}
		}

		Context->Throughput *= 1.0f / RussianRouletteP;
		++Context->Depth;
		// }

		return(Hadamard(RayColor, ShootRay(RenderState, NextRay, Context)));
	}
	else
	{
		// NOTE(hugo): No hit : Background color
		float t = 0.5f * (1.0f + Ray.Dir.y);
		return(Lerp(V3(1.0f, 1.0f, 1.0f), t, V3(0.5f, 0.7f, 1.0f)));

	}
}

PLATFORM_WORK_QUEUE_CALLBACK(ShootRayChunk)
{
	shoot_ray_block_data* ShootRayChunkData = (shoot_ray_block_data *)Data;

	render_state* RenderState = ShootRayChunkData->RenderState;
	float ScreenWidth = RenderState->PersistentRenderValue.ScreenWidth;
	float ScreenHeight = RenderState->PersistentRenderValue.ScreenHeight;
	v3 CameraYAxis = RenderState->PersistentRenderValue.CameraYAxis;

	u32 StartX = ShootRayChunkData->ChunkStartX;
	u32 EndX = StartX + GlobalChunkWidth;
	u32 StartY = ShootRayChunkData->ChunkStartY;
	u32 EndY = StartY + GlobalChunkHeight;
	for(u32 Y = StartY; Y < EndY; ++Y)
	{
		for(u32 X = StartX; X < EndX; ++X)
		{
			v3* Color = ShootRayChunkData->BackbufferChunk + (X - StartX) + (Y - StartY) * GlobalChunkWidth;

			ray Ray = {};
			Ray.Start = RenderState->Camera.P;
#if USE_ENTROPY
			float XOffset = RandomUnilateral(&RenderState->Entropy);
			float YOffset = RandomUnilateral(&RenderState->Entropy);
#else
			float XOffset = RandomUnilateral();
			float YOffset = RandomUnilateral();
#endif
			Assert(XOffset >= 0.0f && XOffset <= 1.0f);
			Assert(YOffset >= 0.0f && YOffset <= 1.0f);
			v2 PixelRelativeCoordInScreen = V2(((float(X) + XOffset) / float(GlobalWindowWidth)) - 0.5f, 0.5f - ((float(Y) + YOffset) / float(GlobalWindowHeight)));
			v3 PixelWorldSpace = RenderState->Camera.P - RenderState->FocalLength * RenderState->Camera.ZAxis +
				PixelRelativeCoordInScreen.x * ScreenWidth * RenderState->Camera.XAxis + PixelRelativeCoordInScreen.y * ScreenHeight * CameraYAxis;
			// TODO(hugo): Do we need to have a normalized direction ? Maybe not...
			Ray.Dir = Normalized(PixelWorldSpace - Ray.Start);

			ray_context Context = {};
			Context.Throughput = V3(1.0f, 1.0f, 1.0f);
			Context.RayShot = 0;
			*Color += ShootRay(RenderState, Ray, &Context);

			ShootRayChunkData->RayCount += Context.RayShot;
		}
	}
}

internal shoot_ray_block_data*
GetShootRayChunkData(render_state* RenderState)
{
	Assert(RenderState->ShootRayChunkCount < ArrayCount(RenderState->ShootRayChunkPool));
	shoot_ray_block_data* Result = RenderState->ShootRayChunkPool + RenderState->ShootRayChunkCount;
	*Result = {};
	++RenderState->ShootRayChunkCount;
	return(Result);
}

#define KD_TREE_NO_CHILD 0xFFFFFFFF
internal u32
CreateKDTree(render_state* RenderState)
{
	Assert(RenderState->TreeCount < RenderState->TreeMaxPoolCount);
	u32 Result = RenderState->TreeCount;

	kdtree* Tree = RenderState->Trees + RenderState->TreeCount;
	Tree->LeftIndex = KD_TREE_NO_CHILD;
	Tree->RightIndex = KD_TREE_NO_CHILD;

	++RenderState->TreeCount;
	return(Result);
}

internal kdtree*
GetKDTreeFromPool(u32 Index, render_state* RenderState)
{
	if(Index < RenderState->TreeCount)
	{
		kdtree* Result = RenderState->Trees + Index;
		return(Result);
	}
	return(0);
}

internal void
RenderBackbuffer(render_state* RenderState)
{
	Assert(GlobalWindowHeight % GlobalChunkHeight == 0);
	Assert(GlobalWindowWidth % GlobalChunkWidth == 0);
	u32 YChunkCount = GlobalWindowHeight / GlobalChunkHeight;
	u32 XChunkCount = GlobalWindowWidth / GlobalChunkWidth;
	for(u32 YChunk = 0; YChunk < YChunkCount; ++YChunk)
	{
		for(u32 XChunk = 0; XChunk < XChunkCount; ++XChunk)
		{
			shoot_ray_block_data* ShootRayChunkData = GetShootRayChunkData(RenderState);
			ShootRayChunkData->ChunkStartX = GlobalChunkWidth * XChunk;
			ShootRayChunkData->ChunkStartY = GlobalChunkHeight * YChunk;
			ShootRayChunkData->BackbufferChunk = PushArray(&RenderState->Arena, GlobalChunkWidth * GlobalChunkHeight, v3, Align(64, true));
			ShootRayChunkData->RenderState = RenderState;
			SDLAddEntry(&RenderState->Queue, ShootRayChunk, ShootRayChunkData);
		}
	}
}

int main(int ArgumentCount, char** Arguments)
{
#if !USE_ENTROPY
	srand(time(NULL));
#endif

	u32 SDLInitParams = SDL_INIT_EVERYTHING;
	SDL_CHECK(SDL_Init(SDLInitParams));

	printf("Cache line size = %dB\n", SDL_GetCPUCacheLineSize());

	u32 WindowFlags = SDL_WINDOW_SHOWN;
	SDL_Window* Window = SDL_CreateWindow("PathTracer", 
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			GlobalWindowWidth, GlobalWindowHeight,
			WindowFlags);

	Assert(Window);

	SDL_Surface* Screen = SDL_GetWindowSurface(Window);
	Assert(Screen);

	render_state RenderState = {};

	{
		memory_index ArenaSize = Gigabytes(2);
		void* ArenaBase = Allocate_(ArenaSize);
		InitialiseArena(&RenderState.Arena, ArenaSize, ArenaBase);
	}
	//RenderState.Camera.P = V3(0.0f, 0.0f, 10.0f);
	RenderState.Camera.P = V3(0.0f, 1.5f, 5.0f);
	RenderState.Camera.XAxis = V3(1.0f, 0.0f, 0.0f);
	RenderState.Camera.ZAxis = V3(0.0f, 0.0f, 1.0f);
	RenderState.FoV = Radians(75.0f);
	RenderState.FocalLength = 2.0f;
	RenderState.AspectRatio = float(GlobalWindowHeight) / float(GlobalWindowWidth);

	RenderState.PersistentRenderValue.ScreenWidth = 2.0f * RenderState.FocalLength * Tan(0.5f * RenderState.FoV);
	RenderState.PersistentRenderValue.ScreenHeight = RenderState.PersistentRenderValue.ScreenWidth * RenderState.AspectRatio;
	RenderState.PersistentRenderValue.CameraYAxis = Cross(RenderState.Camera.ZAxis, RenderState.Camera.XAxis);

	RenderState.Entropy = RandomSeed(1234);

	RenderState.TreeMaxPoolCount = 2 * 2048;
	RenderState.Trees = PushArray(&RenderState.Arena, RenderState.TreeMaxPoolCount, kdtree);
	RenderState.TreeCount = 0;
	RenderState.Trees[0] = LoadKDTreeFromFile("../data/teapot_with_normal.obj", &RenderState);

	RenderState.ShootRayChunkCount = 0;

	v3* Backbuffer = PushArray(&RenderState.Arena, GlobalWindowWidth * GlobalWindowHeight, v3);
#if RAY_COMPUTE_VARIATION
	v3* PreviousScreen = PushArray(&RenderState.Arena, GlobalWindowWidth * GlobalWindowHeight, v3);
#endif

	// NOTE(hugo): Multithreading init
	RenderState.Queue = {};
	sdl_thread_startup Startups[4] = {};
	SDLMakeQueue(&RenderState.Queue, ArrayCount(Startups), Startups);

	PushMaterial(&RenderState, {V3(0.8f, 0.2f, 0.1f), 0.5f, 0.9f});
	PushMaterial(&RenderState, {V3(0.2f, 1.0f, 0.5f), 0.5f, 0.5f});
	PushMaterial(&RenderState, {V3(0.0f, 0.7f, 1.0f), 0.8f, 0.1f});
	PushMaterial(&RenderState, {V3(0.0f, 0.7f, 1.0f), 0.4f, 0.8f});
	PushMaterial(&RenderState, {V3(0.8f, 0.6f, 0.2f), 0.6f, 0.7f});

	PushSphere(&RenderState, {1.0f, V3(-1.0f, 0.5f, 5.0f), 0});
	PushSphere(&RenderState, {2.0f, V3(0.0f, 0.0f, 0.0f), 1});
	PushSphere(&RenderState, {150.0f, V3(0.0f, -152.0f, -10.0f), 3});
	PushSphere(&RenderState, {0.5f, V3(1.5f, -2.2f, 3.0f), 2});
	PushSphere(&RenderState, {3.0f, V3(-4.0f, 1.0f, 0.0f), 1});
	PushSphere(&RenderState, {2.5f, V3(4.0f, 1.0f, 0.0f), 4});

	u32 CurrentAAIndex = 0;

	double PerformanceFrequency = double(SDL_GetPerformanceFrequency());

	while(GlobalRunning)
	{
		// NOTE(hugo): Input
		// {
		SDL_Event Event = {};
		while(SDL_PollEvent(&Event))
		{
			switch(Event.type)
			{
				case SDL_QUIT:
					{
						GlobalRunning = false;
					} break;
				default:
					{
					} break;
			}
		}
		// }

		if(CurrentAAIndex < GlobalAACount)
		{
			printf("Rendering pass %i\n", CurrentAAIndex);
			DEBUGRayCount = 0;
			DEBUGCycleCountPass = SDL_GetPerformanceCounter();
			RenderState.TempMemory = BeginTemporaryMemory(&RenderState.Arena);
			RenderBackbuffer(&RenderState);

			SDLCompleteAllWork(&RenderState.Queue);
			for(u32 WorkIndex = 0; WorkIndex < RenderState.ShootRayChunkCount; ++WorkIndex)
			{
				shoot_ray_block_data* WorkData = RenderState.ShootRayChunkPool + WorkIndex;

				// NOTE(hugo): Gathering each chunk from every work and putting them into the Backbuffer
				// {
				for(u32 Y = 0; Y < GlobalChunkHeight; ++Y)
				{
					for(u32 X = 0; X < GlobalChunkWidth; ++X)
					{
						v3* BackbufferPixel = Backbuffer
							+ WorkData->ChunkStartX + X
							+ (WorkData->ChunkStartY + Y) * GlobalWindowWidth;
						v3* WorkDataPixel = WorkData->BackbufferChunk + X + Y * GlobalChunkWidth;
						*BackbufferPixel += *WorkDataPixel;
					}
				}
				// }
				DEBUGRayCount += WorkData->RayCount;
			}
			EndTemporaryMemory(RenderState.TempMemory);
			RenderState.ShootRayChunkCount = 0;

			{
				u64 CurrentCycleCountPass = SDL_GetPerformanceCounter();
				u64 CyclesPass = CurrentCycleCountPass - DEBUGCycleCountPass;
				double ElapsedMS = 1000.0f * double(CyclesPass) / PerformanceFrequency;
				printf("\tPass %i rendered. %u rays. %fms. %f rays per ms.\n",
						CurrentAAIndex,
						DEBUGRayCount, ElapsedMS, float(DEBUGRayCount) / ElapsedMS);
			}

			++CurrentAAIndex;

#if RAY_COMPUTE_VARIATION
			float BufferVariation = 0.0f;
#endif
			for(u32 PixelIndex = 0; PixelIndex < GlobalWindowWidth * GlobalWindowHeight; ++PixelIndex)
			{
				u32* Pixel = (u32*)(Screen->pixels) + PixelIndex;
				v3 Color = Backbuffer[PixelIndex];
				v3 SRGBColor = LinearToSRGB(Color / float(CurrentAAIndex));
#if RAY_COMPUTE_VARIATION
			{
				BufferVariation += LengthSqr(SRGBColor - PreviousScreen[PixelIndex]);
				PreviousScreen[PixelIndex] = SRGBColor;
			}
#endif
				*Pixel = RGBToPixel(SRGBColor);
			}

#if RAY_COMPUTE_VARIATION
			printf("\tVariation = %f\n", BufferVariation);
#endif

			SDL_UpdateWindowSurface(Window);
			GlobalComputed = true;
		}
	}

	SDL_DestroyWindow(Window);
	SDL_Quit();
	return(0);
}
