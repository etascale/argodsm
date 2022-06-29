/**
 * @file
 * @brief This file provides the dynamic allocator for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 *
 * @details
 * There are two types of dynamic allocators provided by this file.
 * Firstly, there is the global allocator, which provides memory
 * directly from the distributed shared memory pool. However, this
 * allocation is very slow, and thus is only used by preallocation
 * memory pools.
 * Secondly, the dynamic allocator maintains a preallocation pool on
 * each ArgoDSM node, and can therefore allocate memory much faster.
 * As preallocation pools are not threadsafe, the dynamic allocator
 * must be protected within the ArgoDSM node's scope.
 *
 * @see argo::mempool::global_memory_pool
 * @see argo::mempool::dynamic_memory_pool
 */

#ifndef ARGODSM_SRC_ALLOCATORS_DYNAMIC_ALLOCATOR_HPP_
#define ARGODSM_SRC_ALLOCATORS_DYNAMIC_ALLOCATOR_HPP_

#include <map>
#include <mutex>
#include <stack>
#include <utility>

#include "mempools/dynamic_mempool.hpp"
#include "mempools/global_mempool.hpp"

#include "generic_allocator.hpp"
#include "null_lock.hpp"

/* forward declarations */
extern "C" {
#include "allocators/dynamic_allocator.h"
}

namespace argo {
namespace allocators {

/**
 * @brief type alias for global allocation
 * @tparam T type to allocate
 * @note global_memory_pool is threadsafe, therefore no lock required
 */
template<typename T>
using global_allocator = generic_allocator<T, mempools::global_memory_pool<>, null_lock>;

/** @brief type alias for default dynamic allocatior */
using default_dynamic_allocator_t = generic_allocator<char, mempools::dynamic_memory_pool<global_allocator, mempools::ALWAYS>, std::mutex>;

/**
 * @brief default allocator for global allocations
 * @note not intended for direct use
 * @see default_dynamic_allocator
 */
extern global_allocator<char> default_global_allocator;

/**
 * @brief default collective allocator, using bytes as base unit
 */
extern default_dynamic_allocator_t default_dynamic_allocator;

/**
 * @brief An allocator for allocating global shared memory
 */
template<typename T>
class dynamic_allocator {
	public:
		/**
		 * @brief the type that is allocated
		 */
		using value_type = T;

		/**
		 * @brief pointer to element
		 */
		using pointer = T*;

		/**
		 * @brief reference to element
		 */
		using reference = T&;

		/**
		 * @brief pointer to constant element
		 */
		using const_pointer = const T*;

		/**
		 * @brief reference to constant element
		 */
		using const_reference = const T&;

		/**
		 * @brief quantities of elements
		 */
		using size_type = std::size_t;

		/**
		 * @brief difference between two pointers
		 */
		using difference_type = std::ptrdiff_t;

		/**
		 * @brief rebinding type
		 */
		template<typename U>
		struct rebind {
			/**
			 * @brief Allocator of the other type
			 */
			using other = dynamic_allocator<U>;
		};

		/**
		 * @brief default constructor
		 */
		dynamic_allocator() {}

		/**
		 * @brief conversion constructor from other dynamic_allocator
		 * @param other The allocator to convert from
		 */
		template<typename U>
		dynamic_allocator(const dynamic_allocator<U>& other) {}

		/**
		 * @brief allocate Ts using the default dynamic allocator
		 * @param n number of Ts to allocate
		 * @return pointer to the allocated Ts
		 */
		pointer allocate(size_type n) {
			return reinterpret_cast<pointer>(
				default_dynamic_allocator.allocate(n*sizeof(value_type)));
		}

		/**
		 * @brief free an allocated pointer
		 * @param ptr the pointer to free
		 */
		void free(pointer ptr) {
			default_dynamic_allocator.free(reinterpret_cast<char*>(ptr));
		}

		/**
		 * @brief deallocate memory
		 * @param ptr starting address of the memory to free
		 * @param n the number of elements of type T to deallocate
		 */
		void deallocate(pointer ptr, size_t n) {
			default_dynamic_allocator.deallocate(reinterpret_cast<char*>(ptr), n*sizeof(value_type));
		}

		/**
		 * @brief construct a T at given location
		 * @param ptr pointer to location
		 * @param val value to initialize to
		 */
		void construct(pointer ptr, const_reference val) {
			new (reinterpret_cast<void*>(ptr)) value_type(val);
		}

		/**
		 * @brief destroy a T at given location
		 * @param ptr pointer to location
		 * @note this does not deallocate the storage for the element
		 */
		void destroy(pointer ptr) {
			ptr->~value_type();
		}
};

/**
 * @brief test dynamic allocators for equality
 * @return always true
 */
template<typename T, typename U>
bool operator==(const dynamic_allocator<T>&, const dynamic_allocator<U>&) {
	return true;
}

/**
 * @brief test dynamic allocators for inequality
 * @return always false
 */
template<typename T, typename U>
bool operator!=(const dynamic_allocator<T>&, const dynamic_allocator<U>&) {
	return false;
}

}  // namespace allocators

/**
 * @brief template function to construct new objects
 * @tparam T type to construct
 * @tparam APs Allocation parameters
 * @tparam Ps parameter types for constructor
 * @param ps parameters for constructor
 * @return pointer to newly constructed object
 *
 * The function might choose to initialize, depending on the
 * type constructed or the arguments provided by the user.
 *
 * -# Initialization is performed by default.
 * -# If the type is trivial, then no initialization is performed.
 * -# If the user has provided constructor arguments, then initialization is
 *    performed.
 * -# The user can explicitly override these defaults using the appropriate
 *    allocation parameters (@ref argo::allocation).
 * -# Synchronization is NOT performed at any case
 * -# User synchronization parameters are ignored
 */
template<typename T, allocation... APs, typename... Ps>
T* new_(Ps&&... ps) {
	using aps = _internal::alloc_params<APs...>;
	bool initialize = true;

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

	void* ptr = dynamic_alloc(sizeof(T));
	if (initialize) {
		new (ptr) T(std::forward<Ps>(ps)...);
	}
	return static_cast<T*>(ptr);
}

/**
 * @brief template function to delete objects that were allocated using dynamic_new
 * @tparam T type of the object to deallocate
 * @tparam APs deallocation parameters
 * @param ptr pointer to the object to deallocate
 * @note T can usually be inferred from the type of ptr
 *
 * This function can choose to call the destructors
 *
 * -# Deinitialization (calling the destructors) is performed by default.
 * -# If the type is trivially destructible, no destructors are called.
 * -# The user can override the default behaviour with the allocation
 *  parameters (@ref argo::allocation).
 * -# Synchronization is NOT performed at any case
 * -# User synchronization parameters are ignored
 */
template<typename T, allocation... APs>
void delete_(T* ptr) {
	using aps = _internal::alloc_params<APs...>;
	bool deinitialize = true;

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

	if (deinitialize) {
		ptr->~T();
	}
	dynamic_free(static_cast<void*>(ptr));
}

/**
 * @brief Construct a new array of objects in the global memory
 * @tparam T type to construct
 * @tparam APs Allocation parameters
 * @tparam Ps parameter types for the constructor
 * @param size Number of elements in the array
 * @return A pointer to the newly constructed object
 * @sa new_
 *
 * The array elements are default-initialized.
 */
template<typename T, allocation... APs>
T* new_array(size_t size) {
	using aps = _internal::alloc_params<APs...>;
	bool initialize = true;

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

	void* ptr = dynamic_alloc(sizeof(T) * size);
	if (initialize) {
		new (ptr) T[size]();
	}
	return static_cast<T*>(ptr);
}

/**
 * @brief Delete a globally allocated array of object
 * @tparam T Type of the array to be deleted
 * @tparam APs deallocation parameters
 * @param ptr Pointer to the array to be deleted
 * @sa delete_
 */
template<typename T, allocation... APs>
void delete_array(T* ptr) {
	using aps = _internal::alloc_params<APs...>;
	bool deinitialize = true;

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

	if (deinitialize) {
		auto elements =
			argo::allocators::default_dynamic_allocator.allocated_space(
				reinterpret_cast<char *>(ptr)) /
			sizeof(T);
		for (size_t i = 0; i < elements; ++i) {
			ptr[i].~T();
		}
	}
	dynamic_free(static_cast<void*>(ptr));
}

}  // namespace argo

#endif  // ARGODSM_SRC_ALLOCATORS_DYNAMIC_ALLOCATOR_HPP_
