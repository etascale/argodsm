/**
 * @file
 * @brief This file implements the prime-mapp and prime-mapp-block data distributions
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_prime_mapp_distribution_hpp
#define argo_prime_mapp_distribution_hpp argo_prime_mapp_distribution_hpp

#include "base_distribution.hpp"

namespace argo {
	namespace data_distribution {
		/**
		 * @brief the prime-mapp data distribution
		 * @details distributes pages using a two-phase round-robin strategy
		 */
		template<int instance>
		class prime_mapp_distribution : public base_distribution<instance> {
			public:
				virtual node_id_t homenode (char* const ptr) {
					static constexpr std::size_t zero = 0;
					static const std::size_t prime = (3 * base_distribution<instance>::nodes) / 2;
					const std::size_t addr = ptr - base_distribution<instance>::start_address;
					const std::size_t lessaddr = (addr >= granularity) ? addr - granularity : zero;
					const std::size_t pagenum = lessaddr / granularity;
					node_id_t homenode = ((pagenum % prime) >= static_cast<std::size_t>(base_distribution<instance>::nodes))
					? ((pagenum / prime) * (prime - base_distribution<instance>::nodes) + ((pagenum % prime) - base_distribution<instance>::nodes)) % base_distribution<instance>::nodes
					: pagenum % prime;

					if(homenode >= base_distribution<instance>::nodes) {
						exit(EXIT_FAILURE);
					}
					return homenode;
				}

				virtual std::size_t local_offset (char* const ptr) {
					static constexpr std::size_t zero = 0;
					static const std::size_t prime = (3 * base_distribution<instance>::nodes) / 2;
					const std::size_t drift = (ptr - base_distribution<instance>::start_address) % granularity;
					std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
					std::size_t offset, lessaddr = (addr >= granularity) ? addr - granularity : zero;
					std::size_t pagenum = lessaddr / granularity;
					if ((addr <= (base_distribution<instance>::nodes * granularity)) || ((pagenum % prime) >= static_cast<std::size_t>(base_distribution<instance>::nodes))) {
						offset = (pagenum / base_distribution<instance>::nodes) * granularity + (addr > 0 && !homenode(ptr)) * granularity + drift;
					} else {
						node_id_t currhome;
						std::size_t homecounter = 0;
						const node_id_t realhome = homenode(ptr);
						for (addr -= granularity; ; addr -= granularity) {
							lessaddr = addr - granularity;
							pagenum = lessaddr / granularity;
							currhome = homenode(static_cast<char*>(base_distribution<instance>::start_address) + addr);
							homecounter += (currhome == realhome) ? 1 : 0;
							if (((addr <= (base_distribution<instance>::nodes * granularity)) && (currhome == realhome)) ||
									(((pagenum % prime) >= static_cast<std::size_t>(base_distribution<instance>::nodes) && (currhome == realhome)))) {
								offset = (pagenum / base_distribution<instance>::nodes) * granularity + !realhome * granularity;
								offset += homecounter * granularity + drift;
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

		/**
		 * @brief the prime-mapp-block data distribution
		 * @details distributes blocks of pages using a two-phase round-robin strategy
		 */
		template<int instance>
		class prime_mapp_block_distribution : public base_distribution<instance> {
			public:
				virtual node_id_t homenode (char* const ptr) {
					static constexpr std::size_t zero = 0;
					static const std::size_t pageblock = env::allocation_block_size() * granularity;
					static const std::size_t prime = (3 * base_distribution<instance>::nodes) / 2;
					const std::size_t addr = ptr - base_distribution<instance>::start_address;
					const std::size_t lessaddr = (addr >= granularity) ? addr - granularity : zero;
					const std::size_t pagenum = lessaddr / pageblock;
					node_id_t homenode = ((pagenum % prime) >= static_cast<std::size_t>(base_distribution<instance>::nodes))
					? ((pagenum / prime) * (prime - base_distribution<instance>::nodes) + ((pagenum % prime) - base_distribution<instance>::nodes)) % base_distribution<instance>::nodes
					: pagenum % prime;

					if(homenode >= base_distribution<instance>::nodes) {
						exit(EXIT_FAILURE);
					}
					return homenode;
				}

				virtual std::size_t local_offset (char* const ptr) {
					static constexpr std::size_t zero = 0;
					static const std::size_t pageblock = env::allocation_block_size() * granularity;
					static const std::size_t prime = (3 * base_distribution<instance>::nodes) / 2;
					const std::size_t drift = (ptr - base_distribution<instance>::start_address) % granularity;
					std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
					std::size_t offset, lessaddr = (addr >= granularity) ? addr - granularity : zero;
					std::size_t pagenum = lessaddr / pageblock;
					if ((addr <= (base_distribution<instance>::nodes * pageblock)) || ((pagenum % prime) >= static_cast<std::size_t>(base_distribution<instance>::nodes))) {
						offset = (pagenum / base_distribution<instance>::nodes) * pageblock + lessaddr % pageblock + (addr > 0 && !homenode(ptr)) * granularity + drift;
					} else {
						node_id_t currhome;
						std::size_t homecounter = 0;
						const node_id_t realhome = homenode(ptr);
						for (addr -= pageblock; ; addr -= pageblock) {
							lessaddr = addr - granularity;
							pagenum = lessaddr / pageblock;
							currhome = homenode(static_cast<char*>(base_distribution<instance>::start_address) + addr);
							homecounter += (currhome == realhome) ? 1 : 0;
							if (((addr <= (base_distribution<instance>::nodes * pageblock)) && (currhome == realhome)) || 
									(((pagenum % prime) >= static_cast<std::size_t>(base_distribution<instance>::nodes)) && (currhome == realhome))) {
								offset = (pagenum / base_distribution<instance>::nodes) * pageblock + lessaddr % pageblock + !realhome * granularity;
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
