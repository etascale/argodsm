/**
 * @file
 * @brief This file provides an abstraction layer for distributing the shared memory space
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_data_distribution_hpp
#define argo_data_distribution_hpp argo_global_lock_hpp

#include <type_traits>
#include <cstddef>

#include "../types/types.hpp"
#include "../env/env.hpp"

namespace argo {
	namespace data_distribution {
		/* forward declaration */
		template<int instance> class naive_data_distribution;

		/**
		 * @brief smart pointers for global memory addresses
		 * @tparam T pointer to T
		 */
		template<typename T, class Dist = naive_data_distribution<0>>
		class global_ptr {
			private:
				/** @brief the ArgoDSM node this pointer is pointing to */
				node_id_t homenode;

				/** @brief local offset in the ArgoDSM node's local share of the global memory */
				std::size_t local_offset;

			public:
				/** @brief construct nullptr */
				global_ptr() : homenode(-1), local_offset(0) {}

				/**
				 * @brief construct from virtual address pointer
				 * @param ptr pointer to construct from
				 * @param sel select to fetch homenode (0), offset (1) or both (2, default)
				 */
				global_ptr(T* ptr, char sel = 2) {
					switch (sel) {
						case 0:
							homenode = Dist::homenode(reinterpret_cast<char*>(ptr));
							local_offset = 0;
							break;
						case 1:
							local_offset = Dist::local_offset(reinterpret_cast<char*>(ptr));
							homenode = -1;
							break;
						default:
							homenode = Dist::homenode(reinterpret_cast<char*>(ptr));
							local_offset = Dist::local_offset(reinterpret_cast<char*>(ptr));
							break;
					}
				}

				/**
				 * @brief Copy constructor between different pointer types
				 * @param other The pointer to copy from
				 */
				template<typename U>
				explicit global_ptr(global_ptr<U> other)
					: homenode(other.node()), local_offset(other.offset()) {}

				/**
				 * @brief get standard pointer
				 * @return pointer to object this smart pointer is pointing to
				 * @todo implement
				 */
				T* get() const {
					return reinterpret_cast<T*>(Dist::get_ptr(homenode, local_offset));
				}

				/**
				 * @brief dereference smart pointer
				 * @return dereferenced object
				 */
				typename std::add_lvalue_reference<T>::type operator*() const {
					return *this->get();
				}

				/**
				 * @brief return the home node of the value pointed to
				 * @return home node id
				 */
				node_id_t node() {
					return homenode;
				}

				/**
				 * @brief return the offset on the home node's local memory share
				 * @return local offset
				 */
				std::size_t offset() {
					return local_offset;
				}

		};

		/**
		 * @brief the naive data distribution
		 * @details each ArgoDSM node provides an equally-sized chunk of global
		 *          memory, and these chunks are simply concatenated in order or
		 *          ArgoDSM ids to form the global address space.
		 * @tparam instance used to statically allow for multiple instances
		 * @note all functions are defined on char* only, as this guarantees a
		 *       fixed memory base unit of size 1
		 */
		template<int instance>
		class naive_data_distribution {
			private:
				/** @brief number of ArgoDSM nodes */
				static int nodes;

				/** @brief starting address of the memory space */
				static char* start_address;

				/** @brief size of the memory space */
				static long total_size;

				/** @brief one node's share of the memory space */
				static long size_per_node;

			public:
				/**
				 * @brief set runtime parameters for global memory space
				 * @param n numer of ArgoDSM nodes
				 * @param start pointer to the memory space
				 * @param size size of the memory space
				 */
				static void set_memory_space(const int n, char* const start, const long size) {
					nodes = n;
					start_address = start;
					total_size = size;
					size_per_node = size / n;
				}

				/**
				 * @brief gives ownership of a page to the process that first touched it
				 * @param addr address in the global address 
				 * @return the homenode of addr
				 */
				static std::size_t first_touch (const std::size_t& addr);

				/**
				 * @brief compute home node of an address
				 * @param ptr address to find homenode of
				 * @return the computed home node
				 */
				static node_id_t homenode (char* const ptr);

				/**
				 * @brief compute offset into the home node's share of the memory
				 * @param ptr address to find offset of
				 * @return the computed offset
				 */
				static std::size_t local_offset (char* const ptr);

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
		template<int i> int naive_data_distribution<i>::nodes;
		template<int i> char* naive_data_distribution<i>::start_address;
		template<int i> long naive_data_distribution<i>::total_size;
		template<int i> long naive_data_distribution<i>::size_per_node;
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


#endif /* argo_data_distribution */
