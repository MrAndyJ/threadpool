
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
		// 一直等待notEmpty 条件，不为空
		
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
	// 问题：ThreadPool 对象析构之后，把线程池中线程资源回收 
	ThreadPool pool;
	//			10 问题1：如何设置线程的模式呢？
	pool.setMode(PoolMode::MODE_CACHED);
	// 10 所有的设计都应该在start启动线程池之前设置。
	pool.start(2);   // 开启了两个线程 

	// Master - Slave线程模型
	Result res = pool.submitTask(std::make_shared<MyTask>(1, 100));
	Result res2 = pool.submitTask(std::make_shared<MyTask>(2, 100)); // 生成MyTask对象
	Result res3 = pool.submitTask(std::make_shared<MyTask>(1, 100));
	Result res4 = pool.submitTask(std::make_shared<MyTask>(1, 100));
	//res.get();  // 调用get() 方法获得返回值，如果submit没有执行完毕，应该先阻塞，等到线程处理完毕后，nodify()，然后再返回。

	// 随着task执行完毕，task对象就是析构了，依赖于task对象的Result也就没了；所以应该让Result 包含task
	int sum1 = res.get().cast_<int>(); // 只有当rask 返回后，task才能被析构
	int sum2 = res2.get().cast_<int>(); // 只有当rask 返回后，task才能被析构
	int sum3 = res3.get().cast_<int>(); // 只有当rask 返回后，task才能被析构
	int sum4 = res4.get().cast_<int>(); // 只有当rask 返回后，task才能被析构
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