# 1.优化线程队列

原来：

```cpp
std::vector<Thread*> threads_;	//线程列表	线程是new出来的，用裸指针是需要delete的
```

优化后：

```cpp
//使用智能指针，避免手动释放资源，容器析构，元素析构，智能指针析构对应内存析构
std::vector<std::unique_ptr<Thread>> threads_;	
```

# 2.实现submitTask接口(生产任务)

**v1.0：**

```cpp
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//线程通信，等待任务队列有空余
	notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; });
	//如果有空余，把任务放入队列
	taskQue_.emplace(sp);
	taskSize_++;
	//因为新放了任务，任务队列肯定不控了，所以要notify notEmpty_,赶快分配执行任务
	notEmpty_.notify_all();
}
```

**v1.1：增加用户提交任务，最长不能阻塞超过1s，否则判断任务提交失败，返回功能(使用wait_for)**

```cpp
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//线程通信，等待任务队列有空余
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
```

# 3.实现threadFuc(消费任务)

**v1.0：**

```cpp
for (;;)
	{
		//获取锁
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		//等待环境变量notEmpty
		notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
		//如果不空，就从任务队列中取任务
		std::shared_ptr<Task> task = taskQue_.front();
		taskQue_.pop();
		taskSize_--;
		//当前线程负责执行任务
		task->run();			//基类指针指向派生类，发生多态
	}
```

这样写会有一个问题，就是当线程获得锁之后，会等到执行完run出作用域后在释放锁，就会出现其实只有一个线程在执行得现象，因为其他线程要等到到这个线程执行完才可能抢到锁。

**v1.1:添加了作用域，使线程取到任务后就释放锁，另外添加了取到任务后通知。**

```cpp
void ThreadPool::ThreadFunc()
{
	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			//获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			//等待环境变量notEmpty
			notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
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
```

# 4.怎么定义一个类型可以接受任意参数？

之前在Task基类里定义了一个虚函数virtual void run()=0，但是如果要获取返回值呢？

不能定义成模板，因为这里使已经定义成虚函数了。所以需要一个可以接受任意参数的类。

思路：用一个基类指针指向一个派生类，派生类用模板实现，数据存在派生类里面。cast_方法将获取派生类的数据。

涉及到的知识点：

1. 基类指针指向派生类。注意要实现成虚析构，不让析构时不会析构派生类对象。
2. 模板。
3. dynamic_cast。
4. 模板不能声明写到头文件，定义写到原件。

```cpp
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
	Any(T data):base_(std::make_unique<Derive<T>>(data))			//让Any接受任意数据
	{}
	class Base
	{
	public:
		virtual ~Base() = default;
	};
	template<typename T>					//从Any提取数据
	T cast_()
	{
		//基类指针转成派生类指针
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_get());  //智能指针有get方法获取裸指针
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;
	}
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data):data_(data) 
		{}
		T data_;
	};
private:
	//定义一个基类得指针
	std::unique_ptr<Base> base_;
};
```

# 5.获取异步返回值

用户调用get获取任务的返回值，但是任务是另一线程里执行的，在任务执行完，用户调用get应该阻塞起来，需要线程通信。

线程通信：条件变量和信号量。一般简单的线程通信用信号量就行，所以这里实现一个信号量。

```cpp
class Semaphore
{
public:
    Semaphore(int limit = 0) :resLimit_(limit) {}
    ~Semaphore() = default;
    //获取一个信号量
    void wait()
    {
		std::unique_mutex<std::mutex> lock(mtx_);
        cond_wait(lock, [&]->bool {return resLimit_>0;});
        reslimit_--;
    }
    
    //增加一个信号量
    void post()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++；
        cond_notify_all();
    }
private:
    std::mutex mtx_;
    std::condition_variable cond_;
    int resLimit_;
}
```

**到这里实现一个Any类用于任意类型，一个Semaphore用于线程同行。基于这两个类实现一个Result类。对于Result类设计**

1. Any成员变量，用来接受任意类型。
2. Semaphore成员变量，用于线程通信。
3. isValid，用于检查返回是否有效，因为submit失败就不需要get了。
4. setVal方法，给threadFun调用设置Result的 Any
5. get方法，用户调用获取Any。

```cpp
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	//问题一：setVal,获取任务执行完返回值,threadFunc调用
	void setVal(Any any);
	//问题二：get,用户调用，获取any
	Any get();
private:
	Any any_;   //存储任务的返回值
	Semaphore sem_;    //线程通信信号量
	//Task task_;		不允许使用抽象类型Task
	std::shared_ptr<Task> task_; //task
	std::atomic_bool isValid_;	//返回值是否有效
};
```

Result和task的关系。

```cpp
class Task
{
public:
	...
private:
	Result* result_;			//。。。。。。。。。。。。。强智能指针
};

class Result
{
...
	std::shared_ptr<Task> task_; //task
...
};
Result::Result(std::shared_ptr<Task> task, bool isValid)
	:task_(task)					//将task给到result
	,isValid_(isValid)
{
	task_->setResult(this);			//将result给到task
}
```

# 6.实现cached模式

**1.用户设置cahed模式**

```cpp
pool.setMode(PoolMode::MODE_CACHED);
```

**2.用户在提交任务时，要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来。**

```cpp
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
	
/***********************************************************************************************************************************************************************************************************************
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>cached模式要自动扩充thread数量<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
***********************************************************************************************************/
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
```

**3.在theadFunc中，由cached模式创建出来的线程如果超过60s每任务，应该要回收。**

```cpp
void ThreadPool::ThreadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			//获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "tid" << std::this_thread::get_id() << "尝试获取任务..." << std::endl;

			//cached模式下，有可能创建了很多线程，但是空闲60s，应该要回收
			//结束回收（超过initThreadSize_数量的线程要进行回收）
			/***********************************************************************************************************************************************************************************************************************
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>cached模式要扩充的thread空闲超时回收<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
***********************************************************************************************************/
            if (poolMode_ == PoolMode::MODE_CACHED)
			{
				//每一秒钟返回一次    怎么区分超时返回还是任务执行返回
				while (taskQue_.size() == 0)
				{
					//条件变量超时返回
					if(std::cv_status::timeout==notEmpty_.wait_for(lock, std::chrono::seconds(1)))
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
			}
			else
			{
				//等待环境变量notEmpty
				notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
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
}
```

**线程池的回收**

**析构**：

```cpp
ThreadPool::~ThreadPool() 
{
	isPoolRunning_ = false;
	notEmpty_.notify_all();
	//等待线程池里面所有的线程返回(线程的状态：阻塞或者运行)
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}
```

ThreadFunc：结束时线程有两种状态，阻塞和正在运行。

```cpp
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
		while (taskQue_.size() == 0)
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
/************>>>>>>>>>>>>>>>线程池要结束，回收线程资源<<<<<<<<<<<<<***************************************************************************************************************************************/
			//线程池要结束，回收线程资源
			if (!isPoolRunning_)
			{
				threads_.erase(threadid);
				std::cout << "threadid：" << std::this_thread::get_id() << "exit!" << std::endl;
				exitCond_.notify_all();
				return;
			}
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
	//线程池要结束回收线程资源
	threads_.erase(threadid);
	exitCond_.notify_all();
	std::cout << "threadid：" << std::this_thread::get_id() << "exit!" << std::endl;
}
```

# 7.死锁问题

**原来析构：**

```cpp
ThreadPool::~ThreadPool() 
{
	isPoolRunning_ = false;
	notEmpty_.notify_all();
	//等待线程池里面所有的线程返回(线程的状态：阻塞或者运行)
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}
```

**原来ThreadFunc**：

```cpp
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
		while (taskQue_.size() == 0)
		{

			if (poolMode_ == PoolMode::MODE_CACHED)
			{
				if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
				{
					auto now = std::chrono::high_resolution_clock().now();
					auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
					if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
					{
/**********************************************************************************************************************/
/******************************************Cached模式超时回收************************************************************/         /**********************************************************************************************************************/
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
            /**********************************************************************************************************************/
/******************************************taskQue_.size() == 0回收******************************************************/ /**********************************************************************************************************************/
			//线程池要结束，回收线程资源
			if (!isPoolRunning_)
			{
				threads_.erase(threadid);
				std::cout << "threadid：" << std::this_thread::get_id() << "exit!" << std::endl;
				exitCond_.notify_all();
				return;
			}
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
    /**********************************************************************************************************************/
/******************************************线程池结束回收******************************************************/ /**********************************************************************************************************************/
	//线程池要结束，回收线程资源
	threads_.erase(threadid);
	exitCond_.notify_all();
	std::cout << "threadid：" << std::this_thread::get_id() << "exit!" << std::endl;
}
```

**死锁问题：![1711453399214](E:\desktop\线程池项目\2\pic\1711453399214.png)**

**程序有的时候会出现有的线程不能回收，而且是一个非必现问题。**

处理思路：

换到Linux下使用gbd调试：

```shell
g++ main.cpp threadpool.cpp -lpthread -g -std=c++17
```

![1711453776906](E:\desktop\线程池项目\2\pic\1711453776906.jpg)

多次运行会出现死锁问题(非必现)。

使用gbd调试

```shell
sudo gdb attach
```

```shell
alientek@ubuntu:~$ ps -a
    PID TTY          TIME CMD
  19076 pts/0    00:00:00 a.out
  19107 pts/1    00:00:00 ps
alientek@ubuntu:~$ sudo gdb attach 19076
[sudo] alientek 的密码： 
GNU gdb (Ubuntu 9.2-0ubuntu1~20.04.1) 9.2
Copyright (C) 2020 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word"...
attach: 没有那个文件或目录.
Attaching to process 19076
[New LWP 19079]
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
futex_wait_cancelable (private=<optimized out>, expected=0, 
    futex_word=0x7ffe39c3f3ac) at ../sysdeps/nptl/futex-internal.h:183
183	../sysdeps/nptl/futex-internal.h: 没有那个文件或目录.
(gdb)
```

```shell
info threads    //查看线程信息
thread 2		//切换线程
bt				//查看栈信息
f 5				//查看栈帧
```

![1711454203379](E:\desktop\线程池项目\2\pic\1711454203379.jpg)

可以看到程序阻塞在了exitCond_wait。

分析：

![1711454631980](E:\desktop\线程池项目\2\pic\1711454631980.jpg)

原来只考虑了两个状态，阻塞和任务正在执行。但是程序有可能出院状态3，也就是状态3要和pool线程一起抢锁。

1. pool线程先抢到锁，notify已近执行过了，然后wait，ThreadFunc再抢到锁去wait，没有通知他，也在wait，所以死锁。
2. ThreadFunc先抢到锁去wait，pool线程获得锁然后wait，不回去通知，所以死锁。

解决方法：

```cpp
ThreadPool::~ThreadPool() 
{
	isPoolRunning_ = false;
	//等待线程池里面所有的线程返回(线程的状态：阻塞或者运行)
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}
```

![1711454930867](E:\desktop\线程池项目\2\pic\1711454930867.jpg)

| 在设计多线程同步时，注意死锁问题，考虑程序状态要完整。出现死锁可使用gbd定位到死锁位置。 |
| ------------------------------------------------------------ |

# 8.动态库编译

```shell
g++ -fPIC -shared threadpool.cpp -o libtdpool.so -std=c++17
# usr/local/lib/		.so路径
# /usr/local/include/	.include路径
```

```shell
g++ main.cpp -std=c++17 -ltdpool -lpthread  #重新编译
```

# 9.跨平台出现问题（在Linux上运行又出现死锁问题）

windows多少运行正常，在Linux上运行又出现死锁问题。

![1711456319762](E:\desktop\线程池项目\2\pic\1711456319762.png)



![1711458512902](E:\desktop\线程池项目\2\pic\1711458512902.jpg)

分析原因：

Result出作用域要析构，里面的Semaphore也要析构，里面的condition_variable

```cpp
//在Windos平台
~condition_variable() noexcept {
     _Cnd_destroy_in_situ(_Mycnd());
 }
//Linux平套
~condition_variable() noexcept;
```

submit提交的任务还有要运行一段时间，然后要增加一个信号量，但这时候Result已经出作用域析构了，但是Linux里没有对条件变量析构做操作，导致这里使用已经析构的变量阻塞。

```cpp
//获取一个信号量资源
void wait()
{
	std::unique_lock<std::mutex> lock(mtx_);
	cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
	resLimit_--;
}
//增加一个信号源资源
void post()
{
	std::unique_lock<std::mutex> lock(mtx_);
	resLimit_++;
	cond_.notify_all();
}
```

```cpp
//修改Semaphore,添加isExit_变量
class Semaphore
{
public:
	Semaphore(int limit = 0) 
		:resLimit_(limit)
		,isExit_(false)
	{}
	~Semaphore() = default;
	//获取一个信号量资源
	void wait()
	{
		if (isExit_)	return;
		std::unique_lock<std::mutex> lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
	//增加一个信号源资源
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

```

[【C++11 多线程】future与promise（八） - fengMisaka - 博客园 (cnblogs.com)](https://www.cnblogs.com/linuxAndMcu/p/14577275.html)

# 10.最终优化线程池

1. 前面为了实现一个可以接受任意参数的的类型实现了一个Any类型，为了实现异步获取线程的返回值又设计了semaphore类，并将semaphore和Any封装在Result类中，submitTask返回将Result，并将这个Result的指针给到task的成员变量，这样在异步线程执行完setResult的value后，Result再调用get获取返回值，这样就成功拿到了异步线程的返回值。
2. 另外，为了执行任意函数，我们使用了基类指针指向派生类指针，在运行是发生多态，对于函数参数，是派生类携带的。

真对上面两个问题有没有更简单的方法实现呢？C++11新特性future类和可变参模板。

优化submitTask：

```cpp
template<typename Func, typename...Args>
auto submitTask(Func && func, Args... &&n args) ->std:future<decltype(func(args...))>
{
    using RType = decltype(func(args...));
    auto task = std::make_shared<packaged_task<RType()>>(std::bind(std::forwar<Func>(func), std::forwar<Args>(args...));
   	std::future<RType> result = task->get_future();
   ...
   ...
}
```

1. 折叠原理、万能引用、完美转发：减少拷贝，提高效率，既可以接受左值，又可以接受右值，还可以保留其左右值的属性。
2. future类，packaged_task类提供异步通信机制。
