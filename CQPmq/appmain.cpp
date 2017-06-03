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
#include "appmain.h" //Ӧ��AppID����Ϣ������ȷ��д�������Q�����޷�����

using namespace std;
using namespace Beanstalk;

int ac = -1; //AuthCode ���ÿ�Q�ķ���ʱ��Ҫ�õ�
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
		CQ_addLog(ac, CQLOG_WARNING, "net", "δ����");
		return -1;
	}
	sprintf_s(log_buf, "����: %s", msg);
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
		CQ_addLog(ac, CQLOG_INFO, "info", "�����ļ������ڣ���Ĭ��ֵ�Զ�����");
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

	CQ_addLog(ac, CQLOG_DEBUG, "info", "�����������~");

	return 0;
}


/*
* Type=1002 ��Q�˳�
* ���۱�Ӧ���Ƿ����ã������������ڿ�Q�˳�ǰִ��һ�Σ���������ִ�в���رմ��롣
* ������������Ϻ󣬿�Q���ܿ�رգ��벻Ҫ��ͨ���̵߳ȷ�ʽִ���������롣
*/
CQEVENT(int32_t, __eventExit, 0)() {

	CQ_addLog(ac, CQLOG_DEBUG, "net", "�Ͽ����ӣ�886~");
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

	read_config();

	qq = CQ_getLoginQQ(ac);
	if (qq < 0) {
		CQ_addLog(ac, CQLOG_ERROR, "info", "��ȡ�����ѵ�¼��QQ����");
		return -1;
	}
	sprintf_s(log_buf, "��¼�� QQ ��Ϊ: %" PRId64, qq);
	CQ_addLog(ac, CQLOG_DEBUG, "info", log_buf);

	sprintf_s(log_buf, "�������ӣ�%s:%d...", SERVER_ADDR, SERVER_PORT);
	CQ_addLog(ac, CQLOG_DEBUG, "net", log_buf);
	try {
		btdclient.connect(SERVER_ADDR, SERVER_PORT);
	}
	catch (runtime_error) {
		CQ_addLog(ac, CQLOG_ERROR, "net", "�����ϣ���ַд��������");
		return -1;
	}
	
	out_q = to_string(qq) + "_out";
	in_q = to_string(qq) + "_in";
	sprintf_s(log_buf, "���ӳɹ���ʹ�ã�%s��������%s", out_q.c_str(), in_q.c_str());
	CQ_addLog(ac, CQLOG_DEBUG, "net", log_buf);
	btdclient.use(out_q);
	btdclient.watch(in_q);

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
	CQ_addLog(ac, CQLOG_DEBUG, "info", "ͣ���ˣ��Ͽ���������");
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
* Type=2 Ⱥ��Ϣ
*/
CQEVENT(int32_t, __eventGroupMsg, 36)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, const char *fromAnonymous, const char *msg, int32_t font) {

	return EVENT_IGNORE; //���ڷ���ֵ˵��, ����_eventPrivateMsg������
}


/*
* Type=4 ��������Ϣ
*/
CQEVENT(int32_t, __eventDiscussMsg, 32)(int32_t subType, int32_t sendTime, int64_t fromDiscuss, int64_t fromQQ, const char *msg, int32_t font) {

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

/*
* �˵������� .json �ļ������ò˵���Ŀ��������
* �����ʹ�ò˵������� .json ���˴�ɾ�����ò˵�
*/
CQEVENT(int32_t, __menuA, 0)() {
	MessageBoxA(NULL, "����menuA�����������봰�ڣ����߽�������������", "" ,0);
	return 0;
}

CQEVENT(int32_t, __menuB, 0)() {
	MessageBoxA(NULL, "����menuB�����������봰�ڣ����߽�������������", "" ,0);
	return 0;
}
