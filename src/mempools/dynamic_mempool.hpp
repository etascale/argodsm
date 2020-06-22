/**
 * @file
 * @brief This file provides a dynamically growing memory pool for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_dynamic_mempool_hpp
#define argo_dynamic_mempool_hpp argo_dynamic_mempool_hpp

#include "../backend/backend.hpp"
#include "../synchronization/broadcast.hpp"

namespace argo {
	namespace mempools {

		/** @brief helper enum for choosing how to call grow() */
		enum growth_mode_t {
			/** @brief caller always needs to grow the mempool */
			ALWAYS,
			/** @brief growing is handled by ArgoDSM node zero */
			NODE_ZERO_ONLY
		};

		/**
		 * @brief check whether growth needs to happen
		 * @tparam g enumeration of possibilities, used for partial specialization
		 * @return true if grow() should be called,
		 *         false if this is taken care of somewhere else
		 */
		template<growth_mode_t g>
		inline bool do_grow();

		/**
		 * @brief synchronize memory metainformation (address) after growing
		 *        to ensure visibility
		 * @tparam g enumeration of possibilities, used for partial specialization
		 * @param m pointer to the memory information to synchronize
		 */
		template<growth_mode_t g>
		inline void synchronize(memory_t* const m);

		/* full template specializations */

		/**
		 * @brief always enable growing the memory pool
		 * @return always true
		 */
		template<>
		inline bool do_grow<ALWAYS>() {
			return true;
		}

		/**
		 * @brief restrict growing to ArgoDSM node 0
		 * @return true when called on ArgoDSM node 0, false otherwise
		 */
		template<>
		inline bool do_grow<NODE_ZERO_ONLY>() {
			auto nid = backend::node_id();
			return nid == 0;
		}

		/**
		 * @brief used when no additional synchronization is required
		 * @param m ignored
		 */
		template<>
		inline void synchronize<ALWAYS>(memory_t* const m) {
			if(*m == nullptr) {
				throw std::bad_alloc();
			}

			return;
		}

		/**
		 * @copydoc synchronize()
		 * @todo replace literal 0 with non-magic number
		 * @todo replace nullptr with non-magic representation of undefined memory
		 */
		template<>
		inline void synchronize<NODE_ZERO_ONLY>(memory_t* const m) {
			auto memory_ptr = synchronization::broadcast(0, *m);
			/** @todo verify this barrier is needed */
			argo::backend::barrier();
			if(memory_ptr == nullptr) {
				throw std::bad_alloc();
			}
			*m = memory_ptr;
		}

		/**
		 * @brief Dynamically growing memory pool
		 */
		template<template<class> class Allocator, growth_mode_t growth_mode, std::size_t chunk_size=4096>
		class dynamic_memory_pool {
			private:
				/**  @brief typedef for byte-size allocator */
				using allocator_t = Allocator<char>;

				/** @brief the internally used allocator */
				allocator_t* allocator;

				/** @brief current base address of this memory pool's memory */
				memory_t memory;

				/** @brief current size of the memory pool */
				std::size_t max_size;

				/** @brief amount of memory in pool that is already allocated */
				std::size_t offset;
			public:
				/** type of allocation failures within this memory pool */
				using bad_alloc = std::bad_alloc;

				/**
				 * @brief Default constructor: initializes memory on heap and sets offset to 0
				 * @param a a pointer to the allocator to use for growing this mempool
				 * @todo passing the allocator by pointer may be unnecessary
				 */
				dynamic_memory_pool(allocator_t* a) : allocator(a), memory(nullptr), max_size(0), offset{0} {}

				/**
				 * @brief Reserve more memory
				 * @param size Amount of memory reserved
				 * @return The pointer to the first byte of the newly reserved memory area
				 * @todo move size check to separate function?
				 */
				char* reserve(std::size_t size) {
					if(offset+size > max_size) {
						throw bad_alloc();
						/* defensively return nullptr if throwing is not supported */
						return nullptr;
					}
					char* ptr = &memory[offset];
					offset += size;
					return ptr;
				}

				/**
				 * @brief fail to grow the memory pool
				 * @param size minimum size to grow
				 */
				void grow(std::size_t size) {
					/* sanity check */
					if(!size) {
						size = 1;
					}
					/* try to allocate more memory */
					auto alloc_size = ((size + chunk_size - 1) / chunk_size) * chunk_size;
					max_size = alloc_size;
					offset = 0;

					try{
						if(do_grow<growth_mode>()) {
							memory = allocator->allocate(alloc_size);
						}
					}catch(const std::bad_alloc&){
						memory = nullptr;
					}
					synchronize<growth_mode>(&memory);

				}

				/**
				 * @brief check remaining available memory in pool
				 * @return remaining bytes in memory pool
				 */
				std::size_t available() {
					return max_size - offset;
				}
		};

	} // namespace mempools
} // namespace argo

#endif /* argo_dynamic_mempool_hpp */
