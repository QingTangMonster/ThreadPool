#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>
#include <chrono>
#include <memory>
const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 100;
const int THREAD_MAX_IDLE_TIME = 60;
/*****************************************************************************************
************************线程池相关定义*************************************************
*************************************************************************************/
//线程池构造
ThreadPool::ThreadPool()
	: initThreadSize_(0)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning_(false)
	, idleThreadSize(0)
	, threadSizeThreashHold_(THREAD_MAX_THRESHHOLD)
	, curThreadSize_(0)
{}

//线程析构
ThreadPool::~ThreadPool() 
{
	isPoolRunning_ = false;
	
	//等待线程池里面所有的线程返回(线程的状态：阻塞或者运行)
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//设置线程池的模式
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())	return;
	poolMode_ = mode;
}
void ThreadPool::setThreadSizeThreashHold(int threadhold )
{
	if (checkRunningState())	return;
	if(poolMode_ ==PoolMode::MODE_CACHED)
		threadSizeThreashHold_ = threadhold;
}
//设置任务队列的上线阈值
void ThreadPool::setTaskQueMaxThreshHold(int threadhold)
{
	if (checkRunningState())	return;
	taskQueMaxThreshHold_ = threadhold;
}
//开启线程池
void ThreadPool::start(int initThreadSize)
{
	//设置线程池的启动状态
	isPoolRunning_ = true;
	//记录初始线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;
	//创建线程对象
	for (int i = 0; i < initThreadSize_; i++)
	{
		//创建线程对象的时候要把线程函数给到线程
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::ThreadFunc, this)));
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this, std::placeholders::_1));
		//threads_.emplace_back(ptr);           注意不能直接ptr，unique把拷贝和赋值给禁止了
		//threads_.emplace_back(std::move(ptr));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}
	//启动所有线程
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
		idleThreadSize++;			//记录空闲线程数量
	}
}
//用户提交任务接口  用户调用该接口传入任务生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//线程通信，等待任务队列有空余
	//notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; });
	//用户提交任务，最长不能阻塞超过1s，否则判断任务提交失败，返回
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
	{
		//等待1s，条件还没满足
		std::cout << "task queue is full, submit task fail!" << std::endl;
		//return task->getResult();   task从que取出来以后，会被pop，然后析构掉
		return Result(sp, false);
	}


	//如果有空余，把任务放入队列
	taskQue_.emplace(sp);
	taskSize_++;
	//因为新放了任务，任务队列肯定不空了，所以要notify notEmpty_,赶快分配执行任务
	notEmpty_.notify_all();
	//cached模式，任务处理比较紧急，小而且快的任务需要根据任务数量和空闲线程数量，去创建线程
	

	if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize && curThreadSize_ < threadSizeThreashHold_)
	{
		//创建新线程
		std::cout << ">>> create new thread..." << std::endl;
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();
		curThreadSize_++;
		idleThreadSize++;
	}
	//返回一个Result对象

	//std::cout << "task submit true! "<< std::endl;
	return Result(sp);
}
//线程消费任务
void ThreadPool::ThreadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	while(isPoolRunning_)
	{
		std::shared_ptr<Task> task;
		{
			//获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "tid" << std::this_thread::get_id() << "尝试获取任务..." << std::endl;

			//cached模式下，有可能创建了很多线程，但是空闲60s，应该要回收
			//结束回收（超过initThreadSize_数量的线程要进行回收）
			//每一秒钟返回一次    怎么区分超时返回还是任务执行返回
		while (isPoolRunning_&&taskQue_.size() == 0)
		{

			if (poolMode_ == PoolMode::MODE_CACHED)
			{
				if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
				{
					auto now = std::chrono::high_resolution_clock().now();
					auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
					if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
					{
						//开始回收线程
						//记录相关线程数量的值修改
						threads_.erase(threadid);
						curThreadSize_--;
						idleThreadSize--;
						//把线程对象从线程容器中删除  ,需要到对应的是哪个线程的 
						std::cout << "threadid：" << std::this_thread::get_id() << "exit!" << std::endl;
						return;
					}
				}
			}
			else
			{
				//等待环境变量notEmpty
				notEmpty_.wait(lock);
			}			
			//线程池要结束，回收线程资源
			if (!isPoolRunning_)
			{
				break;
			}
		}
		if (!isPoolRunning_)
		{
			threads_.erase(threadid);
			std::cout << "threadid：" << std::this_thread::get_id() << "exit!" << std::endl;
			exitCond_.notify_all();
			return;
		}
		idleThreadSize--;
		std::cout << "tid" << std::this_thread::get_id() << "获取任务成功" << std::endl;
		//如果不空，就从任务队列中取任务
		task = taskQue_.front();
		taskQue_.pop();
		taskSize_--;
			
		//取出来任务后就应该去释放锁
		}
		//取出任务得进行通知
		notFull_.notify_all();
		if (taskQue_.size() > 0)	notEmpty_.notify_all();
		//当前线程负责执行任务
		if (task != nullptr)
		{
			//task->run();			//基类指针指向派生类，发生多态
			//执行任务；把任务的返回值给到result
			task->exec();
		}
		idleThreadSize++;
		lastTime = std::chrono::high_resolution_clock().now();   //更新执行完任务的时间
	}
	//线程池要结束，回收线程资源
	threads_.erase(threadid);
	exitCond_.notify_all();
	std::cout << "threadid：" << std::this_thread::get_id() << "exit!" << std::endl;
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}

/*****************************************************************************************
************************线程相关定义*************************************************
*************************************************************************************/
int Thread::generateId_ = 0;
//线程构造函数
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)
{}
//线程析构函数
Thread::~Thread() {}

//启动线程，执行线程函数
void Thread::start()
{
	//创建一个线程来执行一个线程函数
	std::thread t(func_,threadId_);			//C++11来说有线程对象t和线程函数func_
	t.detach();			//设置分离线程  pthread_detach pthread_t设置成分离线程
}

int Thread::getId()const
{
	return threadId_;
}

/*****************************************************************************************
************************Result相关定义*************************************************
*************************************************************************************/

Result::Result(std::shared_ptr<Task> task, bool isValid)
	:task_(task)
	,isValid_(isValid)
{
	task_->setResult(this);
}

Any Result::get()			//用户调用
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait();			//task任务如果没有执行完，这里会阻塞用户的线程
	return std::move(any_);
}

void Result::setVal(Any any)			//threadFunc调用
{
	//存储task的返回值
	this->any_ = std::move(any);
	sem_.post();	//已近获取人物的返回值，增加信号量资源

}

/*****************************************************************************************
************************Task相关定义*************************************************
*************************************************************************************/

void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run());				//	这里发生多态
	}
}

void Task::setResult(Result* res)
{
	result_ = res;
}

Task::Task()
	:result_(nullptr)
{}