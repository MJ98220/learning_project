#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

using namespace std;

class ThreadPool {
private:
    vector<thread> workers;     //存储工作线程
    queue<function<void()>> tasks;  //任务队列,队列中的每个元素都是一个不接受任何参数且返回类型为 void 的可调用对象
    mutex queueMutex;   //互斥锁，用于保护队列任务
    condition_variable condition;   //条件变量，用于线程间的同步
    atomic<bool> stop;  //线程池是否停止的标志
    void workerThread() {
        while(true) {
            function<void()> task;
            {
                unique_lock<mutex> lock(queueMutex);
                // 等待任务队列中有任务或者线程池停止
                condition.wait(lock, [this] { return stop || !tasks.empty();});
                if (stop && tasks.empty())
                    return;
                // 从任务队列中取出一个任务
                task = move(tasks.front());
                tasks.pop();
            }
            // 执行任务
            task();
        }
    }

public:
    //构造函数，初始化线程池，创建指定数量的线程
    ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i=0; i<numThreads; i++) {
            workers.emplace_back(&ThreadPool::workerThread, this);
        }
    }

    //析构函数，停止线程池并等待所有线程完成
    ~ThreadPool() {
        {
            unique_lock<mutex> lock(queueMutex);
            stop = true;
        }
        //通知所有线程停止等待
        condition.notify_all();
        for (thread &worker : workers) {
            worker.join();
        }
    }

    //向线程池中添加任务
    //模板参数与返回值
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> future<typename result_of<F(Args...)>::type> {
        //推导返回类型
        using return_type = typename result_of<F(Args...)>::type;
        //封装任务
        //make_shared:创建智能指针，指向packaged_task对象
        //packaged_task：封装一个返回类型为 return_type 且无参数的可调用对象
        //bin:将可调用对象 f 和其参数 args... 绑定在一起，形成一个新的可调用对象
        //forward:确保参数的左值 / 右值属性在传递过程中保持不变
        auto task = make_shared< packaged_task<return_type()> >(
            bind(forward<F>(f), forward<Args>(args)...)
        );

        //获取future对象
        future<return_type> res = task->get_future();
        //加锁保护进程
        {
            //互斥锁包装器
            unique_lock<mutex> lock(queueMutex);
            if (stop)
                throw runtime_error("enqueue on stopped ThreadPool");
            //将任务添加到任务队列
            tasks.emplace([task]() { (*task) (); });
        }
        //通知线程有新任务
        condition.notify_one();
        return res;
    }
};

void taskFunction(int id) {
    cout << "Task " << id << " is running on thread " << endl;
    this_thread::sleep_for(chrono::seconds(1));
    cout << "Task " << id << " is finished. " << endl;
}

int main() {
    //创建一个包含4个线程的线程池
    ThreadPool pool(4);

    //向线程池添加8个任务
    vector<future<void>> futures;
    for (int i=0; i<8; i++) {
        futures.emplace_back(pool.enqueue(taskFunction, i));
    }

    //等待所有任务完成
    for (auto &future : futures) {
        future.wait();
    }
    return 0;
}

