
#include "threadpool.h"

#include <thread>
#include <memory>
#include <iostream>

class MyTask : public Task
{
public:
	MyTask(int begin, int end)
		:begin_(begin)
		, end_(end)
	{}
	Any run()
	{
		std::cout << "tid:" << std::this_thread::get_id() << "	begin/..." << std::endl;
		// һֱ�ȴ�notEmpty ��������Ϊ��
		
		std::this_thread::sleep_for(std::chrono::seconds(2));
		int sum = 0;
		for (int i = begin_; i <= end_; ++i)
		{
			sum += i;
		}

		std::cout << "tid:" << std::this_thread::get_id() << "	end/..." << std::endl;
		return sum;
	}
private:
	int begin_;
	int end_;
};


void test1()
{
	// ���⣺ThreadPool ��������֮�󣬰��̳߳����߳���Դ���� 
	ThreadPool pool;
	//			10 ����1����������̵߳�ģʽ�أ�
	pool.setMode(PoolMode::MODE_CACHED);
	// 10 ���е���ƶ�Ӧ����start�����̳߳�֮ǰ���á�
	pool.start(2);   // �����������߳� 

	// Master - Slave�߳�ģ��
	Result res = pool.submitTask(std::make_shared<MyTask>(1, 100));
	Result res2 = pool.submitTask(std::make_shared<MyTask>(2, 100)); // ����MyTask����
	Result res3 = pool.submitTask(std::make_shared<MyTask>(1, 100));
	Result res4 = pool.submitTask(std::make_shared<MyTask>(1, 100));
	//res.get();  // ����get() ������÷���ֵ�����submitû��ִ����ϣ�Ӧ�����������ȵ��̴߳�����Ϻ�nodify()��Ȼ���ٷ��ء�

	// ����taskִ����ϣ�task������������ˣ�������task�����ResultҲ��û�ˣ�����Ӧ����Result ����task
	int sum1 = res.get().cast_<int>(); // ֻ�е�rask ���غ�task���ܱ�����
	int sum2 = res2.get().cast_<int>(); // ֻ�е�rask ���غ�task���ܱ�����
	int sum3 = res3.get().cast_<int>(); // ֻ�е�rask ���غ�task���ܱ�����
	int sum4 = res4.get().cast_<int>(); // ֻ�е�rask ���غ�task���ܱ�����
	std::cout << sum1 + sum2 << std::endl;
}

void test2()
{
	ThreadPool pool;
	pool.start(4);
	Result res = pool.submitTask(std::make_shared<MyTask>(1, 100));
	int sum1 = res.get().cast_<int>();
	std::cout << sum1 << std::endl;
}

int main()
{
	//test1();

	test2();

	std::cout << "mail thread" << std::endl;
	system("pause");
	return 1;
}