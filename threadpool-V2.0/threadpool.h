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

	// �����������캯�� �û���ָ��ָ����������� 
	template<typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{}

	// ���Ĳ�����������ܰ�Any������洢��data������ȡ����
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
	// �������
	class Base			// 1 ����һ������ָ��
	{
	public:
		virtual ~Base() = default;
	};

	// �����ࣺ�������ඨ��Ϊ��ģ�壬���������п��Խ�����������
	template<typename T>
	class Derive : public Base		// 2 �ڶ��������������Ϊģ�壬Ȼ���ټ̳л���
	{
	public:
		Derive(T data) : data_(data)
		{
			
		}
		T data_;   // ������������������ 
	};

private:
	std::unique_ptr<Base> base_;   // ����һ������ָ��
};


// ʵ��һ���ź����� 
class Semaphore
{
public:
	Semaphore(int limit = 0)
		:resLimit_(limit)
	{}
	~Semaphore() = default;

	// ��ȡһ���ź�����Դ  p������-1 
	void wait() // ���䣺���Ż�ȡ����ȡ��ռ��һ����Դ��--��
	{
		std::unique_lock<std::mutex> lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}

	// ����һ���ź�����Դ V +1 
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

// ʵ�ֽ�������ķ���ֵ
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);

	~Result() = default;

	//  8   ������ִ����Ϻ󣬵���setVal,�����񷵻�ֵ��¼��Result�е�Task�У�
	void setVal(Any any)  // ˭������� setVal�أ�������
	{
					// ���ȣ��洢task �ķ���ֵ����taskֱ��ת�Ƹ�Result�е�any_����ֱ��move���߳�ִ�е�task���� ��Դת��
		any_ = std::move(any);
		sem_.post();			// �Ѿ����񷵻�ֵ�������ź�����Դ, ����ͻ�����
	}
	// 8 get�������û��������������ȡtask����ֵ 
	Any get()
	{
		if (!isValid_)  //���ȣ��ж� ����û�����ʧ���ˣ�ֱ���ߣ���������
		{
			throw "������Ч";
		}
		sem_.wait();	// Ȼ��������ֱ���߳�����ִ����Ϻ��̵߳���post()���� �����÷�����
		return std::move(any_);
	}
private:
	Any any_;						 // �洢����ķ���ֵ
	Semaphore sem_;					 // �û����� getʱ��ʹ�ø��ź���������������ִ����Ϻ��ټ���ִ��
	std::shared_ptr<Task> task_;	 // ָ��task_;
	std::atomic_bool isValid_;       // �жϸ������Ƿ���һ����Ч������
};


// 1 ���������࣬���ڸ��ͻ��ṩͳһ�ӿ�
class Task
{
public:
	Task() :result_(nullptr) {}

	~Task() = default;

	// �ú�����Task �� �� result ��ֵ 
	void setResult(Result* res)
	{
		result_ = res;
	}

	void exec()  // �߳�ִ�к�������������� ִ���麯��
	{
		if (result_ != nullptr)
		{
			//Any any = run();
			//result_->setVal(&any);
			result_->setVal(run());   // ���ø÷�������run() �ķ���ֵany_ ������Result�����any����������Դת�ơ�
		}
			
	}
	virtual Any run() = 0;
private:
	Result* result_;   // ��Task������һ��result���������Any����ֵ��
};

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
	Thread(ThreadFunc func);

	~Thread();
	// 2 �����߳� 
	void start();

	// 10 ��ȡ�߳�ID
	int getId() const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;  // 10  �߳�id 
};

// 1 �̳߳����ͣ�
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	void setMode(PoolMode mode); // 1 �����̳߳صĹ���ģʽ

	void setTaskSizeThreshHold(int threshhode); // 2  ����task ������е�������ֵ���ͻ������Լ�����������ʵ�֡�

	void setThreadSizeThreshHold(int threshhold); // 2 10  ����̳߳�cachedģʽ���߳���ֵ

	Result submitTask(std::shared_ptr<Task> sp); // 2 ���̳߳��ύ���񣬴���һ��shared_ptr �����ж���������
	// ����result ����
	//  2 �����̳߳�
	void start(int initThreadSize = std::thread::hardware_concurrency());

	// 2 ��ֹ�̳߳ض��󿽱�����
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;


private:
	 // 2 �����̺߳��� 
	void  threadFunc(int threadid);  // �ú���ֱ�ӿ����̳߳ض����˽�г�Ա
	
	// 10. ���pool ������״̬ 
	bool checkRunningState() const;
private:
	// std::vector<Thread *> threads_;				// 1 �̳߳���vector��������߳�
	// std::vector<std::unique_ptr<Thread>> threads_;
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;  // ��ÿ��threadһ��id 

	int initThreadSize_;							// 1 ��ʼ���߳�����
	int threadSizeThreadHold_;						// 10. �߳���������
	std::atomic_int curThreadSize_;					// 10. ��¼��ǰ�߳�������
	std::atomic_int idleThreadSize_;				// 10. ��¼�����߳�����

	std::queue<std::shared_ptr<Task>> taskQue_;		// 1 �������
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



