/*
* CoolQ MQ for VC++ 
* Api Version 9
* Written by everpcpc, based on cqsdk-vc
*/

#include "stdafx.h"
#include <cinttypes>
#include <string>
#include <sstream>
#include "cqp.h"
#include "beanstalk.h"
#include "appmain.h" //应用AppID等信息，请正确填写，否则酷Q可能无法加载

using namespace std;
using namespace Beanstalk;

int ac = -1; //AuthCode 调用酷Q的方法时需要用到
bool enabled = false;

int64_t qq = -1;
char log_buf[1024];

Client btdclient;
char* SERVER_ADDR = "127.0.0.1";
int SERVER_PORT = 11300;

string in_q = "coolq_in";
string out_q = "coolq_out";


int send_to_mq(const char* msg) {
	if (!btdclient.is_connected()) {
		CQ_addLog(ac, CQLOG_WARNING, "net", "未连接");
		return -1;
	}
	sprintf_s(log_buf, "发送: %s", msg);
	CQ_addLog(ac, CQLOG_DEBUG, "info", log_buf);
	int64_t id = btdclient.put(msg);
	return 0;
}

int read_config() {
	string configFolder = ".\\app\\" CQAPPID;
	string configFile = configFolder + "\\config.ini";

	if (GetFileAttributesA(configFile.data()) == -1) {
		if (GetFileAttributesA(configFolder.data()) == -1) {
			CreateDirectoryA(configFolder.data(), NULL);
		}
		CloseHandle(CreateFileA(configFile.data(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
		CQ_addLog(ac, CQLOG_INFO, "info", "配置文件不存在，以默认值自动生成");
	}

	int port = GetPrivateProfileIntA("btd", "port", -1, configFile.data());

	if (port != -1) {
		SERVER_PORT = port;
	}
	else {
		string value;
		stringstream ss;
		ss << SERVER_PORT;
		ss >> value;
		WritePrivateProfileStringA("btd", "port", value.data(), configFile.data());
		ss.clear();
		value.clear();
	}

	char *buffer = new char[256];
	GetPrivateProfileStringA("btd", "addr", "localhost", buffer, 64, configFile.data());
	if (strcmp("localhost", buffer)) {
		SERVER_ADDR = buffer;
	}
	else {
		WritePrivateProfileStringA("btd", "addr", SERVER_ADDR, configFile.data());
	}
	delete[] buffer;
	return 0;
}

/* 
* 返回应用的ApiVer、Appid，打包后将不会调用
*/
CQEVENT(const char*, AppInfo, 0)() {
	return CQAPPINFO;
}


/* 
* 接收应用AuthCode，酷Q读取应用信息后，如果接受该应用，将会调用这个函数并传递AuthCode。
* 不要在本函数处理其他任何代码，以免发生异常情况。如需执行初始化代码请在Startup事件中执行（Type=1001）。
*/
CQEVENT(int32_t, Initialize, 4)(int32_t AuthCode) {
	ac = AuthCode;
	return 0;
}


/*
* Type=1001 酷Q启动
* 无论本应用是否被启用，本函数都会在酷Q启动后执行一次，请在这里执行应用初始化代码。
* 如非必要，不建议在这里加载窗口。（可以添加菜单，让用户手动打开窗口）
*/
CQEVENT(int32_t, __eventStartup, 0)() {

	CQ_addLog(ac, CQLOG_DEBUG, "info", "插件启动啦喵~");

	return 0;
}


/*
* Type=1002 酷Q退出
* 无论本应用是否被启用，本函数都会在酷Q退出前执行一次，请在这里执行插件关闭代码。
* 本函数调用完毕后，酷Q将很快关闭，请不要再通过线程等方式执行其他代码。
*/
CQEVENT(int32_t, __eventExit, 0)() {

	CQ_addLog(ac, CQLOG_DEBUG, "net", "断开连接，886~");
	btdclient.disconnect();

	return 0;
}

/*
* Type=1003 应用已被启用
* 当应用被启用后，将收到此事件。
* 如果酷Q载入时应用已被启用，则在_eventStartup(Type=1001,酷Q启动)被调用后，本函数也将被调用一次。
* 如非必要，不建议在这里加载窗口。（可以添加菜单，让用户手动打开窗口）
*/
CQEVENT(int32_t, __eventEnable, 0)() {
	enabled = true;

	read_config();

	qq = CQ_getLoginQQ(ac);
	if (qq < 0) {
		CQ_addLog(ac, CQLOG_ERROR, "info", "获取不到已登录的QQ号呢");
		return -1;
	}
	sprintf_s(log_buf, "登录的 QQ 号为: %" PRId64, qq);
	CQ_addLog(ac, CQLOG_DEBUG, "info", log_buf);

	sprintf_s(log_buf, "尝试连接：%s:%d...", SERVER_ADDR, SERVER_PORT);
	CQ_addLog(ac, CQLOG_DEBUG, "net", log_buf);
	try {
		btdclient.connect(SERVER_ADDR, SERVER_PORT);
	}
	catch (runtime_error) {
		CQ_addLog(ac, CQLOG_ERROR, "net", "连不上，地址写错了喵？");
		return -1;
	}
	
	out_q = to_string(qq) + "_out";
	in_q = to_string(qq) + "_in";
	sprintf_s(log_buf, "连接成功，使用：%s，监听：%s", out_q.c_str(), in_q.c_str());
	CQ_addLog(ac, CQLOG_DEBUG, "net", log_buf);
	btdclient.use(out_q);
	btdclient.watch(in_q);

	return 0;
}


/*
* Type=1004 应用将被停用
* 当应用被停用前，将收到此事件。
* 如果酷Q载入时应用已被停用，则本函数*不会*被调用。
* 无论本应用是否被启用，酷Q关闭前本函数都*不会*被调用。
*/
CQEVENT(int32_t, __eventDisable, 0)() {
	enabled = false;
	CQ_addLog(ac, CQLOG_DEBUG, "info", "停用了，断开连接喵～");
	btdclient.disconnect();
	return 0;
}


/*
* Type=21 私聊消息
* subType 子类型，11/来自好友 1/来自在线状态 2/来自群 3/来自讨论组
*/
CQEVENT(int32_t, __eventPrivateMsg, 24)(int32_t subType, int32_t sendTime, int64_t fromQQ, const char *msg, int32_t font) {

	//如果要回复消息，请调用酷Q方法发送，并且这里 return EVENT_BLOCK - 截断本条消息，不再继续处理  注意：应用优先级设置为"最高"(10000)时，不得使用本返回值
	//如果不回复消息，交由之后的应用/过滤器处理，这里 return EVENT_IGNORE - 忽略本条消息
	stringstream ss;
	ss << 21 << '\36';
	ss << subType << '\36';
	ss << sendTime << '\36';
	ss << fromQQ << '\36';
	ss << msg << '\36';
	ss << font << '\36';

	send_to_mq(ss.str().c_str());

	return EVENT_IGNORE;
}


/*
* Type=2 群消息
*/
CQEVENT(int32_t, __eventGroupMsg, 36)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, const char *fromAnonymous, const char *msg, int32_t font) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=4 讨论组消息
*/
CQEVENT(int32_t, __eventDiscussMsg, 32)(int32_t subType, int32_t sendTime, int64_t fromDiscuss, int64_t fromQQ, const char *msg, int32_t font) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=101 群事件-管理员变动
* subType 子类型，1/被取消管理员 2/被设置管理员
*/
CQEVENT(int32_t, __eventSystem_GroupAdmin, 24)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t beingOperateQQ) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=102 群事件-群成员减少
* subType 子类型，1/群员离开 2/群员被踢 3/自己(即登录号)被踢
* fromQQ 操作者QQ(仅subType为2、3时存在)
* beingOperateQQ 被操作QQ
*/
CQEVENT(int32_t, __eventSystem_GroupMemberDecrease, 32)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, int64_t beingOperateQQ) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=103 群事件-群成员增加
* subType 子类型，1/管理员已同意 2/管理员邀请
* fromQQ 操作者QQ(即管理员QQ)
* beingOperateQQ 被操作QQ(即加群的QQ)
*/
CQEVENT(int32_t, __eventSystem_GroupMemberIncrease, 32)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, int64_t beingOperateQQ) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=201 好友事件-好友已添加
*/
CQEVENT(int32_t, __eventFriend_Add, 16)(int32_t subType, int32_t sendTime, int64_t fromQQ) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=301 请求-好友添加
* msg 附言
* responseFlag 反馈标识(处理请求用)
*/
CQEVENT(int32_t, __eventRequest_AddFriend, 24)(int32_t subType, int32_t sendTime, int64_t fromQQ, const char *msg, const char *responseFlag) {

	//CQ_setFriendAddRequest(ac, responseFlag, REQUEST_ALLOW, "");

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=302 请求-群添加
* subType 子类型，1/他人申请入群 2/自己(即登录号)受邀入群
* msg 附言
* responseFlag 反馈标识(处理请求用)
*/
CQEVENT(int32_t, __eventRequest_AddGroup, 32)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, const char *msg, const char *responseFlag) {

	//if (subType == 1) {
	//	CQ_setGroupAddRequestV2(ac, responseFlag, REQUEST_GROUPADD, REQUEST_ALLOW, "");
	//} else if (subType == 2) {
	//	CQ_setGroupAddRequestV2(ac, responseFlag, REQUEST_GROUPINVITE, REQUEST_ALLOW, "");
	//}

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}

/*
* 菜单，可在 .json 文件中设置菜单数目、函数名
* 如果不使用菜单，请在 .json 及此处删除无用菜单
*/
CQEVENT(int32_t, __menuA, 0)() {
	MessageBoxA(NULL, "这是menuA，在这里载入窗口，或者进行其他工作。", "" ,0);
	return 0;
}

CQEVENT(int32_t, __menuB, 0)() {
	MessageBoxA(NULL, "这是menuB，在这里载入窗口，或者进行其他工作。", "" ,0);
	return 0;
}
