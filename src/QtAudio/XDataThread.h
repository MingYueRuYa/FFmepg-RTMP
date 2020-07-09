#ifndef xdatathread_h
#define xdatathread_h

#include "XData.h"

#include <QThread>
#include <list>

class XDataThread : public QThread
{
public:
	//�������б��С���б����ֵ������ɾ����ɵ�����
	int maxList = 100;

	//���б��β����
	virtual void Push(XData d);

	//��ȡ�б������������,����������Ҫ����XData.Drop����
	virtual XData Pop();

	//�����߳�
	virtual bool Start();

	//�˳��̣߳����ȴ��߳��˳���������
	virtual void Stop();

	XDataThread();
	virtual ~XDataThread();
protected:
	//��Ž������� ������� �Ƚ��ȳ�
	std::list<XData> datas;

	//���������б��С
	int dataCount = 0;
	//������� datas;
	QMutex mutex;

	//�����߳��˳�
	bool isExit = false;
};


#endif // xdatathread_h