#ifndef _sqlHeartbeat_hpp_
#define _sqlHeartbeat_hpp_

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono> 
#include <ctime>
#include <mutex>

#ifdef _WIN32
#include <mysql.h>

#else
#include <mysql/mysql.h> 
#endif

class sqlHeartbeat {
private:
    MYSQL* heart_sql;
    std::thread heart_thread;
    std::atomic<bool> heart_running{ false };
    std::atomic<bool>* connection_status_ptr;   // 指向外部连接状态标志的指针
    std::mutex heart_mutex;                     // 保护mysql_指针

public:
    ~sqlHeartbeat()
    {
        stop();
    }

    void start(MYSQL* mysql, std::atomic<bool>* status_ptr = nullptr, int seconds = 30) 
    {
        if (heart_running) {
            std::cerr << "心跳检测已经在运行" << std::endl;
            return;
        }

        {
            //  1、创建lock_guard对象时，它会立即对传入的互斥锁（这里是heart_mutex）调用lock()方法。
            std::lock_guard<std::mutex> lock(heart_mutex);
            heart_sql = mysql;
            connection_status_ptr = status_ptr;  // 保存外部状态指针
        }

        heart_running = true;
        heart_thread = std::thread([this, seconds]() {
            heartbeat_loop(seconds);
            });

        std::cout << "MySQL心跳检测已启动（间隔" << seconds << "秒）" << std::endl;
    }   // 2、离开作用域时自动解锁

    void stop() 
    {
        if (!heart_running) return;

        heart_running = false;
        //  检查线程是否可连接
        if (heart_thread.joinable()) 
        {
            heart_thread.join();    //  阻塞当前线程，直到被调用的线程执行完毕
            /*
            线程执行完毕，资源被系统回收
            heart_thread 对象变为"空线程"，不再关联任何执行线程
            */
        }

        std::cout << "MySQL心跳检测已停止" << std::endl;
    }

    //  暂停心跳检测
    void pause_heartbeat()
    {
        heart_running = false;
    }

    // 更新MySQL连接指针（用于重连后更新）
    void update_mysql_ptr(MYSQL* new_mysql) {
        std::lock_guard<std::mutex> lock(heart_mutex);
        heart_sql = new_mysql;
        heart_running = true;
    }

private:
    void heartbeat_loop(int interval_seconds) 
    {
        while (heart_running) {

            // 线程暂停
            std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));

            bool connection_status = false;

            //  自动锁
            std::lock_guard<std::mutex> lock(heart_mutex);
            if (heart_sql)
            {
                if (mysql_ping(heart_sql) == 0)
                {
                    connection_status = true;
                }
            }

            if (!connection_status) {
                std::cerr << "[" << get_current_time() << "] MySQL连接检查失败" << std::endl;

                if (connection_status_ptr) {
                    *connection_status_ptr = false;
                }
            }
        }
    }

    std::string get_current_time() 
    {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

#ifdef _WIN32
        // Windows: 使用localtime_s
        std::tm tm_now;
        localtime_s(&tm_now, &time_t_now);
#else
        // Linux/macOS: 使用localtime_r（线程安全版本）
        std::tm tm_now;
        localtime_r(&time_t_now, &tm_now);
#endif

        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm_now);
        return buffer;
    }
};

#endif