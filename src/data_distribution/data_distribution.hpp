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

namespace argo {
	namespace data_distribution {
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
