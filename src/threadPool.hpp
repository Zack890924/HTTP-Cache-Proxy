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
                std::unique_lock<std::mutex> lock(queueMutex);
                tasks.emplace(std::forward<F>(task));
            }
            condition.notify_one();
        }
        
    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;

        std::mutex queueMutex;
        std::condition_variable condition;
        bool stop;

};



#endif // THREADPOOL_HPP