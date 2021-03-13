/**
 * @file
 * @brief This file implements the base distribution class for all the policies to derive from
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_base_distribution_hpp
#define argo_base_distribution_hpp argo_base_distribution_hpp

#include <cstdlib>
#include <iostream>
#include <system_error>

#include "../types/types.hpp"

namespace argo {
	namespace data_distribution {
		/** @brief page size for the implementations */
		static constexpr std::size_t granularity = 0x1000UL;

		/** @brief error message string for the implementations */
		static const std::string msg_fetch_homenode_fail = "ArgoDSM failed to fetch a valid backing node. Please report a bug.";
		/** @brief error message string for the implementations */
		static const std::string msg_fetch_offset_fail = "ArgoDSM failed to fetch a valid backing offset. Please report a bug.";
		
		/**
		 * @brief the base data distribution class
		 * @details this is the parent class from which all the memory policies
		 *          inherit in order to implement their own homenode and
		 *          local_offset functions.
		 * @tparam instance used to statically allow for multiple instances
		 * @note all functions are defined on char* only, as this guarantees a
		 *       fixed memory base unit of size 1.
		 */
		template<int instance>
		class base_distribution {
			protected:
				/** @brief number of ArgoDSM nodes */
				static node_id_t nodes;

				/** @brief starting address of the memory space */
				static char* start_address;

				/** @brief size of the memory space */
				static std::size_t total_size;

				/** @brief one node's share of the memory space */
				static std::size_t size_per_node;

			public:
				/**
				 * @brief set runtime parameters for global memory space
				 * @param n numer of ArgoDSM nodes
				 * @param start pointer to the memory space
				 * @param size size of the memory space
				 */
				static void set_memory_space(const node_id_t n, char* const start, const std::size_t size) {
					nodes = n;
					start_address = start;
					total_size = size;
					size_per_node = size / n;
				}

				/**
				 * @brief compute home node of an address
				 * @param ptr address to find homenode of
				 * @return the computed home node
				 */
				virtual node_id_t homenode (char* const ptr) {
					(void)ptr;
					return -1;
				}

				/**
				 * @brief compute offset into the home node's share of the memory
				 * @param ptr address to find offset of
				 * @return the computed offset
				 */
				virtual std::size_t local_offset (char* const ptr) {
					(void)ptr;
					return 0;
				}

				/**
				 * @brief compute a pointer value
				 * @param homenode the adress's home node
				 * @param offset the offset in the home node's memory share
				 * @return a pointer to the requested address
				 */
				static char* get_ptr(const node_id_t homenode, const std::size_t offset) {
					return start_address + homenode*size_per_node + offset;
				}
		};
		template<int i> node_id_t base_distribution<i>::nodes;
		template<int i> char* base_distribution<i>::start_address;
		template<int i> std::size_t base_distribution<i>::total_size;
		template<int i> std::size_t base_distribution<i>::size_per_node;
	} // namespace data_distribution
} // namespace argo

#endif /* argo_base_distribution_hpp */
