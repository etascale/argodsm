/**
 * @file
 * @brief This file implements synchronization primitives for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "backend/backend.hpp"
#include "synchronization.hpp"

namespace argo {
	void barrier(std::size_t threadcount) {
		backend::barrier(threadcount, backend::upgrade_type::upgrade_none);
	}

	void barrier_upgrade_writers(std::size_t threadcount) {
		backend::barrier(threadcount, backend::upgrade_type::upgrade_writers);
	}

	void barrier_upgrade_all(std::size_t threadcount) {
		backend::barrier(threadcount, backend::upgrade_type::upgrade_all);
	}
} // namespace argo

extern "C" {
	void argo_barrier(size_t threadcount) {
		argo::barrier(threadcount);
	}

	void argo_barrier_upgrade_writers(size_t threadcount) {
		argo::barrier_upgrade_writers(threadcount);
	}

	void argo_barrier_upgrade_all(size_t threadcount) {
		argo::barrier_upgrade_all(threadcount);
	}
}
