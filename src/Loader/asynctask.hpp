#pragma once

#include <filesystem>
#include <thread>
#include <nlohmann/json.hpp>

class AsyncTask {
protected:
    virtual void run() = 0;
private:
    bool done = false;
    std::thread* thread = 0;
    static inline void _start(AsyncTask* task) {task->run(); task->done = true;}
public:
    inline AsyncTask() {}
    AsyncTask(const AsyncTask&) = delete;
    AsyncTask(AsyncTask&&) = delete;

    inline void start() {thread = new std::thread(&AsyncTask::_start, this);}
    inline bool isRunning() {return thread;}
    inline bool isDone() {
        if (done && thread) {
            thread->join();
            delete thread;
            thread = 0;
        } return done;
    }
};

class FetchFile : public AsyncTask {
protected:
    void run();
public:
    std::string fileId;
    std::filesystem::path filename;
    inline FetchFile() {}
    inline FetchFile(const std::string& fileId, const std::filesystem::path& filename)
        : fileId(fileId), filename(filename) {}
};

class FetchJson : public AsyncTask {
protected:
    void run();
public:
    std::string fileId;
    nlohmann::json data;
    inline FetchJson() {}
    inline FetchJson(const std::string& fileId) : fileId(fileId) {}
};

class FetchImage : public AsyncTask {
protected:
    void run();
public:
    std::string fileId;
    std::filesystem::path filename;
    inline FetchImage() {}
    inline FetchImage(const std::string& fileId) : fileId(fileId) {}
    inline void Dispose() { std::filesystem::remove(filename); }
};