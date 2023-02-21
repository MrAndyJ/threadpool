


#include <iostream>
#include <future>
#include <thread>
#include "threadpool.h"
using namespace std;


// ����1�� ������̳߳��ύ���������
/*
pool.submitTask(sum1,1,2);

*/
// ����2�� 

int sum1(int a, int b)
{
	this_thread::sleep_for(chrono::seconds(2));
	cout << "sum1(int a, int b)" << endl;
	return a + b;
}

int sum2(int a, int b, int c)
{
	cout << "sum2(int a, int b, int c)" << endl;
	return a + b + c;
}

int main()
{
	//thread t1(sum1, 1, 2);
	//t1.join();

	// ʹ�� packaged_task����
	//packaged_task<int(int, int)> task(sum1);   //  ������񷵻�ֵ Result res = pool.submitTask(std::make_shared<MyTask>(1, 100));
	//future<int> ret = task.get_future();    // ʹ��future �������񷵻�ֵ
	//// task(1, 2);
	//thread t2(std::move(task),1,2);
	//t2.join();
	// cout << ret.get() << endl;    // �ȴ��߳�ִ����ϣ�post��֪ͨwait��Ȼ�����get��
	{
		ThreadPool pool;
		pool.setTaskSizeThreshHold(2);
		 // pool.setMode(PoolMode::MODE_CACHED);
		pool.start(4);
		future<int> ret1 = pool.submitTask([](int a, int b)->int {
			return a + b;
			
			}, 11, 21);
		future<int> ret2 = pool.submitTask(sum1, 1, 2);
		future<int> ret3 = pool.submitTask(sum1, 1, 2);
		future<int> ret4 = pool.submitTask(sum1, 1, 2);
		cout << ret1.get() << endl;
		cout << ret2.get() << endl;
		cout << ret3.get() << endl;
		cout << ret4.get() << endl;
	}

	return 1;
}