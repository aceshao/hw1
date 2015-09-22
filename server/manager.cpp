#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <string.h>
#include <string>
#include <cstdlib>
#include "manager.h"
#include <sys/epoll.h>
#include "config.h"
#include "tools.h"
#include <arpa/inet.h>
#include <signal.h>

using namespace std;

ResourceManager::ResourceManager()
{
	m_mtxResource = new Mutex();
}

ResourceManager::~ResourceManager()
{
	if(m_mtxResource)
	{
		delete m_mtxResource;
		m_mtxResource = NULL;
	}

	for(unsigned int i = 0; i < m_vecPeerInfo.size(); i++)
	{
		if(m_vecPeerInfo[i])
		{
			delete m_vecPeerInfo[i];
			m_vecPeerInfo[i] = NULL;
		}
	}
}

int ResourceManager::Insert(PeerInfo* pi)
{
	m_mtxResource->Lock();
	m_vecPeerInfo.push_back(pi);
	m_mtxResource->Unlock();
	return 0;
}

int ResourceManager::LookUp(string filename, PeerInfo** pi)
{
	m_mtxResource->Lock();
	for(unsigned int i = 0; i < m_vecPeerInfo.size(); i++)
	{
		for(unsigned int j = 0; j < m_vecPeerInfo[i]->files.size(); j++)
		{
			if(m_vecPeerInfo[i]->files[j] == filename)
			{
				*pi = m_vecPeerInfo[i];
				m_mtxResource->Unlock();
				return 0;				
			}
		}
	}
	m_mtxResource->Unlock();
	return 0;
}

int ResourceManager::LookUp(string filename, vector<PeerInfo*>& vecpi)
{
	m_mtxResource->Lock();
	for(unsigned int i = 0; i < m_vecPeerInfo.size(); i++)
	{
		for(unsigned int j = 0; j < m_vecPeerInfo[i]->files.size(); j++)
		{
			if(m_vecPeerInfo[i]->files[j] == filename)
			{
				cout<<"Found the file: ["<<filename<<"]"<<endl;
				vecpi.push_back(m_vecPeerInfo[i]);
				break;
			}
		}
	}

	m_mtxResource->Unlock();
	return 0;
}

int ResourceManager::Update(PeerInfo* pi)
{
	m_mtxResource->Lock();
	for(unsigned int i = 0; i < m_vecPeerInfo.size(); i++)
	{
		if(pi->ip == m_vecPeerInfo[i]->ip && pi->port == m_vecPeerInfo[i]->port)
		{
			for(unsigned int j = 0; j < pi->files.size(); j++)
			{
				m_vecPeerInfo[i]->files.push_back(pi->files[j]);
			}
			m_mtxResource->Unlock();
			return 0;
		}
	}

	m_mtxResource->Unlock();

	Insert(pi);
	return 0;
}

int ResourceManager::UpdateAckTime(unsigned int time, string ip, int port)
{
	m_mtxResource->Lock();
	for(unsigned int i = 0; i < m_vecPeerInfo.size(); i++)
	{
		if(ip == m_vecPeerInfo[i]->ip && port == m_vecPeerInfo[i]->port)
		{
			m_vecPeerInfo[i]->ackTime = time;
			m_mtxResource->Unlock();
			return 0;
		}
	}
	m_mtxResource->Unlock();
	return 0;
}

Manager::Manager(string  configfile)
{
	m_iPid = 0;
	m_pSocket = NULL;
	m_mtxRequest = NULL;	
	m_semRequest = NULL;
	m_mtxSock = NULL;
	Config* config = Config::Instance();
	if( config->ParseConfig(configfile.c_str(), "SYSTEM") != 0)
	{
		cout<<"parse config file:["<<configfile<<"] failed, exit"<<endl;
		exit(-1);
	}

	m_strServerIp = config->GetStrVal("SYSTEM", "serverip", "0.0.0.0");
	m_iServerPort = config->GetIntVal("SYSTEM", "bindport", 5550);
	m_iServerThreadPoolNum = config->GetIntVal("SYSTEM", "threadnum", 5);
}

Manager::~Manager()
{
	if(m_pSocket)
	{
		delete m_pSocket;
		m_pSocket = NULL;
	}
	//if(m_semRequest)
	//{
	//	delete m_semRequest;
	//	m_semRequest = NULL;
	//}
	if(m_mtxRequest)
	{
		delete m_mtxRequest;
		m_mtxRequest = NULL;
	}
	for(unsigned int i = 0; i < m_vecProcessThread.size(); i++)
	{
		if(m_vecProcessThread[i])
		{
			delete m_vecProcessThread[i];
			m_vecProcessThread[i] = NULL;
		}
	}
}

int Manager::Start()
{	
	m_iPid = fork();
	if(m_iPid == -1)
	{
		cout<<"fork failed"<<endl;
		return -1;
	}
	else if (m_iPid == 0)
	{
		if(Init() < 0)
		{
			cout<<"manager init failed"<<endl;
			return 0;
		}
		if(Listen() < 0)
		{
			cout<<"manager listen failed"<<endl;
			return -1;
		}
		Loop();
	}
	return 0;
}

int  Manager::IsStoped()
{
	int result = ::kill(m_iPID, 0);
	if (0 == result || errno != ESRCH)
	{
		return false;
	}
	else	
	{
		m_iPID=0;
		return true;
	}
}

int Manager::Init()
{
	m_pSocket = new Socket(m_strServerIp.c_str(), m_iServerPort, ST_TCP);
	m_semRequest = new Sem(0, 0);
	m_mtxRequest = new Mutex();
	m_mtxSock = new Mutex();
	for(int i = 0; i < m_iServerThreadPoolNum; i++)
	{
		Thread* thread = new Thread(Process, this);
		m_vecProcessThread.push_back(thread);
	}
	cout<<"server has["<<m_iServerThreadPoolNum<<"] threads to handler client request"<<endl;
	return 0;
}

int Manager::Listen()
{
	int ret = 0;
	if (m_pSocket->Create() < 0)
	{
		cout<<"socket create failed"<<endl;
		return -1;
	}
	if(m_pSocket->SetSockAddressReuse(true) < 0)
	{
		cout<<"set socket address reuse failed"<<endl;
	}
	if((ret = m_pSocket->Bind()) < 0)
	{
		cout<<"socket bind failed, errno["<<errno<<"]"<<endl;
		return -1;
	}
	if(m_pSocket->Listen() < 0)
	{
		cout<<"socket listen failed"<<endl;
		return -1;
	}
	cout<<"server begin listen on ip["<< m_strServerIp<<"] port: ["<< m_iServerPort <<"]" <<endl;
	return 0;
}

int Manager::Loop()
{
	int listenfd = m_pSocket->GetSocket();
	struct epoll_event ev, events[MAX_EPOLL_FD];
	int epfd = epoll_create(MAX_EPOLL_FD);
	ev.data.fd = listenfd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
	while(1)
	{
		int nfds = epoll_wait(epfd, events, MAX_EPOLL_FD, -1);
		if(nfds <= 0) continue;
		cout<<"nfds: "<<nfds<<endl;
		for (int i = 0; i < nfds; i++)
		{
			Socket* s = new Socket();
			int iRet = m_pSocket->Accept(s);
			if(iRet < 0)
			{
				cout<<"socket accept failed"<<endl;
				continue;
			}
			cout<<"new connection accepted"<<endl;
			m_mtxRequest->Lock();
			m_rq.push(s);
			m_semRequest->Post();
			m_mtxRequest->Unlock();
		}
		usleep(100000);
	}
return 0;
}

void* Process(void* arg)
{
	Manager* pmgr = (Manager*)arg;
	while(1)
	{
		pmgr->m_semRequest->Wait();
		pmgr->m_mtxRequest->Lock();
		Socket* client = pmgr->m_rq.front();
		pmgr->m_rq.pop();
		pmgr->m_mtxRequest->Unlock();

		char szBuffer[MSG_HEAD_LEN] = {0};
		//pmgr->m_mtxSock->Lock();
		if(client->Recv(szBuffer, MSG_HEAD_LEN) != MSG_HEAD_LEN)
		{
			cout<<"recv failed"<<endl;
			client->Close();
			continue;
		}
		MsgPkg* msg = (MsgPkg*)szBuffer;
		char* szData = new char[msg->msglength + 1];
		bzero(szData, msg->msglength + 1);
		if(msg->msglength != 0)
		{
			if(client->Recv(szData, msg->msglength) != msg->msglength)
			{
				cout<<"recv failed"<<endl;
		//		pmgr->m_mtxSock->Unlock();
				client->Close();
				delete [] szData;
				continue;
			}
		}
		//pmgr->m_mtxSock->Unlock();

		switch(msg->msgcmd)
		{
			case MSG_CMD_ACK:
			{
				string ip = client->GetIp();
				int port = client->GetPort();
				pmgr->m_pResMan.UpdateAckTime(time(NULL), ip, port);
				client->Close();
				break;
			}
			case MSG_CMD_SEARCH:
			{
				string searchFilename = szData;
				cout<<"Begin to search file["<< searchFilename<<"]"<<endl;
				//PeerInfo* pi = NULL;
				//pmgr->m_pResMan.LookUp(searchFilename, &pi);

				vector<PeerInfo*> vecpi;
				pmgr->m_pResMan.LookUp(searchFilename, vecpi);

				if(vecpi.size() == 0)
				{
					cout<<"File not found on any peer server"<<endl;
					int pkglen = sizeof(MsgPkg);
					char* szBack = new char[pkglen];
					MsgPkg* msg = (MsgPkg*)szBack;
					msg->msgcmd = MSG_CMD_SEARCH_RESPONSE;
					msg->msglength = 0; // length = 0 means there is no available file
					client->Send(szBack, pkglen);
					delete [] szBack;
					client->Close();
					break;
				}

				char* Back = new char[MAX_PKG_LEN];
				MsgPkg* msg = (MsgPkg*)Back;
				msg->msgcmd = MSG_CMD_SEARCH_RESPONSE;
				msg->msglength = 0;
				// peer node exist && status ok && ack time ok
				for(unsigned int i = 0; i < vecpi.size(); i++)
				{
					if( vecpi[i] && vecpi[i]->ps == ONLINE )
					{
						// each ip port pair length consists of: port [4] length of ip [4] and ip length[x]
						strncpy(Back + sizeof(MsgPkg) +  msg->msglength, iTo4ByteString(vecpi[i]->port).c_str(), 4);
						msg->msglength += 4; // FOR THE PORT
						strncpy(Back + sizeof(MsgPkg) +  msg->msglength, &SPLIT_CHARACTER, 1);
						msg->msglength += 1; // FOR SPLIT CHARACTER
						strncpy(Back + sizeof(MsgPkg) +  msg->msglength, vecpi[i]->ip.c_str(), MAX_PKG_LEN - msg->msglength - sizeof(MsgPkg));
						msg->msglength += vecpi[i]->ip.length(); // FOR IP LENGTH
					}
				}
				client->Send(Back, msg->msglength + sizeof(MsgPkg));
				client->Close();
				delete [] Back;
				break;
			}
			case MSG_CMD_REGISTER:
			{
				PeerInfo* pi = new PeerInfo;
				pi->ps= ONLINE;
				pi->ackTime = time(NULL);

				RegisterPkg* reg = (RegisterPkg*)szData;
				pi->ip = client->GetIp();
				pi->port = reg->port;
				cout<<"register ip["<<pi->ip<<"] port["<<pi->port<<"]"<<endl;

				string filenames = reg->filename; 
				int index = 0;
				for(; index < msg->msglength - 4; )
				{
					int filenameLen = atoi(filenames.substr(index, 4).c_str());
					index += 4;
					pi->files.push_back(filenames.substr(index, filenameLen));
					cout<<"Register filename: [" << filenames.substr(index, filenameLen)<<"]"<<endl;
					index += filenameLen;
				}

				pmgr->m_pResMan.Update(pi);

				char* szBack = new char[sizeof(MsgPkg)];
				MsgPkg* msg = (MsgPkg*)szBack;
				msg->msgcmd = MSG_CMD_REGISTER;
				msg->msglength = 0; // which stands for register success
				client->Send(szBack, sizeof(MsgPkg));
				delete [] szBack;
				client->Close();
				break;
			}
			default:
			cout<<"msg not support"<<endl;
			client->Close();
		}
		delete [] szData;
		delete client;
	}
return 0;
}








