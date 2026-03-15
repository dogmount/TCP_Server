#ifndef _CLIENT_EVENT_HPP_
#define _CLIENT_EVENT_HPP_

#include"serConfig.hpp"
#include"cellClient.hpp"

//网络事件接口
class clientEvent
{
public:
	//纯虚函数
	//客户端加入事件
	virtual void cJoin(cSocket* client) = 0;

	//客户端离开事件
	virtual void cLeave(cSocket* client) = 0;

	//客户端请求事件
	virtual void pushMsg(SOCKET cSock, const char* data, int len) = 0;
};

#endif