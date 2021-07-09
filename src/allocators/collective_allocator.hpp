/**
 * @file
 * @brief This file provides the collective allocator for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 *
 * @details
 * There are two possible ways of implementing collective allocations:
 * a) one can allocate on ArgoDSM node 0 only, using e.g. barrier-based
 *    synchronization to ensure every node arrives at the same value
 * b) every ArgoDSM node maintains its own copy of the memory pool
 *    information, and synchronization happens implicitly through the
 *    allocation calls being well-ordered
 *
 * Option b) was chosen here for a single preallocation pool, which is
 * thus shared among all ArgoDSM nodes for collective allocation. Only
 * when the preallocation pool runs out of memory, option a) is used
 * to refill the pool. A single collective broadcast operation is used
 * to synchronize the update among all ArgoDSM nodes.
 */

#ifndef argo_collective_allocators_hpp
#define argo_collective_allocators_hpp argo_collective_allocators_hpp

#include <map>
#include <memory>
#include <stack>

#include "../mempools/dynamic_mempool.hpp"
#include "../mempools/global_mempool.hpp"
#include "../backend/backend.hpp"

#include "dynamic_allocator.hpp"
#include "generic_allocator.hpp"
#include "null_lock.hpp"

/* forward declarations */
extern "C" {
#include "collective_allocator.h"
}

namespace argo {
	namespace allocators {
		/**
		 * @brief type alias for collective allocator: allocate from dynamically growing pool (backed by global memory allocator) without locking
		 */
		using collective_allocator = generic_allocator<char, mempools::dynamic_memory_pool<global_allocator, mempools::NODE_ZERO_ONLY>, null_lock>;

		/**
		 * @brief default collective allocator, using bytes as base unit
		 */
		extern collective_allocator default_collective_allocator;

	} // namespace allocators

	/**
	 * @brief template function to construct new objects using collective allocation
	 * @tparam T type to construct
	 * @tparam APs Allocation parameters
	 * @tparam Ps parameter types for constructor
	 * @param ps parameters for constructor
	 * @return pointer to newly constructed object
	 * @sa new_
	 *
	 * The function might choose to initialize or synchronize, depending on the
	 * type constructed or the arguments provided by the user.
	 *
	 * -# Initialization is performed by default.
	 * -# If the type is trivial, then no initialization is performed.
	 * -# If the user has provided constructor arguments, then initialization is
	 *    performed.
	 * -# The user can explicitly override these defaults using the appropriate
	 *    allocation parameters (@ref allocation).
	 * -# Synchronization is performed after initialization.
	 * -# The user can override the behaviour by using the allocation
	 *    parameters.
	 *
	 */
	template<typename T, allocation... APs, typename... Ps>
	T* conew_(Ps&&... ps) {
		using aps = _internal::alloc_params<APs...>;
		bool initialize = true;
		bool synchronize = true;

		// If the type is trivial, do not initialize
		if (std::is_trivial<T>::value) {
			initialize = false;
		}
		// If constructor arguments are given by the user, initialize
		if (sizeof...(Ps) > 0) {
			initialize = true;
		}
		// The user can explicitly override the default behaviour
		if (aps::initialize) {
			initialize = true;
		} else if (aps::no_initialize) {
			initialize = false;
		}
		// Synchronization goes hand to hand with initialization
		synchronize = initialize;
		// Again, the user can override the default behaviour
		if (aps::synchronize) {
			synchronize = true;
		} else if (aps::no_synchronize) {
			synchronize = false;
		}

		void* ptr = collective_alloc(sizeof(T));
		using namespace data_distribution;
		global_ptr<void> gptr(ptr, false, false);
		// The home node of ptr handles initialization
		if (initialize && argo::backend::node_id() == gptr.node()) {
			new (ptr) T(std::forward<Ps>(ps)...);
		}
		// Do not return uninitialized memory to the nodes.
		if (synchronize) {
			argo::backend::barrier();
		}
		return static_cast<T*>(ptr);
	}

	/**
	 * @brief template function to delete objects that were allocated using collective_new
	 * @tparam T type of the object to deallocate
	 * @tparam APs deallocation parameters
	 * @param ptr pointer to the object to deallocate
	 * @note T can usually be inferred from the type of ptr
	 *
	 * This function might choose to destruct or synchronize.
	 *
	 * -# The destructors are called by default (deinitialization).
	 * -# If the type is trivially destructible, no destructors are called.
	 * -# The user can explicilty override this behaviour (see @ref allocation).
	 * -# If the destructor is called, the function will also synchronize.
	 * -# The user can explicitly override this behaviour.
	 *
	 */
	template<typename T, allocation... APs>
	void codelete_(T* ptr) {
		using aps = _internal::alloc_params<APs...>;
		bool deinitialize = true;
		bool synchronize = true;

		if (ptr == nullptr) {
			return;
		}

		// If the type is trivially destructible, do not deinitialize
		if (std::is_trivially_destructible<T>::value) {
			deinitialize = false;
		}
		// The user can explicitly override the default behaviour
		if (aps::deinitialize) {
			deinitialize = true;
		} else if (aps::no_deinitialize) {
			deinitialize = false;
		}
		// Synchronization goes hand to hand with deinitialization
		synchronize = deinitialize;
		// Again, the user can override the default behaviour
		if (aps::synchronize) {
			synchronize = true;
		} else if (aps::no_synchronize) {
			synchronize = false;
		}

		using namespace data_distribution;
		global_ptr<T> gptr(ptr, false, false);
		// The home node of ptr handles deinitialization
		if (deinitialize && argo::backend::node_id() == gptr.node()) {
			ptr->~T();
		}
		// This barrier might be unnecessary, depending on how free is
		// implemented, but let's just put it here to cover all cases.
		if (synchronize) {
			argo::backend::barrier();
		}
		collective_free(static_cast<void*>(ptr));
	}

	/**
	 * @brief Collectively construct a new array of objects in the global memory
	 * @tparam T type to construct
	 * @tparam APs Allocation parameters
	 * @tparam Ps parameter types for the constructor
	 * @param size Number of elements in the array
	 * @return A pointer to the newly constructed object
	 * @sa conew_
	 *
	 * The array elements are default-initialized. For this function to work,
	 * all the nodes need to call it with the same arguments at the same time.
	 *
	 */
	template<typename T, allocation... APs>
	T* conew_array(size_t size) {
		using aps = _internal::alloc_params<APs...>;
		bool initialize = true;
		bool synchronize = true;

		// If the type is trivial, do not initialize
		if (std::is_trivial<T>::value) {
			initialize = false;
		}
		// The user can explicitly override the default behaviour
		if (aps::initialize) {
			initialize = true;
		} else if (aps::no_initialize) {
			initialize = false;
		}
		// Synchronization goes hand to hand with initialization
		synchronize = initialize;
		// Again, the user can override the default behaviour
		if (aps::synchronize) {
			synchronize = true;
		} else if (aps::no_synchronize) {
			synchronize = false;
		}

		void* ptr = collective_alloc(sizeof(T) * size);
		using namespace data_distribution;
		global_ptr<void> gptr(ptr, false, false);
		// The home node of ptr handles initialization
		if (initialize && argo::backend::node_id() == gptr.node()) {
			new (ptr) T[size]();
		}
		if (synchronize) {
			argo::backend::barrier();
		}
		return static_cast<T*>(ptr);
	}

	/**
	 * @brief Delete a globally collectively allocated array of objects
	 * @tparam T Type of the object to be deleted
	 * @tparam APs Deallocation parameters
	 * @param ptr Pointer to the object to be deleted
	 * @ sa codelete_
	 *
	 * For this function to work, all nodes need to call it with the same
	 * arguments at the same time.
	 */
	template<typename T, allocation... APs>
	void codelete_array(T* ptr) {
		using aps = _internal::alloc_params<APs...>;
		bool deinitialize = true;
		bool synchronize = true;

		if (ptr == nullptr) {
			return;
		}

		// If the type is trivially destructible, do not deinitialize
		if (std::is_trivially_destructible<T>::value) {
			deinitialize = false;
		}
		// The user can explicitly override the default behaviour
		if (aps::deinitialize) {
			deinitialize = true;
		} else if (aps::no_deinitialize) {
			deinitialize = false;
		}
		// Synchronization goes hand to hand with deinitialization
		synchronize = deinitialize;
		// Again, the user can override the default behaviour
		if (aps::synchronize) {
			synchronize = true;
		} else if (aps::no_synchronize) {
			synchronize = false;
		}

		using namespace data_distribution;
		global_ptr<T> gptr(ptr, false, false);
		// The home node of ptr handles deinitialization
		if (deinitialize && argo::backend::node_id() == gptr.node()) {
			auto elements =
				argo::allocators::default_collective_allocator.allocated_space(
					reinterpret_cast<char *>(ptr)) /
				sizeof(T);
			for (size_t i = 0; i < elements; ++i) {
				ptr[i].~T();
			}
		}
		if (synchronize) {
			argo::backend::barrier();
		}
		collective_free(static_cast<void*>(ptr));
	}
} // namespace argo

#endif /* argo_collective_allocators_hpp */
