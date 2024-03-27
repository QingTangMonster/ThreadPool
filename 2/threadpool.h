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

//����һ��Any�����������������Ͳ���
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

//����һ�����������û������Զ����������ͣ���Task�̳У���дrun����ʵ���Զ���
class Task
{
public:
	virtual void run()=0;
private:

};

class Thread
{
public:
	//�̺߳�����������
	using ThreadFunc = std::function<void()>;
	//�̹߳��캯��
	Thread(ThreadFunc func);
	//�߳���������
	~Thread();
	//�����߳�
	void start();
private:
	ThreadFunc func_;
};

class ThreadPool
{
public:
	ThreadPool();	//�̳߳ع���
	~ThreadPool();	//�̳߳�����
	void start(int initThreadSize = 4);  //�����̳߳�
	void setMode(PoolMode mode); //�����̳߳ص�ģʽ
	void setTaskQueMaxThreshHold(int threadhold); //����������е�������ֵ
	void submitTask(std::shared_ptr<Task> sp);  //�û��ύ����ӿ�
	ThreadPool(const ThreadPool&) = delete;				//��ֹ�̳߳صĿ���
	ThreadPool& operator=(const ThreadPool&) = delete;	//��ֹ�̳߳صĸ�ֵ
private:
	void ThreadFunc();// �̺߳���

private:
	//std::vector<Thread*> threads_;	//�߳��б�	�߳���new�����ģ�����ָ������Ҫdelete��
	//�Ż�һ��
	std::vector<std::unique_ptr<Thread>> threads_;	//ʹ������ָ�룬�����ֶ��ͷ���Դ������������Ԫ������������ָ��������Ӧ�ڴ�����
	size_t initThreadSize_;//��ʼ���߳�����
	

	//std::queue<Task*> tasks_; //����ʹ�û���ָ��ָ����������ܷ�����̬����дrun����
	//�����û����ܴ�����һ����ʱ�����û��ύ��task���������ڽ����ˣ�����ֱ������ָ���ǲ��׵�
	std::queue <std::shared_ptr<Task>>taskQue_; //�������
	std::atomic_uint taskSize_;	//���������
	int taskQueMaxThreshHold_; //�����������������ֵ
	std::mutex taskQueMtx_; //��֤������е��̰߳�ȫ
	std::condition_variable notEmpty_; //������в��գ���������
	std::condition_variable notFull_;  //������в�������������

	PoolMode poolMode_; //��ǰ�̳߳صĹ���ģʽ
};


#endif 
