/**
 * @file
 * @brief file listing all template instances in the backend
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "types/types.hpp"

/** @brief explicit instantiation for pointers to raw memory */
template void broadcast(node_id_t, memory_t*);
