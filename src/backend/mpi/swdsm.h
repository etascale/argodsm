/**
 * @file
 * @brief this is a legacy file from the ArgoDSM prototype
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 * @deprecated this file is legacy and will be removed as soon as possible
 * @warning do not rely on functions from this file
 */

#ifndef argo_swdsm_h
#define argo_swdsm_h argo_swdsm_h

/* Includes */
#include <cstdint>
#include <type_traits>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <math.h>
#include <mpi.h>
#include <pthread.h>
#include <omp.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "argo.h"
/** @brief Granularity of coherence unit / pagesize  */
#define GRAN 4096L //page size.

#ifndef CACHELINE
/** @brief Size of a ArgoDSM cacheline in number of pages */
#define CACHELINE 1L
#endif

#ifndef NUM_THREADS
/** @brief Number of maximum local threads in each node */
/**@bug this limits amount of local threads because of pthread barriers being initialized at startup*/
#define NUM_THREADS 128
#endif

/** @brief Read a value and always get the latest - 'Read-Through' */
#ifdef __cplusplus
#define ACCESS_ONCE(x) (*static_cast<std::remove_reference<decltype(x)>::type volatile *>(&(x)))
#else
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#endif

/** @brief Hack to avoid warnings when you have unused variables in a function */
#define UNUSED_PARAM(x) (void)(x)

/** @brief 1 for activating two memory requests in parallel, 0 for disabling*/
#define DUAL_LOAD 1

/** @brief Wrapper for unsigned char - basically a byte */
typedef unsigned char argo_byte;

/** @brief Struct for cache control data */
typedef struct myControlData //global cache control data / directory
{
		/** @brief Coherence state, basically only Valid/Invalid now */
		argo_byte state;    //I/P/SW/MW
		/** @brief Tracks if page is dirty or clean */
		argo_byte dirty;   //Is this locally dirty?  
		/** @brief Tracks address of page */
		unsigned long tag;   //addres of global page in distr mem
} control_data;

/** @brief Struct containing statistics */
typedef struct argo_statisticsStruct
{
		/** @brief Time spend locking */
		double locktime;
		/** @brief Time spent self invalidating */
		double selfinvtime; 
		/** @brief Time spent loading pages */
		double loadtime;
		/** @brief Time spent storing pages */
		double storetime; 
		/** @brief Time spent writing back from the writebuffer */
		double writebacktime; 
		/** @brief Time spent flushing the writebuffer */
		double flushtime; 
		/** @brief Time spent in global barrier */
		double barriertime; 
		/** @brief Number of stores */
		unsigned long stores; 
		/** @brief Number of loads */
		unsigned long loads; 
		/** @brief Number of barriers executed */
		unsigned long barriers; 
		/** @brief Number of writebacks from (full) writebuffer */
		unsigned long writebacks; 
		/** @brief Number of locks */
		int locks;
		/** @brief Time spent performing selective acquire */
		double ssitime;
		/** @brief Time spent performing selective release */
		double ssdtime;
} argo_statistics;

/*constants for control values*/
/** @brief Constant for invalid states */
static const argo_byte INVALID=0;
/** @brief Constant for valid states */
static const argo_byte VALID=1;
/** @brief Constant for clean states */
static const argo_byte CLEAN=2;
/** @brief Constant for dirty states */
static const argo_byte DIRTY=3;
/** @brief Constant for writer states */
static const argo_byte WRITER=4;
/** @brief Constant for reader states */
static const argo_byte READER=5;

/**
 * @brief The size of a hardware memory page
 * @note  This should be better centralized for all
 *        modules and backend implementations
 */
constexpr std::size_t page_size = 4096;

/*Handler*/
/**
 * @brief Catches memory accesses to memory not yet cached in ArgoDSM. Launches remote requests for memory not present.
 * @param sig unused param
 * @param si contains information about faulting instruction such as memory address
 * @param unused is a unused param but needs to be declared
 * @see signal.h
 */
void handler(int sig, siginfo_t *si, void *unused);
/**
 * @brief Sets up ArgoDSM's signal handler
 */
void set_sighandler();

/*ArgoDSM init and finish*/
/**
 * @brief Initializes ArgoDSM runtime
 * @param argo_size Size of wanted global address space in bytes
 * @param cache_size Size in bytes of your cache, will be rounded to nearest multiple of cacheline size (in bytes)
 */
void argo_initialize(std::size_t argo_size, std::size_t cache_size);

/**
 * @brief Shutting down ArgoDSM runtime
 */
void argo_finalize();

/*Global alloc*/
/**
 * @brief Allocates memory in global address space
 * @param size amount of memory to allocate
 * @return pointer to allocated memory or NULL if failed.
 */
void * argo_gmalloc(unsigned long size);

/*Synchronization*/

/**
 * @brief Self-Invalidates all memory that has potential writers
 */
void self_invalidation();

/**
 * @brief Global barrier for ArgoDSM - needs to be called by every thread in the
 *        system that need coherent view of the memory
 * @param n number of local thread participating
 */
void swdsm_argo_barrier(int n);

/**
 * @brief acquire function for ArgoDSM (Acquire according to Release Consistency)
 */
void argo_acquire();
/**
 * @brief Release function for ArgoDSM (Release according to Release Consistency)
 */
void argo_release();

/**
 * @brief acquire-release function for ArgoDSM (Both acquire and release
 *        according to Release Consistency)
 */
void argo_acq_rel();

/**
 * @brief stores a page remotely - only writing back what has been written locally since last synchronization point
 * @param index index in local page cache
 * @param addr address to page in global address space
 */
void storepageDIFF(unsigned long index, unsigned long addr);

/*Statistics*/
/**
 * @brief Clears out all statistics
 */
void clearStatistics();

/**
 * @brief wrapper for MPI_Wtime();
 * @return Wall time
 */
double argo_wtime();

/**
 * @brief Prints collected statistics
 */
void printStatistics();

/**
 * @brief Resets current coherence. Collective function called by all threads on all nodes.
 * @param n number of threads currently spawned on each node. 
 */
void argo_reset_coherence(int n);

/**
 * @brief Gives the ArgoDSM node id for the local process
 * @return Returns the ArgoDSM node id for the local process
 * @deprecated Should use argo_get_nid() instead and eventually remove this
 * @see argo_get_nid()
 */
unsigned int getID();

/**
 * @brief Gives the ArgoDSM node id for the local process
 * @return Returns the ArgoDSM node id for the local process
 */
unsigned int argo_get_nid();

/**
 * @brief Gives number of ArgoDSM nodes
 * @return Number of ArgoDSM nodes
 */
unsigned int argo_get_nodes();

/**
 * @brief returns the maximum number of threads per ArgoDSM node (defined by NUM_THREADS)
 * @return NUM_THREADS 
 * @bug NUM_THREADS is not defined properly. DO NOT USE!
 */
unsigned int getThreadCount();

/**
 * @brief Gives the backing memory offset of the ArgoDSM node for the local process
 * @return Returns the reference to the current offset for the local process
 * @note Implementation-specific function for the first-touch data distribution
 */
std::size_t& get_local_data_offset();

/**
 * @brief Gives a pointer to the global address space
 * @return Start address of the global address space
 */
void *argo_get_global_base();

/**
 * @brief Size of global address space
 * @return Size of global address space
 */
size_t argo_get_global_size();

/**
 * @brief Gives out a local thread ID for a local thread assuming NUM_THREADS per node
 * @return a local thread ID for the local thread 
 */
int argo_get_local_tid();

/**
 * @brief Gives out a global thread ID for a local thread assuming NUM_THREADS per node
 * @return a global thread ID for the local thread 
 * @bug NUM_THREADS is not defined properly. DO NOT USE!
 */
int argo_get_global_tid();
/**
 * @brief Registers the local thread to ArgoDSM and gets a local thread ID.
 * @bug NUM_THREADS is not defined properly. DO NOT USE!
 */
void argo_register_thread();
/**
 * @brief Pins and registers the local thread
 * @see argo_register_thread()
 */
void argo_pin_threads();

/*MPI*/
/**
 * @brief Initializes the MPI environment
 */
void initmpi();
/**
 * @brief Initializes a mpi data structure for writing cacheb control data over the network * @brief 
 */
void init_mpi_struct(void);
/**
 * @brief Initializes a mpi data structure for writing cacheblocks over the network
 */
void init_mpi_cacheblock(void);
/**
 * @brief Checks if something is power of 2
 * @param x a non-negative integer
 * @return 1 if x is 0 or a power of 2, otherwise return 0
 */
unsigned long isPowerOf2(unsigned long x);
/**
 * @brief Gets cacheindex for a given address
 * @param addr Address in the global address space
 * @return cacheindex where addr should map to in the ArgoDSM page cache
 */
unsigned long getCacheIndex(unsigned long addr);
/**
 * @brief Gives homenode for a given address
 * @param addr Address in the global address space
 * @param cloc Used to identify the call location in the code
 * @return Process ID of the node backing the memory containing addr
 */
unsigned long getHomenode(unsigned long addr, char cloc = 0);
/**
 * @brief Gets the offset of an address on the local nodes part of the global memory
 * @param addr Address in the global address space
 * @param cloc Used to identify the call location in the code
 * @return addr-(start address of local process part of global memory)
 */
unsigned long getOffset(unsigned long addr, char cloc = 0);
/**
 * @brief Gives an index to the sharer/writer vector depending on the address
 * @param addr Address in the global address space
 * @return index for sharer vector for the page
 */
unsigned long get_classification_index(uint64_t addr);
#endif /* argo_swdsm_h */

