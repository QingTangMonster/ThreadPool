#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>
#include <chrono>
const int TASK_MAX_THRESHHOLD = 1024;
/*****************************************************************************************
************************�̳߳���ض���*************************************************
*************************************************************************************/
//�̳߳ع���
ThreadPool::ThreadPool()
	:initThreadSize_(4)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
{}

//�߳�����
ThreadPool::~ThreadPool(){}


//�����̳߳ص�ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
}
//����������е�������ֵ
void ThreadPool::setTaskQueMaxThreshHold(int threadhold)
{
	taskQueMaxThreshHold_ = threadhold;
}
//�����̳߳�
void ThreadPool::start(int initThreadSize)
{
	//��¼��ʼ�̸߳���
	initThreadSize_ = initThreadSize;
	//�����̶߳���
	for (int i = 0; i < initThreadSize_; i++)
	{
		//�����̶߳����ʱ��Ҫ���̺߳��������߳�
		threads_.emplace_back(new Thread(std::bind(&ThreadPool::ThreadFunc, this)));	
	}
	//���������߳�
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
//�û��ύ����ӿ�  �û����øýӿڴ���������������
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{}
//�߳���������
void ThreadPool::ThreadFunc()
{
	std::cout << "begin threadFunc tid : " << std::this_thread::get_id << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "end threadFunc tid : " << std::this_thread::get_id << std::endl;
}
/*****************************************************************************************
************************�߳���ض���*************************************************
*************************************************************************************/
//�̹߳��캯��
Thread::Thread(ThreadFunc func) 
	:func_(func)
{}
//�߳���������
Thread::~Thread(){}

//�����̣߳�ִ���̺߳���
void Thread::start()
{
	//����һ���߳���ִ��һ���̺߳���
	std::thread t(func_);			//C++11��˵���̶߳���t���̺߳���func_
	t.detach();			//���÷����߳�  pthread_detach pthread_t���óɷ����߳�
}