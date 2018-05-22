#include <stdio.h>

#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "rivten.h"
#include "rivten_math.h"

#define SDL_CHECK(Op) {s32 Result = (Op); Assert(Result == 0);}

global_variable u32 GlobalWindowWidth = 1024;
global_variable u32 GlobalWindowHeight = 1024;
global_variable bool GlobalRunning = true;
global_variable bool GlobalComputed = false;

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

	camera Camera;
	float FocalLength;
	float FoV;
	float AspectRatio;

	persistent_render_value PersistentRenderValue;
};

struct ray
{
	v3 Start;
	v3 Dir;
};

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

internal void
DrawBackbuffer(render_state* RenderState, u32* Backbuffer)
{
	float ScreenWidth = RenderState->PersistentRenderValue.ScreenWidth;
	float ScreenHeight = RenderState->PersistentRenderValue.ScreenHeight;
	v3 CameraYAxis = RenderState->PersistentRenderValue.CameraYAxis;

	for(u32 Y = 0; Y < GlobalWindowHeight; ++Y)
	{
		for(u32 X = 0; X < GlobalWindowWidth; ++X)
		{
			u32* Pixel = Backbuffer + X + Y * GlobalWindowWidth;

			ray Ray = {};
			Ray.Start = RenderState->Camera.P;
			v2 PixelRelativeCoordInScreen = V2((float(X) / float(GlobalWindowWidth)) - 0.5f, 0.5f - (float(Y) / float(GlobalWindowHeight)));
			v3 PixelWorldSpace = RenderState->Camera.P - RenderState->FocalLength * RenderState->Camera.ZAxis + 
				PixelRelativeCoordInScreen.x * ScreenWidth * RenderState->Camera.XAxis + PixelRelativeCoordInScreen.y * ScreenHeight * CameraYAxis;
			// TODO(hugo): Do we need to have a normalized direction ? Maybe not...
			Ray.Dir = Normalized(PixelWorldSpace - Ray.Start);

			sphere* S = RenderState->Spheres + 0;
			float A = LengthSqr(Ray.Dir);
			float B = 2.0f * Dot(Ray.Start - S->P, Ray.Dir);
			float C = LengthSqr(Ray.Start - S->P) - S->Radius * S->Radius;
			// NOTE(hugo): The hit equation then becomes
			// A * t * t + B * t + C = 0
			float Delta = B * B - 4.0f * A * C;
			float t = 0.5f * (1.0f + Ray.Dir.y);

			// NOTE(hugo): Background color
			v3 Color = Lerp(V3(1.0f, 1.0f, 1.0f), t, V3(0.5f, 0.7f, 1.0f));

			if(Delta < 0)
			{
			}
			else if(Delta == 0.0f)
			{
				float t0 = - B / (2.0f * A);
				if(t0 > 0)
				{
					// NOTE(hugo): We hit something forward
					v3 HitPoint = Ray.Start + t0 * Ray.Dir;
					// TODO(hugo): @Optim : we know that the norm is radius ?
					v3 HitNormal = Normalized(HitPoint - S->P); 
					Color = 0.5f * (V3(1.0f, 1.0f, 1.0f) + HitNormal);
				}
			}
			else // Delta > 0
			{
				Color =  V3(1.0f, 0.0f, 0.0f);
				// NOTE(hugo): Since A > 0 (it's a norm), we know
				// that t1 < t2
				float t1 = (- B - SquareRoot(Delta)) / (2.0f * A);
				float t2 = (- B + SquareRoot(Delta)) / (2.0f * A);

				if(t1 > 0)
				{
					v3 HitPoint = Ray.Start + t1 * Ray.Dir;
					// TODO(hugo): @Optim : we know that the norm is radius ?
					v3 HitNormal = Normalized(HitPoint - S->P); 
					Color = 0.5f * (V3(1.0f, 1.0f, 1.0f) + HitNormal);
				}
				else if(t2 > 0)
				{
					// NOTE(hugo): We are inside the sphere ??
				}
			}

			*Pixel = RGBToPixel(Color);
		}
	}
}

int main(int ArgumentCount, char** Arguments)
{
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

	u32* Backbuffer = AllocateArray(u32, GlobalWindowWidth * GlobalWindowHeight);

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

	RenderState.Spheres[0].Radius = 2.0f;
	RenderState.Spheres[0].P = V3(0.0f, 0.0f, 5.0f);
	RenderState.SphereCount = 1;

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

		if(!GlobalComputed)
		{
			DrawBackbuffer(&RenderState, Backbuffer);
			memcpy(Screen->pixels, Backbuffer, sizeof(u32) * GlobalWindowWidth * GlobalWindowHeight);
			//u32* FirstPixel = (u32*)Screen->pixels;
			//*FirstPixel = RGBToPixel(V3(1.0f, 0.0f, 0.0f));
			SDL_UpdateWindowSurface(Window);
			GlobalComputed = true;
		}
	}

	SDL_DestroyWindow(Window);
	SDL_Quit();
	return(0);
}
