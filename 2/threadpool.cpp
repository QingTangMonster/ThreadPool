#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>
#include <chrono>
#include <memory>
const int TASK_MAX_THRESHHOLD = 4;
/*****************************************************************************************
************************线程池相关定义*************************************************
*************************************************************************************/
//线程池构造
ThreadPool::ThreadPool()
	:initThreadSize_(4)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
{}

//线程析构
ThreadPool::~ThreadPool(){}


//设置线程池的模式
void ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
}
//设置任务队列的上线阈值
void ThreadPool::setTaskQueMaxThreshHold(int threadhold)
{
	taskQueMaxThreshHold_ = threadhold;
}
//开启线程池
void ThreadPool::start(int initThreadSize)
{
	//记录初始线程个数
	initThreadSize_ = initThreadSize;
	//创建线程对象
	for (int i = 0; i < initThreadSize_; i++)
	{
		//创建线程对象的时候要把线程函数给到线程
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::ThreadFunc, this)));
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this));
		//threads_.emplace_back(ptr);           注意不能直接ptr，unique把拷贝和赋值给禁止了
		threads_.emplace_back(std::move(ptr));
	}
	//启动所有线程
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
	}
}
//用户提交任务接口  用户调用该接口传入任务生产任务
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//线程通信，等待任务队列有空余
	//notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; });
	//用户提交任务，最长不能阻塞超过1s，否则判断任务提交失败，返回
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; }))
	{
		//等待1s，条件还没满足
		std::cerr << "task queue is full, submit task fail" << std::endl;
		return;
	}
	//如果有空余，把任务放入队列
	taskQue_.emplace(sp);
	taskSize_++;
	//因为新放了任务，任务队列肯定不控了，所以要notify notEmpty_,赶快分配执行任务
	notEmpty_.notify_all();
}
//线程消费任务
void ThreadPool::ThreadFunc()
{

	/*std::cout << "begin threadFunc tid : " << std::this_thread::get_id << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "end threadFunc tid : " << std::this_thread::get_id << std::endl;*/

	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			//获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "tid" << std::this_thread::get_id << "尝试获取任务..." << std::endl;
			//等待环境变量notEmpty
			notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
			std::cout << "tid" << std::this_thread::get_id << "尝试任务成功" << std::endl;
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
			task->run();			//基类指针指向派生类，发生多态
			
		}
	}


}
/*****************************************************************************************
************************线程相关定义*************************************************
*************************************************************************************/
//线程构造函数
Thread::Thread(ThreadFunc func) 
	:func_(func)
{}
//线程析构函数
Thread::~Thread(){}

//启动线程，执行线程函数
void Thread::start()
{
	//创建一个线程来执行一个线程函数
	std::thread t(func_);			//C++11来说有线程对象t和线程函数func_
	t.detach();			//设置分离线程  pthread_detach pthread_t设置成分离线程
}