#include <drogon/drogon.h>

using namespace drogon;

int main() {
    drogon::app().loadConfigFile("../config.json").run();
    return 0;
}

// #include <iostream>
// #include <vector>
// #include <thread>
// #include <queue>
// #include <functional>
// #include <mutex>
// #include <condition_variable>
// #include <atomic>
// #include <liboai.h>

// class ThreadPool {
// public:
//     ThreadPool(size_t numThreads);
//     ~ThreadPool();

//     template<class F>
//     void enqueue(F&& f);

// private:
//     std::vector<std::thread> workers;
//     std::queue<std::function<void()>> tasks;

//     std::mutex queueMutex;
//     std::condition_variable condition;
//     std::atomic<bool> stop;

//     void workerThread();
// };

// ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
//     for (size_t i = 0; i < numThreads; ++i) {
//         workers.emplace_back([this] { this->workerThread(); });
//     }
// }

// ThreadPool::~ThreadPool() {
//     stop.store(true);
//     condition.notify_all();
//     for (std::thread &worker : workers) {
//         worker.join();
//     }
// }

// void ThreadPool::workerThread() {
//     while (!stop) {
//         std::function<void()> task;
//         {
//             std::unique_lock<std::mutex> lock(this->queueMutex);
//             this->condition.wait(lock, [this] { return this->stop.load() || !this->tasks.empty(); });
//             if (this->stop && this->tasks.empty()) {
//                 return;
//             }
//             task = std::move(this->tasks.front());
//             this->tasks.pop();
//         }
//         task();
//     }
// }

// template<class F>
// void ThreadPool::enqueue(F&& f) {
//     {
//         std::unique_lock<std::mutex> lock(queueMutex);
//         tasks.emplace(std::forward<F>(f));
//     }
//     condition.notify_one();
// }

// int main() {
//     // OpenAI test start
//     liboai::OpenAI oai;

//     oai.Completion->create();
//     oai.Completion->create_async();
//     oai.Completion->Download();
//     oai.Completion->DownloadAsync();

//     oai.ChatCompletion->create();
//     oai.ChatCompletion->create_async();
//     oai.ChatCompletion->Download();
//     oai.ChatCompletion->DownloadAsync();



//     // OpenAI test end


//     ThreadPool pool(4);

//     for (int i = 0; i < 8; ++i) {
//         pool.enqueue([i] {
//             std::this_thread::sleep_for(std::chrono::seconds(1));
//             std::cout << "Task " << i << " completed" << std::endl;
//         });

//         std::cout << "q: " << i << std::endl;
//     }

//     std::this_thread::sleep_for(std::chrono::seconds(10));
//     return 0;
// }
