#ifndef _msgHeader_hpp_
#define _msgHeader_hpp_

#include"msgPack.hpp"

enum CMD
{
	//	业务命令
	CMD_REGISTER,			//	注册
	CMD_REGISTER_RET,		//	注册结果
	CMD_LOGIN,				//1、登录
	CMD_LOGIN_RET,			//2、登录结果
	CMD_LOGOUT,				//3、登出
	CMD_LOGOUT_RET,			//4、登出结果
	CMD_USER_FIND,			//查找用户
	CMD_USER_RET,			//用户查找结果
	CMD_USER_ADD,			//5、用户添加关系
	CMD_USER_ADD_AGREE,		//6、关系添加结果--同意
	CMD_USER_ADD_REFUSE,	//   关系添加结果--拒绝
	CMD_USER_DEL,			//7、用户删除关系
	CMD_USER_DEL_RET,		//8、关系删除结果
	CMD_MSG,				//9、消息
	CMD_MSG_RET,			//10、消息接收结果
	
	//	服务命令
	CMD_C2S_HEART,			//11、客户端到服务器心跳包
	CMD_S2C_HEART,			//12、服务器到客户端心跳包
	CMD_ERROR,				//13、错误提示
	CMD_HEADER
};

//	数据包长度和命令
struct DataHeader
{
	DataHeader()
	{
		dataLength = sizeof(DataHeader);
		cmd = CMD_HEADER;
	}
	int dataLength;
	int cmd;
};

//	注册
struct Register :public DataHeader
{
	Register()
	{
		dataLength = sizeof(Register);
		cmd = CMD_REGISTER;
	}
	json data;
};

struct RegisterRet :public DataHeader
{
	RegisterRet()
	{
		dataLength = sizeof(RegisterRet);
		cmd = CMD_REGISTER_RET;
	}
	json data;
};

//	1、登录
struct Login :public DataHeader
{
	Login()
	{
		dataLength = sizeof(Login);
		cmd = CMD_LOGIN;
	}
	json data;
};

//	2、登录结果
struct LoginResult :public DataHeader
{
	LoginResult()
	{
		dataLength = sizeof(LoginResult);
		cmd = CMD_LOGIN_RET;
	}
	bool loginRet;
};

//	3、登出
struct LOGOUT :public DataHeader
{
	LOGOUT()
	{
		dataLength = sizeof(LOGOUT);
		cmd = CMD_LOGOUT;
	}
	char userID[32];
};

// 4、登出结果
struct LogoutResult :public DataHeader
{
	LogoutResult()
	{
		dataLength = sizeof(LogoutResult);
		cmd = CMD_LOGOUT_RET;
	}
	bool logoutRet;
};

// 查找用户
struct USERFIND :public DataHeader
{
	USERFIND()
	{
		dataLength = sizeof(USERFIND);
		cmd = CMD_USER_FIND;
	}
	json data;
};

struct USERFIND_RET :public DataHeader
{
	USERFIND_RET()
	{
		dataLength = sizeof(USERFIND_RET);
		cmd = CMD_USER_RET;
	}
	json data;
};

// 5、用户添加关系
struct USERJOIN :public DataHeader
{
	USERJOIN()
	{
		dataLength = sizeof(USERJOIN);
		cmd = CMD_USER_ADD;
	}
	json data;
};

// 6、同意添加关系
struct JOINRET_AGR :public DataHeader
{
	JOINRET_AGR()
	{
		dataLength = sizeof(JOINRET_AGR);
		cmd = CMD_USER_ADD_AGREE;
	}
	json data;
};

//	拒绝添加关系
struct JOINRET_REF :public DataHeader
{
	JOINRET_REF()
	{
		dataLength = sizeof(JOINRET_REF);
		cmd = CMD_USER_ADD_REFUSE;
	}
	json data;
};

// 7、用户删除关系
struct USEROUT :public DataHeader
{
	USEROUT()
	{
		dataLength = sizeof(USEROUT);
		cmd = CMD_USER_DEL;
	}
	json data;
};

// 8、删除关系结果
struct OUTRET :public DataHeader
{
	OUTRET()
	{
		dataLength = sizeof(OUTRET);
		cmd = CMD_USER_DEL_RET;
	}
	json data;
};

// 9、消息
struct NETMSG :public DataHeader
{
	NETMSG()
	{
		dataLength = sizeof(NETMSG);
		cmd = CMD_MSG;
	}
	json data;
};

// 10、消息接收结果
struct MSGRET :public DataHeader
{
	MSGRET()
	{
		dataLength = sizeof(MSGRET);
		cmd = CMD_MSG_RET;
	}
	bool ret;
};

// 11、c2s心跳包
struct C2SHEART :public DataHeader
{
	C2SHEART()
	{
		dataLength = sizeof(C2SHEART);
		cmd = CMD_C2S_HEART;
	}
	char userID[32];
};

// 12、s2c心跳包
struct S2CHEART :public DataHeader
{
	S2CHEART()
	{
		dataLength = sizeof(S2CHEART);
		cmd = CMD_S2C_HEART;
	}
};

// 13、错误提示
struct msgERROR :public DataHeader
{
	msgERROR()
	{
		dataLength = sizeof(msgERROR);
		cmd = CMD_ERROR;
	}
};
#endif