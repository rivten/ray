#include <stdio.h>

#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "rivten.h"
#include "rivten_math.h"

#define SDL_CHECK(Op) {s32 Result = (Op); Assert(Result == 0);}

global_variable u32 GlobalWindowWidth = 512;
global_variable u32 GlobalWindowHeight = 512;
global_variable bool GlobalRunning = true;

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

struct render_state
{
	u32 SphereCount;
	sphere Spheres[256];

	camera Camera;
	float FocalLength;
	float FoV;
	float AspectRatio;
};

struct ray
{
	v3 Start;
	v3 Dir;
};

internal void
DrawBackbuffer(render_state* RenderState, u32* Backbuffer)
{
	for(u32 Y = 0; Y < GlobalWindowHeight; ++Y)
	{
		for(u32 X = 0; X < GlobalWindowWidth; ++X)
		{
			u32* Pixel = Backbuffer + X + Y * GlobalWindowWidth;

			// TODO(hugo): Most of this can be done outside the loop
			ray Ray = {};
			Ray.Start = RenderState->Camera.P;
			v2 PixelRelativeCoordInScreen = V2((float(X) / float(GlobalWindowWidth)) - 0.5f, (float(Y) / float(GlobalWindowHeight)) - 0.5f);
			float ScreenWidth = 2.0f * RenderState->FocalLength * Tan(0.5f * RenderState->FoV);
			float ScreenHeight = ScreenWidth * RenderState->AspectRatio;
			v3 CameraYAxis = Cross(RenderState->Camera.ZAxis, RenderState->Camera.XAxis);
			v3 PixelWorldSpace = RenderState->Camera.P - RenderState->FocalLength * RenderState->Camera.ZAxis + 
				PixelRelativeCoordInScreen.x * ScreenWidth * RenderState->Camera.XAxis + PixelRelativeCoordInScreen.y * ScreenHeight * CameraYAxis;
			// TODO(hugo): Do we need to have a normalized direction ? Maybe not...
			Ray.Dir = Normalized(PixelWorldSpace - Ray.Start);

#if 0
			u8 R = (u32)(255.0f * (float(X) / float(GlobalWindowWidth))) & 0xFF;
			u8 G = (u32)(255.0f * (float(Y) / float(GlobalWindowHeight))) & 0xFF;
			u8 B = 40;
#else
			u8 R = 0;
			u8 G = 0;
			u8 B = (u32)(255.0f * (0.5f * (- Ray.Dir.y) + 0.5f)) & 0xFF;
#endif
			*Pixel = RGBToPixel(R, G, B);
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


		DrawBackbuffer(&RenderState, Backbuffer);
		memcpy(Screen->pixels, Backbuffer, sizeof(u32) * GlobalWindowWidth * GlobalWindowHeight);
		SDL_UpdateWindowSurface(Window);
	}

	SDL_DestroyWindow(Window);
	SDL_Quit();
	return(0);
}
