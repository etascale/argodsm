/**
 * @file
 * @brief This file provides C bindings for some ArgoDSM synchronization primitives
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef SRC_SYNCHRONIZATION_SYNCHRONIZATION_H_
#define SRC_SYNCHRONIZATION_SYNCHRONIZATION_H_

#include <stddef.h>

/**
 * @brief a barrier for ArgoDSM nodes
 * @param threadcount number of threads on each ArgoDSM node to wait for
 * @details this barrier waits until threadcount threads have reached this
 *          barrier call on each ArgoDSM node, then performs self-downgrade
 *          and self-invalidation on each node.
 */
void argo_barrier(size_t threadcount);

/**
 * @brief a barrier for ArgoDSM nodes
 * @param threadcount number of threads on each ArgoDSM node to wait for
 * @details this barrier waits until threadcount threads have reached this
 *          barrier call on each ArgoDSM node, then performs self-downgrade
 *          and self-invalidation on each node. Additionally, this barrier
 *          upgrades all pages with registered writers to a shared state.
 */
void argo_barrier_upgrade_writers(size_t threadcount);

/**
 * @brief a barrier for ArgoDSM nodes
 * @param threadcount number of threads on each ArgoDSM node to wait for
 * @details this barrier waits until threadcount threads have reached this
 *          barrier call on each ArgoDSM node, then performs self-downgrade
 *          and self-invalidation on each node. Additionally, this barrier
 *          upgrades all pages to a private state.
 */
void argo_barrier_upgrade_all(size_t threadcount);

#endif // SRC_SYNCHRONIZATION_SYNCHRONIZATION_H_
