#include "Instrumentor.hpp"

#include <iostream>

Engine::Instrumentor &Engine::Instrumentor::GetInstance()
{
    static Instrumentor instance;
    return instance;
}

void Engine::Instrumentor::BeginSession(const std::string &filepath)
{
    if (m_Active.load())
        return;

    // Open output file stream
    m_OutputStream.open(filepath);
    if (!m_OutputStream.is_open())
    {
        std::cerr << "[Instrumentor] Could not open results file: " << filepath << "\n";
        return;
    }

    // Activate instrumentor
    m_Active.store(true);
    WriteHeader();

    // Start writer thread
    m_WriterThread = std::thread([this]() { WriterThreadFunc(); });
}

void Engine::Instrumentor::WriteProfile(const ProfileResult &result)
{
    if (!m_Active.load())
        return;

    // Push profile result to queue with lock
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_Queue.push_back(result);
    }
    m_CV.notify_one(); // wake writer thread
}

void Engine::Instrumentor::EndSession()
{
    m_Active.store(false);
    m_CV.notify_one(); // wake writer thread to finish

    // Wait for writer thread to join
    if (m_WriterThread.joinable())
        m_WriterThread.join();

    // Write footer and close file stream
    WriteFooter();
    if (m_OutputStream.is_open())
        m_OutputStream.close();
    m_ProfileCount = 0;
}

bool Engine::Instrumentor::IsActive() const
{
    return m_Active.load();
}

void Engine::Instrumentor::WriteHeader()
{
    m_OutputStream << "{\"otherData\": {},\"traceEvents\":[";
    m_OutputStream.flush();
}

void Engine::Instrumentor::WriterThreadFunc()
{
    while (true)
    {
        std::deque<ProfileResult> pendingResults;
        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);
            m_CV.wait(lock, [this]() { return !m_Active.load() || !m_Queue.empty(); });
            if (!m_Active.load() && m_Queue.empty()) {
                break;
            }
            pendingResults.swap(m_Queue);
        }

        // Write all profile results in the queue
        while (!pendingResults.empty())
        {
            ProfileResult result = pendingResults.front();
            pendingResults.pop_front();

            // Write profile result in JSON format
            if (m_ProfileCount++ > 0)
                m_OutputStream << ",";

            // Sanitize name
            std::string name = result.Name;
            std::replace(name.begin(), name.end(), '"', '\'');

            // Write JSON entry
            m_OutputStream << "{";
            m_OutputStream << "\"cat\":\"function\",";
            m_OutputStream << "\"dur\":" << (result.End - result.Start) << ",";
            m_OutputStream << "\"name\":\"" << name << "\",";
            m_OutputStream << "\"ph\":\"X\",";
            m_OutputStream << "\"pid\":0,";
            m_OutputStream << "\"tid\":" << result.ThreadID << ",";
            m_OutputStream << "\"ts\":" << result.Start;
            m_OutputStream << "}";

        }
        m_OutputStream.flush();
    }
}

void Engine::Instrumentor::WriteFooter()
{
    m_OutputStream << "]}";
    m_OutputStream.flush();
}
