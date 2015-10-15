#include "TaskScheduler.hpp"
#include <iostream>

namespace geiger {
	namespace async {

		Task::Task(Task&& t) {
			timeOfExecution = t.timeOfExecution;
			t.timeOfExecution = std::chrono::steady_clock::time_point::min();

            packed_task = std::move(t.packed_task);
		}

		Task& Task::operator=(Task&& t) {
			timeOfExecution = t.timeOfExecution;
			t.timeOfExecution = std::chrono::steady_clock::time_point::min();
            packed_task = std::move(t.packed_task);

            return *this;
		}

		TaskScheduler::TaskScheduler() {
			threadPoolSize = std::thread::hardware_concurrency() - 1;
			threadPool = new std::thread[threadPoolSize.load()];

			queueSem.reset(new semaphore(0));

			activeThreads.store(0);
			syncedThreads.store(0);

			acceptNew.store(true);
			sync.store(false);

			auto queuePoll = [this]() {
				this->activeThreads++;

				while(this->acceptNew.load()) {

					while(this->acceptNew.load() && !this->queueSem->TryWait());

					if(this->acceptNew.load()) {
						this->queueMut.lock();
						Task t = tasks.top();
						tasks.pop();

						if(t.timeOfExecution <= std::chrono::steady_clock::now()) {
							t.packed_task();
						} else {
							tasks.push(t);
							this->queueSem->Signal();
						}
						this->queueMut.unlock();
					}

					if(this->sync.load()) {
						this->syncedThreads++;

						while(this->sync.load()) {
							if(this->syncedThreads.load() == this->activeThreads.load()) {

								this->sync.store(false);
								this->syncedThreads.store(0);
							}
						}
					}
				}

				this->activeThreads--;
			};

			for(size_t i = 0; i < threadPoolSize.load(); i++) {
				threadPool[i] = std::thread(queuePoll);
			}
		}

		TaskScheduler::~TaskScheduler() {

			acceptNew.store(false);
			sync.store(true);

			for(size_t i = 0; i < threadPoolSize.load(); i++) {
				threadPool[i].join();
			}

			delete[] threadPool;
		}

	}
}
