#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Engine
{

struct ProfileResult
{
    std::string Name;
    long long Start, End;
    uint32_t ThreadID;
};

class Instrumentor
{
public:
    static Instrumentor &GetInstance();

public:
    void BeginSession(const std::string &filepath);
    void WriteProfile(const ProfileResult &result);
    void EndSession();
    bool IsActive() const;

private:
    // Multithreading
    std::mutex m_QueueMutex;
    std::condition_variable m_CV;
    std::thread m_WriterThread;
    std::deque<ProfileResult> m_Queue;

    // Common
    std::atomic_bool m_Active = false;
    int m_ProfileCount = 0;
    std::ofstream m_OutputStream = {};

private:
    Instrumentor() = default;
    ~Instrumentor() = default;

private:
    void WriteHeader();
    void WriterThreadFunc();
    void WriteFooter();
};

} // namespace Engine
