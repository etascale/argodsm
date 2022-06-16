/**
 * @file
 * @brief Declaration of MPI lock
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_BACKEND_MPI_MPI_LOCK_HPP_
#define ARGODSM_SRC_BACKEND_MPI_MPI_LOCK_HPP_

#include <mpi.h>
#include <pthread.h>

/** @brief Provides MPI RMA epoch locking */
class mpi_lock {
	private:
		/** @brief local lock protecting the MPI lock from multi-threaded access */
		pthread_spinlock_t _local_lock;

		/** @brief General statistics */
		int _num_locks{0};

		/**@{*/
		/**
		 * @brief Variables used to track local lock statistics
		 */
		double _local_lock_time{0}, _max_local_lock_time{0};
		double _local_hold_time{0}, _max_local_hold_time{0};
		double _local_acquire_time{0}, _local_release_time{0};
		/**@}*/

		/**@{*/
		/**
		 * @brief Variables used to track MPI lock statistics
		 */
		double _mpi_lock_time{0}, _max_mpi_lock_time{0};
		double _mpi_unlock_time{0}, _max_mpi_unlock_time{0};
		double _mpi_hold_time{0}, _max_mpi_hold_time{0};
		double _mpi_acquire_time{0}, _mpi_release_time{0};
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
		 * LOCAL LOCK STATISTICS
		 * ******************************************************/

		/**
		 * @brief  get timekeeping statistics
		 * @return the total time spent locking a local lock
		 */
		double get_local_lock_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the maximum time spent locking a local lock
		 */
		double get_max_local_lock_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the total time spent holding a local lock
		 */
		double get_local_hold_time();

		/**
		 * @brief  get timekeeping statistics
		 * @return the maximum time spent holding a local lock
		 */
		double get_max_local_hold_time();

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

#endif  // ARGODSM_SRC_BACKEND_MPI_MPI_LOCK_HPP_
