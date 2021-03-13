/**
 * @file
 * @brief This file implements the skew-mapp data distribution
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_skew_mapp_distribution_hpp
#define argo_skew_mapp_distribution_hpp argo_skew_mapp_distribution_hpp

#include "base_distribution.hpp"

namespace argo {
	namespace data_distribution {
		/**
		 * @brief the skew-mapp data distribution
		 * @details cyclically distributes a block of pages per round but skips
		 *          a node for every N (number of nodes used) pages allocated.
		 */
		template<int instance>
		class skew_mapp_distribution : public base_distribution<instance> {
			public:
				virtual node_id_t homenode (char* const ptr) {
					static const std::size_t pageblock = env::allocation_block_size() * granularity;
					const std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
					const std::size_t pagenum = addr / pageblock;
					const node_id_t homenode = (pagenum + pagenum / base_distribution<instance>::nodes + 1) % base_distribution<instance>::nodes;

					if(homenode >= base_distribution<instance>::nodes) {
						std::cerr << msg_fetch_homenode_fail << std::endl;
						throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_fetch_homenode_fail);
						exit(EXIT_FAILURE);
					}
					return homenode;
				}

				virtual std::size_t local_offset (char* const ptr) {
					static const std::size_t pageblock = env::allocation_block_size() * granularity;
					const std::size_t drift = (ptr - base_distribution<instance>::start_address) % granularity;
					const std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
					const std::size_t pagenum = addr / pageblock;
					const std::size_t offset = pagenum / base_distribution<instance>::nodes * pageblock + addr % pageblock + drift;

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

#endif /* argo_skew_mapp_distribution_hpp */
