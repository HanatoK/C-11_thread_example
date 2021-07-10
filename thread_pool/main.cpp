#include <iostream>
#include <thread>
#include <condition_variable>
#include <future>
#include <vector>
#include <functional>
#include <queue>
#include <chrono>
#include <random>

// https://github.com/progschj/ThreadPool/blob/master/ThreadPool.h
// https://github.com/mvorbrodt/blog/blob/master/src/lesson_thread_pool_how_to.cpp
// https://github.com/mtrebi/thread-pool
class ThreadPool {
public:
  ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
  ~ThreadPool();
  template<typename F, typename... Args>
  auto addJob(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;
private:
  std::vector<std::thread> mThreads;
  std::queue<std::function<void()>> mTasks;
  std::mutex mMutex;
  std::condition_variable mCondVar;
  bool mShutdown;
};

ThreadPool::ThreadPool(size_t num_threads) {
  mThreads.reserve(num_threads);
  mShutdown = false;
  for (size_t i = 0; i < num_threads; ++i) {
    mThreads.push_back(
      std::thread(
        [this](){
          while(true) {
            std::function<void()> task;
            {
              std::unique_lock lock(this->mMutex);
              // wait until the queue is not empty
              this->mCondVar.wait(lock, [&](){return !(this->mTasks.empty()) || this->mShutdown;});
              if (this->mShutdown && this->mTasks.empty()) return;
              task = std::move(this->mTasks.front());
              this->mTasks.pop();
            }
            task();
          }
        }
      )
    );
  }
}

template<typename F, typename... Args>
auto ThreadPool::addJob(F&& f, Args&&... args)
  -> std::future<decltype(f(args...))> {
  auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(
    std::bind(std::forward<F>(f), std::forward<Args>(args)...));
  mTasks.push([task_ptr](){(*task_ptr)();});
  mCondVar.notify_one();
  return task_ptr->get_future();
}

ThreadPool::~ThreadPool() {
  mShutdown = true;
  mCondVar.notify_all();
  for (size_t i = 0; i < mThreads.size(); ++i) {
    if (mThreads[i].joinable()) {
      mThreads[i].join();
    }
  }
}

std::random_device rd;
std::mt19937 mt(rd());
std::uniform_int_distribution<int> dist(-2000, 2000);
auto rnd = std::bind(dist, mt);

void simulate_hard_computation() {
  std::this_thread::sleep_for(std::chrono::milliseconds(100 + rnd()));
}

// Simple function that adds multiplies two numbers and prints the result
int multiply(const int a, const int b) {
  simulate_hard_computation();
  const int res = a * b;
  const std::thread::id id = std::this_thread::get_id();
  std::cout << "Thread " << id << " : " << a << " * " << b << " = " << res << std::endl;
  return res;
}

int main() {
  ThreadPool pool(4);
  std::vector<std::future<int>> results;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 10; ++j) {
      results.push_back(pool.addJob(multiply, i, j));
    }
  }
  for (size_t i = 0; i < results.size(); ++i) {
    results[i].get();
  }
  return 0;
}