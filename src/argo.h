/**
 * @file
 * @brief This file provides the full C interface to ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_argo_h
#define argo_argo_h argo_argo_h

#include <stddef.h>

#include "allocators/allocators.h"
#include "synchronization/synchronization.h"

/**
 * @brief initialize ArgoDSM system
 * @param argo_size the desired size of the global memory in bytes
 * @param cache_size the desired size of the local cache in bytes
 */
void argo_init(size_t argo_size = 0, size_t cache_size = 0);

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

#endif /* argo_argo_h */
