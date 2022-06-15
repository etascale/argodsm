/**
 * @file
 * @brief This file provides the global pointer for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_DATA_DISTRIBUTION_GLOBAL_PTR_HPP_
#define ARGODSM_SRC_DATA_DISTRIBUTION_GLOBAL_PTR_HPP_

#include <string>

#include "../env/env.hpp"
#include "data_distribution.hpp"

namespace argo {
	namespace data_distribution {
		/**
		 * @brief smart pointers for global memory addresses
		 * @tparam T pointer to T
		 */
		template<typename T, class Dist = base_distribution<0>>
		class global_ptr {
			private:
				/** @brief the ArgoDSM node this pointer is pointing to */
				node_id_t homenode;

				/** @brief local offset in the ArgoDSM node's local share of the global memory */
				std::size_t local_offset;

				/** @brief pointer to the object in global memory this smart pointer is pointing to */
				T* access_ptr;

				/** @brief array holding an instance of each available policy */
				static Dist* policies[5];

			public:
				/** @brief construct nullptr */
				global_ptr() : homenode(invalid_node_id), local_offset(invalid_offset) {}

				/**
				 * @brief construct from virtual address pointer
				 * @param ptr pointer to construct from
				 * @param compute_homenode If true, compute the homenode in constructor
				 * @param compute_offset If true, compute local offset in constructor
				 */
				global_ptr(T* ptr, const bool compute_homenode = true,
									const bool compute_offset = true)
						: homenode(invalid_node_id), local_offset(invalid_offset), access_ptr(ptr) {
					if(compute_homenode){
						homenode = policy()->homenode(reinterpret_cast<char*>(ptr));
					}
					if(compute_offset){
						local_offset = policy()->local_offset(reinterpret_cast<char*>(ptr));
					}
				}

				/**
				 * @brief Copy constructor between different pointer types
				 * @param other The pointer to copy from
				 */
				template<typename U>
				explicit global_ptr(global_ptr<U> other)
					: homenode(other.node()), local_offset(other.offset()), access_ptr(other.get()) {}

				/**
				 * @brief get standard pointer
				 * @return pointer to object this smart pointer is pointing to
				 * @todo implement
				 */
				T* get() const {
					/**
					 * @note the get_ptr invocation is kept here to re-
					 *       member where and for what it was used for.
					 */
					// return reinterpret_cast<T*>(get_ptr(homenode, local_offset));
					return access_ptr;
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
					// If homenode is not yet calculated we need to find it
					if(homenode == invalid_node_id) {
						homenode = policy()->homenode(
								reinterpret_cast<char*>(access_ptr));
					}
					return homenode;
				}

				/**
				 * @brief return the home node of the value pointed to, or a
				 * default value if the page has not yet been first-touched
				 * under first-touch allocation.
				 * @return home node id, or argo::data_distribution::invalid_node_id
				 * if access_ptr has not been first-touched yet under the
				 * first-touch allocation policy
				 */
				node_id_t peek_node() {
					// If homenode is not yet calculated we need to find it
					if(homenode == invalid_node_id) {
						// Avoid invoking first-touch
						node_id_t new_node = policy()->peek_homenode(
								reinterpret_cast<char*>(access_ptr));
						if(new_node != invalid_node_id) {
							homenode = new_node;
						}
					}
					return homenode;
				}

				/**
				 * @brief return the offset on the home node's local memory share
				 * @return local offset
				 */
				std::size_t offset() {
					if(local_offset == invalid_offset) {
						local_offset = policy()->local_offset(
								reinterpret_cast<char*>(access_ptr));
					}
					return local_offset;
				}

				/**
				 * @brief return the offset on the home node's local memory share
				 * or a default value if the page has not yet been first-touched
				 * under first-touch allocation.
				 * @return local offset, or argo::data_distribution::invalid_offset
				 * if access_ptr has not been first-touched yet under the
				 * first-touch allocation policy
				 */
				std::size_t peek_offset() {
					// If homenode is not yet calculated we need to find it
					if(local_offset == invalid_offset) {
						// Avoid invoking first-touch
						std::size_t new_offset = policy()->peek_local_offset(
								reinterpret_cast<char*>(access_ptr));
						if(new_offset != invalid_offset) {
							local_offset = new_offset;
						}
					}
					return local_offset;
				}

				/**
				 * @brief return a pointer to the selected allocation policy
				 * @return enabled policy
				 */
				Dist* policy() {
					return policies[env::allocation_policy()];
				}
		};
		template<typename T, class Dist>
		Dist* global_ptr<T, Dist>::policies[] = {
			new naive_distribution<0>,
			new cyclic_distribution<0>,
			new skew_mapp_distribution<0>,
			new prime_mapp_distribution<0>,
			new first_touch_distribution<0>
		};
	} // namespace data_distribution
} // namespace argo

#endif // ARGODSM_SRC_DATA_DISTRIBUTION_GLOBAL_PTR_HPP_
