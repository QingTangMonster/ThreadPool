基本实现了线程池类、线程类、任务类。并测试调用了启动了线程池。

![1711267365605](E:\desktop\线程池项目\1\pic\1711267365605.png)

对于线程函数，因为要使用到线程池里面的资源，所以应当定义再线程池里面，但是最后调用因该是线程去调用，所以，因该在创建线程的时候，要把线程函数给到线程。**这是涉及到的知识点是成员函数使用绑定器。**

```cpp
class ThreadPool{
private:
	void ThreadFunc();// 线程函数
...线程池资源
}
class Thread
{
public:
	//线程函数对象类型
	using ThreadFunc = std::function<void()>;
	//线程构造函数
	...
private:
	ThreadFunc func_;
};
//创建线程
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
...
}
```

```cpp
//使用bind成员函数和this绑定在一起生产一个函数对象，调用Thread构造函数new一个Thread。
threads_.emplace_back(new Thread(std::bind(&ThreadPool::ThreadFunc, this)));	
```

