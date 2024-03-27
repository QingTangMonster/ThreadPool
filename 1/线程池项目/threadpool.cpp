#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>
#include <chrono>
const int TASK_MAX_THRESHHOLD = 1024;
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
		threads_.emplace_back(new Thread(std::bind(&ThreadPool::ThreadFunc, this)));	
	}
	//启动所有线程
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
//用户提交任务接口  用户调用该接口传入任务生产任务
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{}
//线程消费任务
void ThreadPool::ThreadFunc()
{
	std::cout << "begin threadFunc tid : " << std::this_thread::get_id << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "end threadFunc tid : " << std::this_thread::get_id << std::endl;
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