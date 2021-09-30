/**
 * @file
 * @brief       This file provices an implementation of a specialized MPI lock allowing
 *              multiple threads to access an MPI Window concurrently.
 * @copyright   Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "mpi_lock.hpp"

/**
 * @brief mpi_lock constructor 
 */
mpi_lock::mpi_lock()
	/* Initialize lock timekeeping */
	: locktime(0),
	maxlocktime(0),
	mpilocktime(0),
	numlocksremote(0),
	/* Initialize unlock timekeeping */
	unlocktime(0),
	maxunlocktime(0),
	mpiunlocktime(0),
	/* Initialize general statkeeping */
	holdtime(0),
	maxholdtime(0),
	acquiretime(0),
	releasetime(0)
{ 
	pthread_spin_init(&spin_lock, PTHREAD_PROCESS_PRIVATE);
};


/**
 * @brief acquire mpi_lock
 * @param lock_type MPI_LOCK_SHARED or MPI_LOCK_EXCLUSIVE
 * @param target    target node of the lock
 * @param window    MPI window to lock
 */
void mpi_lock::lock(int lock_type, int target, MPI_Win window){
	double mpi_start, mpi_end, lock_start, lock_end;

	lock_start = MPI_Wtime();

	// Take global spinlock and stat lock
	pthread_spin_lock(&spin_lock);

	// Lock MPI
	mpi_start = MPI_Wtime();
	acquiretime = mpi_start;
	MPI_Win_lock(lock_type, target, 0, window);
	mpi_end = MPI_Wtime();
	lock_end = MPI_Wtime();

	numlocksremote++;
	/* Update max lock time */
	if((lock_end-lock_start) > maxlocktime){
		maxlocktime = lock_end-lock_start;
	}
	/* Update time spent in lock */
	mpilocktime += mpi_end-mpi_start;
	locktime += lock_end-lock_start;
}

/** 
 * @brief release mpi_lock
 * @param target    target node of the lock
 * @param window    MPI window to lock
 */
void mpi_lock::unlock(int target, MPI_Win window){
	double g, unlock_start, unlock_end, mpi_start, mpi_end;

	unlock_start = MPI_Wtime();

	/* Unlock MPI */
	mpi_start = MPI_Wtime();
	MPI_Win_unlock(target, window);
	mpi_end = MPI_Wtime();
	releasetime = mpi_end;

	unlock_end = MPI_Wtime();
	/* Update max time spent in unlock */
	if((unlock_end-unlock_start) > maxunlocktime){
		maxunlocktime = unlock_end-unlock_start;
	}
	/* Update max time held if this is a new record */
	if((releasetime-acquiretime) > maxholdtime){
		maxholdtime = releasetime-acquiretime;
	}
	/* Update time spent in lock */
	holdtime += (releasetime-acquiretime);
	mpiunlocktime += mpi_end-mpi_start;
	unlocktime += unlock_end-unlock_start;

	/* Release the spinlock */
	pthread_spin_unlock(&spin_lock);
}


/** 
 * @brief try to acquire lock
 * @param lock_type MPI_LOCK_SHARED or MPI_LOCK_EXCLUSIVE
 * @param target    target node of the lock
 * @param window    MPI window to lock
 * @return          true if successful, false otherwise
 */
bool mpi_lock::trylock(int lock_type, int target, MPI_Win window){
	/* TODO: Not really fully tested and no stat tracking */
	// Try to take global spinlock
	if(!pthread_spin_trylock(&spin_lock)){
		// Lock MPI
		MPI_Win_lock(lock_type, target, 0, window);
		return true;
	}
	return false;
}



/*********************************************************
 * LOCK STATISTICS
 * ******************************************************/

/** 
 * @brief  get timekeeping statistics
 * @return the total time spent for all threads in the mpi_lock
 */
double mpi_lock::get_locktime(){
	return locktime;
}

/** 
 * @brief  get timekeeping statistics
 * @return the average time spent per lock
 */
double mpi_lock::get_avglocktime(){
	double numlocks = static_cast<double>(numlocksremote);
	if(numlocks>0){
		return locktime/numlocks;
	}else{
		return 0.0;
	}
}

/** 
 * @brief  get timekeeping statistics
 * @return the average time spent per lock
 */
double mpi_lock::get_maxlocktime(){
	return maxlocktime;
}

/** 
 * @brief  get timekeeping statistics
 * @return the total time spent for all threads waiting for MPI_Win_lock
 */
double mpi_lock::get_mpilocktime(){
	return mpilocktime;
}

/** 
 * @brief  get timekeeping statistics
 * @return the total number of locks taken
 */
int mpi_lock::get_numlocks(){
	return numlocksremote;
}

/*********************************************************
 * UNLOCK STATISTICS
 * ******************************************************/

/** 
 * @brief  get timekeeping statistics
 * @return the total time spent for all threads in the mpi_unlock
 */
double mpi_lock::get_unlocktime(){
	return unlocktime;
}

/** 
 * @brief  get timekeeping statistics
 * @return the average time spent per unlock
 */
double mpi_lock::get_avgunlocktime(){
	double numlocks = static_cast<double>(numlocksremote);
	if(numlocks>0){
		return unlocktime/numlocks;
	}else{
		return 0.0;
	}
}

/** 
 * @brief  get timekeeping statistics
 * @return the maximum time spent in mpi_unlock
 */
double mpi_lock::get_maxunlocktime(){
	return maxunlocktime;
}

/** 
 * @brief  get timekeeping statistics
 * @return the total time spent for all threads waiting for mpi_win_lock
 */
double mpi_lock::get_mpiunlocktime(){
	return mpiunlocktime;
}

/*********************************************************
 * GENERAL STATISTICS
 * ******************************************************/

/** 
 * @brief  get timekeeping statistics
 * @return the total time spent holding an mpi_lock
 */
double mpi_lock::get_holdtime(){
	return holdtime;
}

/** 
 * @brief  get timekeeping statistics
 * @return the average time spent holding an mpi_lock
 */
double mpi_lock::get_avgholdtime(){
	double numlocks = static_cast<double>(numlocksremote);
	if(numlocks>0){
		return holdtime/numlocks;
	}else{
		return 0.0;
	}
}

/** 
 * @brief  get timekeeping statistics
 * @return the maximum time spent holding an mpi_lock
 */
double mpi_lock::get_maxholdtime(){
	return maxholdtime;
}

/**
 * @brief reset the timekeeping statistics
 */
void mpi_lock::reset_stats(){
	/* Lock stuff */
	locktime = 0;
	maxlocktime = 0;
	mpilocktime = 0;
	numlocksremote = 0;
	/* Unlock stuff */
	unlocktime = 0;
	maxunlocktime = 0;
	mpiunlocktime = 0;
	/* Other stuff */
	holdtime = 0;
	maxholdtime = 0;
}
