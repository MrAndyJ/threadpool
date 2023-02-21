
#include "threadpool.h"

#include <functional>
#include <thread>
#include <iostream>
#include <vector>
#include <memory>
const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THREADHOLD = 200;
const int THREAD_MAX_IDLE_TIME = 10; // 单位：秒

// 2 线程池构造 
ThreadPool::ThreadPool()
	:initThreadSize_(0)							     // 2 初始化线程数量
	, taskSize_(0)									 // 2 初始化任务数量，
	,idleThreadSize_(0)								 // 10 空闲线程数量
	,curThreadSize_(0)								 // 10 当前线程数量
	, taskQueueMaxThreshHold_(TASK_MAX_THRESHHOLD)   // 2 初始化任务队列的上限值
	,threadSizeThreadHold_(THREAD_MAX_THREADHOLD)	 // 10. 设置当前线程阈值  
	, poolMode_(PoolMode::MODE_FIXED)				 // 2 初始化线程池模式
	,isPoolRunning_(false)							 // andy10 : 
{}
// 2 线程池析构函数
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;  // 11 

	// 析构线程中的线程时候：主线程和线程池中的线程
	// 线程池中线程有两种状态：阻塞  && 正在执行任务 
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();  // 唤醒 all 所有睡眠线程
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });  // 如果线程池数量等于0，线程池析构函数结果，所有资源被析构
}
  // 2 设置 task任务队列上限阈值
void ThreadPool::setTaskSizeThreshHold(int threadhold)
{
	taskQueueMaxThreshHold_ = threadhold;
}
void ThreadPool::setThreadSizeThreshHold(int threshhold)
{
	if (checkRunningState())// Andy10. 如果线程已经启动 , 不能设置 
	{
		return;
	}
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreadHold_ = threshhold;
	}

}

// 2 外部用户 向任务队列中添加任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)  // 父类 sp 指针，指向 Task对象
{
	//  4. 步骤1: 获取锁 
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// 4  步骤2：用户提交任务之前，先查看任务队列是否有空余，如果没有空余就等待一秒钟，然后返回失败
	//if (taskQue_.size() >= taskQueueMaxThreshHold_)
	//{
	//	notFull_.wait(lock);
	//}
	// 修改：让用户等待一秒，如果还是满任务，给用户返回任务失败
	// 使用wait_for wait_until  替换wait,实现加锁的功能
	// 区别：wait会一直等待notify, wait_for可以设计等待时间，如果超时，不再等待
	// notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueueMaxThreshHold_; });
	// notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() < taskQueueMaxThreshHold_; });
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() < taskQueueMaxThreshHold_; }))
	{
		std::cout << "task queue is full, submit task fail" << std::endl;
		return Result(sp, false);   // 用Result包装task , 能延续task的声明周期
	}
	// 4 步骤3：如果有空余，把任务放入任务队列中
	taskQue_.emplace(sp);
	taskSize_++;

	// 4 步骤4：因为新放了任务，任务队列肯定不空，通知线程赶快分配任务。
	notEmpty_.notify_all(); 

	// 10 问题2 需要根据任务数量和空闲线程数量，判断是否需要创建新的线程出来？？？
	// cached模式 任务比较紧急，场景：小而快的任务，需要根据任务数量来创建线程；
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_				// 任务队列任务数量 大于  空闲线程数量
		&& curThreadSize_ < threadSizeThreadHold_)   // 当前线程小于 线程最大阈值
	{
		std::cout << ">>> create new thread..." << std::endl;
		auto ptr = std::make_unique<Thread>((std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1)));  // 
		// threads_.emplace_back(std::move(ptr));  // 10 vector在底层拷贝构造一份，但是 unique不允许拷贝构造, 所以用move()
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();

		curThreadSize_++;
		idleThreadSize_++;
	}
	return Result(sp);
}

// 2 开启线程池  
// 如果有任务，线程池中的线程就要去消化
void ThreadPool::start(int initThreadSize)
{
	isPoolRunning_ = true;				// 10 设置线程池启动状态
	// 2 记录初始线程数量
	initThreadSize_ = initThreadSize;   // 10 初始化线程数量
	curThreadSize_ = initThreadSize;	// 10 当前线程数量

	// 2 根据 初始线程数量来创建线程对象
	for (int i = 0; i < initThreadSize_; i++)
	{
		// 创建thread线程对象时候，直接thread对象传递一个线程函数
		//thread  t1 = new Thread(std::bind(&ThreadPool::threadFunc,this))); 
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));  // 使用函数对象给线程类传递
		// 使用函数对象绑定后，就相当于一个整体了，类型是void的 function函数对象。
		// 问题1：上面的功能使用智能指针来操作
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));  // 错误，尝试引用已删除的函数
		// threads_.emplace_back(std::move(ptr));  // vector在底层拷贝构造一份，但是 unique不允许拷贝构造
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}															

	// 2 启动线程    ： 启动initThreadSize_个线程 
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
		idleThreadSize_++;   // 记录空闲线程数量，刚开始创建的线程都是空闲的
		// threads_[i]->start();   // 启动线程需要有一个线程函数；
		// 现在所有的 实现在都ThreadTool类中，线程函数创建在哪里呢？ 定义在private中。
	}
}

// 2 线程执行函数： 线程要从任务队列中取任务: 线程池中的每个线程都执行该函数。
void ThreadPool::threadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();  // 11 

	while (1)
	{
		std::shared_ptr<Task> task;
		{
			// 先获取锁 
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id() << "  尝试获取任务/..." << std::endl;
			// 10.  问题3： 在cached 模式下，有可能已经创建了很多线程，但是空闲时间超过60s，应该把多余结束掉？
			// 超过initTHreadnum 后，回收掉
			// 当前时间 - 上一次线程执行的时间 >  60S
			while (taskQue_.size() == 0)  // 如果有任务，先执行完成
			{

				// cache模式下，自动回收线程
				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					// 每 1S返回一次，怎么区分超时返回？还是有任务在执行返回呢？
						// 判断等一秒超时？
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{
							// 开始回收当前线程 
							// 记录线程数量的相关参数
							// 把线程对象从线程列表中回收？没办法匹配 threadFunc没办法确定对应的thread对象
							// threadid = > thread 对象，= 》 从容器中删除
							threads_.erase(threadid);
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "回收的线程id : threadid ::" << std::this_thread::get_id() << std::endl;
							return;
						}
					}
				}
				else	// fixed模式 ： 队列不空 
				{ 
					notEmpty_.wait(lock);    // 等待 notEmpty.notifty(); 如果被唤醒之后，进行while()判断，如果发现taskQue_.size()等于0，直接析构当前线程。
				}
				if (!isPoolRunning_)  // 如果线程池已经析构；
				{
					threads_.erase(threadid);
					std::cout << "threadid 被析构 " << std::this_thread::get_id() << "exit " << std::endl;
					exitCond_.notify_all();    // 当最后一个线程执行完毕，exitCond_ 通知析构函数
					return;    // 结束该线程
				}

			}
			// 一直等待notEmpty 条件，不为空
			//notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
			idleThreadSize_--;  // 10 任务起来后，空闲线程 --；

			std::cout << "tid:" << std::this_thread::get_id() << "获取任务成功/..." << std::endl;
			// 从任务队列中取一个任务出来
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;    // 这里执行完一个任务后，就会被析构掉

			// 如果任务队列依然有任务，直接通知其他线程执行 
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}

			// 取出一个后，通知客户当前任务队列有空闲，可以继续提交任务
			notFull_.notify_all();
		}

		// 当前线程执行这个任务
		if (task != nullptr)
		{
			// 做两件事：首先执行任务，然后再将任务的结果Task 资源转移给Result中的Task 
			task->exec();  // task 拿到了函数执行结果
		}
		idleThreadSize_++;  //	10 执行完一个线程，线程池空线程数量++

		lastTime = std::chrono::high_resolution_clock().now();  // 11  更新线程执行完任务所花的时间
	}
}

// 10 检测当先线程状态
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}

void ThreadPool::setMode(PoolMode mode) // 1 设置线程池的工作模式
{
	if (checkRunningState())
	{
		return;
	}
	poolMode_ = mode;
}
//////////////////////////////////THread

// 3  线程类构造
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)
{}

Thread::~Thread() {}

void Thread::start()
{
	std::thread t1(func_,threadId_);
	t1.detach();
}

int Thread::generateId_ = 0;

int Thread::getId() const
{
	return threadId_;
}
///////////////////  Result类的方法实现

Result::Result(std::shared_ptr<Task> task, bool isValid)
	:isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}


//void Result::setVal(Any any)  // 谁调用这个 setVal呢？？？？
//{
//	// 存储task 的返回值 ： 直接move 资源转移
//	any_ = std::move(any);
//	sem_.post();  // 已经任务返回值，增加信号量资源, 激活客户任务
//}
//
//// 8 get方法，用户调用这个方法获取task返回值 
//Any Result::get()
//{
//	if (!isValid_)  //首先，判断 如果用户调用失败了，直接走，不用阻塞
//	{
//		throw "任务无效";
//	}
//	sem_.wait();	// 然后，阻塞，直到线程任务执行完毕后，线程调用post()方法 启动该方法。
//	return std::move(any_);
//}