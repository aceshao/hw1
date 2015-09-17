#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <string.h>
#include <string>
#include <cstdlib>
#include "manager.h"
#include "config.h"

#include <sys/epoll.h>
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
			cout<<"server file: ["<<m_vecPeerInfo[i]->files[j]<<"]"<<endl;
			if(m_vecPeerInfo[i]->files[j] == filename)
			{
				cout<<"found.....!!!!"<<endl;
				*pi = m_vecPeerInfo[i];
				m_mtxResource->Unlock();
				return 0;				
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
			m_vecPeerInfo.erase(m_vecPeerInfo.begin() + i);
			m_vecPeerInfo.push_back(pi);
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

Manager::Manager()
{

}

Manager::~Manager()
{
	if(m_pSocket)
	{
		delete m_pSocket;
		m_pSocket = NULL;
	}
	if(m_semRequest)
	{
		delete m_semRequest;
		m_semRequest = NULL;
	}
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
	int pid = fork();
	if(pid == -1)
	{
		cout<<"fork failed"<<endl;
		return -1;
	}
	else if (pid == 0)
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

int Manager::Init()
{
	m_pSocket = new Socket(serverIp.c_str(), serverPort, ST_TCP);
	m_semRequest = new Sem(0, 0);
	m_mtxRequest = new Mutex();
	for(unsigned int i = 0; i < serverThreadNumber; i++)
	{
		Thread* thread = new Thread(Process, this);
		m_vecProcessThread.push_back(thread);
	}
	return 0;
}

int Manager::Listen()
{
	if (m_pSocket->Create() < 0)
	{
		cout<<"socket create failed"<<endl;
		return -1;
	}
	if(m_pSocket->Bind() < 0)
	{
		cout<<"socket bind failed"<<endl;
		return -1;
	}
	if(m_pSocket->Listen() < 0)
	{
		cout<<"socket listen failed"<<endl;
		return -1;
	}
	cout<<"now begin listen"<<endl;
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
				client->Close();
				delete [] szData;
				continue;
			}
		}

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
				cout<<"begin to search file["<< searchFilename<<"]"<<endl;
				PeerInfo* pi = NULL;
				pmgr->m_pResMan.LookUp(searchFilename, &pi);
				cout<<"after lookup"<<endl;

				if(! pi)
					cout<<"not found on server"<<endl;
				// peer node exist && status ok && ack time ok
				//if( pi && pi->ps == ONLINE && time(NULL) - pi->ackTime < 30)
				if( pi && pi->ps == ONLINE )
				{
					// calculate the response packet lenght
					int pkglen = sizeof(MsgPkg) + pi->ip.length() + 4;
					char* szBack = new char[pkglen];
					MsgPkg* msg = (MsgPkg*)szBack;
					msg->msgcmd = MSG_CMD_SEARCH_RESPONSE;
					msg->msglength = pi->ip.length() + 4;
					SearchResponsePkg* resp = (SearchResponsePkg*)(szBack + sizeof(MsgPkg));
					resp->port = pi->port;
					strncpy(resp->ip, pi->ip.c_str(), pi->ip.length());
					client->Send(szBack, pkglen);
					delete [] szBack;
					client->Close();
					break;
				}
				else
				{
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
					cout<<"push_back filename: "<<filenames.substr(index, filenameLen)<<endl;
					index += filenameLen;
				}

				pmgr->m_pResMan.Update(pi);
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








