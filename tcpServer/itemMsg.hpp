#ifndef _ITEM_MSG_HPP_
#define _ITEM_MSG_HPP_

#define group_id_min 100000

#include<vector>

#include"serConfig.hpp"
#include"msgHeader.hpp"

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
	SOCKET sock;			// 目标客户端socket
	std::vector<char> data; // 待发送的数据

	sendQueueItem(SOCKET& s, const char* msgData, int len)
	{
		sock = s;
		data.resize(len);
		memcpy(data.data(), msgData, len);
	}
};

//纯虚函数---子服务器和消息处理队列的接口类
class cell_msg_api
{
public:
	// 虚析构，确保正确释放派生类对象
	virtual ~cell_msg_api() = default;
	//客户端加入事件
	//virtual void cJoin(cSocket* client) = 0;

	//客户端离开事件
	virtual void cLeave(int cID) = 0;

	//纯虚函数，推送消息到队列
	virtual void pushMsg(SOCKET cSock, const char* data, int len) = 0;
};

#endif