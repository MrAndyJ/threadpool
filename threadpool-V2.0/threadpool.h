#pragma once
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <functional>

class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	// 第三步，构造函数 让基类指针指向派生类对象 
	template<typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{}

	// 第四步，这个方法能把Any对象里存储的data数据提取出来
	template<typename T>
	T cast_()
	{
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch";
		}
		return pd->data_;
	}

private:
	// 定义基类
	class Base			// 1 定义一个基类指针
	{
	public:
		virtual ~Base() = default;
	};

	// 派生类：把派生类定义为类模板，在派生类中可以接收任意类型
	template<typename T>
	class Derive : public Base		// 2 第二步，将子类设计为模板，然后再继承基类
	{
	public:
		Derive(T data) : data_(data)
		{
			
		}
		T data_;   // 保存了任意其他类型 
	};

private:
	std::unique_ptr<Base> base_;   // 定义一个基类指针
};


// 实现一个信号量类 
class Semaphore
{
public:
	Semaphore(int limit = 0)
		:resLimit_(limit)
	{}
	~Semaphore() = default;

	// 获取一个信号量资源  p操作，-1 
	void wait() // 记忆：等着获取，获取到占用一个资源，--；
	{
		std::unique_lock<std::mutex> lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}

	// 增加一个信号量资源 V +1 
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}

private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;

};

class Task;

// 实现接收任务的返回值
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);

	~Result() = default;

	//  8   当任务执行完毕后，调用setVal,把任务返回值记录在Result中的Task中；
	void setVal(Any any)  // 谁调用这个 setVal呢？？？？
	{
					// 首先：存储task 的返回值，将task直接转移给Result中的any_对象，直接move将线程执行的task对象 资源转移
		any_ = std::move(any);
		sem_.post();			// 已经任务返回值，增加信号量资源, 激活客户任务
	}
	// 8 get方法，用户调用这个方法获取task返回值 
	Any get()
	{
		if (!isValid_)  //首先，判断 如果用户调用失败了，直接走，不用阻塞
		{
			throw "任务无效";
		}
		sem_.wait();	// 然后，阻塞，直到线程任务执行完毕后，线程调用post()方法 启动该方法。
		return std::move(any_);
	}
private:
	Any any_;						 // 存储任务的返回值
	Semaphore sem_;					 // 用户调用 get时候，使用该信号量阻塞，等任务执行完毕后，再继续执行
	std::shared_ptr<Task> task_;	 // 指向task_;
	std::atomic_bool isValid_;       // 判断该任务是否是一个有效的任务。
};


// 1 任务抽象基类，用于给客户提供统一接口
class Task
{
public:
	Task() :result_(nullptr) {}

	~Task() = default;

	// 该函数给Task 中 的 result 赋值 
	void setResult(Result* res)
	{
		result_ = res;
	}

	void exec()  // 线程执行函数调用这个方法 执行虚函数
	{
		if (result_ != nullptr)
		{
			//Any any = run();
			//result_->setVal(&any);
			result_->setVal(run());   // 调用该方法，将run() 的返回值any_ 保存在Result对象的any，并且是资源转移。
		}
			
	}
	virtual Any run() = 0;
private:
	Result* result_;   // 在Task中增加一个result来接任务的Any返回值。
};

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
	Thread(ThreadFunc func);

	~Thread();
	// 2 启动线程 
	void start();

	// 10 获取线程ID
	int getId() const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;  // 10  线程id 
};

// 1 线程池类型：
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	void setMode(PoolMode mode); // 1 设置线程池的工作模式

	void setTaskSizeThreshHold(int threshhode); // 2  设置task 任务队列的上限阈值，客户根据自己电脑配置来实现。

	void setThreadSizeThreshHold(int threshhold); // 2 10  设计线程池cached模式下线程阈值

	Result submitTask(std::shared_ptr<Task> sp); // 2 给线程池提交任务，传递一个shared_ptr 用于判断生命周期
	// 返回result 类型
	//  2 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency());

	// 2 禁止线程池对象拷贝构造
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;


private:
	 // 2 定义线程函数 
	void  threadFunc(int threadid);  // 该函数直接可以线程池对象的私有成员
	
	// 10. 检测pool 的运行状态 
	bool checkRunningState() const;
private:
	// std::vector<Thread *> threads_;				// 1 线程池用vector容器存放线程
	// std::vector<std::unique_ptr<Thread>> threads_;
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;  // 给每个thread一个id 

	int initThreadSize_;							// 1 初始化线程数量
	int threadSizeThreadHold_;						// 10. 线程数量上限
	std::atomic_int curThreadSize_;					// 10. 记录当前线程总数量
	std::atomic_int idleThreadSize_;				// 10. 记录空闲线程数量

	std::queue<std::shared_ptr<Task>> taskQue_;		// 1 任务队列
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



