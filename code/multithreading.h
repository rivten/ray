#pragma once

struct platform_work_queue;
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue *Queue, void *Data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

struct platform_work_queue_entry
{
    platform_work_queue_callback *Callback;
    void *Data;
};

struct platform_work_queue
{
    u32 volatile CompletionGoal;
    u32 volatile CompletionCount;

    u32 volatile NextEntryToWrite;
    u32 volatile NextEntryToRead;
    SDL_sem *SemaphoreHandle;

    platform_work_queue_entry Entries[256];
};

struct sdl_thread_startup
{
    platform_work_queue *Queue;
};

#ifdef _WIN32
#include <intrin.h>
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
inline u32 AtomicCompareExchangeU32(u32 volatile *Value, u32 New, u32 Expected)
{
    u32 Result = _InterlockedCompareExchange((long volatile *)Value, New, Expected);

    return(Result);
}
#else
#define CompletePreviousReadsBeforeFutureReads asm volatile ("" ::: "memory")
#define CompletePreviousWritesBeforeFutureWrites asm volatile ("" ::: "memory")
inline u32 AtomicCompareExchangeU32(u32 volatile* Value, u32 New, u32 Expected)
{
	u32 Result = __sync_val_compare_and_swap(Value, Expected, New);
	return(Result);
}
#endif


// TODO(hugo) : Is it accurate to say that 
// only the main thread can use this function ?
internal void
SDLAddEntry(platform_work_queue* Queue, platform_work_queue_callback* Callback, void* Data)
{
	u32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
	Assert(NewNextEntryToWrite != Queue->NextEntryToRead);
	platform_work_queue_entry* Entry = Queue->Entries + Queue->NextEntryToWrite;
	Entry->Callback = Callback;
	Entry->Data = Data;
	++Queue->CompletionGoal;
	SDL_CompilerBarrier();
	Queue->NextEntryToWrite = NewNextEntryToWrite;
	SDL_SemPost(Queue->SemaphoreHandle);
}

internal bool
SDLDoNextWorkQueueEntry(platform_work_queue* Queue)
{
	bool WeShouldSleep = false;
	u32 OriginalNextEntryToRead = Queue->NextEntryToRead;
	u32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
	if(OriginalNextEntryToRead != Queue->NextEntryToWrite)
	{
		SDL_bool WasSet = SDL_AtomicCAS((SDL_atomic_t *)&Queue->NextEntryToRead,
				OriginalNextEntryToRead, NewNextEntryToRead);
		if(WasSet)
		{
			platform_work_queue_entry Entry = Queue->Entries[OriginalNextEntryToRead];
			Entry.Callback(Queue, Entry.Data);
			SDL_AtomicIncRef((SDL_atomic_t *)&Queue->CompletionCount);
		}
	}
	else
	{
		WeShouldSleep = true;
	}

	return(WeShouldSleep);
}

internal void
SDLCompleteAllWork(platform_work_queue* Queue)
{
	while(Queue->CompletionGoal != Queue->CompletionCount)
	{
		SDLDoNextWorkQueueEntry(Queue);
	}

	Queue->CompletionGoal = 0;
	Queue->CompletionCount = 0;
}

int
ThreadProc(void* Parameter)
{
	sdl_thread_startup* Thread = (sdl_thread_startup *)Parameter;
	platform_work_queue* Queue = Thread->Queue;

	for(;;)
	{
		if(SDLDoNextWorkQueueEntry(Queue))
		{
			SDL_SemWait(Queue->SemaphoreHandle);
		}
	}

#ifndef _WIN32
	// NOTE(hugo) : Triggers a "Unreachable code" warning
	// on MSVC
	return(0);
#endif
}

internal void
SDLMakeQueue(platform_work_queue* Queue, u32 ThreadCount, sdl_thread_startup* Startups)
{
    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;

    Queue->NextEntryToWrite = 0;
    Queue->NextEntryToRead = 0;

    u32 InitialCount = 0;
    Queue->SemaphoreHandle = SDL_CreateSemaphore(InitialCount);

    for(u32 ThreadIndex = 0;
        ThreadIndex < ThreadCount;
        ++ThreadIndex)
    {
        sdl_thread_startup *Startup = Startups + ThreadIndex;
        Startup->Queue = Queue;

        SDL_Thread *ThreadHandle = SDL_CreateThread(ThreadProc, 0, Startup);
        SDL_DetachThread(ThreadHandle);
    }
}

