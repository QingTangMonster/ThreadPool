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

//����һ��Any�����������������Ͳ���
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
	Any(T data) :base_(std::make_unique<Derive<T>>(data))			//��Any������������
	{}
	template<typename T>					//��Any��ȡ����
	T cast_()
	{
		//����ָ��ת��������ָ��
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());  //����ָ����get������ȡ��ָ��
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
	//����һ�������ָ��
	std::unique_ptr<Base> base_;
};

//ʵ��һ���ź�����
class Semaphore
{
public:
	Semaphore(int limit = 0) 
		:resLimit_(limit)
		,isExit_(false)
	{}
	~Semaphore() = default;
	//��ȡһ���ź�����Դ
	void wait()
	{
		if (isExit_)	return;
		std::unique_lock<std::mutex> lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
	//����һ���ź�Դ��Դ
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

//ʵ�ֽ����ύ���̳߳ص�task����ִ�����ķ���ֵResult

class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	//����һ��setVal,��ȡ����ִ���귵��ֵ,threadFunc����
	void setVal(Any any);
	//�������get,�û����ã���ȡany
	Any get();
private:
	Any any_;   //�洢����ķ���ֵ
	Semaphore sem_;    //�߳�ͨ���ź���
	//Task task_;		������ʹ�ó�������Task
	std::shared_ptr<Task> task_; //task
	std::atomic_bool isValid_;	//����ֵ�Ƿ���Ч
};

//����һ�����������û������Զ����������ͣ���Task�̳У���дrun����ʵ���Զ���
class Task
{
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	virtual Any run() = 0;
private:
	Result* result_;			//��������������������������ǿ����ָ��
};

class Thread
{
public:
	//�̺߳�����������
	using ThreadFunc = std::function<void(int)>;
	//�̹߳��캯��
	Thread(ThreadFunc func);
	//�߳���������
	~Thread();
	//�����߳�
	void start();
	//��ȡ�߳�id
	int getId()const;

private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;  //�����߳�id
};

class ThreadPool
{
public:
	ThreadPool();	//�̳߳ع���
	~ThreadPool();	//�̳߳�����
	void start(int initThreadSize = std::thread::hardware_concurrency());  //�����̳߳�
	void setMode(PoolMode mode); //�����̳߳ص�ģʽ
	void setThreadSizeThreashHold(int threadhold); //�����̵߳�������ֵ
	void setTaskQueMaxThreshHold(int threadhold); //����������е�������ֵ
	Result submitTask(std::shared_ptr<Task> sp);  //�û��ύ����ӿ�
	ThreadPool(const ThreadPool&) = delete;				//��ֹ�̳߳صĿ���
	ThreadPool& operator=(const ThreadPool&) = delete;	//��ֹ�̳߳صĸ�ֵ
private:
	void ThreadFunc(int threadid);// �̺߳���
	bool checkRunningState() const; //����̳߳�����״̬
private:
	//std::vector<Thread*> threads_;	//�߳��б�	�߳���new�����ģ�����ָ������Ҫdelete��
	//�Ż�һ��
	/*****************************************************************************************************************
	*****************************************>>    �߳����       <<****************************************************
	*****************************************************************************************************************/
	//std::vector<std::unique_ptr<Thread>> threads_;	//ʹ������ָ�룬�����ֶ��ͷ���Դ������������Ԫ������������ָ��������Ӧ�ڴ�����
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	size_t initThreadSize_;//��ʼ���߳�����
	std::atomic_uint idleThreadSize; //�����߳�����
	int threadSizeThreashHold_;	//�߳�������������ֵ
	std::atomic_int curThreadSize_;//��¼��ǰ�߳�����

	/*****************************************************************************************************************
	*****************************************>>    �������       <<****************************************************
	*****************************************************************************************************************/
	//std::queue<Task*> tasks_; //����ʹ�û���ָ��ָ����������ܷ�����̬����дrun����
	//�����û����ܴ�����һ����ʱ�����û��ύ��task���������ڽ����ˣ�����ֱ������ָ���ǲ��׵�
	std::queue <std::shared_ptr<Task>>taskQue_; //�������
	std::atomic_uint taskSize_;	//���������
	int taskQueMaxThreshHold_; //�����������������ֵ

	/*****************************************************************************************************************
	*****************************************>>    ͨ�����       <<****************************************************
	*****************************************************************************************************************/
	std::mutex taskQueMtx_; //��֤������е��̰߳�ȫ
	std::condition_variable notEmpty_; //������в��գ���������
	std::condition_variable notFull_;  //������в�������������
	std::condition_variable exitCond_; //�ȴ��߳���Դȫ������

	/*****************************************************************************************************************
	*****************************************>>    ״̬���       <<****************************************************
	*****************************************************************************************************************/
	PoolMode poolMode_; //��ǰ�̳߳صĹ���ģʽ
	std::atomic_bool isPoolRunning_; //��ǰ�̳߳ص�����״̬

};


#endif 

