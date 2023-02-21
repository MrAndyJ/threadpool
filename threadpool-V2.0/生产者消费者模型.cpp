#include <iostream>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <thread>
#include <queue>
using namespace std;

mutex mtx;
condition_variable cond;

#if 0

class Queue
{
public:
	void task1(int i)
	{

		unique_lock<mutex> lock(mtx);
		while (!que.empty())
		{
			cond.wait(lock);
		}
		que.push(i);
		cout << "生产者生产一个产品: " << i << endl;
		cond.notify_all();
	}

	// 消费者
	void task2()
	{
		unique_lock<mutex> lock(mtx);
		while (que.empty())
		{
			cond.wait(lock);
		}
		int a = que.front();
		cout << "消费者消费了一个商品" <<a<< endl;
		que.pop();
		cond.notify_all();
	}

private:
	queue<int> que;
};

void producer(Queue* que)
{
	for (int i = 0; i < 10; ++i)
	{
		que->task1(i);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void consumer(Queue* que)
{
	for (int i = 0; i < 10; ++i)
	{
		que->task2();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}


int main1()
{
	cout << "主线程执行" << endl;
	Queue q;
	thread t1(producer,&q);
	thread t2(consumer,&q);
	//
	t1.join();
	t2.join();
	// system("pause");
	return 1;
}



#endif 


/*
C++11 提供的emplace方法
*/

class Base
{
public:
	virtual ~Base() = default;
};


class Derive : public Base
{
public:
	Derive(int c = 10) :a(c) {}
	int a;
};

class Any1
{
public:
	Any1() = default;
	~Any1() = default;
	Any1(const Any1&) = delete;
	Any1& operator=(const Any1&) = delete;
	Any1(Any1&&) = default;
	Any1& operator=(Any1&&) = default;

	// 第三步，构造函数 让基类指针指向派生类对象 
	template<typename T>
	Any1(T data) : base_(std::make_unique<Derive<T>>(data))
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

Any1 func()
{

	return 1 + 2;
}





int main2()
{
	shared_ptr<int> spq(new int(10));
	shared_ptr<int> sp = make_shared<int>(19);

	system("pause");
	return 1;
}