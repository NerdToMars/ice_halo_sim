#ifndef SRC_THREADINGPOOL_H_
#define SRC_THREADINGPOOL_H_

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>


namespace IceHalo {

class ThreadingPool {
public:
  ~ThreadingPool() = default;

  void Start();
  void AddJob(std::function<void()> job);
  void WaitFinish();
  bool TaskRunning();

  static ThreadingPool* GetInstance();

private:
  ThreadingPool(size_t num = 1);

  size_t thread_num_;
  std::vector<std::thread> pool_;
  std::atomic<bool> alive_;

  std::queue<std::function<void()> > queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_condition_;

  std::atomic<int> running_jobs_;
  std::atomic<int> alive_threads_;
  std::mutex task_mutex_;
  std::condition_variable task_condition_;

  void WorkingFunction();

  static const int kHardwareConcurrency;
  static ThreadingPool* instance_;
  static std::mutex instance_mutex_;
};

}   // namespace IceHalo


#endif  // SRC_THREADINGPOOL_H_
