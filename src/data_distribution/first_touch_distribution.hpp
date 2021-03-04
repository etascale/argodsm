/**
 * @file
 * @brief This file implements the first-touch data distribution
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_first_touch_distribution_hpp
#define argo_first_touch_distribution_hpp argo_first_touch_distribution_hpp

#include <mutex>
#include <iostream>

#include "base_distribution.hpp"

/**
 * @note backend.hpp is not included here as it includes global_ptr.hpp,
 *       which means that the data distribution definitions precede the
 *       backend definitions, and that is why we need to forward here.
 */
namespace argo {
	/* forward argo::backend function declarations */
	namespace backend {
		node_id_t node_id();
		std::size_t& backing_offset();
		/* forward argo::backend::atomic function declarations */
		namespace atomic {
			void _store_public_dir(const void* desired,
				const std::size_t size, const std::size_t rank, const std::size_t disp);
			void _store_local_dir(const std::size_t desired,
				const std::size_t rank, const std::size_t disp);
			void _load_local_dir(void* output_buffer,
				const std::size_t rank, const std::size_t disp);
			void _compare_exchange_dir(const void* desired, const void* expected, void* output_buffer,
				const std::size_t size, const std::size_t rank, const std::size_t disp);
		} // namespace atomic
	} // namespace backend
} // namespace argo

namespace argo {
	namespace data_distribution {
		/**
		 * @brief the first-touch data distribution
		 * @details gives ownership of a page to the node that first touched it.
		 */
		template<int instance>
		class first_touch_distribution : public base_distribution<instance> {
			private:
				/**
				 * @brief try and claim ownership of a page
				 * @param addr address in the global address space
				 * @return the homenode of addr
				 */
				static std::size_t first_touch (const std::size_t& addr);
				/** @brief protects the owner directory */
				std::mutex owner_mutex;

			public:
				virtual node_id_t homenode (char* const ptr) {
					std::size_t homenode_bit_id;
					const std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
					const std::size_t owner_window_index = 2 * (addr / granularity);

					std::unique_lock<std::mutex> def_lock(owner_mutex, std::defer_lock);
					def_lock.lock();
					argo::backend::atomic::_load_local_dir(&homenode_bit_id, argo::backend::node_id(), owner_window_index);
					if (!homenode_bit_id) { homenode_bit_id = first_touch(addr); }
					def_lock.unlock();

					node_id_t n, homenode;
					for(n = 0; n < base_distribution<instance>::nodes; n++) {
						if((1UL << n) == homenode_bit_id) {
							homenode = n;
						}
					}

					if(homenode >= static_cast<node_id_t>(base_distribution<instance>::nodes)) {
						exit(EXIT_FAILURE);
					}
					return homenode;
				}

				virtual std::size_t local_offset (char* const ptr) {
					std::size_t offset;
					static const std::size_t global_null = base_distribution<instance>::total_size + 1;
					const std::size_t drift = (ptr - base_distribution<instance>::start_address) % granularity;
					const std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
					const std::size_t owner_window_index = 2 * (addr / granularity);

					std::unique_lock<std::mutex> def_lock(owner_mutex, std::defer_lock);
					def_lock.lock();
					/* spin till an update from a remote node has been reflected in the local window */
					do {
						argo::backend::atomic::_load_local_dir(&offset, argo::backend::node_id(), owner_window_index+1);
					} while(offset == global_null);
					def_lock.unlock();
					offset += drift;

					if(offset >= static_cast<std::size_t>(base_distribution<instance>::size_per_node)) {
						std::cerr << "Maximum backing store size exceeded on node "
								<< argo::backend::node_id() << "\n"
								<< "\t- allocate more distributed memory or\n"
								<< "\t- ensure better distribution of data"
								<< std::endl;
						exit(EXIT_FAILURE);
					}
					return offset;
				}
		};

		template<int instance>
		std::size_t first_touch_distribution<instance>::first_touch (const std::size_t& addr) {
			/* variables for CAS */
			std::size_t homenode_bit_id, result;
			constexpr std::size_t compare = 0;
			const std::size_t id = 1UL << argo::backend::node_id();
			const std::size_t owner_window_index = 2 * (addr / granularity);
	    
			/* check/try to acquire ownership of the page with CAS to process' 0 index */
			argo::backend::atomic::_compare_exchange_dir(&id, &compare, &result, sizeof(std::size_t), 0, owner_window_index);

			/* this process was the first one to deposit the id */
			if (result == 0) {
				homenode_bit_id = id;

				/* mark the page in the local window */
				argo::backend::atomic::_store_local_dir(id, argo::backend::node_id(), owner_window_index);
				argo::backend::atomic::_store_local_dir(argo::backend::backing_offset(), argo::backend::node_id(), owner_window_index+1);

				/* mark the page in the public windows */
				node_id_t n;
				for(n = 0; n < base_distribution<instance>::nodes; n++) {
					if (n != argo::backend::node_id()) {
						argo::backend::atomic::_store_public_dir(&id, sizeof(std::size_t), n, owner_window_index);
						argo::backend::atomic::_store_public_dir(&argo::backend::backing_offset(), sizeof(std::size_t), n, owner_window_index+1);
					}
				}
		
				/* since a new page was acquired increase the homenode offset */
				argo::backend::backing_offset() += granularity;
			} else {
				homenode_bit_id = result;
			}
			
			return homenode_bit_id;
		}
	} // namespace data_distribution
} // namespace argo

#endif /* argo_first_touch_distribution_hpp */
