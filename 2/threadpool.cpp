#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>
#include <chrono>
#include <memory>
const int TASK_MAX_THRESHHOLD = 4;
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
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::ThreadFunc, this)));
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this));
		//threads_.emplace_back(ptr);           ע�ⲻ��ֱ��ptr��unique�ѿ����͸�ֵ����ֹ��
		threads_.emplace_back(std::move(ptr));
	}
	//���������߳�
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
	}
}
//�û��ύ����ӿ�  �û����øýӿڴ���������������
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//�߳�ͨ�ţ��ȴ���������п���
	//notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; });
	//�û��ύ�����������������1s�������ж������ύʧ�ܣ�����
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; }))
	{
		//�ȴ�1s��������û����
		std::cerr << "task queue is full, submit task fail" << std::endl;
		return;
	}
	//����п��࣬������������
	taskQue_.emplace(sp);
	taskSize_++;
	//��Ϊ�·�������������п϶������ˣ�����Ҫnotify notEmpty_,�Ͽ����ִ������
	notEmpty_.notify_all();
}
//�߳���������
void ThreadPool::ThreadFunc()
{

	/*std::cout << "begin threadFunc tid : " << std::this_thread::get_id << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "end threadFunc tid : " << std::this_thread::get_id << std::endl;*/

	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			//��ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "tid" << std::this_thread::get_id << "���Ի�ȡ����..." << std::endl;
			//�ȴ���������notEmpty
			notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
			std::cout << "tid" << std::this_thread::get_id << "��������ɹ�" << std::endl;
			//������գ��ʹ����������ȡ����
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			//ȡ����������Ӧ��ȥ�ͷ���
		}	
		//ȡ������ý���֪ͨ
		notFull_.notify_all();
		if (taskQue_.size() > 0)	notEmpty_.notify_all();
		//��ǰ�̸߳���ִ������
		if (task != nullptr)
		{
			task->run();			//����ָ��ָ�������࣬������̬
			
		}
	}


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