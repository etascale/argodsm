/**
 * @file
 * @brief This file provides an interface for the ArgoDSM write buffer.
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_write_buffer_hpp
#define argo_write_buffer_hpp argo_write_buffer_hpp

#include <deque>
#include <iterator>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <mpi.h>
#include <vector>

#include "backend/backend.hpp"
#include "env/env.hpp"
#include "virtual_memory/virtual_memory.hpp"
#include "qd.hpp"

/** @brief Block size based on backend definition */
const std::size_t block_size = page_size*CACHELINE;

/**
 * @brief		Argo cache lock structure
 * @deprecated 	prototype implementation, should be replaced with API calls
 */
extern std::vector<cache_lock> cache_locks;

/**
 * @brief	A write buffer in FIFO style with the capability to erase any
 * element from the buffer while preserving ordering.
 * @tparam	T the type of the write buffer
 */
template<typename T>
class write_buffer
{
	private:
		/** @brief This container holds cache indexes that should be written back */
		std::deque<T> _buffer;

		/** @brief The maximum size of the write buffer */
		std::size_t _max_size;

		/**
		 * @brief The number of elements to write back once attempting to
		 * add an element to an already full write buffer.
		 */
		std::size_t _write_back_size;

		/** @brief QD lock ensuring atomicity of updates */
		qdlock _qd_lock;

		/** @brief Mutex to protect reading/writing to the statistics */
		mutable std::mutex _stat_mutex;

		/** @brief Statistic of the total number of pages added to the write buffer */
		std::size_t _page_count{0};
		/** @brief Statistic of the number of times the write buffer was partially flushed */
		std::size_t _partial_flush_count{0};

		/** @brief Statistic of time spent flushing the write buffer */
		double _flush_time{0};
		/** @brief Statistic of time spent waiting for the write buffer to be flushed */
		double _flush_wait_time{0};
		/** @brief Statistic of time spent writing back pages when adding to a full buffer */
		double _write_back_time{0};
		/** @brief Statistic of time spent in the QD lock */
		double _buffer_lock_time{0};

		/**
		 * @brief	Check if the write buffer is empty
		 * @return	True if empty, else False
		 */
		bool empty() const {
			return _buffer.empty();
		}

		/**
		 * @brief	Get the size of the buffer
		 * @return	The size of the buffer
		 */
		size_t size() const {
			return _buffer.size();
		}

		/**
		 * @brief	Get the buffer element at index i
		 * @param	i The requested buffer index
		 * @return	The element at index i of type T
		 */
		T at(std::size_t i) const {
			return _buffer.at(i);
		}

		/**
		 * @brief	Check if val exists in the buffer
		 * @param	val The value to check for
		 * @return	True if val exists in the buffer, else False
		 */
		bool has(T val) {
			typename std::deque<T>::iterator it = std::find(_buffer.begin(),
					_buffer.end(), val);
			return (it != _buffer.end());
		}

		/**
		 * @brief	Constructs a new element and emplaces it at the back of the buffer
		 * @param	args Properties of the new element to emplace back
		 */
		template<typename... Args>
			void emplace_back( Args&&... args) {
				_buffer.emplace_back(std::forward<Args>(args)...);
			}

		/**
		 * @brief	Removes the front element from the buffer and returns it
		 * @return	The element that was popped from the buffer
		 */
		T pop() {
			auto elem = std::move(_buffer.front());
			_buffer.pop_front();
			return elem;
		}

		/**
		 * @brief	Perform writeback of a single cached page
		 * @param	cache_index the cache index to write back from
		 * @pre		Require ibsem and cachemutex to be taken
		 */
		void write_back_index(std::size_t cache_index) {
			cache_locks[cache_index].lock();
			assert(cacheControl[cache_index].dirty == DIRTY);
			const std::uintptr_t page_address = cacheControl[cache_index].tag;
			void* page_ptr = static_cast<char*>(
				argo::virtual_memory::start_address()) + page_address;

			// Write back the page
			mprotect(page_ptr, block_size, PROT_READ);
			cacheControl[cache_index].dirty = CLEAN;
			for(std::size_t i = 0; i < CACHELINE; i++){
				storepageDIFF(cache_index+i, page_size*i+page_address);
			}
			cache_locks[cache_index].unlock();
		}

		/**
		 * @brief	Flushes first _write_back_size elements of the  ArgoDSM 
		 * 			write buffer to memory
		 * @pre		Require qd_lock to be held
		 */
		void flush_partial() {
			double t_start = MPI_Wtime();

			// For each element, write back the corresponding ArgoDSM page
			for(std::size_t i = 0; i < _write_back_size; i++) {
				write_back_index(pop());
			}
			double t_end = MPI_Wtime();

			// Update timer statistics
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			_write_back_time += t_end - t_start;
			_partial_flush_count++;
		}

		/**
		 * @brief	Helper function to call _add
		 * @param	buf Pointer to a write_buffer object
		 * @param	val Value to add to buf
		 */
		static void _add_helper(write_buffer* buf, T val) {
			buf->_add(val);
		}

		/**
		 * @brief	Internal function to add an element to the write buffer
		 * @param	val The value of type T to add to the buffer
		 * @pre		Require qd_lock to be held
		 */
		void _add(T val) {
			// For debug builds, check for duplicate additions
			assert(!has(val));

			// If the buffer is full, write back _write_back_size indices
			if(size() >= _max_size){
				flush_partial();
			}

			// Add val to the back of the buffer
			emplace_back(val);
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			_page_count++;
		}

		/**
		 * @brief	Helper function to call _erase
		 * @param	buf Pointer to a write_buffer object
		 * @param	val Value to erase from buf
		 */
		static void _erase_helper(write_buffer* buf, T val) {
			buf->_erase(val);
		}

		/**
		 * @brief	Internal implementation of erase
		 * @param	val The value of type T to erase
		 * @pre		_qd_lock must be taken
		 */
		void _erase(T val) {
			// Attempt to get iterator to element equal to val
			typename std::deque<T>::iterator it =
				std::find(_buffer.begin(), _buffer.end(), val);
			// If found, erase it
			if(it != _buffer.end()){
				_buffer.erase(it);
			}
		}

		/**
		 * @brief	Helper function to call _flush
		 * @param	buf Pointer to a write_buffer object
		 * @param	flag Flag to signal when _flush is done
		 */
		static void _flush_helper(write_buffer* buf, std::atomic<bool>* flag) {
			buf->_flush(flag);
		}

		/**
		 * @brief	Internal implementation of flush
		 * @param	w_flag Flag to signal when flush is done
		 * @pre		_qd_lock must be taken
		 */
		void _flush(std::atomic<bool>* w_flag) {
			double t_start = MPI_Wtime();

			// Write back pagediffs until the buffer is empty
			while(!empty()) {
				write_back_index(pop());
			}
			double t_stop = MPI_Wtime();

			// Signal that flush is done
			w_flag->store(true, std::memory_order_release);

			// Update timer statistics
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			_flush_time += t_stop-t_start;
		}

	public:
		/**
		 * @brief	Constructor
		 */
		write_buffer()	:
			_max_size(argo::env::write_buffer_size()/CACHELINE),
			_write_back_size(argo::env::write_buffer_write_back_size()/CACHELINE) { }

		/**
		 * @brief	Copy constructor
		 * @param	other The write_buffer object to copy from
		 */
		write_buffer(const write_buffer & other) {
			// Ensure protection of data
			std::lock_guard<qdlock> lk(_qd_lock);

			// Copy data
			_buffer = other._buffer;
			_max_size = other._max_size;
			_write_back_size = other._write_back_size;
			_flush_time = other._flush_time;
			_flush_wait_time = other._flush_wait_time;
			_write_back_time = other._write_back_time;
			_buffer_lock_time = other._buffer_lock_time;
			_page_count = other._page_count;
			_partial_flush_count = other._partial_flush_count;
		}

		/**
		 * @brief	Copy assignment operator
		 * @param	other the write_buffer object to copy assign from
		 * @return	reference to the created copy
		 */
		write_buffer& operator=(const write_buffer & other) {
			if(&other != this) {
				// Ensure protection of data
				std::unique_lock<qdlock> lock_this(_qd_lock, std::defer_lock);
				std::unique_lock<qdlock> lock_other(other._qd_lock, std::defer_lock);
				std::lock(lock_this, lock_other);
				// Copy data
				_buffer = other._buffer;
				_max_size = other._max_size;
				_write_back_size = other._write_back_size;
				_flush_time = other._flush_time;
				_flush_wait_time = other._flush_wait_time;
				_write_back_time = other._write_back_time;
				_buffer_lock_time = other._buffer_lock_time;
				_page_count = other._page_count;
				_partial_flush_count = other._partial_flush_count;
			}
			return *this;
		}

		/**
		 * @brief	Move constructor
		 * @param	other the write_buffer object to move from
		 */
		write_buffer(write_buffer && other) {
			// Ensure protection of data
			std::lock_guard<qdlock> lock_other(other._qd_lock);

			// Copy data
			_buffer = std::move(other._buffer);
			_max_size = std::move(other._max_size);
			_write_back_size = std::move(other._write_back_size);
			_flush_time = std::move(other._flush_time);
			_flush_wait_time = std::move(other._flush_wait_time);
			_write_back_time = std::move(other._write_back_time);
			_buffer_lock_time = std::move(other._buffer_lock_time);
			_page_count = std::move(other._page_count);
			_partial_flush_count = std::move(other._partial_flush_count);
		}

		/**
		 * @brief	Move assignment operator
		 * @param	other the write_buffer object to move assign from
		 * @return	reference to the moved object
		 */
		write_buffer& operator=(write_buffer && other) {
			if (&other != this) {
				// Ensure protection of data
				std::unique_lock<qdlock> lock_this(_qd_lock, std::defer_lock);
				std::unique_lock<qdlock> lock_other(other._qd_lock, std::defer_lock);
				std::lock(lock_this, lock_other);

				_buffer = std::move(other._buffer);
				_max_size = std::move(other._max_size);
				_write_back_size = std::move(other._write_back_size);
				_flush_time = std::move(other._flush_time);
				_flush_wait_time = std::move(other._flush_wait_time);
				_write_back_time = std::move(other._write_back_time);
				_buffer_lock_time = std::move(other._buffer_lock_time);
				_page_count = std::move(other._page_count);
				_partial_flush_count = std::move(other._partial_flush_count);
			}
			return *this;
		}

		/**
		 * @brief	If val exists in buffer, delete it. Else, do nothing.
		 * @param	val The value of type T to erase
		 */
		void erase(T val) {
			double t_start = MPI_Wtime();
			// Delegate erasing to lock holder (can be self)
			_qd_lock.DELEGATE_N(&write_buffer::_erase_helper, this, val);
			double t_end = MPI_Wtime();

			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			_buffer_lock_time += t_end - t_start;
		}

		/**
		 * @brief	Flushes the ArgoDSM write buffer to memory
		 */
		void flush() {
			// Use an atomic flag to detect when flush is done
			std::atomic<bool> w_flag;
			w_flag.store(false, std::memory_order_release);

			double t_start = MPI_Wtime();
			// Delegate flushing to lock holder (can be self)
			_qd_lock.DELEGATE_N(&write_buffer::_flush_helper, this, &w_flag);
			double t_end = MPI_Wtime();

			// Wait until flush is completed
			double w_start = MPI_Wtime();
			while(!w_flag.load(std::memory_order_acquire));
			double w_end = MPI_Wtime();

			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			_buffer_lock_time += t_end - t_start;
			_flush_wait_time += w_end - w_start;
		}

		/**
		 * @brief	Adds a new element to the write buffer
		 * @param	val The value of type T to add to the buffer
		 */
		void add(T val) {
			double t_start = MPI_Wtime();
			// Delegate adding to lock holder (can be self)
			_qd_lock.DELEGATE_N(&write_buffer::_add_helper, this, val);
			double t_end = MPI_Wtime();

			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			_buffer_lock_time += t_end - t_start;
		}

		/**
		 * @brief	Get the time spent flushing the write buffer
		 * @return	The time in seconds
		 */
		double get_flush_time() const {
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			return _flush_time;
		}

		/**
		 * @brief	Get the time spent waiting for the buffer to be flushed
		 * @return	The time in seconds
		 */
		double get_flush_wait_time() const {
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			return _flush_wait_time;
		}

		/**
		 * @brief	Get the time spent partially flushing the write buffer
		 * @return	The time in seconds
		 */
		double get_write_back_time() const {
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			return _write_back_time;
		}

		/**
		 * @brief	Get the time spent waiting for the write buffer lock
		 * @return	The time in seconds
		 */
		double get_buffer_lock_time() const {
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			return _buffer_lock_time;
		}

		/**
		 * @brief	get buffer size
		 * @return	the size
		 */
		std::size_t get_size() const {
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			return _buffer.size();
		}

		/**
		 * @brief	get total number of pages added
		 * @return	number of pages added
		 */
		std::size_t get_page_count() const {
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			return _page_count;
		}

		/**
		 * @brief	get the number of times partially flushed
		 * @return	number of times partially flushed
		 */
		std::size_t get_partial_flush_count() const {
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			return _partial_flush_count;
		}

		/**
		 * @brief	reset all statistics
		 * @note	this does not reset the actual write buffer
		 */
		void reset_stats() {
			std::lock_guard<std::mutex> stat_lock(_stat_mutex);
			_flush_time = 0;
			_flush_wait_time = 0;
			_write_back_time = 0;
			_buffer_lock_time = 0;
			_page_count = 0;
			_partial_flush_count = 0;
		}

}; //class

#endif /* argo_write_buffer_hpp */
