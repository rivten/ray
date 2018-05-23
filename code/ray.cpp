#include <stdio.h>

#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "rivten.h"
#include "rivten_math.h"
#include "random.h"

#define USE_ENTROPY 0

#if !USE_ENTROPY
#include <time.h>
#include <stdlib.h>
internal float
RandomUnilateral(void)
{
	float Result = (float)(rand()) / (float)(RAND_MAX + 1);
	return(Result);
}
#endif

#define SDL_CHECK(Op) {s32 Result = (Op); Assert(Result == 0);}
#define MAX_FLOAT32 FLT_MAX

global_variable u32 GlobalWindowWidth = 1024;
global_variable u32 GlobalWindowHeight = 1024;
global_variable bool GlobalRunning = true;
global_variable bool GlobalComputed = false;
global_variable u32 GlobalAACount = 20000;


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

struct render_state
{
	u32 SphereCount;
	sphere Spheres[256];

	u32 MaterialCount;
	material Materials[256];

	camera Camera;
	float FocalLength;
	float FoV;
	float AspectRatio;

	random_series Entropy;

	persistent_render_value PersistentRenderValue;
};

struct ray
{
	v3 Start;
	v3 Dir;
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

struct hit_record
{
	v3 P;
	v3 N;
	float t;
	u32 MaterialIndex;
};

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

internal v3
ShootRay(render_state* RenderState, ray Ray)
{
	hit_record ClosestHitRecord = {};
	ClosestHitRecord.t = MAX_FLOAT32;

	for(u32 SphereIndex = 0; SphereIndex < RenderState->SphereCount; ++SphereIndex)
	{
		sphere* S = RenderState->Spheres + SphereIndex;
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
				if(t0 < ClosestHitRecord.t)
				{
					// NOTE(hugo): We hit something forward
					ClosestHitRecord.t = t0;
					ClosestHitRecord.P = Ray.Start + t0 * Ray.Dir;
					// TODO(hugo): @Optim : we know that the norm is radius ?
					ClosestHitRecord.N = Normalized(ClosestHitRecord.P - S->P);
					ClosestHitRecord.MaterialIndex = S->MaterialIndex;
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
				if(t1 < ClosestHitRecord.t)
				{
					ClosestHitRecord.t = t1;
					ClosestHitRecord.P = Ray.Start + t1 * Ray.Dir;
					// TODO(hugo): @Optim : we know that the norm is radius ?
					ClosestHitRecord.N = Normalized(ClosestHitRecord.P - S->P);
					ClosestHitRecord.MaterialIndex = S->MaterialIndex;
				}
			}
			else if(t2 > 0)
			{
				// NOTE(hugo): We are inside the sphere ??
			}
		}

	}
	if(ClosestHitRecord.t < MAX_FLOAT32)
	{
		ray NextRay = {};
		NextRay.Start = ClosestHitRecord.P;
		v3 TargetDiffuse = ClosestHitRecord.N + GetRandomPointInUnitSphere(&RenderState->Entropy);
		v3 TargetSpecular = Reflect(Ray.Dir, ClosestHitRecord.N);
		material* M = RenderState->Materials + ClosestHitRecord.MaterialIndex;

		NextRay.Dir = Normalized(Lerp(TargetSpecular, M->Scatter, TargetDiffuse));
		return(M->Attenuation * Hadamard(M->Albedo, ShootRay(RenderState, NextRay)));
	}
	else
	{
		// NOTE(hugo): No hit : Background color
		float t = 0.5f * (1.0f + Ray.Dir.y);
		return(Lerp(V3(1.0f, 1.0f, 1.0f), t, V3(0.5f, 0.7f, 1.0f)));

	}
}

internal void
RenderBackbuffer(render_state* RenderState, v3* Backbuffer)
{
	float ScreenWidth = RenderState->PersistentRenderValue.ScreenWidth;
	float ScreenHeight = RenderState->PersistentRenderValue.ScreenHeight;
	v3 CameraYAxis = RenderState->PersistentRenderValue.CameraYAxis;

	for(u32 Y = 0; Y < GlobalWindowHeight; ++Y)
	{
		for(u32 X = 0; X < GlobalWindowWidth; ++X)
		{
			v3* Color = Backbuffer + X + Y * GlobalWindowWidth;

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

			*Color += ShootRay(RenderState, Ray);
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


	u32 WindowFlags = SDL_WINDOW_SHOWN;
	SDL_Window* Window = SDL_CreateWindow("PathTracer", 
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			GlobalWindowWidth, GlobalWindowHeight,
			WindowFlags);

	Assert(Window);

	SDL_Surface* Screen = SDL_GetWindowSurface(Window);
	Assert(Screen);

	v3* Backbuffer = AllocateArray(v3, GlobalWindowWidth * GlobalWindowHeight);

	render_state RenderState = {};
	RenderState.Camera.P = V3(0.0f, 0.0f, 10.0f);
	RenderState.Camera.XAxis = V3(1.0f, 0.0f, 0.0f);
	RenderState.Camera.ZAxis = V3(0.0f, 0.0f, 1.0f);
	RenderState.FoV = Radians(75.0f);
	RenderState.FocalLength = 2.0f;
	RenderState.AspectRatio = float(GlobalWindowHeight) / float(GlobalWindowWidth);

	RenderState.PersistentRenderValue.ScreenWidth = 2.0f * RenderState.FocalLength * Tan(0.5f * RenderState.FoV);
	RenderState.PersistentRenderValue.ScreenHeight = RenderState.PersistentRenderValue.ScreenWidth * RenderState.AspectRatio;
	RenderState.PersistentRenderValue.CameraYAxis = Cross(RenderState.Camera.ZAxis, RenderState.Camera.XAxis);

	RenderState.Entropy = RandomSeed(1234);

	PushMaterial(&RenderState, {V3(0.8f, 0.2f, 0.1f), 0.5f, 0.9f});
	PushMaterial(&RenderState, {V3(0.2f, 1.0f, 0.5f), 0.5f, 0.5f});
	PushMaterial(&RenderState, {V3(0.0f, 0.7f, 1.0f), 0.8f, 0.1f});

	PushSphere(&RenderState, {1.0f, V3(-1.0f, 0.5f, 5.0f), 0});
	PushSphere(&RenderState, {2.0f, V3(0.0f, 0.0f, 0.0f), 1});
	PushSphere(&RenderState, {150.0f, V3(0.0f, -152.0f, -10.0f), 2});
	PushSphere(&RenderState, {0.5f, V3(1.5f, -2.2f, 3.0f), 0});
	PushSphere(&RenderState, {3.0f, V3(-4.0f, 1.0f, 0.0f), 1});
	PushSphere(&RenderState, {2.5f, V3(4.0f, 1.0f, 0.0f), 2});

	u32 CurrentAAIndex = 0;

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
			RenderBackbuffer(&RenderState, Backbuffer);
			++CurrentAAIndex;
			for(u32 PixelIndex = 0; PixelIndex < GlobalWindowWidth * GlobalWindowHeight; ++PixelIndex)
			{
				u32* Pixel = (u32*)(Screen->pixels) + PixelIndex;
				v3 Color = Backbuffer[PixelIndex];
				*Pixel = LinearToPixel(Color / float(CurrentAAIndex));
			}
			SDL_UpdateWindowSurface(Window);
			GlobalComputed = true;
		}
	}

	SDL_DestroyWindow(Window);
	SDL_Quit();
	return(0);
}
