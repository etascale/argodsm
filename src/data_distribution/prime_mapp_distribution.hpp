/**
 * @file
 * @brief This file implements the prime-mapp data distribution
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_prime_mapp_distribution_hpp
#define argo_prime_mapp_distribution_hpp argo_prime_mapp_distribution_hpp

#include "base_distribution.hpp"

namespace argo {
	namespace data_distribution {
		/**
		 * @brief the prime-mapp data distribution
		 * @details distributes blocks of pages using a two-phase round-robin strategy.
		 */
		template<int instance>
		class prime_mapp_distribution : public base_distribution<instance> {
			public:
				virtual node_id_t homenode (char* const ptr) {
					static const std::size_t pageblock = env::allocation_block_size() * granularity;
					static const std::size_t prime = (3 * base_distribution<instance>::nodes) / 2;
					const std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
					const std::size_t pagenum = addr / pageblock;
					const node_id_t homenode = ((pagenum % prime) >= static_cast<std::size_t>(base_distribution<instance>::nodes))
					? ((pagenum / prime) * (prime - base_distribution<instance>::nodes) + ((pagenum % prime) - base_distribution<instance>::nodes)) % base_distribution<instance>::nodes
					: pagenum % prime;

					if(homenode >= base_distribution<instance>::nodes) {
						exit(EXIT_FAILURE);
					}
					return homenode;
				}

				virtual std::size_t local_offset (char* const ptr) {
					static const std::size_t pageblock = env::allocation_block_size() * granularity;
					static const std::size_t prime = (3 * base_distribution<instance>::nodes) / 2;
					const std::size_t drift = (ptr - base_distribution<instance>::start_address) % granularity;
					std::size_t offset, addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
					std::size_t pagenum = addr / pageblock;
					if ((addr <= (base_distribution<instance>::nodes * pageblock)) || ((pagenum % prime) >= static_cast<std::size_t>(base_distribution<instance>::nodes))) {
						offset = (pagenum / base_distribution<instance>::nodes) * pageblock + addr % pageblock + drift;
					} else {
						node_id_t currhome;
						std::size_t homecounter = 0;
						const node_id_t realhome = homenode(ptr);
						for (addr -= pageblock; ; addr -= pageblock) {
							pagenum = addr / pageblock;
							currhome = homenode(static_cast<char*>(base_distribution<instance>::start_address) + addr);
							homecounter += (currhome == realhome) ? 1 : 0;
							if (((addr <= (base_distribution<instance>::nodes * pageblock)) && (currhome == realhome)) || 
									(((pagenum % prime) >= static_cast<std::size_t>(base_distribution<instance>::nodes)) && (currhome == realhome))) {
								offset = (pagenum / base_distribution<instance>::nodes) * pageblock + addr % pageblock;
								offset += homecounter * pageblock + drift;
								break;
							}
						}
					}

					if(offset >= static_cast<std::size_t>(base_distribution<instance>::size_per_node)) {
						exit(EXIT_FAILURE);
					}
					return offset;
				}
		};
	} // namespace data_distribution
} // namespace argo

#endif /* argo_prime_mapp_distribution_hpp */
