/**
 * @file
 * @brief This file implements some of the basic ArgoDSM calls
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "argo.hpp"

#include "allocators/collective_allocator.hpp"
#include "allocators/dynamic_allocator.hpp"

#include "virtual_memory/virtual_memory.hpp"

namespace vm = argo::virtual_memory;
namespace mem = argo::mempools;
namespace alloc = argo::allocators;

/* some memory pools for default use */
/** @todo should be static? */
mem::global_memory_pool<>* default_global_mempool;
mem::dynamic_memory_pool<alloc::global_allocator, mem::NODE_ZERO_ONLY> collective_prepool(&alloc::default_global_allocator);
mem::dynamic_memory_pool<alloc::global_allocator, mem::ALWAYS> dynamic_prepool(&alloc::default_global_allocator);

namespace argo {
	void init(std::size_t size) {
		vm::init();
		std::size_t requested_size = size;
		using mp = mem::global_memory_pool<>;
		requested_size += mp::reserved;
		backend::init(requested_size);
		default_global_mempool = new mp();
		argo_reset();
	}

	void finalize() {
		delete default_global_mempool;
	}

	int node_id() {
		return static_cast<int>(argo::backend::node_id());
	}

	int number_of_nodes() {
		return static_cast<int>(argo::backend::number_of_nodes());
	}

} // namespace argo

extern "C" {
	void argo_init(size_t size) {
		argo::init(size);
	}

	void argo_finalize() {
		argo::finalize();
	}

	void argo_reset() {
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

	int argo_node_id() {
		return argo::node_id();
	}

	int argo_number_of_nodes() {
		return argo::number_of_nodes();
	}
}
