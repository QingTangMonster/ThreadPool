#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
enum class PoolMode
{
	MODE_FIXED,
	MODE_CACHED,
};

//定义一个Any类用来接受所有类型参数
class Any
{
public:
	class Base
	{

	};
	class Derive : public Base
	{
		
	};
private:
	std::shared_ptr<Base> basePtr;
};

//定义一个基类任务，用户可以自定义任务类型，从Task继承，重写run方法实现自定义
class Task
{
public:
	virtual void run()=0;
private:

};

class Thread
{
public:
	//线程函数对象类型
	using ThreadFunc = std::function<void()>;
	//线程构造函数
	Thread(ThreadFunc func);
	//线程析构函数
	~Thread();
	//启动线程
	void start();
private:
	ThreadFunc func_;
};

class ThreadPool
{
public:
	ThreadPool();	//线程池构造
	~ThreadPool();	//线程池析构
	void start(int initThreadSize = 4);  //开启线程池
	void setMode(PoolMode mode); //设置线程池的模式
	void setTaskQueMaxThreshHold(int threadhold); //设置任务队列的上线阈值
	void submitTask(std::shared_ptr<Task> sp);  //用户提交任务接口
	ThreadPool(const ThreadPool&) = delete;				//禁止线程池的拷贝
	ThreadPool& operator=(const ThreadPool&) = delete;	//禁止线程池的赋值
private:
	void ThreadFunc();// 线程函数

private:
	//std::vector<Thread*> threads_;	//线程列表	线程是new出来的，用裸指针是需要delete的
	//优化一下
	std::vector<std::unique_ptr<Thread>> threads_;	//使用智能指针，避免手动释放资源，容器析构，元素析构，智能指针析构对应内存析构
	size_t initThreadSize_;//初始的线程数量
	

	//std::queue<Task*> tasks_; //必须使用基类指针指向派生类才能发生多态，重写run函数
	//但是用户可能传进来一个临时对象，用户提交完task后，生命周期结束了，所以直接用裸指针是不妥的
	std::queue <std::shared_ptr<Task>>taskQue_; //任务队列
	std::atomic_uint taskSize_;	//任务的数量
	int taskQueMaxThreshHold_; //任务队列数量上限阈值
	std::mutex taskQueMtx_; //保证任务队列的线程安全
	std::condition_variable notEmpty_; //任务队列不空，可以消费
	std::condition_variable notFull_;  //任务队列不满，可以生产

	PoolMode poolMode_; //当前线程池的工作模式
};


#endif 
