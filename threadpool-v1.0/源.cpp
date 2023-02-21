

#include <iostream>
#include <functional>
#include <thread>
#include <vector>
using namespace std;

template<typename Tty>
class myfunction
{

};


//template <typename R,typename...A>
//class myfunction(R(A...))  // R����ֵ���ͣ�A��������
//{
//public:
//	using PFUNC = R(*)(A...);
//	myfunction(PFUNC pfunc) : pfunc(pfunc)
//	{
//		
//	}
//	R operator()(A...arg)
//	{
//		return _func(arg...);
//	}
//
//private:
//	PFUNC _func;
//
//}

template<typename R, typename A1>  // R����ֵ���ͣ�A1��������
class myfunction<R(A1)>
{
public:
	using PFUNC = R(*)(A1);
	myfunction(PFUNC pfunc) :_pfunc(pfunc) {}

	R operator()(A1 arg)  // arg ʵ��
	{
		return _pfunc(arg);
	}
private:
	PFUNC _pfunc;
};



class MyThread
{
public:
	MyThread(function<void(int)> func) :_func(func)
	{
		
	}
	~MyThread();

	thread start()
	{
		thread t(_func, 1);
		return t;
	}

private:
	function<void(int)> _func;  // ���庯�����󣬽���void(int)
	int _no;

};

class MyThreadPool
{
public:
	MyThreadPool() = default;
	~MyThreadPool() = default;

	void startpool(int size)
	{
		for (int i = 0; i < size; ++i)
		{
			_pool.push_back(new MyThread(bind(&MyThreadPool::runInThread, this, i)));
			/*function<void(int)> *func =new bind(&MyThreadPool::runInThread, this, i);*/
		}
		
		for (int i = 0; i < size; ++i)
		{
			_handler.push_back(_pool[i]->start());
		}
		for (thread& t : _handler)
		{
			t.join();  // �ȴ����߳̽���
		}
	}

	void runInThread(int id)
	{
		cout << "call	" << id << endl;
	}
private:
	vector<MyThread*> _pool;
	vector<thread> _handler;
};


int main()
{
	//MyThreadPool t1;
	//function<void(int)> func = bind(&MyThreadPool::runInThread, &t1, 1);
	//func;
	MyThreadPool t1;
	t1.startpool(3);

	myfunction<void()> func1;

	return 1;
}