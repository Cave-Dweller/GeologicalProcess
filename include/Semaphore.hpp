/*=======================LICENSE=======================
||  Semaphore.hpp                                     ||
||                                                    ||
||           Copyright 2015 Kiel Geiger               ||
||                                                    ||
||              kielgeiger@gmail.com                  ||
||                                                    ||
||   Licensed under the Apache License, Version 2.0   ||
||   (the "License"); you may not use this file       ||
||   except in compliance with the License.You may    ||
||   obtain a copy of the License at                  ||
||                                                    ||
||       http://www.apache.org/licenses/LICENSE-2.0   ||
||                                                    ||
||   Unless required by applicable law or agreed to   ||
||   in writing, software distributed under the       ||
||   License is distributed on an "AS IS" BASIS,      ||
||   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,    ||
||   either express or implied. See the License for   ||
||   the specific language governing permissions and  ||
||   limitations under the License.                   ||
||                                                    ||
 =====================================================*/


#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>

namespace geiger {

    namespace ConcUtils {


  /*--------------------------------------------------*/
 /*                     INTERFACE                    */
/*--------------------------------------------------*/


        template<typename Mutex = std::mutex>
        class Semaphore
        {
            public:
                /** \brief Initializes count_.
                 *
                 * \param c The count of threads allowed to access some resource, default 1
                 *
                 * Initializes count_ and max_count_ with c, the number of threads allowed through without blocking.
                 *
                 * Special case when c < 0, it is assumed that the semaphore will be used for
                 * inter-thread communication only, and max_count_ is set to 0.
                */
                explicit Semaphore(int c = 1) : init_count_( (c >= 0) ? c : 0), count_( (c >= 0 ? c : 0)), destructing{false} {}

                Semaphore(const Semaphore& sem) = delete;
                Semaphore(Semaphore&& sem) = delete;

                /** \brief Waits for all threads to finish before destructing.
                 *
                 * Calls cv_.wait() with the predicate
                 *
                 * @code []() {
                 *           return this->count_ == this->max_count_;
                 *       } @endcode
                 *
                 * to ensure all currently waiting threads finish and no more
                 * may acquire mut_.
                */
                virtual ~Semaphore();

                Semaphore& operator=(const Semaphore& sem) = delete;
                Semaphore& operator=(Semaphore&& sem) = delete;

                /** \brief If count_ is 0, blocks. Otherwise, decrements count_.
                 *
                 * If count_ is 0, Wait() blocks the calling thread. If
                 * count_ is greater than 0, Wait() decrements count_ and returns immediately.
                */
                virtual void Wait();

                /** \brief Tries to wait, returns false if unable.
                 *
                 * If count_ is greater than 0, it is decremented and TryWait()
                 * returns true immediately. If count_ is 0, TryWait() returns false
                 * immediately.
                */
                virtual bool TryWait();

                /** \brief Increments count_ if count_ < max_count_.
                 *
                 * If count_ is less than max_count_, count_ is incremented. Additionally,
                 * cv_.notify_one() is called and if any threads are currently blocked by Wait(), one
                 * is unblocked. Has no effect if count_ is equal to max_count_.
                */
                virtual void Signal();

            protected:
                const int init_count_;
                std::atomic<int> count_;
                Mutex mut_;
                std::condition_variable_any cv_;
                std::atomic<bool> destructing;
        };

        typedef Semaphore<> semaphore; //default using a plain std::mutex is treated like a unique type



  /*--------------------------------------------------*/
 /*                   IMPLEMENTATION                 */
/*--------------------------------------------------*/



        template<typename Mutex>
        Semaphore<Mutex>::~Semaphore() {
			destructing.store(true);

			std::unique_lock<Mutex> l_{this->mut_};
			cv_.wait(l_, [this]() { return this->count_ == this->init_count_; });
        }

        template<typename Mutex>
        void Semaphore<Mutex>::Wait() {

            if(!destructing.load()) {
				std::unique_lock<Mutex> l_{this->mut_};

				//unlock mut_, take ownership of it,
				//block current thread if necessary,
				//and add it to a queue of waiting threads
				this->cv_.wait(l_, [&](){return this->count_.load() > 0;});

				//decrement the count of "open resources"
				--this->count_;
            }

            //std::unique_lock uses RAII and releases mut_ at end of scope
        }

        template<typename Mutex>
        bool Semaphore<Mutex>::TryWait() {
			std::unique_lock<Mutex> l_{this->mut_};

            //if one or more resources are "free"
            if(this->count_.load() > 0 && !destructing.load()) {

                //"claim" a resource, return true
                --this->count_;
                return true;

            } else {

                //just return false
                return false;

            }
            //mut_ released
        }

        template<typename Mutex>
        void Semaphore<Mutex>::Signal() {
            std::lock_guard<std::mutex> l_{this->mut_};

            //if not all resources are "free"
            if(this->count_.load() < (this->init_count_ > 0) ? this->init_count_ : 1) {

                //"free" a resource
                ++this->count_;

                //notify a thread
                cv_.notify_one();

            }
        }

    } //namespace ConcUtils
} //namespace geiger

#endif // SEMAPHORE_H
