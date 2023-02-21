
#include "threadpool.h"

#include <functional>
#include <thread>
#include <iostream>
#include <vector>
#include <memory>
const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THREADHOLD = 200;
const int THREAD_MAX_IDLE_TIME = 10; // ��λ����

// 2 �̳߳ع��� 
ThreadPool::ThreadPool()
	:initThreadSize_(0)							     // 2 ��ʼ���߳�����
	, taskSize_(0)									 // 2 ��ʼ������������
	,idleThreadSize_(0)								 // 10 �����߳�����
	,curThreadSize_(0)								 // 10 ��ǰ�߳�����
	, taskQueueMaxThreshHold_(TASK_MAX_THRESHHOLD)   // 2 ��ʼ��������е�����ֵ
	,threadSizeThreadHold_(THREAD_MAX_THREADHOLD)	 // 10. ���õ�ǰ�߳���ֵ  
	, poolMode_(PoolMode::MODE_FIXED)				 // 2 ��ʼ���̳߳�ģʽ
	,isPoolRunning_(false)							 // andy10 : 
{}
// 2 �̳߳���������
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;  // 11 

	// �����߳��е��߳�ʱ�����̺߳��̳߳��е��߳�
	// �̳߳����߳�������״̬������  && ����ִ������ 
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();  // ���� all ����˯���߳�
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });  // ����̳߳���������0���̳߳��������������������Դ������
}
  // 2 ���� task�������������ֵ
void ThreadPool::setTaskSizeThreshHold(int threadhold)
{
	taskQueueMaxThreshHold_ = threadhold;
}
void ThreadPool::setThreadSizeThreshHold(int threshhold)
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

// 2 �ⲿ�û� ������������������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)  // ���� sp ָ�룬ָ�� Task����
{
	//  4. ����1: ��ȡ�� 
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// 4  ����2���û��ύ����֮ǰ���Ȳ鿴��������Ƿ��п��࣬���û�п���͵ȴ�һ���ӣ�Ȼ�󷵻�ʧ��
	//if (taskQue_.size() >= taskQueueMaxThreshHold_)
	//{
	//	notFull_.wait(lock);
	//}
	// �޸ģ����û��ȴ�һ�룬������������񣬸��û���������ʧ��
	// ʹ��wait_for wait_until  �滻wait,ʵ�ּ����Ĺ���
	// ����wait��һֱ�ȴ�notify, wait_for������Ƶȴ�ʱ�䣬�����ʱ�����ٵȴ�
	// notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueueMaxThreshHold_; });
	// notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() < taskQueueMaxThreshHold_; });
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() < taskQueueMaxThreshHold_; }))
	{
		std::cout << "task queue is full, submit task fail" << std::endl;
		return Result(sp, false);   // ��Result��װtask , ������task����������
	}
	// 4 ����3������п��࣬������������������
	taskQue_.emplace(sp);
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
		auto ptr = std::make_unique<Thread>((std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1)));  // 
		// threads_.emplace_back(std::move(ptr));  // 10 vector�ڵײ㿽������һ�ݣ����� unique������������, ������move()
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();

		curThreadSize_++;
		idleThreadSize_++;
	}
	return Result(sp);
}

// 2 �����̳߳�  
// ����������̳߳��е��߳̾�Ҫȥ����
void ThreadPool::start(int initThreadSize)
{
	isPoolRunning_ = true;				// 10 �����̳߳�����״̬
	// 2 ��¼��ʼ�߳�����
	initThreadSize_ = initThreadSize;   // 10 ��ʼ���߳�����
	curThreadSize_ = initThreadSize;	// 10 ��ǰ�߳�����

	// 2 ���� ��ʼ�߳������������̶߳���
	for (int i = 0; i < initThreadSize_; i++)
	{
		// ����thread�̶߳���ʱ��ֱ��thread���󴫵�һ���̺߳���
		//thread  t1 = new Thread(std::bind(&ThreadPool::threadFunc,this))); 
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));  // ʹ�ú���������߳��ഫ��
		// ʹ�ú�������󶨺󣬾��൱��һ�������ˣ�������void�� function��������
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

// 2 �߳�ִ�к����� �߳�Ҫ�����������ȡ����: �̳߳��е�ÿ���̶߳�ִ�иú�����
void ThreadPool::threadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();  // 11 

	while (1)
	{
		std::shared_ptr<Task> task;
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
			task->exec();  // task �õ��˺���ִ�н��
		}
		idleThreadSize_++;  //	10 ִ����һ���̣߳��̳߳ؿ��߳�����++

		lastTime = std::chrono::high_resolution_clock().now();  // 11  �����߳�ִ��������������ʱ��
	}
}

// 10 ��⵱���߳�״̬
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}

void ThreadPool::setMode(PoolMode mode) // 1 �����̳߳صĹ���ģʽ
{
	if (checkRunningState())
	{
		return;
	}
	poolMode_ = mode;
}
//////////////////////////////////THread

// 3  �߳��๹��
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
///////////////////  Result��ķ���ʵ��

Result::Result(std::shared_ptr<Task> task, bool isValid)
	:isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}


//void Result::setVal(Any any)  // ˭������� setVal�أ�������
//{
//	// �洢task �ķ���ֵ �� ֱ��move ��Դת��
//	any_ = std::move(any);
//	sem_.post();  // �Ѿ����񷵻�ֵ�������ź�����Դ, ����ͻ�����
//}
//
//// 8 get�������û��������������ȡtask����ֵ 
//Any Result::get()
//{
//	if (!isValid_)  //���ȣ��ж� ����û�����ʧ���ˣ�ֱ���ߣ���������
//	{
//		throw "������Ч";
//	}
//	sem_.wait();	// Ȼ��������ֱ���߳�����ִ����Ϻ��̵߳���post()���� �����÷�����
//	return std::move(any_);
//}