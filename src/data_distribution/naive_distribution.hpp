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
		};
	} // namespace data_distribution
} // namespace argo

#endif /* argo_naive_distribution_hpp */
