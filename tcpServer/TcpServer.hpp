#ifndef _TCPSERVER_HPP_
#define _TCPSERVER_HPP_

#include"cellClient.hpp"
#include"cellServer.hpp"
#include"clientEvent.hpp"

class TcpServer :public clientEvent
{
private:
	//服务器线程
	cellThread _thread;
	//服务器套接字
	SOCKET _sock;
	//子服务器列表
	std::vector<cellServer*> _cellserver;

public:
	TcpServer()
	{
		_sock = INVALID_SOCKET;
	}

	virtual ~TcpServer()
	{
		close_serverSocket();
	}

	//	初始化Socket
	SOCKET initSocket()
	{
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);			//	winSocket的启动函数
		WSADATA wsaData;
		WSAStartup(ver, &wsaData);			//	返回值等于0代表初始化成功
#endif	
		//1、建立一个socket
		if (INVALID_SOCKET != _sock)		//	如果之前已经创建过socket则清理并重新创建
		{
			std::cout << "<socket>:" << _sock << " 关闭旧链接" << std::endl;
			close_serverSocket();
		}

		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock) {
			std::cout << "ERROR: 创建套接字失败" << std::endl;
		}
		else
		{
			std::cout << "建立<socket>:" << _sock << " 成功" << std::endl;
		}
		return _sock;
	}

	//	绑定IP和端口号
	int bindPort(const char* ip, unsigned short port)
	{
		/*if (INVALID_SOCKET == _sock)
		{
			initSocket();
		}*/
		//	bind绑定用于接收客户端链接的网络端口
		sockaddr_in serverAddr = {};
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(port);
		if (ip)
		{
			serverAddr.sin_addr.s_addr = inet_addr(ip);
		}
		else
		{
			serverAddr.sin_addr.s_addr = INADDR_ANY;
		}
		int ret = bind(_sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
		if (SOCKET_ERROR == ret)
		{
			std::cout << "ERROR: 绑定端口<" << port << ">失败" << std::endl;
		}
		else
		{
			std::cout << "绑定网络端口成功" << std::endl;
		}
		return ret;
	}

	//	监听端口号
	int listenMsg(int n)
	{
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)
		{
			std::cout << "ERROR: Socket=<" << _sock << ">监听失败" << std::endl;
		}
		else
		{
			std::cout << "监听成功" << std::endl;
		}
		return ret;
	}

	//	接受客户端连接
	int acceptLink()
	{
		//4、accept等待接受客户端链接
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET clientSocket = INVALID_SOCKET;

#ifdef _WIN32
		clientSocket = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else 
		clientSocket = accept(_sock, (sockaddr*)&clientAddr, (socklent_t*)&nAddrLen);
#endif
		if (INVALID_SOCKET == clientSocket)
		{
			std::cout << "ERROR: 接收到无效客户端" << std::endl;
		}
		else
		{
			std::cout << "新客户端加入：IP =" << inet_ntoa(clientAddr.sin_addr) << std::endl;
			addClient_toCellserver(new cSocket(clientSocket));
			//_clients.push_back(new cSocket(clientSocket));
		}
		return clientSocket;
	}

	void addClient_toCellserver(cSocket* pclient)
	{
		cellServer* minServer = _cellserver[0];
		for (auto pserver : _cellserver)
		{
			if (minServer->get_clientcount() > pserver->get_clientcount())
			{
				minServer = pserver;
			}
		}
		minServer->addClient(pclient);
	}

	void server_start(int cServerCount)
	{
		for (int n = 0; n < cServerCount; n++)
		{
			cellServer* ser = new cellServer(n + 1);
			_cellserver.push_back(ser);
			//	重点！！！回调接口
			ser->set_eventOBJ(this);
			ser->cell_start();
		}
		_thread.Start(nullptr,
			[this](cellThread* pThread) {
				onRun(pThread);
			});
	}

	//	关闭Socket
	void close_serverSocket()
	{
		std::cout << "EasyTcpServer.Close begin..." << std::endl;
		_thread.Close();
		if (_sock != INVALID_SOCKET)
		{
			for (auto s : _cellserver)
			{
				delete s;
			}
			_cellserver.clear();
			//	关闭套接字
#ifdef _WIN32
			closesocket(_sock);
			//清除windows socket环境
			WSACleanup();
#else
			close(_sock);
#endif 
			_sock = INVALID_SOCKET;
		}
		std::cout << "EasyTcpServer.Close end..." << std::endl;
	}

	//	处理网络消息
	void onRun(cellThread* pThread)
	{
		while (pThread->isRun())
		{
			//！！！select I/O多路复用处理多客户端连接
			//1、定义fd_set集合
			fd_set fdRead;
			//清空集合
			FD_ZERO(&fdRead);
			FD_SET(_sock, &fdRead);

			//select阻隔时间间隔
			timeval t = { 1,0 };
			//nfds是一个整数值，是指fd_set集合中所有描述符（socket)的范围，而不是数量
			//既是所有文件描述符最大值+1，在windows中这个参数可以写0
			int ret = select(_sock + 1, &fdRead, nullptr, nullptr, &t);
			/*
			select 的作用：
			阻塞程序，直到指定的 socket 有事件发生
			返回有事件发生的 socket 数量
			修改 fd_set 集合，只保留有事件发生的 socket
			*/
			if (ret < 0)
			{
				std::cout << "EasyTcpServer.OnRun select exit..." << std::endl;
				pThread->Exit();
				break;
			}

			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);
				acceptLink();
			}
		}
	}

	//客户端加入事件
	virtual void cJoin(cSocket* client)
	{

	}

	//	客户端断连时
	virtual void cLeave(cSocket* pclient)
	{

	}

	virtual void pushMsg(SOCKET cSock, const char* data, int len)
	{

	}

};
#endif	//	_TcpServer_hpp_