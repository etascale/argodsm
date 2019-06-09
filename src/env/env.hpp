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
	} // namespace env
} // namespace argo

#endif
