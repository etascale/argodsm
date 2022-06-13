/**
 * @file
 * @brief       This file provides an implementation of a specialized MPI lock allowing
 *              multiple threads to access an MPI Window concurrently.
 * @copyright   Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "mpi_lock.hpp"

mpi_lock::mpi_lock() {
	pthread_spin_init(&_local_lock, PTHREAD_PROCESS_PRIVATE);
};


void mpi_lock::lock(int lock_type, int target, MPI_Win window){

	// Take the local lock
	double local_start = MPI_Wtime();
	pthread_spin_lock(&_local_lock);
	double local_end = MPI_Wtime();
	_local_acquire_time = local_end;

	// Take the MPI lock
	double mpi_start = MPI_Wtime();
	MPI_Win_lock(lock_type, target, 0, window);
	double mpi_end = MPI_Wtime();
	_mpi_acquire_time = mpi_end;

	_num_locks++;
	// Update max lock times
	if((local_end - local_start) > _max_local_lock_time){
		_max_local_lock_time = local_end - local_start;
	}
	if((mpi_end - mpi_start) > _max_mpi_lock_time){
		_max_mpi_lock_time = mpi_end - mpi_start;
	}
	// Update time spent in lock
	_mpi_lock_time += mpi_end - mpi_start;
	_local_lock_time += local_end - local_start;
}

void mpi_lock::unlock(int target, MPI_Win window){
	// Unlock MPI
	double mpi_start = MPI_Wtime();
	MPI_Win_unlock(target, window);
	double mpi_end = MPI_Wtime();
	_mpi_release_time = mpi_end;

	// Update time spent in MPI lock
	_mpi_unlock_time += mpi_end - mpi_start;
	_mpi_hold_time += _mpi_release_time - _mpi_acquire_time;
	// Update max time holding an MPI lock if new record
	if((_mpi_release_time - _mpi_acquire_time) > _max_mpi_hold_time){
		_max_mpi_hold_time = _mpi_release_time - _mpi_acquire_time;
	}

	// Update the time spent in local lock
	_local_release_time = MPI_Wtime();
	_local_hold_time += _local_release_time - _local_acquire_time;
	// Update max time holding a local lock if new record
	if((_local_release_time - _local_acquire_time) > _max_local_hold_time){
		_max_local_hold_time = _local_release_time - _local_acquire_time;
	}

	// Release the local lock
	pthread_spin_unlock(&_local_lock);
}

bool mpi_lock::trylock(int lock_type, int target, MPI_Win window){
	// Try to take local lock
	if(!pthread_spin_trylock(&_local_lock)){
		_local_acquire_time = MPI_Wtime();
		// Lock MPI
		MPI_Win_lock(lock_type, target, 0, window);
		_mpi_acquire_time = MPI_Wtime();
		return true;
	}
	return false;
}


/*********************************************************
 * LOCAL LOCK STATISTICS
 * ******************************************************/

double mpi_lock::get_local_lock_time(){
	return _local_lock_time;
}

double mpi_lock::get_max_local_lock_time(){
	return _max_local_lock_time;
}

double mpi_lock::get_local_hold_time(){
	return _local_hold_time;
}

double mpi_lock::get_max_local_hold_time(){
	return _max_local_hold_time;
}

/*********************************************************
 * MPI LOCK STATISTICS
 * ******************************************************/

double mpi_lock::get_mpi_lock_time(){
	return _mpi_lock_time;
}

double mpi_lock::get_max_mpi_lock_time(){
	return _max_mpi_lock_time;
}

double mpi_lock::get_mpi_unlock_time(){
	return _mpi_unlock_time;
}

double mpi_lock::get_max_mpi_unlock_time(){
	return _max_mpi_unlock_time;
}

double mpi_lock::get_mpi_hold_time(){
	return _mpi_hold_time;
}

double mpi_lock::get_max_mpi_hold_time(){
	return _max_mpi_hold_time;
}



/*********************************************************
 * GENERAL
 * ******************************************************/

int mpi_lock::get_num_locks(){
	return _num_locks;
}

void mpi_lock::reset_stats(){
	_num_locks = 0;
	
	// reset local lock statistics
	_local_lock_time = 0;
	_max_local_lock_time = 0;
	_local_hold_time = 0;
	_max_local_hold_time = 0;
	
	// reset MPI lock statistics
	_mpi_lock_time = 0;
	_max_mpi_lock_time = 0;
	_mpi_unlock_time = 0;
	_max_mpi_unlock_time = 0;
	_mpi_hold_time = 0;
	_max_mpi_hold_time = 0;
}
