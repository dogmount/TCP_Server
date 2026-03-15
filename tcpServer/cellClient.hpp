#ifndef _CELL_CLIENT_HPP_
#define _CELL_CLIENT_HPP_

#include"serConfig.hpp"

//客户端心跳检测死亡计时时间
#define CLIENT_HREAT_DEAD_TIME 60000

class cSocket
{
public:
	cSocket(SOCKET sockfd = INVALID_SOCKET)
	{
		sock_fd = sockfd;
		memset(msg_buf, 0, sizeof(msg_buf));
		last_msgPos = 0;

		resetHeart();
	}

	~cSocket()
	{
		if (INVALID_SOCKET != sock_fd)
		{
#ifdef _WIN32
			closesocket(sock_fd);
#else
			close(sock_fd);
#endif
			sock_fd = INVALID_SOCKET;
		}
	}

	SOCKET sockfd()
	{
		return sock_fd;
	}

	char* msgBuf()
	{
		return msg_buf;
	}

	int getLastPos()
	{
		return last_msgPos;
	}

	void setLastPos(int pos)
	{
		last_msgPos = pos;
	}

	int get_id()
	{
		return user_id;
	}

	void set_id(int uid)
	{
		user_id = uid;
	}

	//设置所属服务器id
	void set_server_id(int id)
	{
		server_id = id;
	}

	//心跳检测
	bool checkHeart(time_t dt)
	{
		dead_heart += dt;
		if (dead_heart >= CLIENT_HREAT_DEAD_TIME)
		{
			printf("checkHeart dead:s=%d,time=%ld\n", sock_fd, dead_heart);
			return true;
		}
		return false;
	}

	void resetHeart()
	{
		dead_heart = 0;
	}
private:
	//	套接字描述符
	SOCKET sock_fd;

	//	客户端id
	int user_id = -1;
	//	服务器id
	int server_id = -1;

	//	第二缓冲区 消息缓冲区
	char msg_buf[RECV_BUFF_SIZE * 10] = {};
	//	消息缓冲区尾部位置
	int last_msgPos;

	//	心跳死亡计时
	time_t dead_heart;
};

#endif