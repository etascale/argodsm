/**
 * @file
 * @brief This file implements some of the basic ArgoDSM calls
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "argo.hpp"

#include "allocators/collective_allocator.hpp"
#include "allocators/dynamic_allocator.hpp"
#include "backend/backend.hpp"
#include "data_distribution/data_distribution.hpp"
#include "env/env.hpp"
#include "virtual_memory/virtual_memory.hpp"

namespace vm = argo::virtual_memory;
namespace mem = argo::mempools;
namespace alloc = argo::allocators;
namespace dd = argo::data_distribution;

/* some memory pools for default use */
/** @todo should be static? */
mem::global_memory_pool<>* default_global_mempool;
mem::dynamic_memory_pool<alloc::global_allocator, mem::NODE_ZERO_ONLY> collective_prepool(&alloc::default_global_allocator);
mem::dynamic_memory_pool<alloc::global_allocator, mem::ALWAYS> dynamic_prepool(&alloc::default_global_allocator);

namespace argo {
	void init(std::size_t argo_size, std::size_t cache_size) {
		env::init();
		vm::init();

		std::size_t requested_argo_size = argo_size;
		if(requested_argo_size == 0) {
			requested_argo_size = env::memory_size();
		}
		using mp = mem::global_memory_pool<>;
		/* add some space for internal use, see issue #22 */
		requested_argo_size += mp::reserved;

		std::size_t requested_cache_size = cache_size;
		if(requested_cache_size == 0) {
			requested_cache_size = env::cache_size();
		}

		std::size_t requested_argo_policy = env::allocation_policy();
		if(!(requested_argo_policy <= 4ul)) {
			throw std::invalid_argument(
				"Invalid policy (must be a number between 0 and 4)");
		}

		std::size_t requested_argo_block_size = env::allocation_block_size();
		if(requested_argo_block_size == 0) {
			throw std::invalid_argument(
				"Invalid page block size (must be a number bigger than 0)");
		}

		std::size_t requested_argo_load_size = env::load_size();
		if(requested_argo_load_size == 0) {
			throw std::invalid_argument(
				"Invalid load size (must be a number bigger than 0)");
		}

		std::size_t requested_argo_mpi_windows_per_node = env::mpi_windows_per_node();
		if(requested_argo_mpi_windows_per_node == 0) {
			throw std::invalid_argument(
				"Invalid number of MPI windows (must be a number bigger than 0)");
		}

		std::size_t requested_argo_print_level = env::print_statistics();
		if(requested_argo_print_level > 3) {
			throw std::invalid_argument(
				"Invalid statistics level (must be a number between 0 and 3)");
		}

		/* note: the backend must currently initialize before the mempool can be set */
		backend::init(requested_argo_size, requested_cache_size);
		default_global_mempool = new mp();
		reset();
	}

	void finalize() {
		delete default_global_mempool;
	}

	/**
	 * @brief reset ArgoDSM allocators
	 * @warning this is mainly intended for testing, not for public use
	 */
	static void reset_allocators() {
		default_global_mempool->reset();
		using namespace alloc;
		using namespace mem;
		collective_prepool = dynamic_memory_pool<global_allocator, NODE_ZERO_ONLY>(&default_global_allocator);
		dynamic_prepool = dynamic_memory_pool<global_allocator, ALWAYS>(&default_global_allocator);
		default_global_allocator = global_allocator<char>();
		default_dynamic_allocator = default_dynamic_allocator_t();
		default_collective_allocator = collective_allocator();
		default_global_allocator.set_mempool(default_global_mempool);
		default_dynamic_allocator.set_mempool(&dynamic_prepool);
		default_collective_allocator.set_mempool(&collective_prepool);
	}

	void reset() {
		argo::backend::reset_coherence();
		reset_allocators();
		backend::barrier();
	}

	argo::node_id_t node_id() {
		return argo::backend::node_id();
	}

	argo::num_nodes_t number_of_nodes() {
		return argo::backend::number_of_nodes();
	}

	std::size_t get_block_size() {
		return dd::policy_block_size();
	}
}  // namespace argo

extern "C" {
	void argo_init(size_t argo_size, size_t cache_size) {
		argo::init(argo_size, cache_size);
	}

	void argo_finalize() {
		argo::finalize();
	}

	void argo_reset() {
		argo::reset();
	}

	int argo_node_id() {
		return argo::node_id();
	}

	int argo_number_of_nodes() {
		return argo::number_of_nodes();
	}

	bool argo_is_argo_address(void* addr) {
		return argo::is_argo_address(addr);
	}

	argo::node_id_t argo_get_homenode(void* addr) {
		return argo::get_homenode(addr);
	}

	size_t argo_get_block_size() {
		return static_cast<size_t>(argo::get_block_size());
	}
}
