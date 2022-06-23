/**
 * @file
 * @brief This file provides an abstraction layer for distributing the shared memory space
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_DATA_DISTRIBUTION_DATA_DISTRIBUTION_HPP_
#define ARGODSM_SRC_DATA_DISTRIBUTION_DATA_DISTRIBUTION_HPP_

// Alphabetic order below
#include "base_distribution.hpp"
#include "cyclic_distribution.hpp"
#include "first_touch_distribution.hpp"
#include "naive_distribution.hpp"
#include "prime_mapp_distribution.hpp"
#include "skew_mapp_distribution.hpp"

/**
 * @note backend.hpp is not included here as it includes global_ptr.hpp,
 *       which means that the data distribution definitions precede the
 *       backend definitions, and that is why we need to forward here.
 */
namespace argo {
/* forward argo::backend function declarations */
namespace backend {
	node_id_t number_of_nodes();
}  // namespace backend
}  // namespace argo

namespace argo {
namespace data_distribution {

/**
 * @brief Enumeration for the available distributions
 */
enum memory_policy {
	/**
	 * @brief the naive distribution scheme
	 * @note distributes data at the default
	 *       page granularity level (4KB).
	 * @see naive_distribution.hpp
	 * @see @ref ARGO_ALLOCATION_POLICY
	 */
	naive,
	/**
	 * @brief the cyclic policy
	 * @note distributes data at a configurable page granularity level.
	 * @see cyclic_distribution.hpp
	 * @see @ref ARGO_ALLOCATION_POLICY
	 * @see @ref ARGO_ALLOCATION_BLOCK_SIZE
	 */
	cyclic,
	/**
	 * @brief the skew-mapp policy
	 * @note distributes data at a configurable page granularity level.
	 * @see skew_mapp_distribution.hpp
	 * @see @ref ARGO_ALLOCATION_POLICY
	 * @see @ref ARGO_ALLOCATION_BLOCK_SIZE
	 */
	skew_mapp,
	/**
	 * @brief the prime-mapp policy
	 * @note distributes data at a configurable page granularity level.
	 * @see prime_mapp_distribution.hpp
	 * @see @ref ARGO_ALLOCATION_POLICY
	 * @see @ref ARGO_ALLOCATION_BLOCK_SIZE
	 */
	prime_mapp,
	/**
	 * @brief the first-touch policy
	 * @note distributes data at the default page granularity level (4KB).
	 * @see first_touch_distribution.hpp
	 * @see @ref ARGO_ALLOCATION_POLICY
	 */
	first_touch
};

/**
 * @brief identifies if we distribute data using a cyclic memory policy
 * @return true if we utilize a cyclic policy
 *         false if we don't utilize a cyclic policy
 */
static inline bool is_cyclic_policy() {
	std::size_t policy = env::allocation_policy();
	switch (policy) {
		case cyclic	: return true;
		case skew_mapp	: return true;
		case prime_mapp	: return true;
		default		: return false;
	}
}

/**
 * @brief identifies if we distribute data using the prime-mapp memory policy
 * @return true if we utilize the prime-mapp policy
 *         false if we don't utilize prime-mapp policy
 */
static inline bool is_prime_mapp_policy() {
	return env::allocation_policy() == prime_mapp;
}

/**
 * @brief identifies if we distribute data using the first-touch memory policy
 * @return true if we utilize the first-touch policy
 *         false if we don't utilize first-touch policy
 */
static inline bool is_first_touch_policy() {
	return env::allocation_policy() == first_touch;
}

/**
 * @brief based on the chosen policy, gets the required size we need to
 *        add to the standardisation of the ArgoDSM global memory space
 * @return the padding size based on the chosen policy
 */
static inline std::size_t policy_padding() {
	std::size_t padding = (is_cyclic_policy()) ? env::allocation_block_size() : 1;
	padding *= (is_prime_mapp_policy()) ? ((3 * argo::backend::number_of_nodes()) / 2) : 1;
	return padding;
}

/**
 * @brief based on the chosen policy, gets the policy block size
 * @return The requested block size for all cyclical policies,
 * 	   the size of a page for the first-touch policy or the chunk
 * 	   size per node for the naive policy
 */
static inline std::size_t policy_block_size() {
	if(is_cyclic_policy()) {
		std::size_t requested_block_size = env::allocation_block_size();
		return requested_block_size*granularity;
	}
	return is_first_touch_policy() ? granularity : base_distribution<0>::get_size_per_node();
}

}  // namespace data_distribution
}  // namespace argo

#endif // ARGODSM_SRC_DATA_DISTRIBUTION_DATA_DISTRIBUTION_HPP_
