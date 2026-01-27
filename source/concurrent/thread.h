#ifndef _THREAD_H_
#define _THREAD_H_

#if defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#include <sched.h>
#elif _WIN32
#include <process.h>
#include <windows.h>
#endif

#include <exception>
#include <cstddef>
#include <utility>
#include <string>
#include <memory>
#include <boost/noncopyable.hpp>
#include "task.h"

namespace concurrent
{

#define THREAD_WAIT_TIMEOUT 5000
    
// Non copyable & Non movable
class Thread : public boost::noncopyable
{
    public :
        
        explicit Thread(const std::string& name = "");
        virtual ~Thread();
        
        void setTask(TaskPtr pTask)
        {
            assert(pTask);
            m_task = std::move(pTask);
        }

        const std::string getThreadName()const {return m_name;}
        
        void start(std::size_t stackSize=0) throw(std::runtime_error); 
        void join();
        bool isAlive() const;
        int bindThreadToCPUCore(int coreId);
        
        //STATIC UTILITY METHODS
        static unsigned int getNumberOfCores();
        static unsigned long getCurrentThreadID();
        static bool isHyperThreading();

        static inline void yield()
        {
            // C++11 way std::this_thread::yield
#ifdef __linux__
            pthread_yield();
#elif defined(__APPLE__)
            sched_yield();
#elif _WIN32
            SwitchToThread();
#endif
        }

        static inline void sleep(unsigned long milliseconds)
        {
            // C++11 way std::this_thread::sleep_for
            // However doesn`t exist in MSVC 2013
            auto iterations = milliseconds / 1000;
            for (unsigned long i(0); i<iterations; i++ )
            {
#if defined(__linux__) || defined(__APPLE__)
                usleep(1000000);
#elif _WIN32
                Sleep(1000);
#endif
            }
        }

    private:

        // Move ctor deletion
        Thread(Thread&& other) = delete;
        // Move assignment operator deletion
        Thread& operator=(Thread&& other) = delete;

        TaskPtr m_task;
        std::string m_name;
        bool m_started;
        bool m_joined;

    #if defined(__linux__) || defined(__APPLE__)
        pthread_t m_threadID;
        pthread_attr_t m_threadAttr;
    #elif _WIN32
        unsigned long m_threadID;
        HANDLE m_threadHandle;
    #endif

        bool isJoiningOk() const;

        static void* internalThreadFunction(void* argument);
        void setThreadName();
};

using ThreadPtr = std::unique_ptr<Thread>; 
    
}//namespace
#endif