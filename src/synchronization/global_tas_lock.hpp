/**
 * @file
 * @brief This file provides a tas lock for the ArgoDSM system based on the TAS lock made by David Klaftenegger
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_global_tas_lock_hpp
#define argo_global_tas_lock_hpp argo_global_lock_hpp

#include "../backend/backend.hpp"
#include "../data_distribution/data_distribution.hpp"
#include <chrono>
#include <thread>

namespace argo {
	namespace globallock {
		/** @brief a global test-and-set lock */
		class global_tas_lock {
			private:
				/** @brief constant signifying lock is free */
				static const bool unlocked = false;
				/** @brief constant signifying lock is taken */
				static const bool locked = true;

				/** @brief import global_ptr */
				using global_flag = typename argo::data_distribution::global_ptr<bool>;

				/**
				 * @brief pointer to lock flag
				 * @todo should be replaced with an ArgoDSM-specific atomic type
				 *       to allow efficient synchronization over more backends
				 */
				global_flag flag;

			public:
				/**
				 * @brief construct global tas lock from existing flag in global address space
				 * @param f pointer to global flag
				 */
				global_tas_lock(bool* f) : flag(global_flag(f)) {
					*flag = unlocked;
				};

				/**
				 * @brief try to lock
				 * @return true if lock was successfully taken,
				 *         false otherwise
				 */
				bool try_lock() {
					auto old = backend::atomic::exchange(flag, locked, atomic::memory_order::relaxed);
					if(old == unlocked){
						backend::acquire();
						return true;
					}
					else{
						return false;
					}
				}

				/**
				 * @brief release the lock
				 */
				void unlock() {
					backend::release();
					backend::atomic::store(flag, unlocked);
				}

				/**
				 * @brief take the lock
				 */
				void lock() {
					while(!try_lock())
						std::this_thread::yield();
				}

				/**
				 * @brief internally used type for lock field
				 * @note this type may change without warning,
				 *       user code must use this type alias
				 */
				using internal_field_type = bool;
		};
	} // namespace globallock
} // namespace argo

#endif /* argo_global_tas_lock_hpp */
