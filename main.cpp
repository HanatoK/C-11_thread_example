#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class MultiThreadCalc {
private:
  std::vector<double> mData;
  std::vector<double> mReference;
  std::vector<std::thread> mThreads;
  std::vector<std::condition_variable> mConds;
  std::vector<std::mutex> mMutexes;
  std::vector<int> mTaskStates;
  size_t mNumBlocks;
  bool mShutdown;
  bool mFirstTime;
  void worker(size_t thread_index) {
    while (mTaskStates[thread_index] == 0 && !mShutdown) {
      std::unique_lock<std::mutex> lk(mMutexes[thread_index]);
      const size_t mStride = mThreads.size();
      for (size_t block_index = 0; block_index < mNumBlocks; ++block_index) {
        const size_t data_index = block_index * mStride + thread_index;
        if (data_index < mData.size()) {
          const double x = double(data_index) / mData.size();
          mData[data_index] = std::sin(x);
        }
      }
      mTaskStates[thread_index] = 1;
      mConds[thread_index].notify_one();
      mConds[thread_index].wait(lk, [this, thread_index]() {
        return mTaskStates[thread_index] == 0;
      });
    }
  }
public:
  MultiThreadCalc(size_t numData,
                  size_t numThreads = std::thread::hardware_concurrency())
      : mData(numData, 0), mThreads(numThreads), mConds(numThreads),
        mMutexes(numThreads), mTaskStates(numThreads), mShutdown(false),
        mFirstTime(true) {
    mNumBlocks = numData / numThreads + 1;
    std::cout << "mNumBlocks = " << mNumBlocks << std::endl;
    std::vector<double> mReference(mData.size());
    const auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < mReference.size(); ++i) {
      const double x = double(i) / mReference.size();
      mReference[i] = std::sin(x);
    }
    const auto end = std::chrono::steady_clock::now();
    std::cout << "Elapsed time (reference) in milliseconds: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms" << std::endl;
  }
  ~MultiThreadCalc() {
    std::cout << "~MultiThreadCalc()\n";
    mShutdown = true;
    for (size_t i = 0; i < mConds.size(); ++i) {
      std::unique_lock<std::mutex> lk(mMutexes[i]);
      lk.unlock();
      mTaskStates[i] = 0;
      mConds[i].notify_one();
    }
    for (size_t i = 0; i < mThreads.size(); ++i) {
      if (mThreads[i].joinable()) mThreads[i].join();
    }
  }
  void run() {
    for (int i = 0; i < 10; ++i) {
      std::fill(mData.begin(), mData.end(), 0.0);
      for (size_t i = 0; i < mConds.size(); ++i) {
        mTaskStates[i] = 0;
        if (mFirstTime) mThreads[i] = std::thread(&MultiThreadCalc::worker, this, i);
        if (!mFirstTime) mConds[i].notify_one();
      }
      if (mFirstTime) {
        mFirstTime = false;
      }
      const auto start = std::chrono::steady_clock::now();
      for (size_t i = 0; i < mConds.size(); ++i) {
        std::unique_lock<std::mutex> lk(mMutexes[i]);
        mConds[i].wait(lk, [this, i]() { return mTaskStates[i] == 1; });
      }
      const auto end = std::chrono::steady_clock::now();
      double error = 0;
      for (size_t i = 0; i < mReference.size(); ++i) {
        error += std::abs(mReference[i] - mData[i]);
      }
      std::cout << "Elapsed time (parallel) in milliseconds: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                << " ms ; error = " << error << std::endl;
    }
  }
};

int main() {
  MultiThreadCalc obj(25497563);
  obj.run();
  return 0;
}
