/**
 * @file
 * @brief This file provides synchronization primitives for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_synchronization_hpp
#define argo_synchronization_hpp argo_synchronization_hpp

#include <cstddef>

#include "broadcast.hpp"

namespace argo {
	/**
	 * @brief a barrier for ArgoDSM nodes
	 * @param threadcount number of threads on each ArgoDSM node to wait for
	 * @details this barrier waits until threadcount threads have reached this
	 *          barrier call on each ArgoDSM node, then performs self-downgrade
	 *          and self-invalidation on each node.
	 */
	void barrier(std::size_t threadcount=1);

	/**
	 * @brief a barrier for ArgoDSM nodes
	 * @param threadcount number of threads on each ArgoDSM node to wait for
	 * @details this barrier waits until threadcount threads have reached this
	 *          barrier call on each ArgoDSM node, then performs self-downgrade
	 *          and self-invalidation on each node. Additionally, this barrier
	 *          upgrades all pages with registered writers to a shared state.
	 */
	void barrier_upgrade_writers(std::size_t threadcount=1);

	/**
	 * @brief a barrier for ArgoDSM nodes
	 * @param threadcount number of threads on each ArgoDSM node to wait for
	 * @details this barrier waits until threadcount threads have reached this
	 *          barrier call on each ArgoDSM node, then performs self-downgrade
	 *          and self-invalidation on each node. Additionally, this barrier
	 *          upgrades all pages to a private state.
	 */
	void barrier_upgrade_all(std::size_t threadcount=1);
} // namespace argo

extern "C" {
#include "synchronization.h"
}

#endif /* argo_synchronization_hpp */
