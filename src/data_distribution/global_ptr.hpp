/**
 * @file
 * @brief This file provides the global pointer for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_global_ptr_hpp
#define argo_global_ptr_hpp argo_global_ptr_hpp

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
				global_ptr() : homenode(-1), local_offset(0) {}

				/**
				 * @brief construct from virtual address pointer
				 * @param ptr pointer to construct from
				 * @param sel select to invoke the homenode, the local_offset or both
				 */
				global_ptr(T* ptr, const std::string& sel = "")
						: homenode(-1), local_offset(0), access_ptr(ptr) {
					if (!sel.compare("getHomenode")) {
						homenode = policy()->homenode(reinterpret_cast<char*>(ptr));
					} else if (!sel.compare("getOffset")) {
						local_offset = policy()->local_offset(reinterpret_cast<char*>(ptr));
					} else {
						homenode = policy()->homenode(reinterpret_cast<char*>(ptr));
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
					return homenode;
				}

				/**
				 * @brief return the offset on the home node's local memory share
				 * @return local offset
				 */
				std::size_t offset() {
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

#endif /* argo_global_ptr_hpp */
