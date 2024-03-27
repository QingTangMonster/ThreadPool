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
************************�̳߳���ض���*************************************************
*************************************************************************************/
//�̳߳ع���
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

//�߳�����
ThreadPool::~ThreadPool() 
{
	isPoolRunning_ = false;
	
	//�ȴ��̳߳��������е��̷߳���(�̵߳�״̬��������������)
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//�����̳߳ص�ģʽ
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
//����������е�������ֵ
void ThreadPool::setTaskQueMaxThreshHold(int threadhold)
{
	if (checkRunningState())	return;
	taskQueMaxThreshHold_ = threadhold;
}
//�����̳߳�
void ThreadPool::start(int initThreadSize)
{
	//�����̳߳ص�����״̬
	isPoolRunning_ = true;
	//��¼��ʼ�̸߳���
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;
	//�����̶߳���
	for (int i = 0; i < initThreadSize_; i++)
	{
		//�����̶߳����ʱ��Ҫ���̺߳��������߳�
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::ThreadFunc, this)));
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this, std::placeholders::_1));
		//threads_.emplace_back(ptr);           ע�ⲻ��ֱ��ptr��unique�ѿ����͸�ֵ����ֹ��
		//threads_.emplace_back(std::move(ptr));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}
	//���������߳�
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
		idleThreadSize++;			//��¼�����߳�����
	}
}
//�û��ύ����ӿ�  �û����øýӿڴ���������������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//�߳�ͨ�ţ��ȴ���������п���
	//notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; });
	//�û��ύ�����������������1s�������ж������ύʧ�ܣ�����
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
	{
		//�ȴ�1s��������û����
		std::cout << "task queue is full, submit task fail!" << std::endl;
		//return task->getResult();   task��queȡ�����Ժ󣬻ᱻpop��Ȼ��������
		return Result(sp, false);
	}


	//����п��࣬������������
	taskQue_.emplace(sp);
	taskSize_++;
	//��Ϊ�·�������������п϶������ˣ�����Ҫnotify notEmpty_,�Ͽ����ִ������
	notEmpty_.notify_all();
	//cachedģʽ��������ȽϽ�����С���ҿ��������Ҫ�������������Ϳ����߳�������ȥ�����߳�
	

	if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize && curThreadSize_ < threadSizeThreashHold_)
	{
		//�������߳�
		std::cout << ">>> create new thread..." << std::endl;
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();
		curThreadSize_++;
		idleThreadSize++;
	}
	//����һ��Result����

	//std::cout << "task submit true! "<< std::endl;
	return Result(sp);
}
//�߳���������
void ThreadPool::ThreadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	while(isPoolRunning_)
	{
		std::shared_ptr<Task> task;
		{
			//��ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "tid" << std::this_thread::get_id() << "���Ի�ȡ����..." << std::endl;

			//cachedģʽ�£��п��ܴ����˺ܶ��̣߳����ǿ���60s��Ӧ��Ҫ����
			//�������գ�����initThreadSize_�������߳�Ҫ���л��գ�
			//ÿһ���ӷ���һ��    ��ô���ֳ�ʱ���ػ�������ִ�з���
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
						//��ʼ�����߳�
						//��¼����߳�������ֵ�޸�
						threads_.erase(threadid);
						curThreadSize_--;
						idleThreadSize--;
						//���̶߳�����߳�������ɾ��  ,��Ҫ����Ӧ�����ĸ��̵߳� 
						std::cout << "threadid��" << std::this_thread::get_id() << "exit!" << std::endl;
						return;
					}
				}
			}
			else
			{
				//�ȴ���������notEmpty
				notEmpty_.wait(lock);
			}			
			//�̳߳�Ҫ�����������߳���Դ
			if (!isPoolRunning_)
			{
				break;
			}
		}
		if (!isPoolRunning_)
		{
			threads_.erase(threadid);
			std::cout << "threadid��" << std::this_thread::get_id() << "exit!" << std::endl;
			exitCond_.notify_all();
			return;
		}
		idleThreadSize--;
		std::cout << "tid" << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;
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
			//task->run();			//����ָ��ָ�������࣬������̬
			//ִ�����񣻰�����ķ���ֵ����result
			task->exec();
		}
		idleThreadSize++;
		lastTime = std::chrono::high_resolution_clock().now();   //����ִ���������ʱ��
	}
	//�̳߳�Ҫ�����������߳���Դ
	threads_.erase(threadid);
	exitCond_.notify_all();
	std::cout << "threadid��" << std::this_thread::get_id() << "exit!" << std::endl;
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}

/*****************************************************************************************
************************�߳���ض���*************************************************
*************************************************************************************/
int Thread::generateId_ = 0;
//�̹߳��캯��
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)
{}
//�߳���������
Thread::~Thread() {}

//�����̣߳�ִ���̺߳���
void Thread::start()
{
	//����һ���߳���ִ��һ���̺߳���
	std::thread t(func_,threadId_);			//C++11��˵���̶߳���t���̺߳���func_
	t.detach();			//���÷����߳�  pthread_detach pthread_t���óɷ����߳�
}

int Thread::getId()const
{
	return threadId_;
}

/*****************************************************************************************
************************Result��ض���*************************************************
*************************************************************************************/

Result::Result(std::shared_ptr<Task> task, bool isValid)
	:task_(task)
	,isValid_(isValid)
{
	task_->setResult(this);
}

Any Result::get()			//�û�����
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait();			//task�������û��ִ���꣬����������û����߳�
	return std::move(any_);
}

void Result::setVal(Any any)			//threadFunc����
{
	//�洢task�ķ���ֵ
	this->any_ = std::move(any);
	sem_.post();	//�ѽ���ȡ����ķ���ֵ�������ź�����Դ

}

/*****************************************************************************************
************************Task��ض���*************************************************
*************************************************************************************/

void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run());				//	���﷢����̬
	}
}

void Task::setResult(Result* res)
{
	result_ = res;
}

Task::Task()
	:result_(nullptr)
{}