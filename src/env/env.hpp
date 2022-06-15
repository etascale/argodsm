/**
 * @file
 * @brief This file provides facilities for handling environment variables
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_env_env_hpp
#define argo_env_env_hpp argo_env_env_hpp

#include <cstddef>

/**
 * @page envvars Environment Variables
 *
 * @envvar{ARGO_MEMORY_SIZE} request a specific memory size in bytes
 * @details This environment variable is only used if argo::init() is called with no
 *          argo_size parameter (or it has value 0). It can be accessed through
 *          @ref argo::env::memory_size() after argo::env::init() has been called.
 *
 * @envvar{ARGO_CACHE_SIZE} request a specific cache size in bytes
 * @details This environment variable is only used if argo::init() is called with no
 *          cache_size parameter (or it has value 0). It can be accessed through
 *          @ref argo::env::cache_size() after argo::env::init() has been called.
 *
 * @envvar{ARGO_WRITE_BUFFER_SIZE} request a specific write buffer size in cache blocks
 * @details This environment variable defaults to 512 cache blocks if not specified.
 *          It can be accessed through @ref argo::env::write_buffer_size() after
 *          argo::env::init() has been called.
 *
 * @envvar{ARGO_WRITE_BUFFER_WRITE_BACK_SIZE} request a specific write buffer write
 * back size in cache blocks.
 * @details This environment variable defaults to 32 cache blocks if not specified.
 *          It can be accessed through @ref argo::env::write_buffer_write_back_size()
 *          after argo::env::init() has been called.
 * 
 * @envvar{ARGO_ALLOCATION_POLICY} request a specific allocation policy with a number
 * @details This environment variable can be accessed through
 *          @ref argo::env::allocation_policy() after argo::env::init() has been called.
 * 
 * @envvar{ARGO_ALLOCATION_BLOCK_SIZE} request a specific allocation block size in number of pages
 * @details This environment variable can be accessed through
 *          @ref argo::env::allocation_block_size() after argo::env::init() has been called.
 *
 * @envvar{ARGO_LOAD_SIZE} request a specific load size in number of pages
 * @details This environment variable determines the maximum amount of DSM pages that
 * 			are fetched on each remote load operation. It can be accessed through
 *          @ref argo::env::load_size() after argo::env::init() has been called.
 *
 * @envvar{ARGO_MPI_WINDOWS_PER_NODE} request a specific number of MPI Windows
 * @details This environment variable determines the amount of MPI windows that are
 * 			used to protect the global data and pyxis data structures. Each structure
 * 			uses the requested number of MPI windows per node, multiplied by the
 * 			number of nodes in the system. It can be accessed through
 * 			@ref argo::env::mpi_windows_per_node() after argo::env::init() has been called.
 *
 * @envvar{ARGO_PRINT_STATISTICS} control the detail of statistics printed at end
 * @details This environment variable determines the amount of statistics printed at once
 * 			ArgoDSM finalizes. 0 prints no statistics, 1 prints general system information,
 * 			2 in addition prints basic node information, and 3 in addition prints detailed
 * 			node information. It can be accessed through @ref argo::env::print_statistics()
 * 			after argo::env::init() has been called.
 */

namespace argo {
	/**
	 * @brief namespace for environment variable handling functionality
	 * @note argo::env::init() must be called before other functions in this
	 *       namespace work correctly
	 */
	namespace env {
		/**
		 * @brief read and store environment variables used by ArgoDSM
		 * @details the environment is only read once, to avoid having
		 *          to check that values are not changing later
		 */
		void init();

		/**
		 * @brief get the memory size requested by environment variable
		 * @return the requested memory size in bytes
		 * @see @ref ARGO_MEMORY_SIZE
		 */
		std::size_t memory_size();

		/**
		 * @brief get the cache size requested by environment variable
		 * @return the requested cache size in bytes
		 * @see @ref ARGO_CACHE_SIZE
		 */
		std::size_t cache_size();

		/**
		 * @brief get the write buffer size requested by environment variable
		 * @return the requested write buffer size in cache blocks
		 * @see @ref ARGO_WRITE_BUFFER_SIZE
		 */
		std::size_t write_buffer_size();

		/**
		 * @brief get the write buffer write back size requested by environment variable
		 * @return the requested write buffer write back size in cache blocks
		 * @see @ref ARGO_WRITE_BUFFER_WRITE_BACK_SIZE
		 */
		std::size_t write_buffer_write_back_size();

		/**
		 * @brief get the allocation policy requested by environment variable
		 * @return the requested allocation policy as a number
		 * @see @ref ARGO_ALLOCATION_POLICY
		 */
		std::size_t allocation_policy();

		/**
		 * @brief get the allocation block size requested by environment variable
		 * @return the requested allocation block size as a number of pages
		 * @see @ref ARGO_ALLOCATION_BLOCK_SIZE
		 */
		std::size_t allocation_block_size();

		/**
		 * @brief get the number of pages fetched remotely per load requested
		 * by environment variable
		 * @return the number of pages
		 * @see @ref ARGO_LOAD_SIZE
		 */
		std::size_t load_size();

		/**
		 * @brief get the number of MPI windows per node requested
		 * @return the number of MPI windows per node
		 * @see @ref ARGO_MPI_WINDOWS_PER_NODE
		 */
		std::size_t mpi_windows_per_node();

		/**
		 * @brief Get the requested statistics print level
		 * @return The requested statistics print level
		 * @see @ref ARGO_PRINT_STATISTICS
		 */
		std::size_t print_statistics();
	} // namespace env
} // namespace argo

#endif
