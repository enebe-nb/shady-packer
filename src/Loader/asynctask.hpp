#pragma once

#include "main.hpp"
#include <thread>
#include <nlohmann/json.hpp>
#include <SokuLib-util.h>

class AsyncTask {
protected:
    virtual void run() = 0;
private:
    boolean done = false;
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
private:
    std::string fileId;
    std::string filename;
public:
    inline FetchFile(const std::string& fileId, const std::string& filename)
        : fileId(fileId), filename(filename) {}
};

class FetchJson : public AsyncTask {
protected:
    void run();
private:
    std::string fileId;
public:
    nlohmann::json data;
    inline FetchJson(const std::string& fileId) : fileId(fileId) {}
};

class FetchImage : public AsyncTask {
protected:
    void run();
private:
    std::string fileId;
public:
    struct Texture* texture = 0;
    inline FetchImage(const std::string& fileId) : fileId(fileId) {}
};