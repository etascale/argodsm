/**
 * @file
 * @brief This file implements the naive data distribution
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_naive_distribution_hpp
#define argo_naive_distribution_hpp argo_naive_distribution_hpp

#include "base_distribution.hpp"

namespace argo {
	namespace data_distribution {
		/**
		 * @brief the naive data distribution
		 * @details each ArgoDSM node provides an equally-sized chunk of global
		 *          memory, and these chunks are simply concatenated in order or
		 *          ArgoDSM ids to form the global address space.
		 */
		template<int instance>
		class naive_distribution : public base_distribution<instance> {
			public:
				virtual node_id_t homenode (char* const ptr) {
					const std::size_t addr = ptr - base_distribution<instance>::start_address;
					node_id_t homenode = addr / base_distribution<instance>::size_per_node;

					if(homenode >= base_distribution<instance>::nodes) {
						std::cerr << msg_fetch_homenode_fail << std::endl;
						throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_fetch_homenode_fail);
						exit(EXIT_FAILURE);
					}
					return homenode;
				}

				virtual node_id_t peek_homenode(char* const ptr) {
					return homenode(ptr);
				}

				virtual std::size_t local_offset (char* const ptr) {
					const std::size_t addr = ptr - base_distribution<instance>::start_address;
					std::size_t offset = addr - (homenode(ptr)) * base_distribution<instance>::size_per_node;
                    
					if(offset >= static_cast<std::size_t>(base_distribution<instance>::size_per_node)) {
						std::cerr << msg_fetch_offset_fail << std::endl;
						throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_fetch_offset_fail);
						exit(EXIT_FAILURE);
					}
					return offset;
				}

				virtual std::size_t peek_local_offset (char* const ptr) {
					return local_offset(ptr);
				}

				// CSPext:

				virtual node_id_t parity_node(char* const ptr) {
					// replication == home node if nodes == 1
					std::size_t nodes = base_distribution<instance>::nodes;
					if (nodes == 1) {
						return 0;
					}

					if (env::replication_policy() == 0) {
						return (peek_homenode(ptr) + 1) % nodes;
					}
					else if (env::replication_policy() == 1) {
						std::size_t local_page_number = local_offset(ptr) / data_distribution::granularity;
						std::size_t cyclical_page_number = homenode(ptr) + (local_page_number * nodes);
						std::size_t data_blocks = nodes - 1;
						return (((cyclical_page_number / data_blocks) + 1) * data_blocks) % nodes;
					}
					return invalid_node_id;
				}

				// CSPext:

				virtual std::size_t parity_offset(char* const ptr) {
					std::size_t nodes = base_distribution<instance>::nodes;
					if (nodes == 1) {
						return local_offset(ptr);
					}
					if (env::replication_policy() == 0) {
						return local_offset(ptr);
					}
					else if (env::replication_policy() == 1) {
						std::size_t parity_page_number = local_offset(ptr) / (data_distribution::granularity * (base_distribution<instance>::nodes - 1));
						return parity_page_number * data_distribution::granularity;
					}
					return invalid_offset;
				}


		};
	} // namespace data_distribution
} // namespace argo

#endif /* argo_naive_distribution_hpp */
