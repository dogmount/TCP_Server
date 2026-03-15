#ifndef _TCPSERVER_HPP_
#define _TCPSERVER_HPP_

#include"cellClient.hpp"
#include"cellServer.hpp"
#include"msgHeader.hpp"
#include"sqlCommand.hpp"

#include<random>
#include<thread>
#include<mutex>
#include<atomic>
#include<queue>
#include<condition_variable>

#define group_id_min 100000
#define cellServer_threadCount 4

//	1、消息处理单元
struct msgQueueItem
{
	SOCKET sock;				//客户端socket
	DataHeader* header;			//消息头
	std::vector<char> data;		//完整的消息数据

	msgQueueItem(SOCKET& s, const char* msgData, int len)
	{
		sock = s;
		data.resize(len);
		memcpy(data.data(), msgData, len);
		header = (DataHeader*)data.data();
		// 转换回主机字节序
		header->cmd = ntohl(header->cmd);
		header->dataLength = ntohl(header->dataLength);
	}
};

//	2、消息发送单元
struct sendQueueItem
{
	SOCKET sock;           // 目标客户端socket
	std::vector<char> data; // 待发送的数据

	sendQueueItem(SOCKET& s, const char* msgData, int len)
	{
		sock = s;
		data.resize(len);
		memcpy(data.data(), msgData, len);
	}
};

class TcpServer :public clientEvent
{
private:
	conSql* _sql;
	SOCKET _sock;
	std::vector<cSocket*> _clients;
	std::vector<cellServer*> _cellserver;

	//消息处理队列
	std::queue<msgQueueItem*> msgQueue;
	std::mutex msgMutex;
	std::condition_variable msgSign;

	//消息发送队列
	std::queue<sendQueueItem*> sendQueue;
	std::mutex sendMutex;
	std::condition_variable sendSign;

	//处理线程和发送线程
	std::thread processorThread;
	std::thread senderThread;
	std::atomic<bool> running{ false };
public:
	TcpServer()
	{
		_sql = new conSql();
		_sql->buildSqlTable("./test.sql");
		
		_sock = INVALID_SOCKET;
	}

	virtual ~TcpServer()
	{
		delete _sql;
		close_serverSocket();
	}

	//	初始化Socket
	SOCKET initSocket()
	{
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);			//	winSocket的启动函数
		WSADATA wsaData;
		if (WSAStartup(ver, &wsaData) != 0)	//	等于0代表初始化成功
		{
			std::cout << "初始化Winsock失败" << std::endl;
#ifdef _WIN32		
			WSACleanup();
#endif	
			return _sock;
		}
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
		sockaddr_in serverAddr;
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
		sockaddr_in clientAddr;
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
		_clients.push_back(pclient);
		cellServer* minServer = _cellserver[0];
		for (std::vector<cellServer*>::iterator it = _cellserver.begin(); it != _cellserver.end(); it++)
		{
			if (minServer->get_clientcount() > (*it)->get_clientcount())
			{
				minServer = (*it);
			}
		}
		minServer->addClient(pclient);
	}

	void server_start()
	{
		for (int n = 0; n < cellServer_threadCount; n++)
		{
			cellServer* ser = new cellServer(n);
			_cellserver.push_back(ser);
			//	重点！！！回调接口
			ser->set_eventOBJ(this);
			ser->cell_start();
		}
	}

	//	关闭Socket
	void close_serverSocket()
	{
		if (_sock != INVALID_SOCKET)
		{
			//	关闭套接字
#ifdef _WIN32
			closesocket(_sock);
			//清除windows socket环境
			WSACleanup();
#else
			close(_sock);
#endif 
		}
		_sock = INVALID_SOCKET;
	}

	//	处理网络消息
	bool onRun()
	{
		if (isRun())
		{
			//！！！select I/O多路复用处理多客户端连接
			//1、定义fd_set集合
			fd_set fdRead;
			//fd_set fdWrite;
			//fd_set fdExp;

			//清空集合
			FD_ZERO(&fdRead);
			//FD_ZERO(&fdWrite);
			//FD_ZERO(&fdExp);

			/*
			---将serverSocket加入读、写和异常集合
			读集合：监视是否有新的客户端连接
			写集合：监视 socket 是否可写（通常不需要监听服务器监听socket的写事件）
			异常集合：监视 socket 是否有异常发生
			*/
			FD_SET(_sock, &fdRead);
			//FD_SET(_sock, &fdWrite);
			//FD_SET(_sock, &fdExp);

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
				std::cout << "select任务结束。" << std::endl;
				close_serverSocket();
				return false;
			}

			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);
				acceptLink();
				return true;
			}
			return true;
		}
		return false;
	}

	//	判断是否在工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//	发送数据
	int sendMsg(SOCKET clientSocket)
	{
		
	}

	//	向所有符合条件的客户端发送消息
	void sendMsgToall()
	{
		for (int n = (int)_clients.size() - 1; n >= 0; n--)
		{

		}
	}

	//	客户端登录时记录Uid
	void logUid(SOCKET sock, const std::string s)
	{
		for (int n = (int)_clients.size() - 1; n >= 0; n--)
		{
			if (_clients[n]->sockfd() == sock)
			{
				_clients[n]->set_id(s);
			}
		}
	}

	//	检测客户是否在线
	bool checkOnline(SOCKET& ret, const user_relation rel)
	{
		std::string targetId;
		if (rel.f_teamname1 == "")
		{
			targetId = rel.f_user_id1;
		}
		else if (rel.f_teamname2 == "")
		{
			targetId = rel.f_user_id2;
		}

		for (int n = (int)_clients.size() - 1; n >= 0; n--)
		{
			if (_clients[n]->get_id() == targetId)
			{
				ret = _clients[n]->sockfd();
				return true;
			}
		}
		return false;
	}

	//客户端加入事件
	virtual void cJoin(cSocket* client)
	{

	}

	//	客户端断连时
	virtual void cLeave(cSocket* pclient)
	{
		for (int n = (int)_clients.size() - 1; n >= 0; n--)
		{
			if (_clients[n] == pclient)
			{
				std::vector<cSocket*>::iterator iter = _clients.begin() + n;
				if (iter != _clients.end())
					_clients.erase(iter);
			}
		}
	}

	virtual void pushMsg(SOCKET cSock, const char* data, int len)
	{
		msgQueueItem* item = new msgQueueItem(cSock, data, len); // 内部拷贝
		{
			std::lock_guard<std::mutex> lock(msgMutex);
			msgQueue.push(item);
		}
		msgSign.notify_one();
	}

private:
	void msgLoop()
	{
		while (running)
		{
			msgQueueItem* item = nullptr;

			//	循环循环处理
			std::unique_lock<std::mutex> lock(msgMutex);
			msgSign.wait(lock, [this] { return !msgQueue.empty() || !running; });
			if (!running) break;
			if (!msgQueue.empty())
			{
				item = msgQueue.front();
				msgQueue.pop();
			}

			if (item) {
				processMessage(item);      // 处理消息并可能生成响应
				delete item;
			}
		}
	}

	// 实际处理消息的函数（需根据业务实现）
	void processMessage(msgQueueItem* item)
	{

		switch (item->header->cmd)
		{
		case CMD_REGISTER:
			{
			bool b = true;
			int tryCount = 10;
			json t = jsonUNPacket(item->data,item->header->dataLength);
			user_data temp = userUNPACK(t);
			int account = generate_account();
			while (_sql->check_uid(account) && b)
			{
				if (tryCount > 0)
				{
					account = generate_account();
					tryCount--;
				}
				else
				{
					b = false;
				}
			}
			tryCount = 5;
			temp.uid = account;
			while (!_sql->insert_user(temp) && b)
			{
				if (tryCount > 0)
				{
					tryCount--;
				}
				else
				{
					b = false;
				}
			}
			if (b)
			{
				json j = userPACK(temp);
				RegisterRet ret;
				ret.data = j;
				std::vector<char> buf = jsonPacket(ret);
				enqueueSend(item->sock, buf.data(), buf.size());
			}
			else
			{
				msgERROR e;
				e.cmd = htonl(e.cmd);
				e.dataLength = htonl(e.dataLength);
				enqueueSend(item->sock, (const char*)&e, sizeof(e));
			}
			break;
			}
		case CMD_LOGIN:
		{
			json t = jsonUNPacket(item->data, item->header->dataLength);
			user_data temp = userUNPACK(t);
			if (_sql->check_user(temp))
			{
				//允许登录
				logUid(item->sock, temp.uid);
				LoginResult e;
				e.loginRet = true;
				e.cmd = htonl(e.cmd);
				e.dataLength = htonl(e.dataLength);
				enqueueSend(item->sock, (const char*)&e, sizeof(e));
			}
			else
			{
				//返回错误
				LoginResult e;
				e.loginRet = false;
				e.cmd = htonl(e.cmd);
				e.dataLength = htonl(e.dataLength);
				enqueueSend(item->sock, (const char*)&e, sizeof(e));
			}
			break;
		}
		case CMD_LOGOUT:
		{
			//用户登出，清理用户列表
			break;
		}
		case CMD_USER_FIND:
		{
			//查找用户
			json t = jsonUNPacket(item->data, item->header->dataLength);
			user_data temp = userUNPACK(t);
			json j;
			if (_sql->search_user(temp.uid, j))
			{
				//查找成功返回用户数据
				USERFIND_RET ret;
				ret.data = j;
				std::vector<char> buf = jsonPacket(ret);
				enqueueSend(item->sock, buf.data(), buf.size());
			}
			else
			{
				msgERROR e;
				e.cmd = htonl(e.cmd);
				e.dataLength = htonl(e.dataLength);
				enqueueSend(item->sock, (const char*)&e, sizeof(e));
			}
			break;
		}
		case CMD_USER_ADD:
		{
			json t = jsonUNPacket(item->data, item->header->dataLength);
			user_relation temp = singalrelUNPACK(t);
			bool b = true;
			if (_sql->if_relation(temp.f_user_id1, temp.f_user_id2))
			{
				b = false;
			}
			if (b)
			{
				_sql->insert_relation(temp);
				SOCKET target;
				if (checkOnline(target, temp))
				{
					USERJOIN uj;
					uj.data = singalrelPACK(temp);
					std::vector<char> buf = jsonPacket(uj);
					enqueueSend(target, buf.data(), buf.size());
				}
			}
			else
			{
				msgERROR e;
				e.cmd = htonl(e.cmd);
				e.dataLength = htonl(e.dataLength);
				enqueueSend(item->sock, (const char*)&e, sizeof(e));
			}
			break;
		}
		case CMD_USER_ADD_AGREE:
		{
			json t = jsonUNPacket(item->data, item->header->dataLength);
			user_relation temp = singalrelUNPACK(t);
			_sql->update_relation(temp);
			break;
		}
		case CMD_USER_ADD_REFUSE:
		{
			json t = jsonUNPacket(item->data, item->header->dataLength);
			user_relation temp = singalrelUNPACK(t);
			_sql->delete_relation(temp);
			break; 
		}
		case CMD_USER_DEL: 
		{
			json t = jsonUNPacket(item->data, item->header->dataLength);
			user_relation temp = singalrelUNPACK(t);
			_sql->delete_relation(temp);
			break;
		}
		case CMD_MSG:
		{
			json t = jsonUNPacket(item->data, item->header->dataLength);
			msg_text temp = singalMsgUNPack(t);
			_sql->insert_msg(temp);
			break;
		}
		default:
		{
			break;
		}
		}
	}

	// 将数据加入发送队列
	void enqueueSend(SOCKET sock, const char* data, int len) {
		sendQueueItem* sendItem = new sendQueueItem(sock, data, len);
		{
			std::lock_guard<std::mutex> lock(sendMutex);
			sendQueue.push(sendItem);
		}
		sendSign.notify_one();
	}
	//生成五位随机数账号
	int generate_account() {
		// 随机数设备，用于种子
		std::random_device rd;
		// 梅森旋转算法引擎
		std::mt19937 gen(rd());
		// 均匀分布 [10000, 99999]
		std::uniform_int_distribution<int> dist(10000, 99999);

		return dist(gen);
	}

	//json序列化
	template<typename T>
	std::vector<char> jsonPacket(const T& msg)
	{
		//将json转换为字符串
		std::string jsonStr = msg.data.dump();
		int len = sizeof(DataHeader) + jsonStr.size();

		DataHeader temp;
		temp.cmd = msg.cmd;
		temp.dataLength = len;
		// 转换为网络字节序
		temp.cmd = htonl(temp.cmd);
		temp.dataLength = htonl(temp.dataLength);

		std::vector<char> buffer(len);
		memcpy(buffer.data(), &temp, sizeof(temp));
		memcpy(buffer.data() + sizeof(temp), jsonStr.data(), jsonStr.size());
		
		return buffer;
	}

	//json反序列化
	json jsonUNPacket(const std::vector<char>& jsonPacket, int& len)
	{
		int jsonlen = len - sizeof(DataHeader);
		
		const char* bodyData = jsonPacket.data() + sizeof(DataHeader);
		std::string jsonStr(bodyData, jsonlen);
		json j = json::parse(jsonStr);
		return j;
	}
};

#endif	//	_TcpServer_hpp_