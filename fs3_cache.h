#ifndef FS3_CACHE_INCLUDED
#define FS3_CACHE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_cache.h
//  Description    : This is the interface for the sector cache in the FS3
//                   filesystem.
//
//  Author         : Christopher Kurcz
//  Last Modified  : 11/18/2021
//

// Include
#include <fs3_controller.h>
#include <fs3_common.h>

// Defines
#define FS3_DEFAULT_CACHE_SIZE 0x8; // 8 cache entries, by default

// Type Definitions
    // cache entry struct
    typedef struct FS3CacheEntry_{
        int track;
        int sector;
        void *dataBuffer;
        uint32_t countUsed;
    } FS3CacheEntry;

// Cache Functions

int fs3_init_cache(uint16_t cachelines);
    // Initialize the cache with a fixed number of cache lines

int fs3_close_cache(void);
    // Close the cache, freeing any buffers held in it

int fs3_put_cache(FS3TrackIndex trk, FS3SectorIndex sct, void *buf);
    // Put an element in the cache

void * fs3_get_cache(FS3TrackIndex trk, FS3SectorIndex sct);
    // Get an element from the cache (returns NULL if not found)

int fs3_log_cache_metrics(void);
    // Log the metrics for the cache 

#endif
