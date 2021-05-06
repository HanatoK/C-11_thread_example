#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <random>

class MultiThreadCalc {
private:
  std::vector<double> mInput;
  std::vector<double> mReference;
  std::vector<double> mData;
  std::vector<std::thread> mThreads;
  std::vector<std::condition_variable> mConds;
  std::vector<std::mutex> mMutexes;
  std::vector<int> mTaskStates;
  size_t mNumBlocks;
  bool mShutdown;
  bool mFirstTime;
  void worker(size_t thread_index) {
    std::unique_lock<std::mutex> lk(mMutexes[thread_index]);
    while (!mShutdown) {
      mConds[thread_index].wait(lk, [this, thread_index]() {
        return mTaskStates[thread_index] == 0;
      });
      const size_t mStride = mThreads.size();
      for (size_t block_index = 0; block_index < mNumBlocks; ++block_index) {
        const size_t data_index = block_index * mStride + thread_index;
        if (data_index < mData.size()) {
          mData[data_index] = std::sin(mInput[data_index]);
        }
      }
      mTaskStates[thread_index] = 1;
      mConds[thread_index].notify_one();
    }
  }
public:
  MultiThreadCalc(size_t numData,
                  size_t numThreads = std::thread::hardware_concurrency())
      : mInput(numData, 0), mReference(numData, 0), mData(numData, 0),
        mThreads(numThreads), mConds(numThreads), mMutexes(numThreads),
        mTaskStates(numThreads), mShutdown(false), mFirstTime(true) {
    mNumBlocks = numData / numThreads + 1;
    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<double> d{0,1};
    std::generate(mInput.begin(), mInput.end(), [&](){return d(gen);});
    std::cout << "mNumBlocks = " << mNumBlocks << std::endl;
    const auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < mReference.size(); ++i) {
      mReference[i] = std::sin(mInput[i]);
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
      mTaskStates[i] = 0;
      lk.unlock();
      mConds[i].notify_one();
    }
    for (size_t i = 0; i < mThreads.size(); ++i) {
      if (mThreads[i].joinable()) mThreads[i].join();
    }
  }
  void run() {
    for (int i = 0; i < 10; ++i) {
      std::fill(mData.begin(), mData.end(), 0.0);
      std::vector<std::unique_lock<std::mutex>> locks;
      for (size_t i = 0; i < mConds.size(); ++i) {
        locks.push_back(std::unique_lock<std::mutex>(mMutexes[i]));
        mTaskStates[i] = 0;
        locks[i].unlock();
        if (mFirstTime) mThreads[i] = std::thread(&MultiThreadCalc::worker, this, i);
        mConds[i].notify_one();
      }
      if (mFirstTime) {
        mFirstTime = false;
      }
      const auto start = std::chrono::steady_clock::now();
      for (size_t i = 0; i < mConds.size(); ++i) {
        locks[i].lock();
        mConds[i].wait(locks[i], [this, i]() { return mTaskStates[i] == 1; });
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
  MultiThreadCalc obj(25497563, 8);
  obj.run();
  return 0;
}
