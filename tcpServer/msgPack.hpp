#ifndef _msgPack_hpp_
#define _msgPack_hpp_

#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

//	用户数据
struct user_data
{
	std::string uid;
	std::string uname;
	std::string upassword;
	std::string upicture;
};

//	用户关系信息
struct user_relation
{
	std::string f_user_id1;
	std::string f_user_id2;

	//	用户1给用户2的分组名和备注名
	std::string f_teamname1;
	std::string f_markname1;

	//	用户2给用户1的分组名和备注名
	std::string f_teamname2;
	std::string f_markname2;
};

//	文本聊天信息
struct msg_text
{
	/*std::string f_senderid;
	std::string f_targetid;*/
	int f_senderid;
	int f_targetid;
	//std::string f_msgtype = "1";
	std::string f_msgtype;
	std::string f_msgcontent;
	//std::string f_url = "";
	std::string f_url;
	std::string f_create_time;
};

//	用户数据打包
json userPACK(const user_data& udata)
{
	//	创建json对象
	json userJson;
	userJson["uid"] = udata.uid;
	userJson["uname"] = udata.uname;
	userJson["upassword"] = udata.upassword;
	userJson["upicture"] = udata.upicture;

	return userJson;
}

//	用户数据解包
user_data userUNPACK(const json& userJson)
{
	user_data udata;

	// 从json对象中提取数据并赋值给结构体
	udata.uid = userJson["uid"];
	udata.uname = userJson["uname"];
	udata.upassword = userJson["upassword"];
	udata.upicture = userJson["upicture"];

	return udata;
}

//	单个用户关系打包
json singalrelPACK(const user_relation& urel)
{
	json relJson; 
	relJson["f_user_id1"] = urel.f_user_id1;
	relJson["f_user_id2"] = urel.f_user_id2;
	relJson["f_teamname1"] = urel.f_teamname1;
	relJson["f_markname1"] = urel.f_markname1;
	relJson["f_teamname2"] = urel.f_teamname2;
	relJson["f_markname2"] = urel.f_markname2;

	return relJson;
}

//	单个用户关系解包
user_relation singalrelUNPACK(const json& relJson)
{
	user_relation urel;

	// 从json对象中提取数据并赋值给结构体
	urel.f_user_id1 = relJson["f_user_id1"];
	urel.f_user_id2 = relJson["f_user_id2"];
	urel.f_teamname1 = relJson["f_teamname1"];
	urel.f_markname1 = relJson["f_markname1"];
	urel.f_teamname2 = relJson["f_teamname2"];
	urel.f_markname2 = relJson["f_markname2"];

	return urel;
}

//	全部用户关系打包
json relsPACK(const std::vector<user_relation>& urels)
{
	//	创建json数组
	json j = json::array();

	for (std::vector<user_relation>::const_iterator it = urels.begin(); it != urels.end(); it++)
	{
		json relJson;
		relJson["f_user_id1"] = (*it).f_user_id1;
		relJson["f_user_id2"] = (*it).f_user_id2;
		relJson["f_teamname1"] = (*it).f_teamname1;
		relJson["f_markname1"] = (*it).f_markname1;
		relJson["f_teamname2"] = (*it).f_teamname2;
		relJson["f_markname2"] = (*it).f_markname2;
		j.push_back(relJson);
	}
	return j;
}

// 全部用户关系解包
std::vector<user_relation> relsUNPACK(const json& j)
{
	std::vector<user_relation> urels;

	// 检查输入是否为json数组
	if (!j.is_array()) {
		std::cerr << "Input is not a JSON array" << std::endl;
		//throw std::runtime_error("Input is not a JSON array");
	}

	// 遍历json数组
	for(auto it=j.begin();it!=j.end();it++)
	{
		user_relation urel;

		// 从json对象中提取数据并赋值给结构体
		urel.f_user_id1 = (*it)["f_user_id1"];
		urel.f_user_id2 = (*it)["f_user_id2"];
		urel.f_teamname1 = (*it)["f_teamname1"];
		urel.f_markname1 = (*it)["f_markname1"];
		urel.f_teamname2 = (*it)["f_teamname2"];
		urel.f_markname2 = (*it)["f_markname2"];

		urels.push_back(urel);
	}

	return urels;
}

//	单文本打包
json singalMsg(const msg_text& msg)
{
	json userJson;
	userJson["f_senderid"] = msg.f_senderid;
	userJson["f_targetid"] = msg.f_targetid;
	userJson["f_msgtype"] = msg.f_msgtype;
	userJson["f_msgcontent"] = msg.f_msgcontent;
	userJson["f_url"] = msg.f_url;
	userJson["f_create_time"] = msg.f_create_time;

	return userJson;
}

//	单文本解包
msg_text singalMsgUNPack(const json& j)
{
	msg_text msg;

	// 从json对象中提取数据并赋值给结构体
	msg.f_senderid = j["f_senderid"];
	msg.f_targetid = j["f_targetid"];
	msg.f_msgtype = j["f_msgtype"];
	msg.f_msgcontent = j["f_msgcontent"];
	msg.f_url = j["f_url"];
	msg.f_create_time = j["f_create_time"];

	return msg;
}

//	文本聊天打包
json textPACK(const std::vector<msg_text>& msg)
{
	//	创建json数组
	json j = json::array();

	for (std::vector<msg_text>::const_iterator it = msg.begin(); it != msg.end(); it++)
	{
		json userJson;
		userJson["f_senderid"] = (*it).f_senderid;
		userJson["f_targetid"] = (*it).f_targetid;
		userJson["f_msgtype"] = (*it).f_msgtype;
		userJson["f_msgcontent"] = (*it).f_msgcontent;
		userJson["f_url"] = (*it).f_url;
		userJson["f_create_time"] = (*it).f_create_time;

		j.push_back(userJson);
	}
	
	return j;
}

// 文本聊天解包
std::vector<msg_text> textUNPACK(const json& j)
{
	std::vector<msg_text> umsgs;

	// 检查输入是否为json数组
	if (!j.is_array()) {
		std::cerr << "Input is not a JSON array" << std::endl;
		//throw std::runtime_error("Input is not a JSON array");
	}

	// 遍历json数组
	for (auto it = j.begin(); it != j.end(); it++)
	{
		msg_text msg;

		// 从json对象中提取数据并赋值给结构体
		msg.f_senderid = (*it)["f_senderid"];
		msg.f_targetid = (*it)["f_targetid"];
		msg.f_msgtype = (*it)["f_msgtype"];
		msg.f_msgcontent = (*it)["f_msgcontent"];
		msg.f_url = (*it)["f_url"];
		msg.f_create_time = (*it)["f_create_time"];

		umsgs.push_back(msg);
	}

	return umsgs;
}

#endif