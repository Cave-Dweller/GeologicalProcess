#ifndef TASKSCHEDULER_HPP
#define TASKSCHEDULER_HPP

#include "Semaphore.hpp"
#include <future>
#include <thread>
#include <queue>
#include <chrono>
#include <functional>
#include <type_traits>

namespace geiger {
	namespace async {

		using namespace geiger::ConcUtils;

		struct Task {

			Task(std::function<void()>&& func, std::chrono::steady_clock::time_point tp) : packed_task(func), timeOfExecution(tp) {}

			Task(const Task& t) = default;
			Task(Task&& t);

			Task& operator=(const Task& t) = default;
			Task& operator=(Task&& t);

            std::function<void()> packed_task;
            std::chrono::steady_clock::time_point timeOfExecution;

			//this returns whether the *priority* of this is less than the *priority* of other
            bool operator<(const Task& other) const {
				return timeOfExecution > other.timeOfExecution;
            }

		};

		class TaskScheduler {
			public:
				TaskScheduler();
				~TaskScheduler();

				//Tasks to be executed at a particular time

				template<typename Clock, typename Duration, typename Function, typename... Args>
				std::future<std::result_of_t<std::decay_t<Function>(std::decay_t<Args>...)>> SubmitTask(std::chrono::time_point<Clock, Duration> tp, Function&& f, Args&&... args);

				template<typename Clock, typename Duration, typename Function, typename... Args>
				void SubmitTaskFireAndForget(std::chrono::time_point<Clock, Duration> tp, Function&& f, Args&&... args);

				//Tasks to be executed at an offset from current time

				template<typename Rep, typename Period, typename Function, typename... Args>
				std::future<std::result_of_t<std::decay_t<Function>(std::decay_t<Args>...)>> SubmitTask(std::chrono::duration<Rep, Period> dur, Function&& f, Args&&... args);

				template<typename Rep, typename Period, typename Function, typename... Args>
				void SubmitTaskFireAndForget(std::chrono::duration<Rep, Period> dur, Function&& f, Args&&... args);

				//Tasks to be executed at the default time (now)

				template<typename Function, typename... Args>
				std::future<std::result_of_t<std::decay_t<Function>(std::decay_t<Args>...)>> SubmitTask(Function&& f, Args&&... args);

				template<typename Function, typename... Args>
				void SubmitTaskFireAndForget(Function&& f, Args&&... args);

			private:

				std::thread* threadPool;
				std::atomic<size_t> threadPoolSize;
				std::atomic<size_t> activeThreads;

                std::unique_ptr<semaphore> queueSem;
                std::mutex queueMut;
                std::priority_queue<Task> tasks;

                std::atomic<bool> acceptNew;
                std::atomic<bool> sync;
                std::atomic<size_t> syncedThreads;

		};

		//Helper function to handle functions returning void
		template<typename Ret, typename Function, typename... Args>
		void SetPromise(std::shared_ptr<std::promise<Ret>>& p, Function&& f, Args&&... args) {
			if(!p) {
				p.reset(new std::promise<Ret>{});
			}

			p->set_value(f(std::forward<Args>(args)...));
		}

		template<typename Function, typename... Args>
		void SetPromise(std::shared_ptr<std::promise<void>>& p, Function&& f, Args&&... args) {
			f(std::forward<Args>(args)...);
			if(!p) {
				p.reset(new std::promise<void>{});
			}

			p->set_value();
		}

		template<typename Clock, typename Duration, typename Function, typename... Args>
		std::future<std::result_of_t<std::decay_t<Function>(std::decay_t<Args>...)>> TaskScheduler::SubmitTask(std::chrono::time_point<Clock, Duration> tp, Function&& f, Args&&... args) {

			using namespace std::chrono;

            auto dur = tp - time_point<Clock, Duration>::min();
            auto steady_time = steady_clock::time_point(dur);

			using ret_t = std::result_of_t<std::decay_t<Function>(std::decay_t<Args>...)>;

			std::shared_ptr<std::promise<ret_t>> setter{new std::promise<ret_t>{}};
			std::future<ret_t> getter = setter->get_future();

            std::function<void()> func = [setter, f, args...] () mutable {
            	SetPromise(setter, f, std::forward<Args>(args)...);
            };

            Task t(std::move(func), steady_time);

            queueMut.lock();
            tasks.push(std::move(t));
            queueSem->Signal();
            queueMut.unlock();

            return getter;
		}

		template<typename Clock, typename Duration, typename Function, typename... Args>
		void TaskScheduler::SubmitTaskFireAndForget(std::chrono::time_point<Clock, Duration> tp, Function&& f, Args&&... args) {

			using namespace std::chrono;

			auto dur = tp - time_point<Clock, Duration>::min();
            auto steady_time = steady_clock::time_point(dur);

			std::function<void()> func = [f(std::move(f)), args...] () {
                f(std::forward<Args>(args)...);
			};

			Task t(std::move(func), steady_time);

			queueMut.lock();
            tasks.push(std::move(t));
            queueSem->Signal();
            queueMut.unlock();
		}

		template<typename Rep, typename Period, typename Function, typename... Args>
		std::future<std::result_of_t<std::decay_t<Function>(std::decay_t<Args>...)>> TaskScheduler::SubmitTask(std::chrono::duration<Rep, Period> dur, Function&& f, Args&&... args) {

			using namespace std::chrono;

            using ret_t = std::result_of_t<std::decay_t<Function>(std::decay_t<Args>...)>;

            steady_clock::time_point tp = std::chrono::steady_clock::now() + dur;

			std::shared_ptr<std::promise<ret_t>> setter{new std::promise<ret_t>{}};
			std::future<ret_t> getter = setter->get_future();

            std::function<void()> func = [setter, f, args...] () mutable {
            	SetPromise(setter, f, std::forward<Args>(args)...);
            };

            Task t(std::move(func), tp);

            queueMut.lock();
            tasks.push(std::move(t));
            queueSem->Signal();
            queueMut.unlock();

            return getter;
		}

		template<typename Rep, typename Period, typename Function, typename... Args>
		void TaskScheduler::SubmitTaskFireAndForget(std::chrono::duration<Rep, Period> dur, Function&& f, Args&&... args) {

			using namespace std::chrono;

			steady_clock::time_point tp = std::chrono::steady_clock::now() + dur;

			std::function<void()> func = [f(std::move(f)), args...] () {
                f(std::forward<Args>(args)...);
			};

			Task t(std::move(func), tp);

			queueMut.lock();
            tasks.push(std::move(t));
            queueSem->Signal();
            queueMut.unlock();
		}

		template<typename Function, typename... Args>
		std::future<std::result_of_t<std::decay_t<Function>(std::decay_t<Args>...)>> TaskScheduler::SubmitTask(Function&& f, Args&&... args) {

			using namespace std::chrono;

			using ret_t = std::result_of_t<std::decay_t<Function>(std::decay_t<Args>...)>;

			steady_clock::time_point tp = steady_clock::now() + microseconds(-1);

			std::shared_ptr<std::promise<ret_t>> setter{new std::promise<ret_t>{}};
			std::future<ret_t> getter = setter->get_future();

            std::function<void()> func = [setter, f(std::move(f)), args...] () mutable {
            	SetPromise(setter, f, std::forward<Args>(args)...);
            };

            Task t(std::move(func), tp);

            queueMut.lock();
            tasks.push(std::move(t));
            queueSem->Signal();
            queueMut.unlock();

            return getter;
		}

		template<typename Function, typename... Args>
		void TaskScheduler::SubmitTaskFireAndForget(Function&& f, Args&&... args) {

			using namespace std::chrono;

			steady_clock::time_point tp = steady_clock::now() + microseconds(-1);

			std::function<void()> func = [f(std::move(f)), args...] () {
                f(std::forward<Args>(args)...);
			};

			Task t(std::move(func), tp);

			queueMut.lock();
            tasks.push(std::move(t));
            queueSem->Signal();
            queueMut.unlock();
		}

	}
}

#endif // TASKSCHEDULER_HPP
