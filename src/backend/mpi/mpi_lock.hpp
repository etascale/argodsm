/**
 * @file
 * @brief Declaration of MPI lock
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef mpi_lock_h
#define mpi_lock_h mpi_lock_h

#include <atomic>
#include <pthread.h>
#include <mpi.h>

/** @brief Provides MPI RMA epoch locking */
class mpi_lock {
	private:
		/* @brief atomic spinlock */
		pthread_spinlock_t spin_lock;

		/** @brief Timekeeping for lock */
		std::atomic<double> locktime;
		double maxlocktime, mpilocktime;
		int numlocksremote;

		/** @brief Timekeeping for unlock */
		std::atomic<double> unlocktime;
		double maxunlocktime, mpiunlocktime;

		/** @brief General statistics */
		double holdtime, maxholdtime, flagtime, acquiretime, releasetime;

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
		 * LOCK STATISTICS
		 * ******************************************************/

		/** 
		 * @brief  get timekeeping statistics
		 * @return the total time spent for all threads in the mpi_lock
		 */
		double get_locktime();

		/** 
		 * @brief  get timekeeping statistics
		 * @return the maximum time spent locking a window
		 */
		double get_avglocktime();

		/** 
		 * @brief  get timekeeping statistics
		 * @return the maximum time spent locking a window
		 */
		double get_maxlocktime();

		/** 
		 * @brief  get timekeeping statistics
		 * @return the total time spent for all threads waiting for MPI_Win_lock
		 */
		double get_mpilocktime();

		/** 
		 * @brief  get timekeeping statistics
		 * @return the total number of locks taken
		 */
		int get_numlocks();

		/*********************************************************
		 * UNLOCK STATISTICS
		 * ******************************************************/

		/** 
		 * @brief  get timekeeping statistics
		 * @return the total time spent unlocking the lock
		 */
		double get_unlocktime();

		/** 
		 * @brief  get timekeeping statistics
		 * @return the average time spent unlocking the lock
		 */
		double get_avgunlocktime();

		/** 
		 * @brief  get timekeeping statistics
		 * @return the maximum time spent unlocking a window
		 */
		double get_maxunlocktime();

		/** 
		 * @brief  get timekeeping statistics
		 * @return the total time spent unlocking the MPI Windows
		 */
		double get_mpiunlocktime();

		/*********************************************************
		 * GENERAL STATISTICS
		 * ******************************************************/

		/** 
		 * @brief  get lock statistics
		 * @return the total amount of time holding an mpi_lock
		 */
		double get_holdtime();

		/** 
		 * @brief  get lock statistics
		 * @return the total amount of time holding an mpi_lock
		 */
		double get_avgholdtime();

		/** 
		 * @brief  get lock statistics
		 * @return the total amount of time holding an mpi_lock
		 */
		double get_maxholdtime();

		/**
		 * @brief reset the timekeeping statistics
		 */
		void reset_stats();
};

#endif /* mpi_lock_h */
