/**
 * @file
 * @brief This file provides the full C++ interface to ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_argo_hpp
#define argo_argo_hpp argo_argo_hpp

#include <cstddef>

#include "allocators/allocators.hpp"
#include "backend/backend.hpp"
#include "types/types.hpp"
#include "synchronization/synchronization.hpp"
#include "data_distribution/data_distribution.hpp"

extern "C" {
#include "argo.h"
}

/**
 * @brief ArgoDSM namespace
 */
namespace argo {
	/**
	 * @brief namespace for different memory pools
	 * @todo move Documentation to appropriate file
	 */
	namespace mempools { }

	/**
	 * @brief namespace for global locks, i.e locks that work across multiple ArgoDSM nodes
	 * @todo move Documentation to appropriate file
	 */
	namespace globallock { }

	/**
	 * @brief namespace for data distribution modules
	 * @todo move Documentation to appropriate file
	 */
	namespace data_distribution { }

	/**
	 * @brief initialize ArgoDSM system
	 * @param argo_size The desired size of the global memory in bytes, or 0. If the
	 *                  value is omitted (or specified as 0), then the value in
	 *                  environment variable @ref ARGO_MEMORY_SIZE is used instead.
	 *                  If @ref ARGO_MEMORY_SIZE is unset, then a default is used.
	 * @param cache_size The desired size of the local cache in bytes, or 0. If the
	 *                   value is omitted (or specified as 0), then the value in
	 *                   environment variable @ref ARGO_CACHE_SIZE is used instead.
	 *                   If @ref ARGO_CACHE_SIZE is unset, then a default is used.
	 */
	void init(std::size_t argo_size = 0, std::size_t cache_size = 0);

	/**
	 * @brief shut down ArgoDSM system
	 */
	void finalize();

	/**
	 * @brief reset ArgoDSM system
	 * @warning this is mainly intended for testing, not for public use
	 */
	void reset();

	/**
	 * @brief A unique ArgoDSM node identifier. Counting starts from 0.
	 * @return The node ID
	 */
	int node_id();
	/**
	 * @brief Number of ArgoDSM nodes being run
	 * @return The total number of ArgoDSM nodes
	 */
	int number_of_nodes();

	/**
	 * @brief Check if addr belongs in the ArgoDSM memory space
	 * @tparam T type of addr
	 * @param addr A memory address
	 * @return True if addr is in the ArgoDSM memory space, else false
	 */
	template<typename T>
	bool is_argo_address(T* addr) {
		char* check_addr = static_cast<char*>(addr);
		char* argo_start = backend::global_base();
		char* argo_end = argo_start + backend::global_size();
		return static_cast<bool>(check_addr >= argo_start && check_addr < argo_end);
	}

	/**
	 * @brief Get the ArgoDSM node id where addr is physically backed
	 * @param addr A valid address in the ArgoDSM memory space
	 * @tparam T type of addr
	 * @return The id of the node physically backing addr, or
	 * argo::data_distribution::invalid_node_id if it has not yet
	 * been first-touched under the first-touch allocation policy
	 * @pre addr must be an address in ArgoDSM memory
	 */
	template<typename T>
	int get_homenode(T* addr) {
		data_distribution::global_ptr<T> gptr(addr, false, false);
		return gptr.peek_node();
	}

	/**
	 * @brief Get the block size of the current allocation policy
	 * @return The block size of the current allocation policy in bytes
	 */
	std::size_t get_block_size();
} // namespace argo

#endif /* argo_argo_hpp */
