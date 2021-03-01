/**
 * @file
 * @brief This file provides an abstraction layer for distributing the shared memory space
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_data_distribution_hpp
#define argo_data_distribution_hpp argo_data_distribution_hpp

#include "naive_distribution.hpp"
#include "cyclic_distribution.hpp"
#include "skew_mapp_distribution.hpp"
#include "prime_mapp_distribution.hpp"
#include "first_touch_distribution.hpp"

/**
 * @note backend.hpp is not included here as it includes global_ptr.hpp,
 *       which means that the data distribution definitions precede the
 *       backend definitions, and that is why we need to forward here.
 */
namespace argo {
	/* forward argo::backend function declarations */
	namespace backend {
		node_id_t number_of_nodes();
	} // namespace backend
} // namespace argo

namespace argo {
	namespace data_distribution {
		/**
		 * @brief Enumeration for the available distributions
		 */
		enum memory_policy {
			/**
			 * @brief the naive distribution scheme
			 * @note distributes data at the default
			 *       page granularity level (4KB).
			 * @see naive_distribution.hpp
			 * @see @ref ARGO_ALLOCATION_POLICY
			 */
			naive,
			/**
			 * @brief the cyclic policy
			 * @note distributes data at the default
			 *       page granularity level (4KB).
			 * @see cyclic_distribution.hpp
			 * @see @ref ARGO_ALLOCATION_POLICY
			 */
			cyclic,
			/**
			 * @brief the cyclic-block policy
			 * @note distributes data at a multiple
			 *       of the default page granularity
			 *       level (4KB).
			 * @see cyclic_distribution.hpp
			 * @see @ref ARGO_ALLOCATION_POLICY
			 * @see @ref ARGO_ALLOCATION_BLOCK_SIZE
			 */
			cyclic_block,
			/**
			 * @brief the skew-mapp policy
			 * @note distributes data at the default
			 *       page granularity level (4KB).
			 * @see skew_mapp_distribution.hpp
			 * @see @ref ARGO_ALLOCATION_POLICY
			 */
			skew_mapp,
			/**
			 * @brief the skew-mapp-block policy
			 * @note distributes data at a multiple
			 *       of the default page granularity
			 *       level (4KB).
			 * @see skew_mapp_distribution.hpp
			 * @see @ref ARGO_ALLOCATION_POLICY
			 * @see @ref ARGO_ALLOCATION_BLOCK_SIZE
			 */
			skew_mapp_block,
			/**
			 * @brief the prime-mapp policy
			 * @note distributes data at the default
			 *       page granularity level (4KB).
			 * @see prime_mapp_distribution.hpp
			 * @see @ref ARGO_ALLOCATION_POLICY
			 */
			prime_mapp,
			/**
			 * @brief the prime-mapp-block policy
			 * @note distributes data at a multiple
			 *       of the default page granularity
			 *       level (4KB).
			 * @see prime_mapp_distribution.hpp
			 * @see @ref ARGO_ALLOCATION_POLICY
			 * @see @ref ARGO_ALLOCATION_BLOCK_SIZE
			 */
			prime_mapp_block,
			/**
			 * @brief the first-touch policy
			 * @note distributes data at the default
			 *       page granularity level (4KB).
			 * @see first_touch_distribution.hpp
			 * @see @ref ARGO_ALLOCATION_POLICY
			 */
			first_touch
		};

		/**
		 * @brief identifies if we distribute data using a
		 *        block memory policy
		 * @return true if we utilize a block policy
		 *         false if we don't utilize a block policy
		 */
		static inline bool is_block_policy() {
			std::size_t policy = env::allocation_policy();
			switch (policy) {
				case cyclic_block	: return true;
				case skew_mapp_block	: return true;
				case prime_mapp_block	: return true;
				default			: return false;
			}
		}

		/**
		 * @brief identifies if we distribute data using the
		 *        prime-mapp(-block) memory policy
		 * @return true if we utilize the prime-mapp(-block) policy
		 *         false if we don't utilize prime-mapp(-block) policy
		 */
		static inline bool is_prime_policy() {
			std::size_t policy = env::allocation_policy();
			switch (policy) {
				case prime_mapp		: return true;
				case prime_mapp_block	: return true;
				default			: return false;
			}
		}

		/**
		 * @brief identifies if we distribute data using the
		 *        first-touch memory policy
		 * @return true if we utilize the first-touch policy
		 *         false if we don't utilize first-touch policy
		 */
		static inline bool is_first_touch_policy() {
			std::size_t policy = env::allocation_policy();
			return (policy == first_touch) ? true : false;
		}

		/**
		 * @brief based on the chosen policy, gets the required
		 *        size we need to add to the standardisation of
		 *        the ArgoDSM global memory space
		 * @return the padding size based on the chosen policy
		 */
		static inline std::size_t policy_padding() {
			std::size_t padding = (is_block_policy()) ? env::allocation_block_size() : 1;
			padding *= (is_prime_policy()) ? ((3 * argo::backend::number_of_nodes()) / 2) : 1;
			return padding;
		}
#if 0
		/** @brief a test-and-test-and-set lock */
		class data_distribution {
			public:
				/** @todo Documentation */
				unsigned long nodes;
				/** @todo Documentation */
				unsigned long memory_size;
				/** @todo Documentation */
				unsigned long node_size;
				/** @todo Documentation */
				char* start_address;

				/** @todo Documentation */
				data_distribution(unsigned long arg_nodes, unsigned long arg_memory_size, void* start) : nodes(arg_nodes), memory_size(arg_memory_size), start_address(static_cast<char*>(start)) {
					/**
					 *@todo fix integer division
					 */
					node_size = memory_size/nodes;
				}

				/**
				 *@brief Translates a pointer in virtual space to an address in the global address space
				 *@todo is it ok to take a void* ?
				 * @todo Documentation
				 */
				long translate_virtual_to_global(void* address){

					if(static_cast<char*>(address) == NULL){
						throw std::runtime_error("ArgoDSM - NULL pointer exception");
					}
					else if(	 static_cast<char*>(address) < start_address
										 || static_cast<char*>(address) >= (start_address + memory_size)){
						throw std::runtime_error("ArgoDSM - Pointer out of bounds exception");
					}
					else{
						return static_cast<char*>(address) - start_address;
					}
				}

				/** @todo Documentation */
				template<typename T>
				address_location get_location(T* address){
					address_location loc;
					unsigned long GA = translate_virtual_to_global(address);

					loc.homenode = GA/node_size;
					if(loc.homenode >= nodes){
						throw std::runtime_error("ArgoDSM - Global address out of homenode range");
					}

					loc.offset = GA - (loc.homenode)*node_size;//offset in local memory on remote node (homenode
					if(loc.offset >= node_size){
						throw std::runtime_error("ArgoDSM - Global address out of range on local node");
					}
					return loc;
				}
		};
#endif
	} // namespace data_distribution
} // namespace argo

#endif /* argo_data_distribution_hpp */
