/**
 * @file
 * @brief this is a legacy file from the ArgoDSM prototype
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 * @deprecated this file is legacy and will be removed as soon as possible
 * @warning do not rely on functions from this file
 */

#ifndef SRC_BACKEND_MPI_SWDSM_H_
#define SRC_BACKEND_MPI_SWDSM_H_

// C headers
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
// C++ headers
#include <cstdint>
#include <type_traits>

#include "argo.h"
#include "backend/backend.hpp"

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

/** @brief Wrapper for unsigned char - basically a byte */
typedef unsigned char argo_byte;

/** @brief Struct for cache control data */
typedef struct myControlData //global cache control data / directory
{
		/** @brief Coherence state, basically only Valid/Invalid now */
		argo_byte state;    // I/P/SW/MW
		/** @brief Tracks if page is dirty or clean */
		argo_byte dirty;   // is this locally dirty?
		/** @brief Tracks address of page */
		std::uintptr_t tag;   // address of global page in distr memory
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
		std::size_t stores;
		/** @brief Number of loads */
		std::size_t loads;
		/** @brief Number of barriers executed */
		std::size_t barriers;
		/** @brief Number of writebacks from (full) writebuffer */
		std::size_t writebacks;
		/** @brief Number of locks */
		int locks;
		/** @brief Time spent performing selective acquire */
		double ssitime;
		/** @brief Time spent performing selective release */
		double ssdtime;
} argo_statistics;

/*constants for control values*/
/** @brief Constant for invalid states */
static const argo_byte INVALID = 0;
/** @brief Constant for valid states */
static const argo_byte VALID = 1;
/** @brief Constant for clean states */
static const argo_byte CLEAN = 2;
/** @brief Constant for dirty states */
static const argo_byte DIRTY = 3;
/** @brief Constant for writer states */
static const argo_byte WRITER = 4;
/** @brief Constant for reader states */
static const argo_byte READER = 5;

/**
 * @brief The size of a hardware memory page
 * @note  This should be better centralized for all
 *        modules and backend implementations
 */
constexpr std::size_t page_size = 4096;

/* External declarations */
/**
 * @brief Argo cache data structure
 * @deprecated Should be replaced with a cache API
 */
extern control_data* cacheControl;
/**
 * @brief globalSharers is needed to access and modify the pyxis directory
 * @deprecated Should eventually be handled by a cache module
 * @see swdsm.cpp
 */
extern std::uint64_t* globalSharers;
/**
 * @brief A cache mutex protects all operations on cacheControl
 * @deprecated Should eventually be handled by a cache module
 * @see swdsm.cpp
 */
extern pthread_mutex_t cachemutex;
/**
 * @brief ibsem is used to serialize all Infiniband (MPI) operations
 * @deprecated Should not be needed once the cache module or a parallel
 * MPI communication is allowed.
 * @see swdsm.cpp
 */
extern sem_t ibsem;
/**
 * @todo MPI communication channel for exclusive accesses
 * @deprecated MPI communication should be handled by a module and
 * accessed through a proper API
 * @see swdsm.cpp
 */
extern MPI_Win* globalDataWindow;
/**
 * @brief sharerWindow protects the pyxis directory
 * @deprecated Should not be needed once the pyxis directory is
 * managed from elsewhere through a cache module.
 * @see swdsm.cpp
 */
extern MPI_Win sharerWindow;
/**
 * @brief Needed to update argo statistics
 * @deprecated Should be replaced by API calls to a stats module
 * @see swdsm.cpp
 */
extern argo_statistics stats;
/**
 * @brief Needed to update information about cache pages touched
 * @deprecated Should eventually be handled by a cache module
 */
extern argo_byte* touchedcache;
/**
 * @brief MPI communicator for node processes
 * @deprecated prototype implementation detail
 * @see swdsm.cpp
 */
extern MPI_Comm workcomm;
/**
 * @brief MPI window for the first-touch data distribution
 * @see swdsm.cpp
 * @see first_touch_distribution.hpp
 */
extern MPI_Win owners_dir_window;
/**
 * @brief MPI window for the first-touch data distribution
 * @see swdsm.cpp
 * @see first_touch_distribution.hpp
 */
extern MPI_Win offsets_tbl_window;
/**
 * @brief MPI directory for the first-touch data distribution
 * @see swdsm.cpp
 * @see first_touch_distribution.hpp
 */
extern std::uintptr_t* global_owners_dir;
/**
 * @brief MPI table for the first-touch data distribution
 * @see swdsm.cpp
 * @see first_touch_distribution.hpp
 */
extern std::uintptr_t* global_offsets_tbl;

/**
 * @brief stores a page remotely - only writing back what has been written locally since last synchronization point
 * @param index index in local page cache
 * @param addr address to page in global address space
 */
extern void storepageDIFF(std::size_t index, std::uintptr_t addr);

/*Write Buffer*/
#include "write_buffer.hpp"  // Needed only in the line below
/**
 * @brief Write buffer to ensure selectively handled pages can be removed
 * @deprecated This should eventually be handled by a cache module
 * @see swdsm.cpp
 */
extern write_buffer<std::size_t>* argo_write_buffer;

/*Handler*/
/**
 * @brief Catches memory accesses to memory not yet cached in ArgoDSM. Launches remote requests for memory not present.
 * @param sig unused param
 * @param si contains information about faulting instruction such as memory address
 * @param context the context used when the signal was received
 * @see signal.h
 */
void handler(int sig, siginfo_t *si, void *context);
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

/*Synchronization*/

/**
 * @brief Self-Invalidates all memory that has potential writers
 */
void self_invalidation();

/**
 * @brief Perform upgrade of page classifications
 * @param upgrade the type of classification upgrade to perform
 */
void self_upgrade(argo::backend::upgrade_type upgrade);

/**
 * @brief Global barrier for ArgoDSM - needs to be called by every thread in the
 *        system that need coherent view of the memory
 * @param n number of local thread participating
 * @param upgrade the type of classification upgrade to perform
 */
void swdsm_argo_barrier(int n, argo::backend::upgrade_type upgrade =
				  argo::backend::upgrade_type::upgrade_none);

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

/*Statistics*/
/**
 * @brief Clears out all statistics
 */
void clearStatistics();

/**
 * @brief Prints collected statistics
 */
void printStatistics();

/**
 * @brief Resets current ArgoDSM coherence
 * @note Collective function which should be called only by one thread per node
 */
void argo_reset_coherence();

/**
 * @brief Gives the ArgoDSM node id for the local process
 * @return Returns the ArgoDSM node id for the local process
 * @deprecated Should use argo_get_nid() instead and eventually remove this
 * @see argo_get_nid()
 */
argo::node_id_t getID();

/**
 * @brief Gives the ArgoDSM node id for the local process
 * @return Returns the ArgoDSM node id for the local process
 */
argo::node_id_t argo_get_nid();

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
 * @brief Gives a pointer to the global address space
 * @return Start address of the global address space
 */
void *argo_get_global_base();

/**
 * @brief Size of global address space
 * @return Size of global address space
 */
size_t argo_get_global_size();

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
std::size_t isPowerOf2(std::size_t x);
/**
 * @brief Gets cacheindex for a given address
 * @param addr Address in the global address space
 * @return cacheindex where addr should map to in the ArgoDSM page cache
 */
std::size_t getCacheIndex(std::uintptr_t addr);
/**
 * @brief Gives homenode for a given address
 * @param addr Address in the global address space
 * @return Process ID of the node backing the memory containing addr
 */
argo::node_id_t get_homenode(std::uintptr_t addr);
/**
 * @brief Gives homenode for a given address
 * @param addr Address in the global address space
 * @return Process ID of the node backing the memory containing addr,
 * or argo::data_distribution::invalid_node_id if addr has not been first-touched
 * @note This version does not invoke a first-touch call if an
 * address has not been first-touched
 */
argo::node_id_t peek_homenode(std::uintptr_t addr);
/**
 * @brief Gets the offset of an address on the local nodes part of the global memory
 * @param addr Address in the global address space
 * @return addr-(start address of local process part of global memory)
 */
std::size_t get_offset(std::uintptr_t addr);
/**
 * @brief Gets the offset of an address on the local nodes part of the global memory
 * @param addr Address in the global address space
 * @return addr-(start address of local process part of global memory),
 * or argo::data_distribution::invalid_offset if addr has not been first-touched yet
 * @note This version does not invoke a first-touch call if an
 * address has not been first-touched
 */
std::size_t peek_offset(std::uintptr_t addr);
/**
 * @brief Gives an index to the sharer/writer vector depending on the address
 * @param addr Address in the global address space
 * @return index for sharer vector for the page
 */
std::size_t get_classification_index(std::uintptr_t addr);
/**
 * @brief Check whether a page is either cached on the node or
 * locally backed.
 * @param addr Address in the global address space
 * @return true if cached or locally backed, else false
 * @warning This is strictly meant for testing prefetching
 * @todo This should be moved in to a dedicated cache class
 */
bool _is_cached(std::uintptr_t addr);
#endif // SRC_BACKEND_MPI_SWDSM_H_
