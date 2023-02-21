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
const int THREAD_MAX_IDLE_TIME = 10; // ��λ����


// 1 �̳߳�֧�ֵ�ģʽ���̶�ģʽ��̬ģʽ
enum class PoolMode
{
	MODE_FIXED,
	MODE_CACHED
};

// 1 �����߳����� 
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
	// 2 �����߳� 
	void start()
	{
		std::thread t1(func_, threadId_);
		t1.detach();
	}

	// 10 ��ȡ�߳�ID
	int getId() const
	{
		return threadId_;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;  // 10  �߳�id 
};
int Thread::generateId_ = 0;


// 1 �̳߳����ͣ�
class ThreadPool
{
public:
	ThreadPool()
		:initThreadSize_(0)							     // 2 ��ʼ���߳�����
		, taskSize_(0)									 // 2 ��ʼ������������
		, idleThreadSize_(0)								 // 10 �����߳�����
		, curThreadSize_(0)								 // 10 ��ǰ�߳�����
		, taskQueueMaxThreshHold_(TASK_MAX_THRESHHOLD)   // 2 ��ʼ��������е�����ֵ
		, threadSizeThreadHold_(THREAD_MAX_THREADHOLD)	 // 10. ���õ�ǰ�߳���ֵ  
		, poolMode_(PoolMode::MODE_FIXED)				 // 2 ��ʼ���̳߳�ģʽ
		, isPoolRunning_(false)							 // andy10 : 
	{}
	~ThreadPool()
	{
		isPoolRunning_ = false;  // 11 

		// �����߳��е��߳�ʱ�����̺߳��̳߳��е��߳�
		// �̳߳����߳�������״̬������  && ����ִ������ 
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();  // ���� all ����˯���߳�
		exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });  // ����̳߳���������0���̳߳��������������������Դ������
	}

	void setMode(PoolMode mode)	// 1 �����̳߳صĹ���ģʽ
	{
		if (checkRunningState())
		{
			return;
		}
		poolMode_ = mode;
	}
	void setTaskSizeThreshHold(int threshhold) // 2  ����task ������е�������ֵ���ͻ������Լ�����������ʵ�֡�
	{
		taskQueueMaxThreshHold_ = threshhold;
	}

	void setThreadSizeThreshHold(int threshhold) // 2 10  ����̳߳�cachedģʽ���߳���ֵ
	{
		if (checkRunningState())// Andy10. ����߳��Ѿ����� , �������� 
		{
			return;
		}
		if (poolMode_ == PoolMode::MODE_CACHED)
		{
			threadSizeThreadHold_ = threshhold;
		}

	}

	template<typename Func , typename...Args>
	auto submitTask(Func&& func, Args&&... args) // 2 ���̳߳��ύ���񣬴���һ��shared_ptr �����ж���������
	{
		using RType = decltype(func(args...));		 // ��ȡ���񷵻�ֵ   // std::packaged_task<RType()>
		// std::packaged_task<RType()>��һ����������ʹ��bind �������� �����󶨵��ú���������
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

		/*
			 ������񷵻�ֵ Result res = pool.submitTask(std::make_shared<MyTask>(1, 100));
			packaged_task<int(int, int)> task(sum1);  
			future<int> ret = task.get_future();    // ʹ��future �������񷵻�ֵ
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
		// 4 ����3������п��࣬������������������
		taskQue_.emplace([task]()->void {(*task)(); });
		taskSize_++;

		// 4 ����4����Ϊ�·�������������п϶����գ�֪ͨ�̸߳Ͽ��������
		notEmpty_.notify_all();

		// 10 ����2 ��Ҫ�������������Ϳ����߳��������ж��Ƿ���Ҫ�����µ��̳߳���������
		// cachedģʽ ����ȽϽ�����������С�����������Ҫ�������������������̣߳�
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_				// ��������������� ����  �����߳�����
			&& curThreadSize_ < threadSizeThreadHold_)   // ��ǰ�߳�С�� �߳������ֵ
		{
			std::cout << ">>> create new thread..." << std::endl;

			auto ptr = std::make_unique<Thread>((std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)));  // 
			// threads_.emplace_back(std::move(ptr));  // 10 vector�ڵײ㿽������һ�ݣ����� unique������������, ������move()
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			threads_[threadId]->start();

			curThreadSize_++;
			idleThreadSize_++;
		}
		return result;  // ʹ��result.get(); 


	}
	//  2 �����̳߳�
	void start(int initThreadSize = std::thread::hardware_concurrency())
	{
		isPoolRunning_ = true;				// 10 �����̳߳�����״̬
		// 2 ��¼��ʼ�߳�����
		initThreadSize_ = initThreadSize;   // 10 ��ʼ���߳�����
		curThreadSize_ = initThreadSize;	// 10 ��ǰ�߳�����

		// 2 ���� ��ʼ�߳������������̶߳���
		for (int i = 0; i < initThreadSize_; i++)
		{
			// ����1������Ĺ���ʹ������ָ��������
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));  // ���󣬳���������ɾ���ĺ���
			// threads_.emplace_back(std::move(ptr));  // vector�ڵײ㿽������һ�ݣ����� unique������������
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
		}

		// 2 �����߳�    �� ����initThreadSize_���߳� 
		for (int i = 0; i < initThreadSize_; i++)
		{
			threads_[i]->start();
			idleThreadSize_++;   // ��¼�����߳��������տ�ʼ�������̶߳��ǿ��е�
			// threads_[i]->start();   // �����߳���Ҫ��һ���̺߳�����
			// �������е� ʵ���ڶ�ThreadTool���У��̺߳��������������أ� ������private�С�
		}
	}
	// 2 ��ֹ�̳߳ض��󿽱�����
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// 2 �����̺߳��� 
	void  threadFunc(int threadid)  // �ú���ֱ�ӿ����̳߳ض����˽�г�Ա
	{
		auto lastTime = std::chrono::high_resolution_clock().now();  // 11 

		while (1)
		{
			Task task;
			{
				// �Ȼ�ȡ�� 
				std::unique_lock<std::mutex> lock(taskQueMtx_);

				std::cout << "tid:" << std::this_thread::get_id() << "  ���Ի�ȡ����/..." << std::endl;
				// 10.  ����3�� ��cached ģʽ�£��п����Ѿ������˺ܶ��̣߳����ǿ���ʱ�䳬��60s��Ӧ�ðѶ����������
				// ����initTHreadnum �󣬻��յ�
				// ��ǰʱ�� - ��һ���߳�ִ�е�ʱ�� >  60S
				while (taskQue_.size() == 0)  // �����������ִ�����
				{
					// cacheģʽ�£��Զ������߳�
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						// ÿ 1S����һ�Σ���ô���ֳ�ʱ���أ�������������ִ�з����أ�
							// �жϵ�һ�볬ʱ��
						if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
							{
								// ��ʼ���յ�ǰ�߳� 
								// ��¼�߳���������ز���
								// ���̶߳�����߳��б��л��գ�û�취ƥ�� threadFuncû�취ȷ����Ӧ��thread����
								// threadid = > thread ����= �� ��������ɾ��
								threads_.erase(threadid);
								curThreadSize_--;
								idleThreadSize_--;

								std::cout << "���յ��߳�id : threadid ::" << std::this_thread::get_id() << std::endl;
								return;
							}
						}
					}
					else	// fixedģʽ �� ���в��� 
					{
						notEmpty_.wait(lock);    // �ȴ� notEmpty.notifty(); ���������֮�󣬽���while()�жϣ��������taskQue_.size()����0��ֱ��������ǰ�̡߳�
					}
					if (!isPoolRunning_)  // ����̳߳��Ѿ�������
					{
						threads_.erase(threadid);
						std::cout << "threadid ������ " << std::this_thread::get_id() << "exit " << std::endl;
						exitCond_.notify_all();    // �����һ���߳�ִ����ϣ�exitCond_ ֪ͨ��������
						return;    // �������߳�
					}

				}
				// һֱ�ȴ�notEmpty ��������Ϊ��
				//notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
				idleThreadSize_--;  // 10 ���������󣬿����߳� --��

				std::cout << "tid:" << std::this_thread::get_id() << "��ȡ����ɹ�/..." << std::endl;
				// �����������ȡһ���������
				task = taskQue_.front();
				taskQue_.pop();
				taskSize_--;    // ����ִ����һ������󣬾ͻᱻ������

				// ������������Ȼ������ֱ��֪ͨ�����߳�ִ�� 
				if (taskQue_.size() > 0)
				{
					notEmpty_.notify_all();
				}

				// ȡ��һ����֪ͨ�ͻ���ǰ��������п��У����Լ����ύ����
				notFull_.notify_all();
			}

			// ��ǰ�߳�ִ���������
			if (task != nullptr)
			{
				// �������£�����ִ������Ȼ���ٽ�����Ľ��Task ��Դת�Ƹ�Result�е�Task 
				task(); // task �õ��˺���ִ�н��
			}
			idleThreadSize_++;  //	10 ִ����һ���̣߳��̳߳ؿ��߳�����++

			lastTime = std::chrono::high_resolution_clock().now();  // 11  �����߳�ִ��������������ʱ��
		}
	}

	// 10. ���pool ������״̬ 
	bool checkRunningState() const 
	{
		return isPoolRunning_;
	}
private:
	// std::vector<Thread *> threads_;				// 1 �̳߳���vector��������߳�
	// std::vector<std::unique_ptr<Thread>> threads_;
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;  // ��ÿ��threadһ��id 

	int initThreadSize_;							// 1 ��ʼ���߳�����
	int threadSizeThreadHold_;						// 10. �߳���������
	std::atomic_int curThreadSize_;					// 10. ��¼��ǰ�߳�������
	std::atomic_int idleThreadSize_;				// 10. ��¼�����߳�����

	using Task = std::function<void()>;
	std::queue<Task> taskQue_;		// 1 �������
	std::atomic_int taskSize_;						// 1 ��¼��ǰ�����������������Ӻͼ��٣���֤ԭ�Ӳ���
	// std::atomic<int> taskSize_;
	size_t taskQueueMaxThreshHold_;					// 1 ��¼������е�����ֵ

	std::mutex taskQueMtx_;							// 1 ��֤������е��̰߳�ȫ��һ������ֻ��һ���̲߳�����
	std::condition_variable notFull_;				// 1 �ⲿ��Դ��������з�����ʱ���������
	std::condition_variable notEmpty_;				// 1 �̳߳����ж�������в��յ�ʱ����������
	std::condition_variable exitCond_;				// 11.  �ȴ��߳���Դȫ������

	PoolMode poolMode_;								//1 ��ǰ�̳߳ع���ģʽ
	std::atomic_bool isPoolRunning_;				// Andy10 : ��ʾ��ǰ�̳߳ص�����״̬���ڶ���߳���ʹ�ã����Ϊbool 
};



