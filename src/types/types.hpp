/**
 * @file
 * @brief This file defines common types used in ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_TYPES_TYPES_HPP_
#define ARGODSM_SRC_TYPES_TYPES_HPP_

namespace argo {
	/** @brief ArgoDSM node identifier type */
	using node_id_t = unsigned int;

	/** @brief ArgoDSM number of nodes type */
	using num_nodes_t = unsigned int;

	/**
	 * @brief type of memory base addresses
	 * @see dynamic_memory_pool
	 */
	using memory_t = char*;
} // namespace argo

#endif // ARGODSM_SRC_TYPES_TYPES_HPP_
