#pragma once

#define MaxRandomNumber 0xffffffff

struct random_series
{
	u64 Alpha;
	u64 Beta;
};

inline random_series RandomSeed(u64 Alpha, u64 Beta)
{
    random_series Series;

    Series.Alpha = Alpha;
    Series.Beta = Beta;

    return(Series);
}

inline u64
RotateLeft(u64 Value, u8 Rotate)
{
	return((Value << Rotate) | (Value >> (64 - Rotate)));
}

// NOTE(hugo): Using xoroshiro128+
inline u64 RandomNextU64(random_series *Series)
{
	u64 Alpha = Series->Alpha;
	u64 Beta = Series->Beta;
	u64 Result = Alpha + Beta;
	
	Beta = Beta ^ Alpha;
	Series->Alpha = RotateLeft(Alpha, 24) ^ Beta ^ (Beta << 16);
	Series->Beta = RotateLeft(Beta, 37);

    return(Result);
}

inline u32 RandomNextU32(random_series* Series)
{
	u64 NextU64 = RandomNextU64(Series);
	u32 Result = (u32)(NextU64 >> 32);
	return(Result);
}

inline u32 RandomChoice(random_series *Series, u32 ChoiceCount)
{
    u32 Result = (RandomNextU32(Series) % ChoiceCount);
    
    return(Result);
}

inline float RandomUnilateral(random_series *Series)
{
    float Divisor = 1.0f / (float)MaxRandomNumber;
    float Result = Divisor*(float)RandomNextU32(Series);

    return(Result);
}

inline float RandomBilateral(random_series *Series)
{
    float Result = 2.0f*RandomUnilateral(Series) - 1.0f;

    return(Result);
}

inline float RandomBetween(random_series *Series, float Min, float Max)
{
    float Result = Lerp(Min, RandomUnilateral(Series), Max);

    return(Result);
}

inline s32 RandomBetween(random_series *Series, s32 Min, s32 Max)
{
    s32 Result = Min + (s32)(RandomNextU32(Series)%((Max + 1) - Min));

    return(Result);
}

inline v2 RandomBilateralV2(random_series *Series)
{
	v2 Result = V2(RandomBilateral(Series), RandomBilateral(Series));
	return(Result);
}
