#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>


class ThreadPool{
    public:
        explicit ThreadPool(size_t threadCount);
        ~ThreadPool();

        template<class F>
        void enqueue(F&& task){
            {
                std::unique_lock<std::mutex> lock(_queueMutex);
                _tasks.emplace(std::forward<F>(task));
            }
            _condition.notify_one();
        }
        
    private:
        std::vector<std::thread> _workers;
        std::queue<std::function<void()>> _tasks;

        std::mutex _queueMutex;
        std::condition_variable _condition;
        bool _stop;

};



#endif // THREADPOOL_HPP