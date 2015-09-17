#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
using namespace std;

// those below are for index server
const string serverIp = "0.0.0.0";
const int serverPort = 5500;
const int serverThreadNumber = 5;
// end

// those below are for peer - client
const int MAX_PKG_LEN = 1000;

//end

// those below are for peer - server
const string peer_serverIp = "0.0.0.0";
const int peer_serverPort = 5555;
const int peer_serverThreadNumber = 5;
const char fileBufferDir[] = "./file/";
//end

// those below are for common use
const int MAX_EPOLL_FD = 30;
const int MSG_HEAD_LEN = 8;

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
