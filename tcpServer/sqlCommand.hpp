#ifndef _SQL_COMMAND_HPP_
#define _SQL_COMMAND_HPP_

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

//	心跳包
#include"sqlHeartbeat.hpp"
//	数据包
#include"msgPack.hpp"

#ifdef _WIN32
#include <mysql.h>

#else
#include <mysql/mysql.h> 
#endif


class conSql
{
private:
	MYSQL m_sql;									//数据库句柄
	MYSQL_RES* sql_res;								//查询结果集 
	MYSQL_ROW sql_row;								//记录结构体
	std::atomic<bool> isConnected = { false };		//连接状态
	std::mutex db_mutex;							//数据库互斥锁

	//	默认数据库配置
	const std::string m_host = "127.0.0.1";
	const std::string m_user = "root";
	const std::string m_password = "123456";
	const std::string m_database = "chat_db";
	const unsigned int m_port = 3306;

	//	心跳检测
	sqlHeartbeat* moniter;

	//	建表文件路径
	const std::string SQLfilepath = "./test.sql";

public:
	conSql():sql_res(nullptr),isConnected(false)
	{
		//	连接mysql
		sql_connect();

		//	开启心跳检测
		moniter = new sqlHeartbeat();
		moniter->start(&m_sql, &isConnected);

		//	初始化数据库
		initDB(SQLfilepath);
	}

	~conSql()
	{
		// 释放结果集
		if (nullptr!=sql_res)
		{
			mysql_free_result(sql_res);
			sql_res = nullptr;
		}

		// 关闭数据库连接
		if (isConnected)
		{
			mysql_close(&m_sql);

			//	停止心跳检测并释放资源
			moniter->stop();
			free (moniter);

			isConnected = false;
		}
	}

	//	创建sql连接
	void sql_connect()
	{
		std::lock_guard<std::mutex> lock(db_mutex);
		//	关闭旧连接
		mysql_close(&m_sql);

		if (nullptr == mysql_init(&m_sql))
		{
			std::cerr << "MySQL初始化失败" << std::endl;
		}

		// 设置连接超时
		unsigned int timeout = 10; // 10秒
		mysql_options(&m_sql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
		mysql_options(&m_sql, MYSQL_OPT_READ_TIMEOUT, &timeout);
		mysql_options(&m_sql, MYSQL_OPT_WRITE_TIMEOUT, &timeout);

		if (mysql_options(&m_sql, MYSQL_SET_CHARSET_NAME, "gbk"))
		{
			std::cerr << "设置字符集失败: " << mysql_error(&m_sql) << std::endl;
		}

		//	连接数据库服务器
		MYSQL* ret = mysql_real_connect(&m_sql, m_host.c_str(), m_user.c_str(), m_password.c_str(), NULL, m_port, NULL, 0);
		if (nullptr == ret)
		{
			std::cerr << "连接数据库服务器失败: " << mysql_error(&m_sql) << std::endl;
		}
		std::cout << "数据库服务器连接成功" << std::endl;
		isConnected = true;
	}

	bool executeSQL(const std::string& filePath)
	{
		std::ifstream file_sql(filePath);
		if (!file_sql.is_open()) {
			std::cerr << "无法打开文件: " << filePath << std::endl;
			return false;
		}
		std::string sql;	//	sql建表语句
		std::string line;	//	文件单行读取

		while (std::getline(file_sql, line)) {
			// 跳过注释和空行
			if (line.empty() || line[0] == '-' || line[0] == '#') {
				continue;
			}

			sql += line;
			// 检测SQL语句结束（以分号结尾）
			if (line.find(';') != std::string::npos) {
				// 执行SQL
				if (mysql_query(&m_sql, sql.c_str()) != 0) {
					std::cerr << "执行SQL失败: " << mysql_error(&m_sql) << std::endl;
					//std::cerr << "SQL: " << sql << std::endl;
					return false;
				}
				sql.clear();
			}
		}
		file_sql.close();
		return true;
	}

	//	建表
	bool buildSqlTable(const std::string& filePath)
	{
		try
		{
			return executeSQL(filePath);

		}catch (const std::exception& e)
		{
			std::cerr << "建表异常: " << e.what() << std::endl;
			return false;
		}
	}

	// 连接数据库
	bool select_db()
	{
		if (0 != mysql_select_db(&m_sql, m_database.c_str()))
		{
			unsigned int error_code = mysql_errno(&m_sql);

			// 错误码为1049时数据库不存在
			if (1049 == error_code)
			{
				std::cerr << "数据库不存在，尝试创建： " << mysql_error(&m_sql) << std::endl;
				//	数据库不存在时创建
				std::string create_str = "create database if not exists ";
				std::string query_str = create_str + m_database;
				if (mysql_query(&m_sql, query_str.c_str()))
				{
					std::cerr << "创建数据库失败: " << mysql_error(&m_sql) << std::endl;
					return false;
				}
				std::cout << "数据库创建成功！" << std::endl;

				//	创建完成后尝试连接
				if (0 != mysql_select_db(&m_sql, m_database.c_str()))
				{
					std::cerr << "数据库连接失败：" << mysql_error(&m_sql) << std::endl;
					return false;
				}
				else
				{
					std::cout << "数据库连接成功" << std::endl;
					return true;
				}
			}
		}
		else
		{
			std::cout << "数据库连接成功" << std::endl;
			return true;
		}
	}

	//	初始化数据库
	bool initDB(const std::string& filePath)
	{
		if (!select_db()) {
			std::cerr << "使用数据库失败" << std::endl;
			return false;
		}

		if (buildSqlTable(filePath))
		{
			std::cout << "表创建成功" << std::endl;
			return true;
		}
		else
		{
			std::cout << "表创建失败" << std::endl;
			return false;
		}
	}

	//	重新连接数据库
	void reConnect()
	{
		if (!isConnected) {
			moniter->pause_heartbeat();
			sql_connect();
			initDB(SQLfilepath);
			moniter->update_mysql_ptr(&m_sql);
		}
	}

	bool check_uid(const int& uid)
	{
		std::string sql = "select count(*) from t_user where f_user_id =";
		sql += uid;
		if (mysql_query(&m_sql, sql.c_str()) != 0) 
		{
			std::cerr << "执行SQL失败: " << mysql_error(&m_sql) << std::endl;
			std::cerr << "SQL: " << sql << std::endl;
			return true;	//执行失败时重新执行
		}
		else
		{
			if ((sql_res = mysql_store_result(&m_sql)) == NULL) return false;

			if ((sql_row = mysql_fetch_row(sql_res)) != NULL)
			{
				if (sql_row[0] == 0)
				{
					mysql_free_result(sql_res);
					return true;
				}
				else
				{
					mysql_free_result(sql_res);
					return false;
				}
			}
		}
	}

	//	验证登录信息
	bool check_user(const user_data& udata)
	{
		std::string uid = udata.uid;
		std::string upassword = udata.upassword;
		std::string sql = "select * from t_user where f_user_id =";
		sql += uid;
		if (mysql_query(&m_sql, sql.c_str()) != 0) 
		{
			std::cerr << "执行SQL失败: " << mysql_error(&m_sql) << std::endl;
			std::cerr << "SQL: " << sql << std::endl;
			return false;
		}
		else
		{
			if ((sql_res = mysql_store_result(&m_sql))==NULL) return false;
			
			if ((sql_row = mysql_fetch_row(sql_res)) != NULL)
			{
				if (sql_row[1] == uid && sql_row[3] == upassword)
				{
					mysql_free_result(sql_res);
					return true;
				}
				else
				{
					mysql_free_result(sql_res);
					return false;
				}
			}
			//	查询结果为空
			else
			{
				mysql_free_result(sql_res);
				return false;
			}
		}
	}

	//	查询用户
	bool search_user(const std::string& uid, json& ret)
	{
		std::string sql = "select * from t_user where f_user_id =";
		sql += uid;
		if (mysql_query(&m_sql, sql.c_str()) != 0) {
			std::cerr << "执行SQL失败: " << mysql_error(&m_sql) << std::endl;
			std::cerr << "SQL: " << sql << std::endl;
			return false;
		}
		else
		{
			// 获取查询结果集
			sql_res = mysql_store_result(&m_sql);

			//	创建用户信息结构体
			user_data ud;

			if (sql_res) 
			{
				if ((sql_row = mysql_fetch_row(sql_res)) != NULL)
				{
					ud.uid = sql_row[1];
					ud.uname = sql_row[2];
					ud.upassword = "";
					ud.upicture = sql_row[4];

					ret = userPACK(ud);

					mysql_free_result(sql_res);
					return true;
				}
			}
			// 释放结果集
			mysql_free_result(sql_res);
			return false;
		}
	}

	//	添加用户
	bool insert_user(const user_data& ud)
	{
		std::string uid = ud.uid;
		std::string uname = ud.uname;
		std::string password = ud.upassword;
		std::string cface = ud.upicture;

		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (!stmt) {
			std::cerr << "初始化预处理语句失败: " << mysql_error(&m_sql) << std::endl;
			return false;
		}

		// SQL语句，? 是占位符
		const char* sql = "INSERT INTO t_user(f_user_id, f_username, f_password, f_custonface) "
			"VALUES (?, ?, ?, ?)";

		// 准备SQL语句
		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
			std::cerr << "准备预处理语句失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 准备绑定参数
		MYSQL_BIND bind[4];
		memset(bind, 0, sizeof(bind));

		// 第1个参数：f_user_id (字符串类型)
		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = (char*)uid.c_str();
		bind[0].buffer_length = uid.length();
		bind[0].length = new unsigned long(uid.length());  // 实际长度

		// 第2个参数：f_username
		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = (char*)uname.c_str();
		bind[1].buffer_length = uname.length();
		bind[1].length = new unsigned long(uname.length());

		// 第3个参数：f_password
		bind[2].buffer_type = MYSQL_TYPE_STRING;
		bind[2].buffer = (char*)password.c_str();
		bind[2].buffer_length = password.length();
		bind[2].length = new unsigned long(password.length());

		// 第4个参数：f_custonface
		bind[3].buffer_type = MYSQL_TYPE_STRING;
		bind[3].buffer = (char*)cface.c_str();
		bind[3].buffer_length = cface.length();
		bind[3].length = new unsigned long(cface.length());

		// 绑定参数
		if (mysql_stmt_bind_param(stmt, bind) != 0) {
			std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
			// 清理资源
			for (int i = 0; i < 4; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 执行语句
		if (mysql_stmt_execute(stmt) != 0) {
			std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < 4; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 获取影响的行数
		//my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
		//std::cout << "成功插入 " << affected_rows << " 行" << std::endl;

		// 清理资源
		for (int i = 0; i < 4; ++i) delete bind[i].length;
		mysql_stmt_close(stmt);
		return true;
	}

	//	修改用户名
	bool update_username(const std::string& u_id, const std::string& u_name)
	{
		std::string uid = u_id;
		std::string uname = u_name;

		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (!stmt) {
			std::cerr << "初始化预处理语句失败: " << mysql_error(&m_sql) << std::endl;
			return false;
		}

		// SQL语句，? 是占位符
		const char* sql = "update t_user set f_username = ? where f_user_id = ? ";

		// 准备SQL语句
		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
			std::cerr << "准备预处理语句失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 准备绑定参数
		MYSQL_BIND bind[2];
		memset(bind, 0, sizeof(bind));

		// 第1个参数：f_username
		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = (char*)uname.c_str();
		bind[0].buffer_length = uname.length();
		bind[0].length = new unsigned long(uname.length());

		// 第1个参数：f_user_id
		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = (char*)uid.c_str();
		bind[1].buffer_length = uid.length();
		bind[1].length = new unsigned long(uid.length());

		// 绑定参数
		if (mysql_stmt_bind_param(stmt, bind) != 0) {
			std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
			// 清理资源
			for (int i = 0; i < 2; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 执行语句
		if (mysql_stmt_execute(stmt) != 0) {
			std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < 2; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 获取影响的行数
		my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
		std::cout << "成功插入 " << affected_rows << " 行" << std::endl;

		// 清理资源
		for (int i = 0; i < 2; ++i) delete bind[i].length;
		mysql_stmt_close(stmt);
		return true;
	}

	//	修改用户密码
	bool update_password(const std::string& u_id, const std::string& password)
	{
		std::string uid = u_id;
		std::string upassword = password;

		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (!stmt) {
			std::cerr << "初始化预处理语句失败: " << mysql_error(&m_sql) << std::endl;
			return false;
		}

		// SQL语句，? 是占位符
		const char* sql = "update t_user set f_password= ? where f_user_id = ? ";

		// 准备SQL语句
		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
			std::cerr << "准备预处理语句失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 准备绑定参数
		MYSQL_BIND bind[2];
		memset(bind, 0, sizeof(bind));

		// 第1个参数：f_username
		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = (char*)upassword.c_str();
		bind[0].buffer_length = upassword.length();
		bind[0].length = new unsigned long(upassword.length());

		// 第1个参数：f_user_id
		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = (char*)uid.c_str();
		bind[1].buffer_length = uid.length();
		bind[1].length = new unsigned long(uid.length());

		// 绑定参数
		if (mysql_stmt_bind_param(stmt, bind) != 0) {
			std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
			// 清理资源
			for (int i = 0; i < 2; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 执行语句
		if (mysql_stmt_execute(stmt) != 0) {
			std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < 2; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 获取影响的行数
		my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
		std::cout << "成功插入 " << affected_rows << " 行" << std::endl;

		// 清理资源
		for (int i = 0; i < 2; ++i) delete bind[i].length;
		mysql_stmt_close(stmt);
		return true;
	}

	//	用户关系表id判断
	int select_uid(const std::string& uid)
	{
		std::string id = uid;

		// 执行查询
		mysql_query(&m_sql, "SELECT f_user_id1, f_user_id2 FROM t_relationship");
		sql_res = mysql_store_result(&m_sql);

		// 分步骤处理结果
		while ((sql_row = mysql_fetch_row(sql_res)) != NULL) {
			std::string id1 = sql_row[0];
			std::string id2 = sql_row[1];

			if (id == id1) {
				return 1;
			}
			else if (id == id2) {
				return 2;
			}
		}	
	}

	//	判断是否有好友关系
	bool if_relation(const std::string& uid1, const std::string& uid2)

	{
		// 使用预处理语句防止SQL注入
		const char* sql = "SELECT COUNT(*) FROM t_relationship WHERE "
			"(f_user_id1 = ? AND f_user_id2 = ?)";

		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (stmt == nullptr) {
			std::cerr << "初始化预处理语句失败" << std::endl;
			return false;
		}

		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
			std::cerr << "预处理语句失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 绑定参数
		MYSQL_BIND bind[2];
		memset(bind, 0, sizeof(bind));

		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = (char*)(uid1.c_str());
		bind[0].buffer_length = uid1.length();
		bind[0].length = new unsigned long(uid1.length());

		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = (char*)(uid2.c_str());
		bind[1].buffer_length = uid2.length();
		bind[1].length = new unsigned long(uid2.length());

		if (mysql_stmt_bind_param(stmt, bind) != 0) {
			std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < 2; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 执行查询
		if (mysql_stmt_execute(stmt) != 0) {
			std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < 2; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 获取结果
		int count = 0;
		MYSQL_BIND result_bind;
		memset(&result_bind, 0, sizeof(result_bind));

		result_bind.buffer_type = MYSQL_TYPE_LONG;
		result_bind.buffer = (char*)& count;
		result_bind.is_null = 0;

		//	获取数据失败时
		if (mysql_stmt_bind_result(stmt, &result_bind) != 0 ||
			mysql_stmt_fetch(stmt) != 0) {
			std::cerr << "获取结果失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 清理资源
		for (int i = 0; i < 2; ++i) delete bind[i].length;
		mysql_stmt_close(stmt);
		return count > 0;
	}

	//	查询用户关系
	bool search_relation(const std::string& uid, json& ret)
	{
		std::string sql = "select * from t_relationship where f_user_id1 =";
		sql += uid + " or f_user_id2 = " + uid;
		if (mysql_query(&m_sql, sql.c_str()) != 0) {
			std::cerr << "执行SQL失败: " << mysql_error(&m_sql) << std::endl;
			std::cerr << "SQL: " << sql << std::endl;
			return false;
		}
		else
		{
			// 获取查询结果集
			sql_res = mysql_store_result(&m_sql);

			if (sql_res) {
				// 获取字段数量
				int num_fields = mysql_num_fields(sql_res);

				std::vector<user_relation> rel_queue;

				// 遍历所有行
				while ((sql_row = mysql_fetch_row(sql_res)) != NULL) 
				{
					user_relation urel;

					// 从json对象中提取数据并赋值给结构体
					urel.f_user_id1 = sql_row[1];
					urel.f_user_id2 = sql_row[2];
					urel.f_teamname1 = sql_row[3];
					urel.f_markname1 = sql_row[4];
					urel.f_teamname2 = sql_row[5];
					urel.f_markname2 = sql_row[6];

					rel_queue.push_back(urel);
				}
				ret = relsPACK(rel_queue);

				// 释放结果集
				mysql_free_result(sql_res);
				return true;
			}
		}
	}

	//	添加用户关系
	bool insert_relation(const user_relation& rel)
	{
		std::string uid1 = rel.f_user_id1;
		std::string uid2 = rel.f_user_id2;

		std::string teamname1 = rel.f_teamname1;
		std::string markname1 = rel.f_markname1;

		std::string teamname2 = rel.f_teamname2;
		std::string markname2 = rel.f_markname2;

		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (!stmt) {
			std::cerr << "初始化预处理语句失败: " << mysql_error(&m_sql) << std::endl;
			return false;
		}

		// SQL语句，? 是占位符
		const char* sql = "INSERT INTO t_relationship(f_user_id1, f_user_id2, f_teamname1, f_markname1, f_teamname2, f_markname2) "
			"VALUES (?, ?, ?, ?, ?, ?)";

		// 准备SQL语句
		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
			std::cerr << "准备预处理语句失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 准备绑定参数
		MYSQL_BIND bind[6];
		memset(bind, 0, sizeof(bind));

		// 第1个参数：f_user_id1 (字符串类型)
		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = (char*)uid1.c_str();
		bind[0].buffer_length = uid1.length();
		bind[0].length = new unsigned long(uid1.length());  // 实际长度

		// 第2个参数：f_user_id2 (字符串类型)
		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = (char*)uid2.c_str();
		bind[1].buffer_length = uid2.length();
		bind[1].length = new unsigned long(uid2.length());  // 实际长度

		// 第3个参数：f_teamname1
		bind[2].buffer_type = MYSQL_TYPE_STRING;
		bind[2].buffer = (char*)teamname1.c_str();
		bind[2].buffer_length = teamname1.length();
		bind[2].length = new unsigned long(teamname1.length());

		// 第4个参数：f_markname1
		bind[3].buffer_type = MYSQL_TYPE_STRING;
		bind[3].buffer = (char*)markname1.c_str();
		bind[3].buffer_length = markname1.length();
		bind[3].length = new unsigned long(markname1.length());

		// 第5个参数：f_teamname2
		bind[4].buffer_type = MYSQL_TYPE_STRING;
		bind[4].buffer = (char*)teamname2.c_str();
		bind[4].buffer_length = teamname2.length();
		bind[4].length = new unsigned long(teamname2.length());

		// 第6个参数：f_markname1
		bind[5].buffer_type = MYSQL_TYPE_STRING;
		bind[5].buffer = (char*)markname2.c_str();
		bind[5].buffer_length = markname2.length();
		bind[5].length = new unsigned long(markname2.length());

		// 绑定参数
		if (mysql_stmt_bind_param(stmt, bind) != 0) {
			std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
			// 清理资源
			for (int i = 0; i < 6; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 执行语句
		if (mysql_stmt_execute(stmt) != 0) {
			std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < 6; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 获取影响的行数
		my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
		std::cout << "成功插入 " << affected_rows << " 行" << std::endl;

		// 清理资源
		for (int i = 0; i < 6; ++i) delete bind[i].length;
		mysql_stmt_close(stmt);
		return true;
	}

	//	修改用户关系
	bool update_relation(const user_relation& rel)
	{
		std::string uid1 = rel.f_user_id1;
		std::string uid2 = rel.f_user_id2;

		std::string teamname1 = rel.f_teamname1;
		std::string markname1 = rel.f_markname1;

		std::string teamname2 = rel.f_teamname2;
		std::string markname2 = rel.f_markname2;

		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (!stmt) {
			std::cerr << "初始化预处理语句失败: " << mysql_error(&m_sql) << std::endl;
			return false;
		}

		const char* sql = "update t_relationship set f_teamname1 = ?, f_markname1 = ?, f_teamname2 = ?, f_markname2 = ?"
			" where (f_user_id1 = ? and f_user_id2 = ?)";

		// 准备SQL语句
		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
			std::cerr << "准备预处理语句失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 准备绑定参数
		MYSQL_BIND bind[6];
		memset(bind, 0, sizeof(bind));

		// 第1个参数：f_teamname1
		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = (char*)teamname1.c_str();
		bind[0].buffer_length = teamname1.length();
		bind[0].length = new unsigned long(teamname1.length());

		// 第2个参数：f_markname1
		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = (char*)markname1.c_str();
		bind[1].buffer_length = markname1.length();
		bind[1].length = new unsigned long(markname1.length());

		// 第3个参数：f_teamname2
		bind[2].buffer_type = MYSQL_TYPE_STRING;
		bind[2].buffer = (char*)teamname2.c_str();
		bind[2].buffer_length = teamname2.length();
		bind[2].length = new unsigned long(teamname2.length());

		// 第4个参数：f_markname1
		bind[3].buffer_type = MYSQL_TYPE_STRING;
		bind[3].buffer = (char*)markname2.c_str();
		bind[3].buffer_length = markname2.length();
		bind[3].length = new unsigned long(markname2.length());

		// 第5个参数：f_user_id1 (字符串类型)
		bind[4].buffer_type = MYSQL_TYPE_STRING;
		bind[4].buffer = (char*)uid1.c_str();
		bind[4].buffer_length = uid1.length();
		bind[4].length = new unsigned long(uid1.length());  // 实际长度

		// 第6个参数：f_user_id2 (字符串类型)
		bind[5].buffer_type = MYSQL_TYPE_STRING;
		bind[5].buffer = (char*)uid2.c_str();
		bind[5].buffer_length = uid2.length();
		bind[5].length = new unsigned long(uid2.length());  // 实际长度

		// 绑定参数
		if (mysql_stmt_bind_param(stmt, bind) != 0) {
			std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
			// 清理资源
			for (int i = 0; i < 6; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 执行语句
		if (mysql_stmt_execute(stmt) != 0) {
			std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < 6; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 获取影响的行数
		my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
		std::cout << "成功插入 " << affected_rows << " 行" << std::endl;

		// 清理资源
		for (int i = 0; i < 6; ++i) delete bind[i].length;
		mysql_stmt_close(stmt);
		return true;
	}

	//	删除用户关系
	bool delete_relation(const user_relation& rel)
	{
		std::string uid1 = rel.f_user_id1;
		std::string uid2 = rel.f_user_id2;

		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (!stmt) {
			std::cerr << "初始化预处理语句失败: " << mysql_error(&m_sql) << std::endl;
			return false;
		}

		const char* sql = "delete from t_relationship where (f_user_id1 = ? and f_user_id2 = ?)";

		// 准备SQL语句
		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
			std::cerr << "准备预处理语句失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 准备绑定参数
		MYSQL_BIND bind[2];
		memset(bind, 0, sizeof(bind));

		// 第1个参数：f_user_id1 (字符串类型)
		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = (char*)uid1.c_str();
		bind[0].buffer_length = uid1.length();
		bind[0].length = new unsigned long(uid1.length());  // 实际长度

		// 第2个参数：f_user_id2 (字符串类型)
		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = (char*)uid2.c_str();
		bind[1].buffer_length = uid2.length();
		bind[1].length = new unsigned long(uid2.length());  // 实际长度

		// 绑定参数
		if (mysql_stmt_bind_param(stmt, bind) != 0) {
			std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
			// 清理资源
			for (int i = 0; i < 2; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 执行语句
		if (mysql_stmt_execute(stmt) != 0) {
			std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < 2; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 获取影响的行数
		my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
		std::cout << "成功插入 " << affected_rows << " 行" << std::endl;

		// 清理资源
		for (int i = 0; i < 2; ++i) delete bind[i].length;
		mysql_stmt_close(stmt);
		return true;
	}

	int select_SoT_uid(const std::string& uid)
	{
		std::string id = uid;

		// 执行查询
		mysql_query(&m_sql, "SELECT f_senderid, f_targetid FROM t_chatmsg where f_senderid = ? or f_targetid = ?");
		sql_res = mysql_store_result(&m_sql);

		// 分步骤处理结果
		while ((sql_row = mysql_fetch_row(sql_res)) != NULL) {
			std::string sid = sql_row[0];
			std::string tid = sql_row[1];

			if (id == sid) {
				return 1;
			}
			else if (id == tid) {
				return 2;
			}
		}
	}

	//	查询聊天记录
	bool search_msg(const std::string uid, json& ret)
	{
		std::string u_id = uid;

		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (!stmt) {
			std::cerr << "初始化预处理语句失败: " << mysql_error(&m_sql) << std::endl;
			return false;
		}

		const char* sql = "select * from t_chatmsg "
			"where f_senderid = ? or f_targetid = ? ";
			//"order by f_create_time DESC";

		// 准备SQL语句
		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
			std::cerr << "准备预处理语句失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 准备绑定参数
		MYSQL_BIND bind[2];
		memset(bind, 0, sizeof(bind));

		// 第1个参数：f_senderid
		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = (char*)u_id.c_str();
		bind[0].buffer_length = u_id.length();
		bind[0].length = new unsigned long(u_id.length());

		// 第2个参数：f_targetid
		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = (char*)u_id.c_str();
		bind[1].buffer_length = u_id.length();
		bind[1].length = new unsigned long(u_id.length());

		// 绑定参数
		if (mysql_stmt_bind_param(stmt, bind) != 0) {
			std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
			// 清理资源
			for (int i = 0; i < 2; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 执行语句
		if (mysql_stmt_execute(stmt) != 0) {
			std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < 2; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}
		// 清理资源
		for (int i = 0; i < 2; ++i) delete bind[i].length;
		
		// 存储结果
		if (mysql_stmt_store_result(stmt) != 0) {
			std::cerr << "存储结果失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 绑定结果
		MYSQL_BIND bind_results[7];
		memset(bind_results, 0, sizeof(bind_results));

		//	获取查询结果
		int id = 0, senderid = 0, targetid = 0;
		char msg_type[33] = { 0 };           // varchar(32)
		char msg_content[65536] = { 0 };     // text，最大64KB
		char url[501] = { 0 };               // varchar(500)
		char create_time[20] = { 0 };        // timestamp格式：YYYY-MM-DD HH:MM:SS

		unsigned long msg_type_len = 0;
		unsigned long msg_content_len = 0;
		unsigned long url_len = 0;
		unsigned long create_time_len = 0;

		//	1、f_id(int)
		bind_results[0].buffer_type = MYSQL_TYPE_LONG;
		bind_results[0].buffer = (char*)&id;
		bind_results[0].is_null = 0;

		//	2、f_senderid(int)
		bind_results[1].buffer_type = MYSQL_TYPE_LONG;
		bind_results[1].buffer = (char*)&senderid;
		bind_results[1].is_null = 0;

		//	3、f_targetid(int)
		bind_results[2].buffer_type = MYSQL_TYPE_LONG;
		bind_results[2].buffer = (char*)&targetid;
		bind_results[2].is_null = 0;

		// 4、f_msgtype (varchar)
		bind_results[3].buffer_type = MYSQL_TYPE_STRING;
		bind_results[3].buffer = msg_type;
		bind_results[3].buffer_length = sizeof(msg_type) - 1; // 保留null终止符空间
		bind_results[3].length = &msg_type_len;
		bind_results[3].is_null = 0;

		//	5、f_msgcontent (text)
		bind_results[4].buffer_type = MYSQL_TYPE_STRING;
		bind_results[4].buffer = msg_content;
		bind_results[4].buffer_length = sizeof(msg_content) - 1; // 保留null终止符空间
		bind_results[4].length = &msg_content_len;
		bind_results[4].is_null = 0;

		//	6、f_url(varchar)
		bind_results[5].buffer_type = MYSQL_TYPE_STRING;
		bind_results[5].buffer = url;
		bind_results[5].buffer_length = sizeof(url) - 1; // 保留null终止符空间
		bind_results[5].length = &url_len;
		bind_results[5].is_null = 0;

		//	7、f_create_time (timestamp)
		bind_results[6].buffer_type = MYSQL_TYPE_STRING;
		bind_results[6].buffer = create_time;
		bind_results[6].buffer_length = sizeof(create_time) - 1; // 保留null终止符空间
		bind_results[6].length = &create_time_len;
		bind_results[6].is_null = 0;

		if (mysql_stmt_bind_result(stmt, bind_results) != 0) {
			std::cerr << "绑定结果失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_free_result(stmt);
			mysql_stmt_close(stmt);
			return false;
		}

		// 获取结果
		while (mysql_stmt_fetch(stmt) == 0)
		{

			// 处理这一行数据
			std::cout << "id: " << id
				<< ", Sender: " << senderid
				<< ", Target: " << targetid
				<<", msgtype: "<< msg_type
				<<", msg: "<<msg_content
				<< ", url: " << url
				<<", createtime: "<< create_time
				<< std::endl;
		}

		// 清理
		mysql_stmt_free_result(stmt);
		mysql_stmt_close(stmt);
		return true;
	}

	//	获取群聊id
	bool get_groupId(const std::string uid, std::vector<int>& groupIds)
	{
		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (!stmt) return false;
		const char* sql = "SELECT f_user_id2 FROM t_relation WHERE (f_user_id1 = ? and f_user_id2 >= 100000)";
		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) 
		{
			mysql_stmt_close(stmt);
			return false;
		}
		MYSQL_BIND bindParam;
		memset(&bindParam, 0, sizeof(bindParam));

		bindParam.buffer_type = MYSQL_TYPE_STRING;
		bindParam.buffer = (char*)uid.c_str();
		bindParam.buffer_length = uid.length();
		bindParam.length = new unsigned long(uid.length());

		if (mysql_stmt_bind_param(stmt, &bindParam) != 0) 
		{
			mysql_stmt_close(stmt);
			return false;
		}
		if (mysql_stmt_execute(stmt) != 0) {
			mysql_stmt_close(stmt);
			return false;
		}

		mysql_stmt_store_result(stmt);
		MYSQL_BIND bindResult;
		memset(&bindResult, 0, sizeof(bindResult));

		int groupId;
		bindResult.buffer_type = MYSQL_TYPE_LONG;
		bindResult.buffer = (char*)&groupId;

		mysql_stmt_bind_result(stmt, &bindResult);
		while (mysql_stmt_fetch(stmt) == 0) 
		{
			groupIds.push_back(groupId);
		}
		mysql_stmt_free_result(stmt);
		mysql_stmt_close(stmt);
		return true;
	}

	//	查询聊天记录
	bool new_msg(const std::string uid, const std::string time_stamp, json& ret)
	{
		std::string u_id = uid;
		std::vector<int> groupIds;
		if (!get_groupId(u_id, groupIds))
		{
			return false;
		}

		std::string sql = "select * from t_chatmsg "
			"where f_create_time > ? and (f_senderid = ? or f_targetid = ?";

		if (!groupIds.empty()) 
		{
			sql += " or f_targetid in (";
			for (size_t i = 0; i < groupIds.size(); ++i) 
			{
				if (i > 0) sql += ",";
				sql += "?";
			}
			sql += ")";
		}
		sql += ")";

		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (!stmt) 
		{
			std::cerr << "初始化预处理语句失败: " << mysql_error(&m_sql) << std::endl;
			return false;
		}

	// 准备SQL语句
		if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length() != 0))
		{
			std::cerr << "准备预处理语句失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 准备绑定参数
		int paramCount = 3 + groupIds.size();
		std::vector<MYSQL_BIND> bind(paramCount);
		memset(bind.data(), 0, sizeof(MYSQL_BIND) * paramCount);


		//	第1个参数：f_create_time
		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = (char*)time_stamp.c_str();
		bind[0].buffer_length = time_stamp.length();
		bind[0].length = new unsigned long(time_stamp.length());

		// 第2个参数：f_senderid
		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = (char*)u_id.c_str();
		bind[1].buffer_length = u_id.length();
		bind[1].length = new unsigned long(u_id.length());

		// 第3个参数：f_targetid
		bind[2].buffer_type = MYSQL_TYPE_STRING;
		bind[2].buffer = (char*)u_id.c_str();
		bind[2].buffer_length = u_id.length();
		bind[2].length = new unsigned long(u_id.length());

		// 参数4... : groupIds (int)
		for (size_t i = 0; i < groupIds.size(); ++i) {
			bind[3 + i].buffer_type = MYSQL_TYPE_LONG;
			bind[3 + i].buffer = (char*)&groupIds[i];
		}

		// 绑定参数
		if (mysql_stmt_bind_param(stmt, bind.data()) != 0) {
			std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
			// 清理资源
			for (int i = 0; i < paramCount; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 执行语句
		if (mysql_stmt_execute(stmt) != 0) {
			std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < paramCount; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}
		// 清理资源
		for (int i = 0; i < paramCount; ++i) delete bind[i].length;

		// 存储结果
		if (mysql_stmt_store_result(stmt) != 0) {
			std::cerr << "存储结果失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 绑定结果
		MYSQL_BIND bind_results[7];
		memset(bind_results, 0, sizeof(bind_results));

		//	获取查询结果
		int id = 0, senderid = 0, targetid = 0;
		char msg_type[33] = { 0 };           // varchar(32)
		char msg_content[65536] = { 0 };     // text，最大64KB
		char url[501] = { 0 };               // varchar(500)
		char create_time[20] = { 0 };        // timestamp格式：YYYY-MM-DD HH:MM:SS

		unsigned long msg_type_len = 0;
		unsigned long msg_content_len = 0;
		unsigned long url_len = 0;
		unsigned long create_time_len = 0;

		//	1、f_id(int)
		bind_results[0].buffer_type = MYSQL_TYPE_LONG;
		bind_results[0].buffer = (char*)&id;
		bind_results[0].is_null = 0;

		//	2、f_senderid(int)
		bind_results[1].buffer_type = MYSQL_TYPE_LONG;
		bind_results[1].buffer = (char*)&senderid;
		bind_results[1].is_null = 0;

		//	3、f_targetid(int)
		bind_results[2].buffer_type = MYSQL_TYPE_LONG;
		bind_results[2].buffer = (char*)&targetid;
		bind_results[2].is_null = 0;

		// 4、f_msgtype (varchar)
		bind_results[3].buffer_type = MYSQL_TYPE_STRING;
		bind_results[3].buffer = msg_type;
		bind_results[3].buffer_length = sizeof(msg_type) - 1; // 保留null终止符空间
		bind_results[3].length = &msg_type_len;
		bind_results[3].is_null = 0;

		//	5、f_msgcontent (text)
		bind_results[4].buffer_type = MYSQL_TYPE_STRING;
		bind_results[4].buffer = msg_content;
		bind_results[4].buffer_length = sizeof(msg_content) - 1; // 保留null终止符空间
		bind_results[4].length = &msg_content_len;
		bind_results[4].is_null = 0;

		//	6、f_url(varchar)
		bind_results[5].buffer_type = MYSQL_TYPE_STRING;
		bind_results[5].buffer = url;
		bind_results[5].buffer_length = sizeof(url) - 1; // 保留null终止符空间
		bind_results[5].length = &url_len;
		bind_results[5].is_null = 0;

		//	7、f_create_time (timestamp)
		bind_results[6].buffer_type = MYSQL_TYPE_STRING;
		bind_results[6].buffer = create_time;
		bind_results[6].buffer_length = sizeof(create_time) - 1; // 保留null终止符空间
		bind_results[6].length = &create_time_len;
		bind_results[6].is_null = 0;

		if (mysql_stmt_bind_result(stmt, bind_results) != 0) {
			std::cerr << "绑定结果失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_free_result(stmt);
			mysql_stmt_close(stmt);
			return false;
		}

		std::vector<msg_text> umsgs;

		// 获取结果
		while (mysql_stmt_fetch(stmt) == 0)
		{
			msg_text temp;

			// 从json对象中提取数据并赋值给结构体
			//int t = id;
			temp.f_senderid = senderid;
			temp.f_targetid = targetid;
			temp.f_msgtype = msg_type;
			temp.f_msgcontent = msg_content;
			temp.f_url = url;
			temp.f_create_time = create_time;

			umsgs.push_back(temp);
		}

		ret = textPACK(umsgs);
		// 清理
		mysql_stmt_free_result(stmt);
		mysql_stmt_close(stmt);
		return !umsgs.empty();
	}

	//	添加聊天记录
	bool insert_msg(const msg_text& msg)
	{
		std::string sid = std::to_string(msg.f_senderid);
		std::string tid = std::to_string(msg.f_targetid);
		std::string type = msg.f_msgtype;
		std::string msgcontent = msg.f_msgcontent;
		std::string url = msg.f_url;

		MYSQL_STMT* stmt = mysql_stmt_init(&m_sql);
		if (!stmt) {
			std::cerr << "初始化预处理语句失败: " << mysql_error(&m_sql) << std::endl;
			return false;
		}

		//	sql文本
		const char* sql = nullptr;

		//	消息为文本
		if ("1" == type)
		{
			sql = "insert into t_chatmsg(f_senderid, f_targetid, f_msgtype, f_msgcontent) "
				"VALUES (?, ?, ?, ?)";
		}
		//	消息为媒体文件
		else if ("2" == type)
		{
			sql = "insert into t_chatmsg(f_senderid, f_targetid, f_msgtype, f_url) "
				"VALUES (?, ?, ?, ?)";
		}

		// 准备SQL语句
		if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
			std::cerr << "准备预处理语句失败: " << mysql_stmt_error(stmt) << std::endl;
			mysql_stmt_close(stmt);
			return false;
		}

		// 准备绑定参数
		MYSQL_BIND bind[5];
		memset(bind, 0, sizeof(bind));

		// 第1个参数：f_senderid (字符串类型)
		bind[0].buffer_type = MYSQL_TYPE_STRING;
		bind[0].buffer = (char*)sid.c_str();
		bind[0].buffer_length = sid.length();
		bind[0].length = new unsigned long(sid.length());  // 实际长度

		// 第2个参数：f_targetid 
		bind[1].buffer_type = MYSQL_TYPE_STRING;
		bind[1].buffer = (char*)tid.c_str();
		bind[1].buffer_length = tid.length();
		bind[1].length = new unsigned long(tid.length());  // 实际长度

		// 第3个参数：f_msgtype (varchar类型)
		bind[2].buffer_type = MYSQL_TYPE_STRING;
		bind[2].buffer = (char*)type.c_str();
		bind[2].buffer_length = type.length();
		bind[2].length = new unsigned long(type.length());

		// 第4个参数：f_msgcontent (text类型)
		bind[3].buffer_type = MYSQL_TYPE_STRING;
		bind[3].buffer = (char*)msgcontent.c_str();
		bind[3].buffer_length = msgcontent.length();
		bind[3].length = new unsigned long(msgcontent.length());

		// 第5个参数：f_url (varchar类型)
		bind[4].buffer_type = MYSQL_TYPE_STRING;
		bind[4].buffer = (char*)url.c_str();
		bind[4].buffer_length = url.length();
		bind[4].length = new unsigned long(url.length());

		// 绑定参数
		if (mysql_stmt_bind_param(stmt, bind) != 0) {
			std::cerr << "绑定参数失败: " << mysql_stmt_error(stmt) << std::endl;
			// 清理资源
			for (int i = 0; i < 5; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 执行语句
		if (mysql_stmt_execute(stmt) != 0) {
			std::cerr << "执行失败: " << mysql_stmt_error(stmt) << std::endl;
			for (int i = 0; i < 5; ++i) delete bind[i].length;
			mysql_stmt_close(stmt);
			return false;
		}

		// 获取影响的行数
		my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
		std::cout << "成功插入 " << affected_rows << " 行" << std::endl;

		// 清理资源
		for (int i = 0; i < 5; ++i) delete bind[i].length;
		mysql_stmt_close(stmt);
	}
};

#endif