#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
using namespace std;

const string serverIp = "0.0.0.0";
const int serverPort = 5500;
const int serverThreadNumber = 5;

const int MAX_EPOLL_FD = 30;

const int MSG_HEAD_LEN = 6;

enum SocketType
{
    ST_TCP = 1,
    ST_UDP
};

enum MsgCmd
{
	MSG_CMD_ACK,
	MSG_CMD_SEARCH,
	MSG_CMD_SEARCH_RESPONSE,
	MSG_CMD_REGISTER
};

enum PeerStatus
{
	ONLINE = 1,
	OFFLINE
};

struct PeerInfo
{
	PeerStatus ps;
	unsigned int ackTime;
	string ip;
	int port;
	vector<string> files;
};

#pragma pack(1)

struct SearchPkg
{
	char filename[1];

};

struct SearchResponsePkg
{
	int port;
	char ip[1];
};

struct RegisterPkg
{
	char filename[1];
};

struct MsgPkg
{
	MsgCmd msgcmd;
	unsigned int msglength;
};
#pragma pack ()

#endif
