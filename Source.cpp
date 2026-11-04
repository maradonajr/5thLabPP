#include <iostream>
#include <thread>
#include <queue>
#include <utility>
#include <climits>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <Windows.h>

const int COUNT{ 111 };
const int CHUNK{ 10 };
const int NTHREAD{ 4 };

HANDLE hSemGlobal = nullptr;
HANDLE hSemConsole = nullptr;

int global_max = INT_MIN;

class ThreadSafeQueue
{
private:
    std::queue<std::pair<int*, int>> q;
    HANDLE hSem;

public:
    ThreadSafeQueue()
    {
        hSem = CreateSemaphore(nullptr, 1, 1, nullptr);
        if (!hSem)
            throw std::runtime_error("CreateSemaphore (queue) failed");
    }
    ~ThreadSafeQueue()
    {
        CloseHandle(hSem);
    }

    void push(std::pair<int*, int> task)
    {
        q.push(task);
    }

    bool tryPop(std::pair<int*, int>& out)
    {
        WaitForSingleObject(hSem, INFINITE);
        try
        {
            if (q.empty())
            {
                ReleaseSemaphore(hSem, 1, nullptr);
                return false;
            }
            out = q.front();
            q.pop();
            ReleaseSemaphore(hSem, 1, nullptr);
            return true;
        }
        catch (...)
        {
            ReleaseSemaphore(hSem, 1, nullptr);
            throw;
        }
    }
};

ThreadSafeQueue* g_queue = nullptr;

int arr_max(int* arr, int size)
{
    int result = arr[0];
    for (int i = 1; i < size; ++i)
        if (arr[i] > result)
            result = arr[i];
    return result;
}

void worker()
{
    int local_max = INT_MIN;
    std::pair<int*, int> task;

    while (g_queue->tryPop(task))
    {
        int cur = arr_max(task.first, task.second);
        if (cur > local_max) local_max = cur;

        Sleep(2);

        WaitForSingleObject(hSemConsole, INFINITE);
        try
        {
            std::cout << "Thread " << GetCurrentThreadId()
                << " chunk_max = " << cur << "\n";
            ReleaseSemaphore(hSemConsole, 1, nullptr);
        }
        catch (...)
        {
            ReleaseSemaphore(hSemConsole, 1, nullptr);
            throw;
        }
    }

    WaitForSingleObject(hSemGlobal, INFINITE);
    try
    {
        if (local_max > global_max)
            global_max = local_max;
        ReleaseSemaphore(hSemGlobal, 1, nullptr);
    }
    catch (...)
    {
        ReleaseSemaphore(hSemGlobal, 1, nullptr);
        throw;
    }
}

void init(int* arr)
{
    for (int i = 0; i < COUNT; ++i)
        arr[i] = rand() % 100;
}

int main()
{
    std::cout << "main ID = " << GetCurrentThreadId() << "\n";

    int arr[COUNT]{};
    srand(static_cast<unsigned>(time(nullptr)));
    init(arr);

    hSemGlobal = CreateSemaphore(nullptr, 1, 1, nullptr);
    hSemConsole = CreateSemaphore(nullptr, 1, 1, nullptr);
    if (!hSemGlobal || !hSemConsole)
    {
        std::cout << "CreateSemaphore (global/console) failed\n";
        return 1;
    }

    ThreadSafeQueue queue;
    g_queue = &queue;
    for (int i = 0; i < COUNT; i += CHUNK)
    {
        int len = (i + CHUNK <= COUNT) ? CHUNK : (COUNT - i);
        queue.push({ arr + i, len });
    }

    std::thread threads[NTHREAD - 1];
    for (int i = 0; i < NTHREAD - 1; ++i)
        threads[i] = std::thread(worker);

    worker();

    for (int i = 0; i < NTHREAD - 1; ++i)
        threads[i].join();

    int sequential = arr_max(arr, COUNT);
    std::cout << "max nonparallel = " << sequential << "\n";
    std::cout << "max parallel    = " << global_max << "\n";

    CloseHandle(hSemGlobal);
    CloseHandle(hSemConsole);

    return 0;
}