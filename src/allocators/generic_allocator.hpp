/**
 * @file
 * @brief This file provides a generic allocator template for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 *
 * @details
 * Allocators in C++ are supposed to allocate any type T,
 * thus this template follows this basic concept.
 * Additionally, this generic_allocator allocates all memory from an
 * underlying memory pool.
 * If the memory pool itself is not threadsafe, the access to it needs
 * to be guarded with locks. As different levels of abstraction
 * in the ArgoDSM system require different types of locks, this is a
 * template parameter. Also, using the argo::allocators::null_lock
 * allows to bypass the locking calls entirely, for when the underlying
 * memory pool is thread-safe (or safety is otherwise guaranteed).
 */

#ifndef argo_generic_allocator_hpp
#define argo_generic_allocator_hpp argo_generic_allocator_hpp

#include <map>
#include <stack>
#include "../backend/backend.hpp"

namespace argo {

	/**
	 * @brief Enumeration class with parameters for the C++ allocation
	 * functions.
	 */
	enum class allocation {
		/**
		 * @brief Initialize the element(s) allocated
		 *
		 * If this parameter is given to the allocation function, then the
		 * function will initialize the element(s) allocated. This is done by
		 * calling a placement new expression on the memory allocated. This
		 * option is enabled by default for non-trivial types.
		 */
		initialize,
		/**
		 * @brief Do not initialize the element(s) allocated
		 *
		 * The opposite of the ``initialize`` parameter. It is enabled by
		 * default for trivial types.
		 */
		no_initialize,
		/**
		 * @brief Destruct (deinitialize) the element(s) deallocated
		 *
		 * This parameters is the equivalent of ``initialize`` for the delete
		 * family of functions.
		 */
		deinitialize,
		/**
		 * @brief Do not destruct (deinitialize) the deallocated element(s)
		 *
		 * The opposite of ``deinitialize``.
		 */
		no_deinitialize,
		/**
		 * @brief Make the allocation a synchronization point
		 *
		 * This will cause the allocator to synchronize with the other nodes.
		 * This is the default behaviour in the collective allocator if
		 * ``initialize`` is also enabled.
		 */
		synchronize,
		/**
		 * @brief Do not make the allocation a synchronization point
		 *
		 * The opposite of ``synchronize``. Enabled by default in the collective
		 * allocator if ``no_initialize`` is enabled, and always for the dynamic
		 * allocator.
		 */
		no_synchronize
	};

	namespace _internal {
		/**
		 * @brief Helper struct to parse allocation parameters at compilation
		 */
		template <allocation, allocation...>
		struct alloc_param_in;

		template <allocation P, allocation AP, allocation... APs>
		struct alloc_param_in<P, AP, APs...>
			: public std::conditional<P == AP, std::true_type, alloc_param_in<P, APs...>>::type {};

		template <allocation P>
		struct alloc_param_in<P> : public std::false_type {};

		/**
		 * @brief Helper struct; reduces code verbosity when using the parameters
		 */
		template <allocation... Ps>
		struct alloc_params {
			/** @brief @ref allocation::initialize */
			static constexpr bool initialize =
				alloc_param_in<allocation::initialize, Ps...>::value;
			/** @brief @ref allocation::no_initialize */
			static constexpr bool no_initialize =
				alloc_param_in<allocation::no_initialize, Ps...>::value;
			/** @brief @ref allocation::deinitialize */
			static constexpr bool deinitialize =
				alloc_param_in<allocation::deinitialize, Ps...>::value;
			/** @brief @ref allocation::no_deinitialize */
			static constexpr bool no_deinitialize =
				alloc_param_in<allocation::no_deinitialize, Ps...>::value;
			/** @brief @ref allocation::synchronize */
			static constexpr bool synchronize =
				alloc_param_in<allocation::synchronize, Ps...>::value;
			/** @brief @ref allocation::no_synchronize */
			static constexpr bool no_synchronize =
				alloc_param_in<allocation::no_synchronize, Ps...>::value;

			// Having both of each pair enabled makes no sense
			static_assert(!initialize || !no_initialize, "Conflicting parameters");
			static_assert(!deinitialize || !no_deinitialize, "Conflicting parameters");
			static_assert(!synchronize || !no_synchronize, "Conflicting parameters");
		};

    } // namespace internal

	namespace allocators {

		/**
		 * @brief generic template for memory allocators
		 * @tparam T type to allocate
		 * @tparam MemoryPool the type of memory pool to allocate from
		 * @tparam LockType lock type used to protect the memory pool
		 * @details This template can be used for many different kinds of
		 *          memory allocators in ArgoDSM:
		 *          The underlying memory pool is currently either a
		 *          preallocation pool or a large piece of distributed shared
		 *          memory. Locking is only required when the memory pool
		 *          otherwise would experience data races.
		 */
		template<typename T, typename MemoryPool, typename LockType>
		class generic_allocator {
			private:
				/** @brief Type for freelists */
				using freelist_t = std::stack<T*>;

				/** @brief The memory pool to allocate from */
				MemoryPool* mempool;

				/** @brief size of the lock in T units */
				static constexpr std::size_t lock_size =(sizeof(LockType)+sizeof(T)-1)/sizeof(T);

				/** @brief A lock to protect accesses */
				LockType* lock;

				/** @brief A map for storing the size of each allocation */
				std::map<T*, size_t> allocation_size;
				// another option would be std::multimap<size_t, void*>

				/** @brief A map to store allocations after freeing them, according to their size */
				std::map<size_t, freelist_t> freelist;

				/**
				 * @brief helper function for deallocation
				 * @param ptr pointer to deallocate
				 * @param size number of elements to deallocate
				 * @details this helper is used internally after locks have been acquired
				 */
				void deallocate_nosync(T* ptr, size_t size) {
					/* no check for initialization required:
					 * if empty, container is default-constructed */
					freelist[size].push(ptr);
				}

			public:
				/**
				 * @brief the type that is allocated
				 */
				using value_type = T;

				/**
				 * @brief Default constructor: Initialize to nullptr as memory pool
				 * @warning using the new memory pool without initializing it through
				 *       set_mempool() is illegal
				 */
				generic_allocator()
					: mempool(nullptr), lock(new LockType), allocation_size(), freelist() {}
				/**
				 * @brief Construct allocator for a memory pool
				 * @param mp The memory pool to allocate from
				 */
				generic_allocator(MemoryPool* mp)
					: mempool(mp), lock(new LockType), allocation_size(), freelist() {}

				/**
				 * @todo Documentation
				 */
				~generic_allocator() {
					LockType* lock_ptr = lock;
					lock->~LockType();
					this->deallocate(reinterpret_cast<T*>(lock_ptr), lock_size);
				}

				/**
				 * @brief Allocate memory
				 * @param n The amount of Ts for which memory is allocated
				 * @return The pointer to the allocated memory
				 */
				T* allocate(size_t n) {
					/** @todo maybe detect uninitialized mempool in here? */

					lock->lock();
					if(freelist.count(n) != 0 && !freelist[n].empty()) {
						T* allocation = freelist[n].top();
						freelist[n].pop();
						lock->unlock();
						return allocation;
					}
					T* allocation;
					try {
						allocation = static_cast<T*>(mempool->reserve(n*sizeof(T)));
					} catch (const typename MemoryPool::bad_alloc&) {
						auto avail = mempool->available();
						if(avail > 0) {
							allocation = static_cast<T*>(mempool->reserve(avail));
							freelist[avail].push(allocation);
							allocation_size.insert({{allocation, n}});
						}
						try {
							mempool->grow(n*sizeof(T));
						}catch(const std::bad_alloc&){
							lock->unlock();
							throw;
						}
						allocation = static_cast<T*>(mempool->reserve(n*sizeof(T)));
					}
					allocation_size.insert({{allocation, n}});
					lock->unlock();
					return allocation;
				}

				/**
				 * @brief free an allocated pointer
				 * @param ptr the pointer to free
				 */
				void free(T* ptr) {
					lock->lock();
					auto size = allocation_size[ptr];
					deallocate_nosync(ptr, size);
					lock->unlock();
				}

				/**
				 * @brief deallocate memory
				 * @param ptr starting address of the memory to free
				 * @param n the number of elements of type T to deallocate
				 */
				void deallocate(T* ptr, size_t n) {
					lock->lock();
					deallocate_nosync(ptr, n);
					lock->unlock();
				}

				/**
				 * @brief set memory pool
				 * @param mp new memory pool to feed allocations from
				 */
				void set_mempool(MemoryPool* mp) {
					mempool = mp;
				}

				/**
				 * @brief How much space has been allocated for the given chunk
				 * @param ptr Starting address of the chunk
				 * @return Allocated space in bytes
				 */
				size_t allocated_space(T* ptr) {
					return allocation_size.at(ptr);
				}
		};
	} // namespace allocators
} // namespace argo

#endif /* generic_allocator_hpp */
