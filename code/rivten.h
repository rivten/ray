#pragma once

/* NOTE(hugo)
 *    This file contains types, macros and functions I often use.
 */

#include <cstdint>
#include <stdio.h>
#include <string.h> // NOTE(hugo) : for memset

#ifdef __unix__
#include <sys/types.h>
#endif

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef bool b32;
typedef float r32;
typedef double r64;

typedef size_t memory_index;

#define ArrayCount(x) (sizeof((x))/(sizeof((x)[0])))
#define Assert(x) do{if(!(x)){ printf("******\nASSERT FAIL:: FILE: %s, LINE: %i, FUNCTION: %s.\n******\n", __FILE__, __LINE__, __FUNCTION__); *(int*)0=0;}}while(0)

#define global_variable static
// TODO(hugo) : Find out how to restablish this #define
// and then put the internal back on the functions I changed
#define internal static
#define local_persist static

#define InvalidCodePath Assert(!"InvalidCodePath");
#define InvalidDefaultCase default: {InvalidCodePath;} break

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
void ZeroSize(memory_index Size, void* Ptr)
{
	u8* Byte = (u8 *)Ptr;
	while(Size > 0)
	{
		*Byte = 0;
		++Byte;
		--Size;
	}
}

// TODO(hugo) : To get rid of this in order to use only arena framework ?
// {
#include <stdlib.h>
#define ReAllocateArray(Buffer, type, Size) (type *)ReAllocate_(Buffer, (Size) * sizeof(type))
#define AllocateArray(type, Size) (type *)Allocate_((Size) * sizeof(type))
#define AllocateStruct(type) AllocateArray(type, 1)
#define Free(Buffer) free(Buffer)
#define CopyArray(Dest, Source, type, Size) memcpy(Dest, Source, (Size) * sizeof(type))

void* Allocate_(size_t Size)
{
	void* Result = malloc(Size);
	Assert(Result);
	memset(Result, 0, Size);

	return(Result);
}

void* ReAllocate_(void* Buffer, size_t Size)
{
	void* Result = realloc(Buffer, Size);
	Assert(Result);

	return(Result);
}
// }

struct memory_arena
{
	memory_index Size;
	memory_index Used;
	u8* Base;

	u32 TemporaryCount;
};

struct temporary_memory
{
	memory_arena* Arena;
	memory_index Used;
};

void InitialiseArena(memory_arena* Arena, memory_index Size, void* Base)
{
	Arena->Size = Size;
	Arena->Used = 0;
	Arena->TemporaryCount = 0;
	Arena->Base = (u8 *)Base;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
#define PushSize(Arena, Size) PushSize_(Arena, Size)
void* PushSize_(memory_arena* Arena, memory_index Size)
{
	Assert(Arena->Used + Size <= Arena->Size);
	void* Result = Arena->Base + Arena->Used;
	Arena->Used += Size;

	return(Result);
}

temporary_memory BeginTemporaryMemory(memory_arena* Arena)
{
	temporary_memory Result = {};

	Result.Arena = Arena;
	Result.Used = Arena->Used;

	++Arena->TemporaryCount;

	return(Result);
}

void EndTemporaryMemory(temporary_memory TemporaryMemory)
{
	memory_arena* Arena = TemporaryMemory.Arena;
	Assert(Arena->Used >= TemporaryMemory.Used);
	Arena->Used = TemporaryMemory.Used;
	Assert(Arena->TemporaryCount > 0);
	--Arena->TemporaryCount;
}

char* ReadFileContent(const char* Filename)
{
#ifdef _WIN32
	FILE* File = 0;
	fopen_s(&File, Filename, "rb");
#else
	FILE* File = fopen(Filename, "rb");
#endif
	Assert(File);

	fseek(File, 0, SEEK_END);
	size_t FileSize = ftell(File);
	fseek(File, 0, SEEK_SET);
	char* Content = AllocateArray(char, FileSize + 1);
	size_t ReadSize = fread(Content, 1, FileSize, File);
	Assert(ReadSize == FileSize);
	fclose(File);

	Content[FileSize] = 0;

	return(Content);
}

u32 StringLength(const char* Str)
{
	const char* C = Str;
	u32 Length = 0;
	while((*C) != '\0')
	{
		++Length;
		++C;
	}
	return(Length);
}

bool StringEmpty(char* Str)
{
	bool Result = (StringLength(Str) == 0);
	return(Result);
}

bool StringMatch(char* A, char* B)
{
	bool Identical = true;
	if(StringLength(A) != StringLength(B))
	{
		Identical = false;
	}
	else
	{
		char* CharA = A;
		char* CharB = B;
		while(Identical && ((*CharA) != '\0'))
		{
			if(*CharA != *CharB)
			{
				Identical = false;
			}
			++CharA;
			++CharB;
		}
	}
	return(Identical);
}

bool IsCharacterInString(char C, char* Str)
{
	char* StrChar = Str;
	bool Found = false;
	while((!Found) && (*StrChar != '\0'))
	{
		if(*StrChar == C)
		{
			Found = true;
		}
		++StrChar;
	}

	return(Found);
}

void ConsumeToken(char* Token, char** Str, char* Delimiter)
{
	char* InitialToken = Token;
	// NOTE(hugo) : Get rid of first delimiters if any
	char* StrChar = *Str;
	while(IsCharacterInString(*StrChar, Delimiter))
	{
		++StrChar;
	}
	// TODO(hugo) : What to do when the string Str is _only_ containing delimiters ?
	// The rest of this function fails
	if(StringLength(StrChar) > 0)
	{
		while(!IsCharacterInString(*StrChar, Delimiter))
		{
			*Token = *StrChar;
			++Token;
			++StrChar;
		}
		*Token = '\0';
		Token = InitialToken;
		*Str = StrChar;
	}
	else
	{
		Token[0] = '\0';
		*Str = StrChar;
	}
}

struct rvtn_string
{
	u32 Size;
	char* Data;
};

rvtn_string CreateEmptyString(u32 Size, memory_arena* Arena = 0)
{
	rvtn_string Result = {};
	Result.Size = Size;
	if(Arena)
	{
		Result.Data = PushArray(Arena, Result.Size, char);
	}
	else
	{
		Result.Data = AllocateArray(char, Result.Size);
	}

	return(Result);
}

rvtn_string CreateString(char* Str, memory_arena* Arena = 0)
{
	rvtn_string Result = CreateEmptyString(StringLength(Str), Arena);
	for(u32 CharIndex = 0; CharIndex < Result.Size; ++CharIndex)
	{
		Result.Data[CharIndex] = Str[CharIndex];
	}

	return(Result);
};

rvtn_string CreateString(rvtn_string Str, memory_arena* Arena = 0)
{
	rvtn_string Result = CreateEmptyString(Str.Size, Arena);
	for(u32 CharIndex = 0; CharIndex < Result.Size; ++CharIndex)
	{
		Result.Data[CharIndex] = Str.Data[CharIndex];
	}

	return(Result);
};

rvtn_string ConcatString(rvtn_string A, rvtn_string B, memory_arena* Arena = 0)
{
	rvtn_string Result = CreateEmptyString(A.Size + B.Size, Arena);

	for(u32 CharIndex = 0; CharIndex < A.Size; ++CharIndex)
	{
		Result.Data[CharIndex] = A.Data[CharIndex];
	}
	for(u32 CharIndex = 0; CharIndex < B.Size; ++CharIndex)
	{
		Result.Data[A.Size + CharIndex] = B.Data[CharIndex];
	}

	return(Result);
}

void FreeString(rvtn_string* String)
{
	if(String->Data && String->Size > 0)
	{
		Free(String->Data);
	}
	String->Size = 0;
}

bool StringMatch(rvtn_string A, rvtn_string B)
{
	bool Equals = (A.Size == B.Size);
	for(u32 CharIndex = 0; Equals && (CharIndex < A.Size); ++CharIndex)
	{
		Equals = (A.Data[CharIndex] == B.Data[CharIndex]);
	}

	return(Equals);
}

bool StringMatch(rvtn_string A, char* B)
{
	rvtn_string RvtnB = CreateString(B);
	bool Result = StringMatch(A, RvtnB);
	FreeString(&RvtnB);

	return(Result);
}

bool IsCharInString(char C, rvtn_string Str)
{
	bool Found = false;
	for(u32 CharIndex = 0; (!Found) && (CharIndex < Str.Size); ++CharIndex)
	{
		Found = (C == Str.Data[CharIndex]);
	}

	return(Found);
}

bool IsSubstring(rvtn_string Sub, rvtn_string Str)
{
	bool IsSubSmaller = (Sub.Size <= Str.Size);
	bool FoundSub = false;

	if(IsSubSmaller)
	{
		for(u32 CharIndex = 0; (!FoundSub) && (CharIndex < Str.Size); ++CharIndex)
		{
			u32 OtherCharIndex = 0;
			while((Sub.Data[OtherCharIndex] == Str.Data[CharIndex + OtherCharIndex]) && 
					OtherCharIndex < Sub.Size &&
					CharIndex + OtherCharIndex < Str.Size)
			{
				++OtherCharIndex;
			}

			if(OtherCharIndex == Sub.Size)
			{
				FoundSub = true;
			}
		}
	}

	return(FoundSub);
}

// TODO(hugo) : I didn't do the usage code first... shame on me
rvtn_string SubString(rvtn_string Str, u32 FirstIndex, u32 OnePastLastIndex, memory_arena* Arena = 0)
{
	if(FirstIndex > Str.Size)
	{
		FirstIndex = Str.Size;
	}
	if(OnePastLastIndex > Str.Size)
	{
		OnePastLastIndex = Str.Size;
	}
	rvtn_string Result = {};
	if(OnePastLastIndex > FirstIndex)
	{
		Result = CreateEmptyString(OnePastLastIndex - FirstIndex, Arena);
		for(u32 Index = FirstIndex; Index < OnePastLastIndex; ++Index)
		{
			Result.Data[Index - FirstIndex] = Str.Data[Index];
		}
	}
	
	return(Result);
}

bool StringBeginsWith(rvtn_string Str, rvtn_string Begin)
{
	bool IsBeginSmallerThanString = (Begin.Size <= Str.Size);
	bool FoundBeginMismatch = false;

	if(IsBeginSmallerThanString)
	{
		for(u32 CharIndex = 0; 
				(!FoundBeginMismatch) && (CharIndex < Begin.Size); 
				++CharIndex)
		{
			FoundBeginMismatch = (Str.Data[CharIndex] != Begin.Data[CharIndex]);
		}
	}

	return(!FoundBeginMismatch);
}

bool StringEndsWith(rvtn_string Str, rvtn_string End)
{
	bool IsEndSmallerThanString = (End.Size <= Str.Size);
	bool FoundEndMismatch = false;

	if(IsEndSmallerThanString)
	{
		for(u32 CharIndex = 0; 
				(!FoundEndMismatch) && (CharIndex < End.Size); 
				++CharIndex)
		{
			FoundEndMismatch = (Str.Data[Str.Size - 1 - CharIndex] != End.Data[End.Size - 1 - CharIndex]);
		}
	}

	return(!FoundEndMismatch);
}

struct consume_token_result
{
	rvtn_string Token;
	rvtn_string Remain;
};

consume_token_result ConsumeToken(rvtn_string Str, rvtn_string Delimiter, memory_arena* Arena = 0)
{
	consume_token_result Result = {};
	u32 FirstIndex = 0;
	u32 OnePastLastIndex = Str.Size;
	if(StringBeginsWith(Str, Delimiter))
	{
		FirstIndex = Delimiter.Size;
	}

	// NOTE(hugo) : Look for position of next delimiter after the beginning
	bool NextDelimiterFound = false;
	u32 CharIndex = FirstIndex;
	for(;
			(!NextDelimiterFound) && (CharIndex < Str.Size); // TODO(hugo) : Minus Delimiter.Size ?
			++CharIndex)
	{
		bool FoundMismatch = false;
		for(u32 OtherCharIndex = 0;
				(!FoundMismatch) && 
				(OtherCharIndex < Delimiter.Size) && 
				((CharIndex + OtherCharIndex) < Str.Size);
				++OtherCharIndex)
		{
			FoundMismatch = (Str.Data[CharIndex + OtherCharIndex] != Delimiter.Data[OtherCharIndex]);
		}
		if(!FoundMismatch)
		{
			NextDelimiterFound = true;
			break;
		}
	}

	if(NextDelimiterFound)
	{
		OnePastLastIndex = CharIndex;
	}

	Result.Token = SubString(Str, FirstIndex, OnePastLastIndex, Arena);
	Result.Remain = SubString(Str, OnePastLastIndex + Delimiter.Size, Str.Size, Arena);

	return(Result);
}

consume_token_result ConsumeToken(rvtn_string Str, char* Delimiter, memory_arena* Arena = 0)
{
	rvtn_string DelimiterString = CreateString(Delimiter);
	consume_token_result Result = ConsumeToken(Str, DelimiterString, Arena);
	FreeString(&DelimiterString);

	return(Result);
}

char* CString_(rvtn_string Str)
{
	char* Result = AllocateArray(char, Str.Size + 1);
	for(u32 CharIndex = 0; CharIndex < Str.Size; ++CharIndex)
	{
		Result[CharIndex] = Str.Data[CharIndex];
	}
	Result[Str.Size] = 0;
	return(Result);
}

void Print(rvtn_string Str)
{
	char* CStr = CString_(Str);
	printf("%s", CStr);
	printf("\n");
	Free(CStr);
}
