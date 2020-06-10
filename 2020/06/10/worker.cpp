#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using clk = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<clk>;
using dur_double = std::chrono::duration<double>;
using std::chrono::duration_cast;

class Timer {
public:
  Timer(const std::string &cmd) : _cmd{cmd}, _start{clk::now()} {};

  double time_ns() {
    auto duration = clk::now() - _start;
    auto elapsed_s = duration_cast<dur_double>(duration).count();
    return elapsed_s * 1000 * 1000 * 1000;
  }

  ~Timer(){};

private:
  std::string _cmd;
  time_point _start;
};

size_t counter{0};

struct worker {
  worker() = default;
  inline ~worker() { stop_thread(); }
  inline void stop_thread() {
    std::unique_lock<std::mutex> lock(locking_mutex);
    has_work = false;
    exiting = true;
    lock.unlock();
    cond_var.notify_all();
    if (thread.joinable()) {
      thread.join();
    }
  }

  inline void work() {
    std::unique_lock<std::mutex> lock(locking_mutex);
    has_work = true;
    lock.unlock();
    cond_var.notify_one(); // will notify the thread lock
  }

  inline void finish() {
    std::unique_lock<std::mutex> lock(locking_mutex);
    cond_var.wait(lock, [this] { return has_work == false; });
  }

private:
  std::mutex locking_mutex{};
  std::condition_variable cond_var{};
  bool has_work{false};
  bool exiting{false};

  std::thread thread = std::thread([this] {
    while (!exiting) {
      std::unique_lock<std::mutex> lock(locking_mutex);
      cond_var.wait(lock, [this] { return has_work || exiting; });
      if (exiting) {
        break;
      }
      counter++;
      has_work = false;
      lock.unlock();
      cond_var.notify_all();
    }
  });
};

worker w;

double startemptythread() {
  auto t = Timer{__FUNCTION__};
  auto mythread = std::thread([] {});
  mythread.join();
  return t.time_ns();
}

double startasync() {
  auto t = Timer{__FUNCTION__};
  auto f = std::async(std::launch::async, [] { counter++; });
  f.get();
  return t.time_ns();
}

double startworker() {
  auto t = Timer{__FUNCTION__};
  w.work();   // issue the work
  w.finish(); // wait for it to finish
  return t.time_ns();
}

double startthread() {
  auto t = Timer{__FUNCTION__};
  auto mythread = std::thread([] { counter++; });
  mythread.join();
  return t.time_ns();
}

double mean(const std::vector<double> &v) {
  if (v.size() == 0)
    return 0; // right.
  double sum = 0;
  for (auto &x : v) {
    sum += x;
  }
  return sum / v.size();
}

double std_error(const std::vector<double> &v, double meanval) {
  if (v.size() == 0)
    return 0; // right
  double s{0};

  for (auto &x : v) {
    s += (x - meanval) * (x - meanval);
  }

  return std::sqrt(s / v.size());
}

template <class F> void printtime(F f) {
  std::vector<double> timings;
  for (size_t i = 0; i <= 1000; i += 1) {
    timings.push_back(f());
  }
  double meanval = mean(timings);
  double stde = std_error(timings, meanval);
  std::cout.precision(6);
  std::cout << meanval << " ns +/- " << stde << std::endl;
  std::cout << "min: " << *min_element(timings.begin(), timings.end())
            << std::endl;
  std::cout << "max: " << *max_element(timings.begin(), timings.end())
            << std::endl;
}

int main() {
  std::cout << "worker" << std::endl;
  printtime(startworker);
  std::cout << "async" << std::endl;
  printtime(startasync);
  std::cout << "emptythread" << std::endl;
  printtime(startemptythread);
  std::cout << "thread" << std::endl;
  printtime(startthread);
}