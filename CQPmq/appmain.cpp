/*
* CoolQ MQ for VC++ 
* Api Version 9
* Written by everpcpc, based on cqsdk-vc
*/

#include "stdafx.h"
#include <cinttypes>
#include <string>
#include <sstream>
#include <process.h>
#include "cqp.h"
#include "beanstalk.h"
#include "base64.h"
#include "appmain.h" //Ӧ��AppID����Ϣ������ȷ��д�������Q�����޷�����

using namespace std;
using namespace Beanstalk;

int ac = -1; //AuthCode ���ÿ�Q�ķ���ʱ��Ҫ�õ�
bool enabled = false;

int64_t qq = -1;
char log_buf[1024];

unsigned tid;
HANDLE thd;

Client btdclient;
string SERVER_ADDR = "127.0.0.1";
int SERVER_PORT = 11300;

string in_q = "coolq_in";
string out_q = "coolq_out";


bool ensure_mq_connected() {
	if (btdclient.is_connected()) { return TRUE; }

	CQ_addLog(ac, CQLOG_WARNING, "net", "try reconnect");
	try {
		btdclient.reconnect();
		btdclient.use(out_q);
		btdclient.watch(in_q);
		CQ_addLog(ac, CQLOG_INFO, "net", "reconnect OK");

	}
	catch (runtime_error &e) {
		sprintf_s(log_buf, "reconnect failed: %s", e.what());
		CQ_addLog(ac, CQLOG_WARNING, "net", log_buf);
		return FALSE;
	}

	return TRUE;
}

bool send_to_mq(const char* msg) {
	if (!ensure_mq_connected()) {
		CQ_addLog(ac, CQLOG_WARNING, "net", "no connection, will not send msg");
		return FALSE;
	}

	sprintf_s(log_buf, "send: %s", msg);
	CQ_addLog(ac, CQLOG_DEBUG, "info", log_buf);
	try {
		btdclient.put(msg);
	}
	catch (runtime_error &e) {
		sprintf_s(log_buf, "send failed: %s", e.what());
		CQ_addLog(ac, CQLOG_WARNING, "net", log_buf);
		btdclient.disconnect();
		return FALSE;
	}

	return TRUE;
}

bool process_msg(string msg) {
	bool rc = FALSE;
	string msg_type, data;
	int64_t to;
	istringstream istr(msg);
	istr >> msg_type;
	char* buffer = new char[1024];

	if (msg_type == "sendPrivateMsg") {
		istr >> to;
		istr >> data;
		Base64decode(buffer, data.c_str());
		if (CQ_sendPrivateMsg(ac, to, buffer) == 0) rc = TRUE;
	}
	else if (msg_type == "sendGroupMsg") {
		istr >> to;
		istr >> data;
		Base64decode(buffer, data.c_str());
		if (CQ_sendGroupMsg(ac, to, buffer) == 0) rc = TRUE;
	}
	else {
		sprintf_s(log_buf, "msg type not supported: %s", msg_type.c_str());
		CQ_addLog(ac, CQLOG_DEBUG, "info", log_buf);
	}

	delete[] buffer;
	return rc;
}

unsigned __stdcall get_from_mq(void *args) {

	CQ_addLog(ac, CQLOG_DEBUG, "net", "start receiving from mq");
	while (TRUE) {
		if (!ensure_mq_connected()) {
			Sleep(10000);
			continue;
		}

		Job job;
		try {
			btdclient.reserve(job, 60);  // 60s timeout for poll
		}
		catch (runtime_error &e) {
			sprintf_s(log_buf, "recv failed: %s", e.what());
			CQ_addLog(ac, CQLOG_WARNING, "net", log_buf);
			btdclient.disconnect();
			Sleep(3000);
			continue;
		}

		if (job.id() == 0) { continue; }

		sprintf_s(log_buf, "recv: %s", job.body().c_str());
		CQ_addLog(ac, CQLOG_DEBUG, "info", log_buf);

		if (process_msg(job.body())) {
			btdclient.del(job);
		};
	}

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
		CQ_addLog(ac, CQLOG_INFO, "info", "config file absent, will generate");
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

	char *buffer = new char[50];
	GetPrivateProfileStringA("btd", "addr", "-1", buffer, 64, configFile.data());
	if (strcmp("-1", buffer)) {
		stringstream ss;
		ss << buffer;
		ss >> SERVER_ADDR;
	}
	else {
		WritePrivateProfileStringA("btd", "addr", SERVER_ADDR.c_str(), configFile.data());
	}
	delete[] buffer;

	return 0;
}

/* 
* ����Ӧ�õ�ApiVer��Appid������󽫲������
*/
CQEVENT(const char*, AppInfo, 0)() {
	return CQAPPINFO;
}


/* 
* ����Ӧ��AuthCode����Q��ȡӦ����Ϣ��������ܸ�Ӧ�ã���������������������AuthCode��
* ��Ҫ�ڱ��������������κδ��룬���ⷢ���쳣���������ִ�г�ʼ����������Startup�¼���ִ�У�Type=1001����
*/
CQEVENT(int32_t, Initialize, 4)(int32_t AuthCode) {
	ac = AuthCode;
	return 0;
}


/*
* Type=1001 ��Q����
* ���۱�Ӧ���Ƿ����ã������������ڿ�Q������ִ��һ�Σ���������ִ��Ӧ�ó�ʼ�����롣
* ��Ǳ�Ҫ����������������ش��ڡ���������Ӳ˵������û��ֶ��򿪴��ڣ�
*/
CQEVENT(int32_t, __eventStartup, 0)() {

	CQ_addLog(ac, CQLOG_DEBUG, "info", "start miao~");

	return 0;
}


/*
* Type=1002 ��Q�˳�
* ���۱�Ӧ���Ƿ����ã������������ڿ�Q�˳�ǰִ��һ�Σ���������ִ�в���رմ��롣
* ������������Ϻ󣬿�Q���ܿ�رգ��벻Ҫ��ͨ���̵߳ȷ�ʽִ���������롣
*/
CQEVENT(int32_t, __eventExit, 0)() {
	TerminateThread(thd, 0);  // FIXME
	btdclient.disconnect();
	return 0;
}

/*
* Type=1003 Ӧ���ѱ�����
* ��Ӧ�ñ����ú󣬽��յ����¼���
* �����Q����ʱӦ���ѱ����ã�����_eventStartup(Type=1001,��Q����)�����ú󣬱�����Ҳ��������һ�Ρ�
* ��Ǳ�Ҫ����������������ش��ڡ���������Ӳ˵������û��ֶ��򿪴��ڣ�
*/
CQEVENT(int32_t, __eventEnable, 0)() {
	enabled = true;

	qq = CQ_getLoginQQ(ac);
	if (qq < 0) {
		CQ_addLog(ac, CQLOG_ERROR, "info", "cannot get current qq");
		return -1;
	}

	sprintf_s(log_buf, "login with: %" PRId64, qq);
	CQ_addLog(ac, CQLOG_DEBUG, "info", log_buf);

	try {
		read_config();
	}
	catch (runtime_error &e) {
		sprintf_s(log_buf, "read config failed��will use default: %s", e.what());
		CQ_addLog(ac, CQLOG_WARNING, "info", log_buf);
	}

	sprintf_s(log_buf, "try connect: %s:%d...", SERVER_ADDR.c_str(), SERVER_PORT);
	CQ_addLog(ac, CQLOG_DEBUG, "net", log_buf);

	out_q = to_string(qq) + "_out";
	in_q = to_string(qq) + "_in";

	if (btdclient.is_connected()) {
		btdclient.disconnect();
	}

	try {
		btdclient.connect(SERVER_ADDR, SERVER_PORT);
		btdclient.use(out_q);
		btdclient.watch(in_q);
	}
	catch (runtime_error &e) {
		sprintf_s(log_buf, "connect failed: %s", e.what());
		CQ_addLog(ac, CQLOG_ERROR, "net", log_buf);
		return -1;
	}

	sprintf_s(log_buf, "connect success, use: %s, listen: %s", out_q.c_str(), in_q.c_str());
	CQ_addLog(ac, CQLOG_DEBUG, "net", log_buf);

	thd = (HANDLE)_beginthreadex(NULL, 0, &get_from_mq, NULL, 0, &tid);

	return 0;
}


/*
* Type=1004 Ӧ�ý���ͣ��
* ��Ӧ�ñ�ͣ��ǰ�����յ����¼���
* �����Q����ʱӦ���ѱ�ͣ�ã��򱾺���*����*�����á�
* ���۱�Ӧ���Ƿ����ã���Q�ر�ǰ��������*����*�����á�
*/
CQEVENT(int32_t, __eventDisable, 0)() {
	enabled = false;
	TerminateThread(thd, 0);  // FIXME
	btdclient.disconnect();
	return 0;
}


/*
* Type=21 ˽����Ϣ
* subType �����ͣ�11/���Ժ��� 1/��������״̬ 2/����Ⱥ 3/����������
*/
CQEVENT(int32_t, __eventPrivateMsg, 24)(int32_t subType, int32_t sendTime, int64_t fromQQ, const char *msg, int32_t font) {

	//���Ҫ�ظ���Ϣ������ÿ�Q�������ͣ��������� return EVENT_BLOCK - �ضϱ�����Ϣ�����ټ�������  ע�⣺Ӧ�����ȼ�����Ϊ"���"(10000)ʱ������ʹ�ñ�����ֵ
	//������ظ���Ϣ������֮���Ӧ��/�������������� return EVENT_IGNORE - ���Ա�����Ϣ

	stringstream ss;
	char* buffer = new char[1024];
	Base64encode(buffer, msg, sizeof(msg));

	ss << "eventPrivateMsg" << " ";
	ss << subType << " ";
	ss << sendTime << " ";
	ss << fromQQ << " ";
	ss << buffer << " ";
	ss << font << " ";

	send_to_mq(ss.str().c_str());

	delete[] buffer;
	return EVENT_IGNORE;
}


/*
* Type=2 Ⱥ��Ϣ
*/
CQEVENT(int32_t, __eventGroupMsg, 36)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, const char *fromAnonymous, const char *msg, int32_t font) {

	stringstream ss;
	char* buffer = new char[1024];
	Base64encode(buffer, msg, sizeof(msg));

	ss << "eventGroupMsg" << " ";
	ss << subType << " ";
	ss << sendTime << " ";
	ss << fromGroup << " ";
	ss << fromQQ << " ";
	ss << fromAnonymous << " ";
	ss << buffer << " ";
	ss << font << " ";

	send_to_mq(ss.str().c_str());

	delete[] buffer;
	return EVENT_IGNORE; //���ڷ���ֵ˵��, ����_eventPrivateMsg������
}


/*
* Type=4 ��������Ϣ
*/
CQEVENT(int32_t, __eventDiscussMsg, 32)(int32_t subType, int32_t sendTime, int64_t fromDiscuss, int64_t fromQQ, const char *msg, int32_t font) {

	stringstream ss;
	char* buffer = new char[1024];
	Base64encode(buffer, msg, sizeof(msg));

	ss << "eventDiscussMsg" << " ";
	ss << subType << " ";
	ss << sendTime << " ";
	ss << fromDiscuss << " ";
	ss << fromQQ << " ";
	ss << buffer << " ";
	ss << font << " ";

	send_to_mq(ss.str().c_str());

	delete[] buffer;
	return EVENT_IGNORE; //���ڷ���ֵ˵��, ����_eventPrivateMsg������
}


/*
* Type=101 Ⱥ�¼�-����Ա�䶯
* subType �����ͣ�1/��ȡ������Ա 2/�����ù���Ա
*/
CQEVENT(int32_t, __eventSystem_GroupAdmin, 24)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t beingOperateQQ) {

	return EVENT_IGNORE; //���ڷ���ֵ˵��, ����_eventPrivateMsg������
}


/*
* Type=102 Ⱥ�¼�-Ⱥ��Ա����
* subType �����ͣ�1/ȺԱ�뿪 2/ȺԱ���� 3/�Լ�(����¼��)����
* fromQQ ������QQ(��subTypeΪ2��3ʱ����)
* beingOperateQQ ������QQ
*/
CQEVENT(int32_t, __eventSystem_GroupMemberDecrease, 32)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, int64_t beingOperateQQ) {

	return EVENT_IGNORE; //���ڷ���ֵ˵��, ����_eventPrivateMsg������
}


/*
* Type=103 Ⱥ�¼�-Ⱥ��Ա����
* subType �����ͣ�1/����Ա��ͬ�� 2/����Ա����
* fromQQ ������QQ(������ԱQQ)
* beingOperateQQ ������QQ(����Ⱥ��QQ)
*/
CQEVENT(int32_t, __eventSystem_GroupMemberIncrease, 32)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, int64_t beingOperateQQ) {

	return EVENT_IGNORE; //���ڷ���ֵ˵��, ����_eventPrivateMsg������
}


/*
* Type=201 �����¼�-���������
*/
CQEVENT(int32_t, __eventFriend_Add, 16)(int32_t subType, int32_t sendTime, int64_t fromQQ) {

	return EVENT_IGNORE; //���ڷ���ֵ˵��, ����_eventPrivateMsg������
}


/*
* Type=301 ����-�������
* msg ����
* responseFlag ������ʶ(����������)
*/
CQEVENT(int32_t, __eventRequest_AddFriend, 24)(int32_t subType, int32_t sendTime, int64_t fromQQ, const char *msg, const char *responseFlag) {

	//CQ_setFriendAddRequest(ac, responseFlag, REQUEST_ALLOW, "");

	return EVENT_IGNORE; //���ڷ���ֵ˵��, ����_eventPrivateMsg������
}


/*
* Type=302 ����-Ⱥ���
* subType �����ͣ�1/����������Ⱥ 2/�Լ�(����¼��)������Ⱥ
* msg ����
* responseFlag ������ʶ(����������)
*/
CQEVENT(int32_t, __eventRequest_AddGroup, 32)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, const char *msg, const char *responseFlag) {

	//if (subType == 1) {
	//	CQ_setGroupAddRequestV2(ac, responseFlag, REQUEST_GROUPADD, REQUEST_ALLOW, "");
	//} else if (subType == 2) {
	//	CQ_setGroupAddRequestV2(ac, responseFlag, REQUEST_GROUPINVITE, REQUEST_ALLOW, "");
	//}

	return EVENT_IGNORE; //���ڷ���ֵ˵��, ����_eventPrivateMsg������
}
