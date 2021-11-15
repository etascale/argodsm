/**
 * @file
 * @brief       This file provices an implementation of a specialized MPI lock allowing
 *              multiple threads to access an MPI Window concurrently.
 * @copyright   Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "mpi_lock.hpp"

mpi_lock::mpi_lock()
	: num_locks(0),
	/* spin lock statistics */
	spin_lock_time(0),
	max_spin_lock_time(0),
	spin_hold_time(0),
	max_spin_hold_time(0),
	/* MPI lock statistics */
	mpi_lock_time(0),
	max_mpi_lock_time(0),
	mpi_unlock_time(0),
	max_mpi_unlock_time(0),
	mpi_hold_time(0),
	max_mpi_hold_time(0)
{
	pthread_spin_init(&spin_lock, PTHREAD_PROCESS_PRIVATE);
};


void mpi_lock::lock(int lock_type, int target, MPI_Win window){

	// Take global spinlock
	double spin_start = MPI_Wtime();
	pthread_spin_lock(&spin_lock);
	double spin_end = MPI_Wtime();
	spin_acquire_time = spin_end;

	// Lock MPI
	double mpi_start = MPI_Wtime();
	MPI_Win_lock(lock_type, target, 0, window);
	double mpi_end = MPI_Wtime();
	mpi_acquire_time = mpi_end;

	num_locks++;
	/* Update max lock times */
	if((spin_end-spin_start) > max_spin_lock_time){
		max_spin_lock_time = spin_end-spin_start;
	}
	if((mpi_end-mpi_start) > max_mpi_lock_time){
		max_mpi_lock_time = mpi_end - mpi_start;
	}
	/* Update time spent in lock */
	mpi_lock_time += mpi_end-mpi_start;
	spin_lock_time += spin_end-spin_start;
}

void mpi_lock::unlock(int target, MPI_Win window){

	/* Unlock MPI */
	double mpi_start = MPI_Wtime();
	MPI_Win_unlock(target, window);
	double mpi_end = MPI_Wtime();
	mpi_release_time = mpi_end;

	/* Update time spent in MPI lock */
	mpi_unlock_time += mpi_end-mpi_start;
	mpi_hold_time += mpi_release_time-mpi_acquire_time;
	/* Update max time holding an MPI lock if new record*/
	if((mpi_release_time-mpi_acquire_time) > max_mpi_hold_time){
		max_mpi_hold_time = mpi_release_time-mpi_acquire_time;
	}

	/* Update the time spent in spin lock */
	spin_release_time = MPI_Wtime();
	spin_hold_time += spin_release_time-spin_acquire_time;
	/* Update max time holding a spin lock if new record*/
	if((spin_release_time-spin_acquire_time) > max_spin_hold_time){
		max_spin_hold_time = spin_release_time-spin_acquire_time;
	}

	/* Release the spinlock */
	pthread_spin_unlock(&spin_lock);
}

bool mpi_lock::trylock(int lock_type, int target, MPI_Win window){
	// Try to take global spinlock
	if(!pthread_spin_trylock(&spin_lock)){
		spin_acquire_time = MPI_Wtime();
		// Lock MPI
		MPI_Win_lock(lock_type, target, 0, window);
		mpi_acquire_time = MPI_Wtime();
		return true;
	}
	return false;
}


/*********************************************************
 * SPIN LOCK STATISTICS
 * ******************************************************/

double mpi_lock::get_spin_lock_time(){
	return spin_lock_time;
}

double mpi_lock::get_max_spin_lock_time(){
	return max_spin_lock_time;
}

double mpi_lock::get_spin_hold_time(){
	return spin_hold_time;
}

double mpi_lock::get_max_spin_hold_time(){
	return max_spin_hold_time;
}

/*********************************************************
 * MPI LOCK STATISTICS
 * ******************************************************/

double mpi_lock::get_mpi_lock_time(){
	return mpi_lock_time;
}

double mpi_lock::get_max_mpi_lock_time(){
	return max_mpi_lock_time;
}

double mpi_lock::get_mpi_unlock_time(){
	return mpi_unlock_time;
}

double mpi_lock::get_max_mpi_unlock_time(){
	return max_mpi_unlock_time;
}

double mpi_lock::get_mpi_hold_time(){
	return mpi_hold_time;
}

double mpi_lock::get_max_mpi_hold_time(){
	return max_mpi_hold_time;
}



/*********************************************************
 * GENERAL
 * ******************************************************/

int mpi_lock::get_num_locks(){
	return num_locks;
}

void mpi_lock::reset_stats(){
	num_locks = 0;
	/* spin lock statistics */
	spin_lock_time = 0;
	max_spin_lock_time = 0;
	spin_hold_time = 0;
	max_spin_hold_time = 0;
	/* MPI lock statistics */
	mpi_lock_time = 0;
	max_mpi_lock_time = 0;
	mpi_unlock_time = 0;
	max_mpi_unlock_time = 0;
	mpi_hold_time = 0;
	max_mpi_hold_time = 0;
}
