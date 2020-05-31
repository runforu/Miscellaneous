#include <atomic>
#include <functional>
#include <iostream>
#include <list>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include "Common.h"

namespace Utils {

/*  Examples:
    {
        AsyncTaskHelper ath;
        ath.AddTask([&ath](){
            // do something.
            ath.AddTask([](){
                //do something.
            });
        });
        ath.AddTask(std::bind(...));
        ath.WaitForComplete(); // WaitForComplete can be called many times.
    } // Blocked here by WaitForComplete in dtor.
*/
class AsyncTask {
private:
    using Task = std::function<void()>;
    using Lock = std::unique_lock<std::mutex>;

public:
    /// \brief Constructor,
    /// Parameters:
    ///     maxthread: the number of threads in the threadpool, default value of the number of processors is not optimal.
    AsyncTask(size_t maxthread = std::thread::hardware_concurrency()) noexcept;
    ~AsyncTask();

    inline size_t MaxConcurrency() {
        return m_Threads.size();
    }

    inline explicit operator bool() {
        return !m_StopRunning;
    }

    /// \brief Add new task to queue. Can called from Task.
    void AddTask(Task&& task);

    /// \brief Shut down task queue. Can called from any thread.
    /// Parameters:
    ///     force:  true, forbid adding new task and clear pending tasks, waiting for running task.
    ///             false, forbid adding new task and waiting for running task and pending task.
    void Shutdown(bool force = false);

    /// \brief Idempotent. Wait for the task queue complete, make sure add at least one task before waiting, otherwise, it
    /// return immediately. Do not call it in child thread's context, that is, do not call it in the Task.
    void WaitForComplete();

private:
    // Each thread runs this loop.
    void Loop();
    AsyncTask(const AsyncTask& self) = delete;
    AsyncTask(AsyncTask&& self) = delete;
    AsyncTask& operator=(const AsyncTask& self) = delete;
    AsyncTask& operator=(AsyncTask&& self) = delete;

private:
    std::condition_variable m_Condition;
    std::mutex m_Mutex;
    std::vector<std::thread> m_Threads;
    // All the following members are protected by Lock.
    bool m_StopRunning;
    int m_RunningTasks;
    std::list<Task> m_TaskQueue;
};

AsyncTask::AsyncTask(size_t maxthread /*=  std::thread::hardware_concurrency()*/) noexcept : m_StopRunning(false), m_RunningTasks(0) {
    try {
        m_Threads.reserve(maxthread);
        for (decltype(maxthread) i = 0; i < maxthread; ++i) {
            m_Threads.emplace_back(&AsyncTask::Loop, this);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (...) {
        m_StopRunning = true;
    }
}

AsyncTask::~AsyncTask() {
    WaitForComplete();
}

void AsyncTask::AddTask(Task&& task) {
    Lock lock(m_Mutex);
    if (!m_StopRunning) {
        m_TaskQueue.emplace_back(task);
    }
    lock.unlock();
    m_Condition.notify_all();
}

void AsyncTask::Shutdown(bool force) {
    Lock lock(m_Mutex);
    m_StopRunning = true;
    if (force) {
        // Clear pending task.
        m_TaskQueue.clear();
    }

    // Notify all including WaitForComplete.
    lock.unlock();
    m_Condition.notify_all();
}

void AsyncTask::Loop() {
    while (true) {
        try {
            Task job;
            {
                Lock lock(m_Mutex);
                m_Condition.wait(lock, [this] { return !m_TaskQueue.empty() || m_StopRunning; });
                if (m_StopRunning && m_TaskQueue.empty()) {
                    break;
                }
                if (!m_TaskQueue.empty()) {
                    job = std::move(m_TaskQueue.front());
                    m_TaskQueue.pop_front();
                    ++m_RunningTasks;
                }
            }
            if (job) {
                job();
            }
            {
                Lock lock(m_Mutex);
                --m_RunningTasks;
                if (m_TaskQueue.empty() && m_RunningTasks == 0) {
                    m_StopRunning = true;
                    lock.unlock();
                    // No pending tasks and running tasks.
                    m_Condition.notify_all();
                }
            }
        } catch (...) {
        }
    }
}

void AsyncTask::WaitForComplete() {
    {
        Lock lock(m_Mutex);
        m_Condition.wait(lock, [this] { return m_TaskQueue.empty() && m_RunningTasks == 0; });
    }
    // Notify threads to exit by set m_StopRunning with true if no task is added to the pool ^_^.
    m_StopRunning = true;
    // Invoke all waiting thread to exit loop.
    m_Condition.notify_all();

    for (std::thread& thread : m_Threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

}  // namespace Utils

namespace AsynTask_T {

using namespace Utils;
void Run(std::atomic_int& value) {
    std::default_random_engine engine(
        static_cast<unsigned int>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()));
    int random = engine();
    value += random;
    std::this_thread::sleep_for(std::chrono::milliseconds(random % 100));
    value -= random;
}

struct Functor {
    Functor(std::atomic_int& value) : m_value(value) {
    }

    Functor(const Functor& f) : m_value(f.m_value) {
    }

    void operator()() {
        Run(m_value);
    }

    void operator()(std::atomic_int& value) {
        Run(value);
    }

    void Method() {
        Run(m_value);
    }

    void Method(std::atomic_int& value) {
        Run(value);
    }

private:
    std::atomic_int& m_value;
};

void NormalFunc(std::atomic_int& value) {
    Run(value);
}

static std::atomic_int g_value = 0;
void Function() {
    Run(g_value);
}

// case: Idempotent
void TCase0() {
    auto start = std::chrono::system_clock::now();
    auto sleep_time = 10000;
    {
        Utils::AsyncTask at;
        at.AddTask([sleep_time]() {
            Function();
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
            Run(g_value);
        });
        at.WaitForComplete();
        at.WaitForComplete();
        at.WaitForComplete();
        at.WaitForComplete();
    }
    assert(g_value.load() == 0);
    auto elapse = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count();
    assert(elapse >= sleep_time);
}

// case: functor
void TCase1() {
    std::atomic_int value = 0;
    {
        Utils::AsyncTask at;
        at.AddTask(Functor{value});
    }
    assert(value.load() == 0);
}

// case: normal function
void TCase2() {
    {
        Utils::AsyncTask at;
        at.AddTask(Function);
    }
    assert(g_value.load() == 0);
}

// case: std::bind (deprecated since C++ 17, to be removed)
void TCase3() {
    std::atomic_int value = 0;
    {
        Utils::AsyncTask at;
        at.AddTask(std::bind(NormalFunc, std::ref(value)));
        Functor functor{value};
        at.AddTask(std::bind(static_cast<void (Functor::*)()>(&Functor::operator()), &functor));
        at.AddTask(std::bind(static_cast<void (Functor::*)()>(&Functor::Method), &functor));
        at.AddTask(std::bind(static_cast<void (Functor::*)(std::atomic_int&)>(&Functor::operator()), &functor, std::ref(value)));
        at.AddTask(std::bind(static_cast<void (Functor::*)(std::atomic_int&)>(&Functor::Method), &functor, std::ref(value)));
    }
    assert(value.load() == 0);
}

// case: lambda
void TCase4() {
    std::atomic_int value = 0;
    {
        Utils::AsyncTask at;
        at.AddTask([&value]() {
            Functor functor{value};
            functor.operator()(value);
        });
    }
    assert(value.load() == 0);
}

// case: functionality (a little intricate)
void TCase6() {
    std::atomic_int value = 0;
    {
        auto test_task = [](Utils::AsyncTask& at, std::atomic_int& value, int times) {
            static auto worker = [](std::atomic_int& value) {
                Functor functor{value};
                functor();
                functor.operator()(value);
                functor.Method();
                functor.Method(value);
                NormalFunc(value);
                Function();
            };

            static auto lambda0 = [&at, &value, times]() { at.AddTask([&at, &value, times]() { worker(value); }); };

            static auto lambda1 = [&at, &value, times]() {
                worker(value);
                int tmp = times;
                while (tmp-- > 0) {
                    at.AddTask(lambda0);
                }
                worker(value);
            };
            static auto lambda2 = [&at, &value, times]() {
                worker(value);
                int tmp = times;
                while (tmp-- > 0) {
                    at.AddTask(lambda1);
                }
                worker(value);
            };
            static auto lambda3 = [&at, &value, times]() {
                worker(value);
                int tmp = times;
                while (tmp-- > 0) {
                    at.AddTask(lambda2);
                }
                worker(value);
            };
            static auto lambda4 = [&at, &value, times]() {
                worker(value);
                int tmp = times;
                while (tmp-- > 0) {
                    at.AddTask(lambda3);
                }
                worker(value);
            };
            static auto lambda5 = [&at, &value, times]() {
                worker(value);
                int tmp = times;
                while (tmp-- > 0) {
                    at.AddTask(lambda4);
                }
                worker(value);
            };
            static auto lambda6 = [&at, &value, times]() {
                worker(value);
                int tmp = times;
                while (tmp-- > 0) {
                    at.AddTask(lambda5);
                }
                worker(value);
            };
            static auto lambda7 = [&at, &value, times]() {
                worker(value);
                int tmp = times;
                while (tmp-- > 0) {
                    at.AddTask(lambda6);
                }
                worker(value);
            };
            static auto lambda8 = [&at, &value, times]() {
                worker(value);
                int tmp = times;
                while (tmp-- > 0) {
                    at.AddTask(lambda7);
                }
                worker(value);
            };
            static auto lambda9 = [&at, &value, times]() {
                worker(value);
                int tmp = times;
                while (tmp-- > 0) {
                    at.AddTask(lambda8);
                }
                worker(value);
            };

            lambda9();
        };

        Utils::AsyncTask at;
        at.AddTask([&test_task, &at, &value]() { test_task(at, value, 2); });
    }
    assert(value.load() == 0);
    assert(g_value.load() == 0);
}

// case: functionality (a little intricate)
void TCase7() {
    std::atomic_int value = 0;
    {
        auto test_task = [](Utils::AsyncTask& at, std::atomic_int& value, int times) {
            static auto worker = [](std::atomic_int& value) {
                Functor functor{value};
                functor();
                functor.operator()(value);
                functor.Method();
                functor.Method(value);
                NormalFunc(value);
                Function();
            };

            static auto lambda0 = [&at, &value, times]() { at.AddTask([&at, &value, times]() { worker(value); }); };

#define DEF_LAMBDA(at, value, times, num, prev)        \
    static auto lambda##num = [&at, &value, times]() { \
        worker(value);                                 \
        int tmp = times;                               \
        while (tmp-- > 0) {                            \
            at.AddTask(lambda##prev);                  \
        }                                              \
        worker(value);                                 \
    };

            DEF_LAMBDA(at, value, times, 1, 0);
            DEF_LAMBDA(at, value, times, 2, 1);
            DEF_LAMBDA(at, value, times, 3, 2);
            DEF_LAMBDA(at, value, times, 4, 3);
            DEF_LAMBDA(at, value, times, 5, 4);
            DEF_LAMBDA(at, value, times, 6, 5);
            DEF_LAMBDA(at, value, times, 7, 6);
            DEF_LAMBDA(at, value, times, 8, 7);
            DEF_LAMBDA(at, value, times, 9, 8);

            lambda9();
        };

        Utils::AsyncTask at;
        at.AddTask([&test_task, &at, &value]() { test_task(at, value, 2); });
    }
    assert(value.load() == 0);
    assert(g_value.load() == 0);
}

// case: Performance
#pragma warning(disable : 4996 4244)
void TCase8() {
    auto lambda = [](int times, int loop) {
        constexpr int PI_LEN = 1024;
        char* result_pi = new char[times * PI_LEN]{};
        {
            auto calculate_pi = [](char* pi) {
                int a = 1e4, c = 3e3, b = c, d = 0, e = 0, f[3000], g = 1, h = 0;
                do {
                    if (!--b) {
                        sprintf(pi, "%04d", e + d / a), e = d % a, h = b = c -= 15;
                        pi += 4;
                    } else {
                        f[b] = (d = d / g * b + a * (h ? f[b] : 2e3)) % (g = b * 2 - 1);
                    }
                } while (b);
            };
            Utils::AsyncTask at;
            for (auto i = 0; i < times; ++i) {
                at.AddTask([&calculate_pi, buf{result_pi + i * PI_LEN}, loop]() {
                    for (auto i = 0; i < loop; i++) {
                        calculate_pi(buf);
                    }
                });
            }
        }
        for (auto i = 1; i < times; ++i) {
            assert(memcmp(result_pi, result_pi + i * PI_LEN, PI_LEN) == 0);
        }
        delete result_pi;
    };
    constexpr int LOOP = 2048;
    for (auto i = 0; i < 32; ++i) {
        auto start = std::chrono::system_clock::now();
        lambda(i, LOOP);
        auto elapse = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count();
        std::cout << "Run " << i << " * " << LOOP << " tiems of PI computing takes: " << elapse << std::endl;
    }
}

}  // namespace AsynTask_T

void AsynTask_Test() {
    UT_Case_ALL(AsynTask_T);
}