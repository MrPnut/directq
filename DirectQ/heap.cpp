/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "quakedef.h"

byte *scratchbuf = NULL;

int TotalSize = 0;
int TotalPeak = 0;
int TotalReserved = 0;

/*
========================================================================================================================

		ZONE MEMORY

========================================================================================================================
*/

#define HEAP_MAGIC 0x35012560


CQuakeZone::CQuakeZone (void)
{
	// prevent this->EnsureHeap from exploding
	this->hHeap = NULL;

	// create it
	this->EnsureHeap ();
}


void *CQuakeZone::Alloc (int size)
{
	this->EnsureHeap ();
	assert (size > 0);

	int *buf = (int *) HeapAlloc (this->hHeap, 0, size + sizeof (int) * 2);

	assert (buf);
	memset (buf, 0, size + sizeof (int) * 2);

	// mark as no-execute; not critical so fail it silently
	// note that HeapAlloc uses VirtualAlloc behind the scenes, so this is valid
	DWORD dwdummy = 0;
	VirtualProtect (buf, size, PAGE_READWRITE, &dwdummy);

	buf[0] = HEAP_MAGIC;
	buf[1] = size;

	this->Size += size;

	if (this->Size > this->Peak) this->Peak = this->Size;

	TotalSize += size;

	if (TotalSize > TotalPeak) TotalPeak = TotalSize;

	return (buf + 2);
}


void CQuakeZone::Free (void *data)
{
	if (!this->hHeap) return;

	if (!data) return;

	int *buf = (int *) data;
	buf -= 2;

	assert (buf[0] == HEAP_MAGIC);

	// this should never happen but let's protect release builds anyway...
	if (buf[0] != HEAP_MAGIC) return;

	this->Size -= buf[1];
	TotalSize -= buf[1];

	BOOL blah = HeapFree (this->hHeap, 0, buf);
	assert (blah);
}


void CQuakeZone::Compact (void)
{
	HeapCompact (this->hHeap, 0);
}


void CQuakeZone::EnsureHeap (void)
{
	if (!this->hHeap)
	{
		this->hHeap = HeapCreate (0, 0x10000, 0);
		assert (this->hHeap);

		this->Size = 0;
		this->Peak = 0;
	}
}


void CQuakeZone::Discard (void)
{
	if (this->hHeap)
	{
		TotalSize -= this->Size;
		this->Size = 0;
		HeapDestroy (this->hHeap);
		this->hHeap = NULL;
	}
}


CQuakeZone::~CQuakeZone (void)
{
	this->Discard ();
}


float CQuakeZone::GetSizeMB (void)
{
	return (((float) this->Size) / 1024.0f) / 1024.0f;
}


/*
========================================================================================================================

		CACHE MEMORY

	Certain objects which are loaded per map can be cached per game as they are reusable.  The cache should
	always be thrown out when the game changes, and may be discardable at any other time.  The cache is just a
	wrapper around the Zone API.

	The cache only grows, never shrinks, so it does not fragment.

========================================================================================================================
*/

typedef struct cacheobject_s
{
	struct cacheobject_s *next;
	void *data;
	char *name;
} cacheobject_t;


CQuakeCache::CQuakeCache (void)
{
	// so that the check in Init is valid
	this->Heap = NULL;
	this->Init ();
}


CQuakeCache::~CQuakeCache (void)
{
	SAFE_DELETE (this->Heap);
}


float CQuakeCache::GetSizeMB (void)
{
	return this->Heap->GetSizeMB ();
}


void CQuakeCache::Init (void)
{
	if (!this->Heap)
		this->Heap = new CQuakeZone ();

	this->Head = NULL;
}


void *CQuakeCache::Alloc (int size)
{
	return this->Heap->Alloc (size);
}


void *CQuakeCache::Alloc (void *data, int size)
{
	void *buf = this->Heap->Alloc (size);
	memcpy (buf, data, size);
	return buf;
}


void *CQuakeCache::Alloc (char *name, void *data, int size)
{
	cacheobject_t *cache = (cacheobject_t *) this->Heap->Alloc (sizeof (cacheobject_t));

	// alloc on the cache
	cache->name = (char *) this->Heap->Alloc (strlen (name) + 1);
	cache->data = this->Heap->Alloc (size);

	// copy in the name
	strcpy (cache->name, name);

	// copy to the cache buffer
	if (data) memcpy (cache->data, data, size);

	// link it in
	cache->next = this->Head;
	this->Head = cache;

	// return from the cache
	return cache->data;
}


void *CQuakeCache::Check (char *name)
{
	for (cacheobject_t *cache = this->Head; cache; cache = cache->next)
	{
		// these should never happen
		if (!cache->name) continue;
		if (!cache->data) continue;

		if (!_stricmp (cache->name, name))
		{
			Con_DPrintf ("Reusing %s from cache\n", cache->name);
			return cache->data;
		}
	}

	// not found in cache
	return NULL;
}


void CQuakeCache::Flush (void)
{
	// reinitialize the cache
	SAFE_DELETE (this->Heap);
	this->Init ();
}


/*
========================================================================================================================

		ZONE MEMORY

	The Zone is now just a wrapper around the new CQuakeZone class and only exists outside the class so that it
	may be called from cvar constructors.

========================================================================================================================
*/

void *Zone_Alloc (int size)
{
	if (!MainZone) MainZone = new CQuakeZone ();

	return MainZone->Alloc (size);
}


void Zone_FreeMemory (void *ptr)
{
	// release this back to the OS
	if (MainZone) MainZone->Free (ptr);
}


void Zone_Compact (void)
{
	if (MainZone) MainZone->Compact ();
}


/*
========================================================================================================================

		HUNK MEMORY

========================================================================================================================
*/

CQuakeHunk::CQuakeHunk (int maxsizemb)
{
	// sizes in KB
	this->MaxSize = maxsizemb * 1024 * 1024;
	this->LowMark = 0;
	this->HighMark = 0;

	TotalReserved += this->MaxSize;

	// reserve the full block but do not commit it yet
	this->BasePtr = (byte *) VirtualAlloc (NULL, this->MaxSize, MEM_RESERVE, PAGE_NOACCESS);

	if (!this->BasePtr)
		Sys_Error ("CQuakeHunk::CQuakeHunk - VirtualAlloc failed on memory pool");

	// commit an initial block
	this->Initialize ();
}


CQuakeHunk::~CQuakeHunk (void)
{
	VirtualFree (this->BasePtr, this->MaxSize, MEM_DECOMMIT);
	VirtualFree (this->BasePtr, 0, MEM_RELEASE);
	TotalSize -= this->LowMark;
	TotalReserved -= this->MaxSize;
}


int CQuakeHunk::GetLowMark (void)
{
	return this->LowMark;
}

void CQuakeHunk::FreeToLowMark (int mark)
{
	TotalSize -= (this->LowMark - mark);
	this->LowMark = mark;
}

float CQuakeHunk::GetSizeMB (void)
{
	return (((float) this->LowMark) / 1024.0f) / 1024.0f;
}


void *CQuakeHunk::Alloc (int size)
{
	if (this->LowMark + size >= this->MaxSize)
	{
		Sys_Error ("CQuakeHunk::Alloc - overflow on \"%s\" memory pool", this->Name);
		return NULL;
	}

	// size might be > the extra alloc size
	if ((this->LowMark + size) > this->HighMark)
	{
		// round to 1MB boundaries
		this->HighMark = (this->LowMark + size + 0xfffff) & ~0xfffff;

		// this will walk over a previously committed region.  i might fix it...
		if (!VirtualAlloc (this->BasePtr + this->LowMark, this->HighMark - this->LowMark, MEM_COMMIT, PAGE_READWRITE))
		{
			Sys_Error ("CQuakeHunk::Alloc - VirtualAlloc failed for \"%s\" memory pool", this->Name);
			return NULL;
		}
	}

	// fix up pointers and return what we got
	byte *buf = this->BasePtr + this->LowMark;
	this->LowMark += size;

	TotalSize += size;

	if (TotalSize > TotalPeak) TotalPeak = TotalSize;

	return buf;
}

void CQuakeHunk::Free (void)
{
	// decommit all memory
	VirtualFree (this->BasePtr, this->MaxSize, MEM_DECOMMIT);
	TotalSize -= this->LowMark;

	// recommit the initial block
	this->Initialize ();
}


void CQuakeHunk::Initialize (void)
{
	// commit an initial page of 64k
	VirtualAlloc (this->BasePtr, 0x10000, MEM_COMMIT, PAGE_READWRITE);

	this->LowMark = 0;
	this->HighMark = 0x10000;
}


/*
========================================================================================================================

		INITIALIZATION

========================================================================================================================
*/

CQuakeHunk *MainHunks[2] = {NULL, NULL};

CQuakeHunk *MainHunk = NULL;
CQuakeCache *MainCache = NULL;
CQuakeZone *MainZone = NULL;

// keep these zones separate so that we can release memory correctly at the appropriate times
CQuakeZone *ServerZone = NULL;
CQuakeZone *RenderZone = NULL;
CQuakeZone *ClientZone = NULL;
CQuakeZone *ModelZone = NULL;


void Heap_Init (void)
{
	// init the pools we want to keep around all the time
	if (!MainHunk) MainHunk = new CQuakeHunk (256);
	if (!MainCache) MainCache = new CQuakeCache ();
	if (!MainZone) MainZone = new CQuakeZone ();

	// take a chunk of memory for use by temporary loading functions and other doo-dahs
	scratchbuf = (byte *) Zone_Alloc (SCRATCHBUF_SIZE);
}


/*
========================================================================================================================

		REPORTING

========================================================================================================================
*/

extern CQuakeCache *SoundCache;
extern CQuakeZone *SoundHeap;
extern CQuakeZone *IPLogZone;
extern CQuakeZone *PrecacheHeap;

void Heap_Report_f (void)
{
	Con_Printf ("Memory Usage:\n\n");

	if (MainZone) Con_Printf ("      Zone %6.2f MB\n", (MainZone->GetSizeMB () + IPLogZone->GetSizeMB ()));
	if (MainHunk) Con_Printf ("      Hunk %6.2f MB\n", MainHunk->GetSizeMB ());
	if (GameZone) Con_Printf ("      Game %6.2f MB\n", GameZone->GetSizeMB ());
	if (ServerZone) Con_Printf ("    Server %6.2f MB\n", ServerZone->GetSizeMB ());
	if (ClientZone) Con_Printf ("    Client %6.2f MB\n", (ClientZone->GetSizeMB () + PrecacheHeap->GetSizeMB ()));
	if (ClientZone) Con_Printf ("     Sound %6.2f MB\n", (SoundCache->GetSizeMB () + SoundHeap->GetSizeMB ()));
	if (RenderZone) Con_Printf ("  Renderer %6.2f MB\n", RenderZone->GetSizeMB ());
	if (ModelZone) Con_Printf ("    Models %6.2f MB\n", ModelZone->GetSizeMB ());
	if (MainCache) Con_Printf ("     Cache %6.2f MB\n", MainCache->GetSizeMB ());

	Con_Printf ("\n");
	Con_Printf ("     Total %6.2f MB\n", ((((float) TotalSize) / 1024.0f) / 1024.0f));
	Con_Printf ("\n");
}


cmd_t Heap_Report_Cmd ("heap_report", Heap_Report_f);

