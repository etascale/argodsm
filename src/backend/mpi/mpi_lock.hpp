/**
 * @file
 * @brief Declaration of MPI lock
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_mpi_lock_hpp
#define argo_mpi_lock_hpp argo_mpi_lock_hpp

#include <pthread.h>
#include <mpi.h>

/** @brief Provides MPI RMA epoch locking */
class mpi_lock {
	private:
		/** @brief spinlock protecting the MPI lock */
		pthread_spinlock_t spin_lock;

		/** @brief General statistics */
		int num_locks;

		/**@{*/
		/**
		 * @brief Variables used to track spin lock statistics
		 */
		double spin_lock_time, max_spin_lock_time;
		double spin_hold_time, max_spin_hold_time, spin_acquire_time, spin_release_time;
		/**@}*/

		/**@{*/
		/**
		 * @brief Variables used to track MPI lock statistics
		 */
		double mpi_lock_time, max_mpi_lock_time;
		double mpi_unlock_time, max_mpi_unlock_time;
		double mpi_hold_time, max_mpi_hold_time, mpi_acquire_time, mpi_release_time;
		/**@}*/

	public:
		/**
		 * @brief mpi_lock constructor 
		 */
		mpi_lock();

		/*********************************************************
		 * LOCK ACQUISITION AND RELEASE
		 * ******************************************************/

		/** 
		 * @brief acquire mpi_lock
		 * @param lock_type MPI_LOCK_SHARED or MPI_LOCK_EXCLUSIVE
		 * @param target    target node of the lock
		 * @param window    MPI window to lock
		 */
		void lock(int lock_type, int target, MPI_Win window);

		/** 
		 * @brief release mpi_lock
		 * @param target    target node of the lock
		 * @param window    MPI window to lock
		 */
		void unlock(int target, MPI_Win window); 

		/** 
		 * @brief try to acquire lock
		 * @param lock_type MPI_LOCK_SHARED or MPI_LOCK_EXCLUSIVE
		 * @param target    target node of the lock
		 * @param window    MPI window to lock
		 * @return          true if successful, false otherwise
		 */
		bool trylock(int lock_type, int target, MPI_Win window);


		/*********************************************************
		 * SPIN LOCK STATISTICS
		 * ******************************************************/

		/**
		 * @brief  get timekeeping statistics
		 * @return the total time spent locking a spin lock
		 */
		double get_spin_lock_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the maximum time spent locking a spin lock
		 */
		double get_max_spin_lock_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the total time spent holding a spin lock
		 */
		double get_spin_hold_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the maximum time spent holding a spin lock
		 */
		double get_max_spin_hold_time();

		/*********************************************************
		 * MPI LOCK STATISTICS
		 * ******************************************************/

		/**
		 * @brief  get timekeeping statistics
		 * @return the total time spent locking an mpi_lock
		 */
		double get_mpi_lock_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the maximum time spent locking an mpi_lock
		 */
		double get_max_mpi_lock_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the total time spent unlocking an mpi_lock
		 */
		double get_mpi_unlock_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the maximum time spent unlocking an mpi_lock
		 */
		double get_max_mpi_unlock_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the total time spent holding an mpi_lock
		 */
		double get_mpi_hold_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the maximum time spent holding an mpi_lock
		 */
		double get_max_mpi_hold_time();


		/*********************************************************
		 * GENERAL
		 * ******************************************************/

		/**
		 * @brief  get timekeeping statistics
		 * @return the total number of locks taken
		 */
		int get_num_locks();

		/**
		 * @brief reset the timekeeping statistics
		 */
		void reset_stats();
};

#endif /* argo_mpi_lock_hpp */
