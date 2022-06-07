/**
 * @file
 * @brief This file provides a C interface for ArgoDSM-based cohort locks
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "cohort_lock.hpp"

using lock_t = argo::globallock::cohort_lock;

extern "C" {

	cohortlock_t argo_cohortlock_create() {
		lock_t* newlock = new lock_t;
		return reinterpret_cast<cohortlock_t>(newlock);
	}

	void argo_cohortlock_destroy(cohortlock_t lock) {
		lock_t* l = reinterpret_cast<lock_t*>(lock);
		delete l;
	}

	void argo_cohortlock_lock(cohortlock_t lock) {
		lock_t* l = reinterpret_cast<lock_t*>(lock);
		l->lock();
	}

	void argo_cohortlock_unlock(cohortlock_t lock) {
		lock_t* l = reinterpret_cast<lock_t*>(lock);
		l->unlock();
	}
}
