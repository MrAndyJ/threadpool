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
		cout << "����������һ����Ʒ: " << i << endl;
		cond.notify_all();
	}

	// ������
	void task2()
	{
		unique_lock<mutex> lock(mtx);
		while (que.empty())
		{
			cond.wait(lock);
		}
		int a = que.front();
		cout << "������������һ����Ʒ" <<a<< endl;
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
	cout << "���߳�ִ��" << endl;
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
C++11 �ṩ��emplace����
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

	// �����������캯�� �û���ָ��ָ����������� 
	template<typename T>
	Any1(T data) : base_(std::make_unique<Derive<T>>(data))
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