#ifndef _MSG_QUEUE_HPP_
#define _MSG_QUEUE_HPP_

#include"itemMsg.hpp"
#include"cellThread.hpp"
#include"sqlCommand.hpp"

#include <random>
#include <queue>
#include <mutex>
#include <condition_variable>

// 消息队列（接收到的待处理消息）
class MsgQueue {
public:
    MsgQueue()
    {
        _sql = new conSql();
        _sql->buildSqlTable("./test.sql");

        msg_thread.Start(
            nullptr,
            [this](cellThread* pThread) { run(pThread); },
            nullptr
        );
    }

    ~MsgQueue()
    {
        stop();
        delete _sql;
	}

    void push(const msgQueueItem& item) {
        std::lock_guard<std::mutex> lock(msg_mutex);
        msg_queue.push(item);
        msg_cond.notify_one();
    }

    bool pop(msgQueueItem& item) {
        std::unique_lock<std::mutex> lock(msg_mutex);
        msg_cond.wait(lock, [this] { return !msg_queue.empty() || msg_isrun; });
        if (msg_isrun) return false;
        item = msg_queue.front();
        msg_queue.pop();
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(msg_mutex);
        msg_isrun = true;
        msg_cond.notify_all();
    }

private:
    void run(cellThread* pthread)
    {
        while (pthread->isRun())
        {

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
			json t = jsonUNPacket(item->data, item->header->dataLength);
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

private:
    conSql* _sql;
    std::queue<msgQueueItem> msg_queue;
    std::mutex msg_mutex;
    std::condition_variable msg_cond;

    cellThread msg_thread;
    bool msg_isrun = false;
};

#endif