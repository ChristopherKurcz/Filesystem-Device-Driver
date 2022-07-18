////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_cache.c
//  Description    : This is the implementation of the cache for the 
//                   FS3 filesystem interface.
//
//  Author         : Christopher Kurcz
//  Last Modified  : 12/9/2021
//

// Includes
#include <string.h>
#include <cmpsc311_log.h>
#include <stdlib.h>

// Project Includes
#include <fs3_cache.h>

// Static Global Variables
    FS3CacheEntry* FS3Cache;
    uint16_t cacheSize;
    int cacheCreated = 0;
    uint64_t cacheUseCount;

    // cache metrics
    int cacheInserts;
    int cacheHits;
    int cacheGets;
    int cacheMisses;

// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_init_cache
// Description  : Initialize the cache with a fixed number of cache lines
//
// Inputs       : cachelines - the number of cache lines to include in cache
// Outputs      : 0 if successful, -1 if failure

int fs3_init_cache(uint16_t cachelines) {
    // checks to make sure the number of cache lines is valid
    if(cachelines < 0){
        return(-1);
    }
    // checks that the cache is not created
    if(cacheCreated == 1){
        return(-1);
    }

    // sets global variables
    cacheSize = cachelines;
    cacheCreated = 1;
    cacheUseCount = 0;

    // allocates memory for the cache
    FS3Cache = malloc(cacheSize * sizeof(FS3CacheEntry));

    // for each cache line, memory is allocated for the cache line's data, and variables are initialized
    int i;
    for(i = 0; i < cacheSize; i++){
        FS3Cache[i].dataBuffer = malloc(FS3_SECTOR_SIZE);
        FS3Cache[i].track = -1;
        FS3Cache[i].sector = -1;
        FS3Cache[i].countUsed = 0;
    }

    return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close_cache
// Description  : Close the cache, freeing any buffers held in it
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_close_cache(void)  {
    // checks that the cache is created
    if(cacheCreated == 0){
        return(-1);
    }

    // for each cache line, the cache line's data's memory is deallocated
    int i;
    for(i = 0; i < cacheSize; i++){
        free(FS3Cache[i].dataBuffer);
    }

    // deallocates the memory of the cache
    free(FS3Cache);

    // updates the global cache created variable
    cacheCreated = 0;

    return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_put_cache
// Description  : Put an element in the cache
//
// Inputs       : trk - the track number of the sector to put in cache
//                sct - the sector number of the sector to put in cache
// Outputs      : 0 if inserted, -1 if not inserted

int fs3_put_cache(FS3TrackIndex trk, FS3SectorIndex sct, void *buf) {
    // checks that the cache is created
    if(cacheCreated == 0){
        return(-1);
    }

    // loops through the cache, finding the index of either the least recently used cache line
    //  or the cache line with the correct track and sector
    int i;
    int minUsed = FS3Cache[0].countUsed;
    int putIndex = 0;
    for(i = 0; i < cacheSize; i++){
        // if a match is found, it saves the index and stops the loop
        if((FS3Cache[i].track==trk)&&(FS3Cache[i].sector==sct)){
            putIndex = i;
            break;
        } else {
            // if a less recently used cache line is found, it saves the index
            if(FS3Cache[i].countUsed < minUsed){
                minUsed = FS3Cache[i].countUsed;
                putIndex = i;
            }
        }
    }

    // updates global variables
    cacheInserts = cacheInserts + 1;
    cacheUseCount = cacheUseCount + 1;

    // updates the data in the cache line
    FS3Cache[putIndex].track = trk;
    FS3Cache[putIndex].sector = sct;
    FS3Cache[putIndex].countUsed = cacheUseCount;
    memcpy(FS3Cache[putIndex].dataBuffer, buf, FS3_SECTOR_SIZE);

    return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_get_cache
// Description  : Get an element from the cache (
//
// Inputs       : trk - the track number of the sector to find
//                sct - the sector number of the sector to find
// Outputs      : returns NULL if not found or failed, pointer to buffer if found

void * fs3_get_cache(FS3TrackIndex trk, FS3SectorIndex sct)  {
    // checks that the cache is created
    if(cacheCreated == 0){
        return(NULL);
    }

    // loops through the entire cache, trying to find the cache line with the given track and sector
    int i;
    int getIndex = -1;
    for(i = 0; i < cacheSize; i++){
        // if a match is found, it saves the index and stops the loop
        if((FS3Cache[i].track==trk)&&(FS3Cache[i].sector==sct)){
            getIndex = i;
            break;
        }
    }

    // updates global variables
    cacheGets = cacheGets + 1;

    if(getIndex == -1){
        // if no match was found, updates the number of cache misses
        cacheMisses = cacheMisses + 1;
        return(NULL);
    } else {
        // if a match was found, updates global variables
        cacheHits = cacheHits + 1;
        cacheUseCount = cacheUseCount + 1;

        // updates the data in the cache line
        FS3Cache[getIndex].countUsed = cacheUseCount;

        // returns the pointer to the data
        return(FS3Cache[getIndex].dataBuffer);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_log_cache_metrics
// Description  : Log the metrics for the cache 
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_log_cache_metrics(void) {
    cacheGets = cacheGets + 1;
    // checks to make sure there will not be a divide by zero when calculating the cache hit ratio
    if (cacheGets == 0){
        return(-1);
    }

    // calculates the cache hit ratio
    float cacheHitRatio = ((float)cacheHits) / ((float)cacheGets) * 100;

    // logs the different metrics for the cache
    logMessage(FS3DriverLLevel, "** FS3 cache Metrics **");
    logMessage(FS3DriverLLevel, "Cache inserts    [%9d]",cacheInserts);
    logMessage(FS3DriverLLevel, "Cache gets       [%9d]",cacheGets);
    logMessage(FS3DriverLLevel, "Cache hits       [%9d]",cacheHits);
    logMessage(FS3DriverLLevel, "Cache misses     [%9d]",cacheMisses);
    logMessage(FS3DriverLLevel, "Cache hit ratio  [%8.2f%]",cacheHitRatio);

    return(0);
}
