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
    value -= random;
}

struct Functor {
    Functor(std::atomic_int& value) : m_value(value) {}

    Functor(const Functor& f) : m_value(f.m_value) {}

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

void FuncInt(Functor& callable, std::atomic_int& value) {
    callable.Method(value);
}

void NormalFunc(std::atomic_int& value) {
    Run(value);
}

static std::atomic_int g_value = 0;
void Function() {
    Run(g_value);
}

void TCase0() {
    {
        Utils::AsyncTask at;
        at.AddTask(Function);
        at.WaitForComplete();
        at.WaitForComplete();
        at.WaitForComplete();
        at.WaitForComplete();
    }
    assert(g_value.load() == 0);
}

void TCase1() {
    std::atomic_int value = 0;
    {
        Utils::AsyncTask at;
        at.AddTask(Functor{value});
    }
    assert(value.load() == 0);
}

void TCase2() {
    {
        Utils::AsyncTask at;
        at.AddTask(Function);
    }
    assert(g_value.load() == 0);
}

void TCase3() {
    std::atomic_int value = 0;
    {
        Utils::AsyncTask at;
        at.AddTask(std::bind(NormalFunc, std::ref(value)));
    }
    assert(value.load() == 0);
}

void TCase4() {
    std::atomic_int value = 0;
    {
        Utils::AsyncTask at;
        Functor functor{value};
        at.AddTask(std::bind(static_cast<void (Functor::*)(std::atomic_int&)>(&Functor::Method), &functor, std::ref(value)));
    }
    assert(value.load() == 0);
}

void TCase5() {
    std::atomic_int value = 0;
    {
        Utils::AsyncTask at;
        Functor functor{value};
        at.AddTask(std::bind(static_cast<void (Functor::*)()>(&Functor::Method), &functor));
    }
    assert(value.load() == 0);
}

void TCase6() {
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

void TCase7() {
    std::atomic_int value = 0;
    {
        Utils::AsyncTask at;
        for (int i = 0; i < 10; ++i) {
            at.AddTask([&value, &at]() {
                Functor functor{value};
                functor.operator()(value);
                at.AddTask(functor);
                at.AddTask(std::bind(NormalFunc, std::ref(value)));
                at.AddTask([&value]() {
                    Functor functor{value};
                    functor.Method();
                });
                at.AddTask([&value]() {
                    Functor functor{value};
                    functor.Method(value);
                });
                for (int i = 0; i < 10; ++i) {
                    for (int i = 0; i < 10; ++i) {
                        for (int i = 0; i < 10; ++i) {
                            for (int i = 0; i < 10; ++i) {
                                functor.operator()(value);
                                at.AddTask(functor);
                                at.AddTask(std::bind(NormalFunc, std::ref(value)));
                                at.AddTask([&value]() {
                                    Functor functor{value};
                                    functor.Method();
                                });
                                at.AddTask([&value]() {
                                    Functor functor{value};
                                    functor.Method(value);
                                });
                            }
                            functor.operator()(value);
                            at.AddTask(functor);
                            at.AddTask(std::bind(NormalFunc, std::ref(value)));
                            at.AddTask([&value]() {
                                Functor functor{value};
                                functor.Method();
                            });
                            at.AddTask([&value]() {
                                Functor functor{value};
                                functor.Method(value);
                            });
                        }
                        functor.operator()(value);
                        at.AddTask(functor);
                        at.AddTask(std::bind(NormalFunc, std::ref(value)));
                        at.AddTask([&value]() {
                            Functor functor{value};
                            functor.Method();
                        });
                        at.AddTask([&value]() {
                            Functor functor{value};
                            functor.Method(value);
                        });
                    }
                    functor.operator()(value);
                    at.AddTask(functor);
                    at.AddTask(std::bind(NormalFunc, std::ref(value)));
                    at.AddTask([&value]() {
                        Functor functor{value};
                        functor.Method();
                    });
                    at.AddTask([&value]() {
                        Functor functor{value};
                        functor.Method(value);
                    });
                }
                functor.operator()(value);
                at.AddTask(functor);
                at.AddTask(std::bind(NormalFunc, std::ref(value)));
                at.AddTask([&value]() {
                    Functor functor{value};
                    functor.Method();
                });
                at.AddTask([&value]() {
                    Functor functor{value};
                    functor.Method(value);
                });
            });

            Functor functor{value};
            at.AddTask(functor);
            at.AddTask(std::bind(NormalFunc, std::ref(value)));
            at.AddTask([&value]() {
                Functor functor{value};
                functor.Method();
            });
            at.AddTask([&value]() {
                Functor functor{value};
                functor.Method(value);
            });
        }
    }
    assert(value.load() == 0);
}
}  // namespace AsynTask_T

void AsynTask_Test() {
    UT_Case_ALL(AsynTask_T);
}