#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <unordered_map>
enum class PoolMode
{
	MODE_FIXED,
	MODE_CACHED,
};



class Task;

//定义一个Any类用来接受所有类型参数
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	template<typename T>
	Any(T data) :base_(std::make_unique<Derive<T>>(data))			//让Any接受任意数据
	{}
	template<typename T>					//从Any提取数据
	T cast_()
	{
		//基类指针转成派生类指针
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());  //智能指针有get方法获取裸指针
		if (pd == nullptr)
		{
			std::cout << "type is unmatch!" << std::endl;
		}
		return pd->data_;
	}
private:
	class Base
	{
	public:
		virtual ~Base() = default;
	};
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data):data_(data) 
		{}
		T data_;
	};
private:
	//定义一个基类得指针
	std::unique_ptr<Base> base_;
};

//实现一个信号量类
class Semaphore
{
public:
	Semaphore(int limit = 0) 
		:resLimit_(limit)
		,isExit_(false)
	{}
	~Semaphore() = default;
	//获取一个信号量资源
	void wait()
	{
		if (isExit_)	return;
		std::unique_lock<std::mutex> lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
	//增加一个信号源资源
	void post()
	{
		if (isExit_)	return;
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	std::atomic_bool isExit_;
	std::mutex mtx_;
	std::condition_variable cond_;
	int resLimit_;
};

//实现接受提交到线程池的task任务执行完后的返回值Result

class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	//问题一：setVal,获取任务执行完返回值,threadFunc调用
	void setVal(Any any);
	//问题二：get,用户调用，获取any
	Any get();
private:
	Any any_;   //存储任务的返回值
	Semaphore sem_;    //线程通信信号量
	//Task task_;		不允许使用抽象类型Task
	std::shared_ptr<Task> task_; //task
	std::atomic_bool isValid_;	//返回值是否有效
};

//定义一个基类任务，用户可以自定义任务类型，从Task继承，重写run方法实现自定义
class Task
{
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	virtual Any run() = 0;
private:
	Result* result_;			//。。。。。。。。。。。。。强智能指针
};

class Thread
{
public:
	//线程函数对象类型
	using ThreadFunc = std::function<void(int)>;
	//线程构造函数
	Thread(ThreadFunc func);
	//线程析构函数
	~Thread();
	//启动线程
	void start();
	//获取线程id
	int getId()const;

private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;  //保存线程id
};

class ThreadPool
{
public:
	ThreadPool();	//线程池构造
	~ThreadPool();	//线程池析构
	void start(int initThreadSize = std::thread::hardware_concurrency());  //开启线程池
	void setMode(PoolMode mode); //设置线程池的模式
	void setThreadSizeThreashHold(int threadhold); //设置线程的上线阈值
	void setTaskQueMaxThreshHold(int threadhold); //设置任务队列的上线阈值
	Result submitTask(std::shared_ptr<Task> sp);  //用户提交任务接口
	ThreadPool(const ThreadPool&) = delete;				//禁止线程池的拷贝
	ThreadPool& operator=(const ThreadPool&) = delete;	//禁止线程池的赋值
private:
	void ThreadFunc(int threadid);// 线程函数
	bool checkRunningState() const; //检查线程池启动状态
private:
	//std::vector<Thread*> threads_;	//线程列表	线程是new出来的，用裸指针是需要delete的
	//优化一下
	/*****************************************************************************************************************
	*****************************************>>    线程相关       <<****************************************************
	*****************************************************************************************************************/
	//std::vector<std::unique_ptr<Thread>> threads_;	//使用智能指针，避免手动释放资源，容器析构，元素析构，智能指针析构对应内存析构
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	size_t initThreadSize_;//初始的线程数量
	std::atomic_uint idleThreadSize; //空闲线程数量
	int threadSizeThreashHold_;	//线程数量的上线阈值
	std::atomic_int curThreadSize_;//记录当前线程数量

	/*****************************************************************************************************************
	*****************************************>>    任务相关       <<****************************************************
	*****************************************************************************************************************/
	//std::queue<Task*> tasks_; //必须使用基类指针指向派生类才能发生多态，重写run函数
	//但是用户可能传进来一个临时对象，用户提交完task后，生命周期结束了，所以直接用裸指针是不妥的
	std::queue <std::shared_ptr<Task>>taskQue_; //任务队列
	std::atomic_uint taskSize_;	//任务的数量
	int taskQueMaxThreshHold_; //任务队列数量上限阈值

	/*****************************************************************************************************************
	*****************************************>>    通信相关       <<****************************************************
	*****************************************************************************************************************/
	std::mutex taskQueMtx_; //保证任务队列的线程安全
	std::condition_variable notEmpty_; //任务队列不空，可以消费
	std::condition_variable notFull_;  //任务队列不满，可以生产
	std::condition_variable exitCond_; //等待线程资源全部回收

	/*****************************************************************************************************************
	*****************************************>>    状态相关       <<****************************************************
	*****************************************************************************************************************/
	PoolMode poolMode_; //当前线程池的工作模式
	std::atomic_bool isPoolRunning_; //当前线程池的启动状态

};


#endif 

