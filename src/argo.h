/**
 * @file
 * @brief This file provides the full C interface to ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_ARGO_H_
#define ARGODSM_SRC_ARGO_H_

#include <stdbool.h>
#include <stddef.h>

#include "allocators/allocators.h"
#include "synchronization/synchronization.h"

/**
 * @brief initialize ArgoDSM system
 * @param argo_size The desired size of the global memory in bytes, or 0. If the
 *                  value is specified as 0, then the value in environment
 *                  variable @ref ARGO_MEMORY_SIZE is used instead.
 *                  If @ref ARGO_MEMORY_SIZE is unset, then a default is used.
 * @param cache_size The desired size of the local cache in bytes, or 0. If the
 *                   value is specified as 0, then the value in environment
 *                   variable @ref ARGO_CACHE_SIZE is used instead.
 *                   If @ref ARGO_CACHE_SIZE is unset, then a default is used.
 */
void argo_init(size_t argo_size, size_t cache_size);

/**
 * @brief shut down ArgoDSM system
 */
void argo_finalize();

/**
 * @brief reset ArgoDSM system
 * @warning this is mainly intended for testing, not for public use
 */
void argo_reset();

/**
 * @brief A unique ArgoDSM node identifier. Counting starts from 0.
 * @return The node id
 */
int  argo_node_id();

/**
 * @brief Number of ArgoDSM nodes being run
 * @return The total number of ArgoDSM nodes
 */
int  argo_number_of_nodes();

/**
 * @brief Check if addr belongs in the ArgoDSM memory space
 * @param addr A memory address
 * @return True if addr is in the ArgoDSM memory space, else false
 */
bool argo_is_argo_address(void* addr);

/**
 * @brief Get the ArgoDSM node id where addr is physically backed
 * @param addr A valid address in the ArgoDSM memory space
 * @return The id of the node physically backing addr, or
 * argo::data_distribution::invalid_node_id if it has not yet
 * been first-touched under the first-touch allocation policy
 * @pre addr must be an address in ArgoDSM memory
 */
int argo_get_homenode(void* addr);

/**
 * @brief Get the block size of the current allocation policy
 * @return The block size of the current allocation policy in bytes
 */
size_t argo_get_block_size();

#endif  // ARGODSM_SRC_ARGO_H_
