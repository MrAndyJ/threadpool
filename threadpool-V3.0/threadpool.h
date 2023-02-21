#pragma once
#pragma once
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <future>

const int TASK_MAX_THRESHHOLD = 2; //  INT32_MAX;
const int THREAD_MAX_THREADHOLD = 200;
const int THREAD_MAX_IDLE_TIME = 10; // 单位：秒


// 1 线程池支持的模式：固定模式或动态模式
enum class PoolMode
{
	MODE_FIXED,
	MODE_CACHED
};

// 1 定义线程类型 
class Thread
{
public:
	// 2 
	using ThreadFunc = std::function<void(int)>;

	// 3 
	Thread(ThreadFunc func)
		:func_(func)
		, threadId_(generateId_++)
	{}

	~Thread() = default;
	// 2 启动线程 
	void start()
	{
		std::thread t1(func_, threadId_);
		t1.detach();
	}

	// 10 获取线程ID
	int getId() const
	{
		return threadId_;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;  // 10  线程id 
};
int Thread::generateId_ = 0;


// 1 线程池类型：
class ThreadPool
{
public:
	ThreadPool()
		:initThreadSize_(0)							     // 2 初始化线程数量
		, taskSize_(0)									 // 2 初始化任务数量，
		, idleThreadSize_(0)								 // 10 空闲线程数量
		, curThreadSize_(0)								 // 10 当前线程数量
		, taskQueueMaxThreshHold_(TASK_MAX_THRESHHOLD)   // 2 初始化任务队列的上限值
		, threadSizeThreadHold_(THREAD_MAX_THREADHOLD)	 // 10. 设置当前线程阈值  
		, poolMode_(PoolMode::MODE_FIXED)				 // 2 初始化线程池模式
		, isPoolRunning_(false)							 // andy10 : 
	{}
	~ThreadPool()
	{
		isPoolRunning_ = false;  // 11 

		// 析构线程中的线程时候：主线程和线程池中的线程
		// 线程池中线程有两种状态：阻塞  && 正在执行任务 
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();  // 唤醒 all 所有睡眠线程
		exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });  // 如果线程池数量等于0，线程池析构函数结果，所有资源被析构
	}

	void setMode(PoolMode mode)	// 1 设置线程池的工作模式
	{
		if (checkRunningState())
		{
			return;
		}
		poolMode_ = mode;
	}
	void setTaskSizeThreshHold(int threshhold) // 2  设置task 任务队列的上限阈值，客户根据自己电脑配置来实现。
	{
		taskQueueMaxThreshHold_ = threshhold;
	}

	void setThreadSizeThreshHold(int threshhold) // 2 10  设计线程池cached模式下线程阈值
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

	template<typename Func , typename...Args>
	auto submitTask(Func&& func, Args&&... args) // 2 给线程池提交任务，传递一个shared_ptr 用于判断生命周期
	{
		using RType = decltype(func(args...));		 // 获取任务返回值   // std::packaged_task<RType()>
		// std::packaged_task<RType()>是一个函数对象，使用bind 将行数和 参数绑定到该函数对象上
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

		/*
			 打包任务返回值 Result res = pool.submitTask(std::make_shared<MyTask>(1, 100));
			packaged_task<int(int, int)> task(sum1);  
			future<int> ret = task.get_future();    // 使用future 来接任务返回值
			task(1, 2);
		*/
		std::unique_lock<std::mutex> lock(taskQueMtx_);

 		if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() < taskQueueMaxThreshHold_; }))
		{
			std::cout << "task queue is full, submit task fail" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>([]()->RType {return RType(); });
			(*task)();
			return task->get_future();
		}
		// 4 步骤3：如果有空余，把任务放入任务队列中
		taskQue_.emplace([task]()->void {(*task)(); });
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

			auto ptr = std::make_unique<Thread>((std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)));  // 
			// threads_.emplace_back(std::move(ptr));  // 10 vector在底层拷贝构造一份，但是 unique不允许拷贝构造, 所以用move()
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			threads_[threadId]->start();

			curThreadSize_++;
			idleThreadSize_++;
		}
		return result;  // 使用result.get(); 


	}
	//  2 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency())
	{
		isPoolRunning_ = true;				// 10 设置线程池启动状态
		// 2 记录初始线程数量
		initThreadSize_ = initThreadSize;   // 10 初始化线程数量
		curThreadSize_ = initThreadSize;	// 10 当前线程数量

		// 2 根据 初始线程数量来创建线程对象
		for (int i = 0; i < initThreadSize_; i++)
		{
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
	// 2 禁止线程池对象拷贝构造
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// 2 定义线程函数 
	void  threadFunc(int threadid)  // 该函数直接可以线程池对象的私有成员
	{
		auto lastTime = std::chrono::high_resolution_clock().now();  // 11 

		while (1)
		{
			Task task;
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
				task(); // task 拿到了函数执行结果
			}
			idleThreadSize_++;  //	10 执行完一个线程，线程池空线程数量++

			lastTime = std::chrono::high_resolution_clock().now();  // 11  更新线程执行完任务所花的时间
		}
	}

	// 10. 检测pool 的运行状态 
	bool checkRunningState() const 
	{
		return isPoolRunning_;
	}
private:
	// std::vector<Thread *> threads_;				// 1 线程池用vector容器存放线程
	// std::vector<std::unique_ptr<Thread>> threads_;
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;  // 给每个thread一个id 

	int initThreadSize_;							// 1 初始化线程数量
	int threadSizeThreadHold_;						// 10. 线程数量上限
	std::atomic_int curThreadSize_;					// 10. 记录当前线程总数量
	std::atomic_int idleThreadSize_;				// 10. 记录空闲线程数量

	using Task = std::function<void()>;
	std::queue<Task> taskQue_;		// 1 任务队列
	std::atomic_int taskSize_;						// 1 记录当前任务数量，计数增加和减少，保证原子操作
	// std::atomic<int> taskSize_;
	size_t taskQueueMaxThreshHold_;					// 1 记录任务队列的上限值

	std::mutex taskQueMtx_;							// 1 保证任务队列的线程安全，一个任务只能一个线程操作。
	std::condition_variable notFull_;				// 1 外部资源在任务队列非满的时候，添加任务。
	std::condition_variable notEmpty_;				// 1 线程池中判断任务队列不空的时候，消费任务
	std::condition_variable exitCond_;				// 11.  等待线程资源全部回收

	PoolMode poolMode_;								//1 当前线程池工作模式
	std::atomic_bool isPoolRunning_;				// Andy10 : 表示当前线程池的启动状态，在多个线程中使用，设计为bool 
};



