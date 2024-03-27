#include <iostream>
#include <thread>
#include <chrono>
#include "threadpool.h"

using uLong = unsigned long long;
class MyTask : public Task
{
public:
	
	MyTask(uLong begin, uLong end)
		:begin_(begin)
		,end_(end)
	{}
	Any run()				//run方法最终在线程池分配的线程里执行任务
	{
		std::cout << "tid" << std::this_thread::get_id() << "begin!" << std::endl;
		//std::this_thread::sleep_for(std::chrono::seconds(10));
		uLong sum = 0;
		for (uLong i = begin_; i <= end_; ++i)
		{
			sum += i;
		}
		return sum;
	}
private:
	uLong begin_;
	uLong end_;
};


int main()
{
	int i = 1;
	int& x1 = i;
	int& x2= x1;
	ThreadPool pool;
	pool.start(4);
	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 10000000));
	uLong sum1 = res1.get().cast_<uLong>();
	std::cout << sum1 << std::endl;

	std::cout << "main over" << std::endl;
	return 0;
#if 0
	//如何进行线程池的资源回收
	{
		ThreadPool pool;
		//设置线程池的工作模式
		pool.setMode(PoolMode::MODE_CACHED);
		//开始启动线程池
		pool.start(4);
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 10000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(10000001, 20000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(20000001, 30000000));
		pool.submitTask(std::make_shared<MyTask>(1, 10000000));

		pool.submitTask(std::make_shared<MyTask>(10000001, 20000000));
		pool.submitTask(std::make_shared<MyTask>(10000001, 20000000));
		uLong sum1 = res1.get().cast_<uLong>();
		uLong sum2 = res2.get().cast_<uLong>();
		uLong sum3 = res3.get().cast_<uLong>();
	}
	
	
	//std::cout << sum1 + sum2 + sum3 << std::endl;
	//Master-Slavef线程模型
	//Master线程分解任务，然后各个slave线程分配任务
	//等待各个slave线程执行任务，返回结果
	//Master线程合并各个任务结果，输出
	/*pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());

	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());*/

	std::cout << "please enter" << std::endl;
	int c = std::getchar();
	
	
	return 0;
#endif
}