#ifndef _CELL_SERVER_HPP_
#define _CELL_SERVER_HPP_

//#include"cellClient.hpp"
#include"clientEvent.hpp"
#include"cellThread.hpp"
#include"cellTime.hpp"
#include"msgHeader.hpp"

#include<map>
#include<vector>
#include<mutex>

//网络消息接收处理服务类
class cellServer
{
public:
	cellServer(int id)
	{
		cell_id = id;
		cell_event = nullptr;
	}

	~cellServer()
	{
		printf("CELLServer%d.~CELLServer exit begin\n", cell_id);
		cell_close();
		printf("CELLServer%d.~CELLServer exit end\n", cell_id);
	}

	void cell_start()
	{
		cell_thread.Start(
			//onCreate
			nullptr,
			//onRun
			[this](cellThread* pThread) {
				cell_onRun(pThread);
			},
			//onDestory
			[this](cellThread* pThread) {
				clearClients();
			}
		);
	}

	//关闭子服务器
	void cell_close()
	{
		printf("CELLServer%d.Close begin\n", cell_id);
		cell_thread.Close();
		printf("CELLServer%d.Close end\n", cell_id);
	}

	void set_eventOBJ(clientEvent* event)
	{
		cell_event = event;
	}

	//	接收信息，处理粘包少包
	void cell_onRun(cellThread* cThread)
	{
		while (cell_thread.isRun())
		{
			if (!clients_buff.empty())
			{
				//从缓冲队列里取出客户数据
				std::lock_guard<std::mutex> lock(queue_mutex);
				for (auto pclient : clients_buff)
				{
					clients_queue[pclient->sockfd()]= pclient;
					if (cell_event)
						cell_event->cJoin(pclient);
				}
				clients_buff.clear();
				clients_change = true;
			}

			//如果没有需要处理的客户端，就跳过
			if (clients_queue.empty())
			{
				//创建一个对象 t，初始值为 1，表示 1 毫秒。
				std::chrono::milliseconds t(1);
				//调用标准库函数使当前线程主动让出 CPU 时间片，暂停执行至少 t 指定的时间
				std::this_thread::sleep_for(t);
				continue;
			}

			//描述符（socket） 集合
			fd_set fdRead;

			//客户列表更新时
			if (clients_change)
			{
				clients_change = false;
				//清理集合
				FD_ZERO(&fdRead);
				//将描述符（socket）加入集合
				max_Sock = clients_queue.begin()->second->sockfd();
				for (auto iter:clients_queue)
				{
					FD_SET(iter.second->sockfd(), &fdRead);
					if (max_Sock < iter.second->sockfd())
					{
						max_Sock = iter.second->sockfd();
					}
				}
				memcpy(&_fdRead_bak, &fdRead, sizeof(fd_set));
			}
			else
			{
				memcpy(&fdRead, &_fdRead_bak, sizeof(fd_set));
			}

			///nfds 是一个整数值 是指fd_set集合中所有描述符(socket)的范围，而不是数量
			///既是所有文件描述符最大值+1 在Windows中这个参数可以写0
			timeval t{ 0,1 };
			int ret = select(max_Sock + 1, &fdRead, nullptr, nullptr, &t);
			if (ret < 0)
			{
				printf("CELLServer%d.OnRun.select Error exit\n", cell_id);
				cell_thread.Exit();
				break;
			}

			//读数据
			readData(fdRead);
			//心跳检测
			CheckTime();
		}
		printf("CELLServer%d.OnRun exit\n", cell_id);
	}

	void CheckTime()
	{
		//当前时间戳
		auto nowTime = CELLTime::getNowInMilliSec();
		auto dt = nowTime - old_Time;
		old_Time = nowTime;

		for (auto iter = clients_queue.begin(); iter != clients_queue.end(); )
		{
			//心跳检测
			if (iter->second->checkHeart(dt))
			{
				if (cell_event)
				{
					cell_event->cLeave(iter->second);
				}
				clients_change = true;
				delete iter->second;
				auto iterOld = iter;
				iter++;
				clients_queue.erase(iterOld);
				continue;
			}

			////定时发送检测
			//iter->second->checkSend(dt);

			iter++;
		}
	}

	void clientLeave(cSocket* pclient)
	{
		if (cell_event)
		{
			cell_event->cLeave(pclient);
		}
		clients_change = true;
		delete pclient;
	}

	void readData(fd_set& fdRead)
	{
#ifdef _WIN32
		for (int n = 0; n < fdRead.fd_count; n++)
		{
			auto iter = clients_queue.find(fdRead.fd_array[n]);
			if (iter != clients_queue.end())
			{
				if (-1 == cell_recvData(iter->second))
				{
					clientLeave(iter->second);
					clients_queue.erase(iter);
				}
			}
		}
#else
		for (auto iter = clients_queue.begin(); iter != clients_queue.end(); )
		{
			if (FD_ISSET(iter->second->sockfd(), &fdRead))
			{
				if (-1 == cell_recvData(iter->second))
				{
					clientLeave(iter->second);
					auto iterOld = iter;
					iter++;
					clients_queue.erase(iterOld);
					continue;
				}
			}
			iter++;
		}
#endif
	}

	//	接收数据 处理粘包 拆分包
	int cell_recvData(cSocket* pclient)
	{
		// 接收客户端数据
		char* cell_recvbuff = pclient->msgBuf() + pclient->getLastPos();
		int nLen = (int)recv(pclient->sockfd(), cell_recvbuff, (RECV_BUFF_SIZE)-pclient->getLastPos(), 0);

		//printf("nLen=%d\n", nLen);
		if (nLen <= 0)
		{
			//printf("客户端<Socket=%d>已退出，任务结束。\n", pclient->sockfd());
			return -1;
		}
		//将收取到的数据拷贝到消息缓冲区
		//memcpy(pclient->msgBuf() + pclient->getLastPos(), cell_recvbuff, nLen);
		//消息缓冲区的数据尾部位置后移
		pclient->setLastPos(pclient->getLastPos() + nLen);

		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		while (pclient->getLastPos() >= sizeof(DataHeader))
		{
			//这时就可以知道当前消息的长度
			DataHeader* header = (DataHeader*)pclient->msgBuf();
			int len = ntohl(header->dataLength);

			// 检查长度合法性
			if (len < sizeof(DataHeader) || len > RECV_BUFF_SIZE * 10)
			{
				// 数据异常，关闭连接
				return -1;
			}

			//判断消息缓冲区的数据长度大于消息长度
			if (pclient->getLastPos() >= len)
			{
				//消息缓冲区剩余未处理数据的长度
				int nSize = pclient->getLastPos() - len;
				//处理网络消息
				msgHandle(pclient->sockfd(), pclient->msgBuf(), len);
				//将消息缓冲区剩余未处理数据前移
				memcpy(pclient->msgBuf(), pclient->msgBuf() + len, nSize);
				//消息缓冲区的数据尾部位置前移
				pclient->setLastPos(nSize);
			}
			else
			{
				//消息缓冲区剩余数据不够一条完整消息
				break;
			}
		}
		return 0;
	}

	//	响应网络消息
	void msgHandle(SOCKET cSock, const char* msgdata, int len)
	{
		//	拷贝数据给业务逻辑处理
		//std::vector<char> temp(len);
		//memcpy(temp.data(), msgdata, len);
		cell_event->pushMsg(cSock, msgdata, len);
	}

	void addClient(cSocket* pclient)
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		clients_buff.push_back(pclient);
	}

	size_t get_clientcount()
	{
		return clients_queue.size() + clients_buff.size();
	}

private:
	void clearClients()
	{
		for (auto iter : clients_queue)
		{
			delete iter.second;
		}
		clients_queue.clear();

		for (auto iter : clients_buff)
		{
			delete iter;
		}
		clients_buff.clear();
	}

private:
	//客户队列
	std::map<SOCKET, cSocket*>clients_queue;

	//缓冲客户队列
	std::vector<cSocket*> clients_buff;
	//缓冲队列锁
	std::mutex queue_mutex;

	//子服务器线程
	cellThread cell_thread;
	//子服务器事件
	clientEvent* cell_event;

	SOCKET max_Sock;
	//旧的时间戳
	time_t old_Time = CELLTime::getNowInMilliSec();
	//备份客户socket fd_set
	fd_set _fdRead_bak;
	//子服务器id
	int cell_id = -1;
	//客户列表是否变化
	bool clients_change = true;
};

#endif