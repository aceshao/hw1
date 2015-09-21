#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <vector>
using namespace std;


// those below are for peer - client
const int MAX_PKG_LEN = 100000;


// those below are for common use
const int MAX_EPOLL_FD = 30;
const int MSG_HEAD_LEN = 8;

const char SPLIT_CHARACTER = '&';

enum MsgCmd
{
	MSG_CMD_ACK,
	MSG_CMD_SEARCH,
	MSG_CMD_SEARCH_RESPONSE,
	MSG_CMD_REGISTER,
	MSG_CMD_DOWNLOAD,
	MSG_CMD_DOWNLOAD_RESPONSE
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
	int port;
	char filename[1];
};

struct DownloadPkg
{
	char file[1];
};

struct MsgPkg
{
	MsgCmd msgcmd;
	int msglength;
};
#pragma pack ()

#endif
