#include "TcpServer.hpp"
#include <stdio.h> 
#include <mysql.h> // mysql 文件

bool g_bRun = true;
void cmdThread()
{
	while (true)
	{
		char cmdBuf[256] = {};
		std::cin >> cmdBuf;
		if (0 == strcmp(cmdBuf, "exitServer"))
		{
			g_bRun = false;
			printf("退出cmdThread线程\n");
			break;
		}
		else {
			printf("不支持的命令。\n");
		}
	}
}

int main(void) {
	
	//测试
	//conSql* test = new conSql();

	//test->buildSqlTable("./test.sql");

	//test->test();
	//test->search_user("3");
	//test->search_relation("2");
	//test->searchMSG("1");
	//test->createUser("3", "林俊杰", "131400", "2");
	//test->insert_user("3", "林俊杰", "131400", "2");
	
	//test->insert_relation("1","2","我的好友","二狗","挚友","二傻");

	//test->update_username("3","林俊杰");
	//test->update_relation("1","电驴","三傻");

	//test->search_user("3");

	//std::cout << "count : " << test->if_relation("3", "2") << std::endl;
	json ret;

	/*test->search_relation("1",ret);
	std::vector<user_relation> urels;
	urels = relsUNPACK(ret);
	for (auto it = urels.begin(); it != urels.end(); it++)
	{
		std::cout << " " << (*it).f_user_id1
			<< " " << (*it).f_user_id1
			<< " " << (*it).f_teamname1
			<< " " << (*it).f_markname1
			<< " " << (*it).f_teamname2
			<< " " << (*it).f_markname2 << std::endl;
	}*/

	int type = 1;
	//test->insert_msg("1", "2", "1", "王子龙");
	/*test->new_msg("1", "2026-02-02 17:46:12", ret);
	std::vector<msg_text> msgs;
	msgs = textUNPACK(ret);
	for (auto it = msgs.begin(); it != msgs.end(); it++)
	{
		std::cout << "Sender: " << (*it).f_senderid
			<< " , Target:" << (*it).f_targetid
			<< " , msgtype:" << (*it).f_msgtype
			<< " , msg: " << (*it).f_msgcontent
			<< " , url: " << (*it).f_url
			<< " , createtime:" << (*it).f_create_time << std::endl;
	}*/

	//test->search_msg("1",ret);

	//free(test);

	TcpServer server;
	server.initSocket();
	server.bindPort(nullptr, 4567);
	server.listenMsg(5);
	server.server_start();

	//控制线程
	std::thread t1(cmdThread);
	t1.detach();

	while (g_bRun)
	{
		server.onRun();
	}

	server.close_serverSocket();

	system("pause");
	return 0;
}