///////////////////////////////////////////////////////////////////////////////
// socketcanobj.cpp: implementation of the CSocketcanObj class.
//
// This file is part is part of CANAL (CAN Abstraction Layer)
// http://www.vscp.org)
//
// Copyright (C) 2000-2014 
// Ake Hedman, Grodans Paradis AB, <akhe@grodansparadis.com>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include <stdio.h>
#include "unistd.h"
#include "stdlib.h"
#include <string.h>
#include "limits.h"
#include "syslog.h"
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

// Different on Kernel 2.6 and cansocket examples
// currently using locally from can-utils
// TODO remove include form makefile when they are in sync
#include <linux/can.h>
#include <linux/can/raw.h>

#include <signal.h>
#include <ctype.h>
#include <libgen.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <net/if.h>
#include "socketcanobj.h"

// Prototypes
void *workThread(void *p);
bool socketcanToCanal(char * p, PCANALMSG pMsg);

////////////////////////////////////////////////////////////////////////////////
// Construction/Destruction
////////////////////////////////////////////////////////////////////////////////

CSocketcanObj::CSocketcanObj()
{
	strcpy(m_socketcanobj.m_devname, "vcan0");
	dll_init(&m_socketcanobj.m_rcvList, SORT_NONE);
	dll_init(&m_socketcanobj.m_sndList, SORT_NONE);
}

CSocketcanObj::~CSocketcanObj()
{
	close(); // Close comm channel in case its open
	dll_removeAllNodes(&m_socketcanobj.m_rcvList);
	dll_removeAllNodes(&m_socketcanobj.m_sndList);
}


//------------------------------------------------------------------------------
// Open the CAN232 comm port
//
// Parameters are as follows
//
// interface;mask;filter
//
//------------------------------------------------------------------------------

bool CSocketcanObj::open(const char *pDevice, unsigned long flags)
{
	int rv = true;
	char devname[IFNAMSIZ + 1];
	fd_set rdfs;
	struct timeval tv;
	struct sockaddr_can addr;
	struct ifreq ifr;
	struct cmsghdr *cmsg;
	struct canfd_frame frame;
	char ctrlmsg[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32))];
	const int canfd_on = 1;
	
	//----------------------------------------------------------------------
	//	Set default parameters
	//----------------------------------------------------------------------
	strcpy(devname, "vcan0");
	unsigned long nMask = 0;
	unsigned long nFilter = 0;


	//----------------------------------------------------------------------
	//	Accure Mutex
	//----------------------------------------------------------------------
	pthread_attr_t thread_attr;
	pthread_attr_init(&thread_attr);
	pthread_mutex_init(&m_socketcanObjMutex, NULL);

	m_socketcanobj.m_bRun = true;

	//----------------------------------------------------------------------
	//	Parse given parameters
	//----------------------------------------------------------------------
	// Interface
	char *p = strtok((char *) pDevice, ";");
	if (NULL != p) strncpy(devname, p, strlen(p));

	// Mask
	p = strtok(NULL, ";");
	if (NULL != p) {
		if ((NULL != strstr(p, "0x")) || (NULL != strstr(p, "0X"))) {
			sscanf(p + 2, "%lx", &nMask);
		} else {
			nMask = atol(p);
		}
	}

	// Filter
	p = strtok(NULL, ";");
	if (NULL != p) {
		if ((NULL != strstr(p, "0x")) || (NULL != strstr(p, "0X"))) {
			sscanf(p + 2, "%lx", &nFilter);
		} else {
			nFilter = atol(p);
		}
	}


	//----------------------------------------------------------------------
	//
	//----------------------------------------------------------------------

	// Initiate statistics
	m_socketcanobj.m_stat.cntReceiveData = 0;
	m_socketcanobj.m_stat.cntReceiveFrames = 0;
	m_socketcanobj.m_stat.cntTransmitData = 0;
	m_socketcanobj.m_stat.cntTransmitFrames = 0;

	m_socketcanobj.m_stat.cntBusOff = 0;
	m_socketcanobj.m_stat.cntBusWarnings = 0;
	m_socketcanobj.m_stat.cntOverruns = 0;

	//----------------------------------------------------------------------
	// Start thread 
	//----------------------------------------------------------------------
	if (pthread_create(&m_threadId,
			&thread_attr,
			workThread,
			this)) {
		rv = false;
		close();
	}

	//----------------------------------------------------------------------
	// Release the mutex for other threads to use
	//----------------------------------------------------------------------
	pthread_mutex_unlock(&m_socketcanObjMutex);

	return rv;
}


//------------------------------------------------------------------------------
//  close
//------------------------------------------------------------------------------

int CSocketcanObj::close(void)
{
	int rv = 0;

	// Do nothing if already terminated
	if (!m_socketcanobj.m_bRun) return 1;

	// Terminate the thread
	m_socketcanobj.m_bRun = false;

	UNLOCK_MUTEX(m_socketcanObjMutex);
	LOCK_MUTEX(m_socketcanObjMutex);

	// Give the worker thread some time to terminate
	int *trv;
	pthread_join(m_threadId, (void **) &trv);
	pthread_mutex_destroy(&m_socketcanObjMutex);

	::close(m_socketcanobj.m_sock);

	return rv;
}

//------------------------------------------------------------------------------
//  writeMsg
//------------------------------------------------------------------------------

int CSocketcanObj::writeMsg(PCANALMSG pCanalMsg)
{
	int rv = 0;

	// Must be room for the message
	if (m_socketcanobj.m_sndList.nCount < SOCKETCAN_MAX_SNDMSG) {
		if (NULL != pCanalMsg) {
			dllnode *pNode = new dllnode;
			if (NULL != pNode) {
				canalMsg *pnewMsg = new canalMsg;
				pNode->pObject = pnewMsg;
				pNode->pKey = NULL;
				pNode->pstrKey = NULL;
				if (NULL != pnewMsg) {
					memcpy(pnewMsg, pCanalMsg, sizeof( canalMsg));
				}

				LOCK_MUTEX(m_socketcanObjMutex);

				dll_addNode(&m_socketcanobj.m_sndList, pNode);

				UNLOCK_MUTEX(m_socketcanObjMutex);

				rv = true;
			}
		}
	}
	return rv;
}

//------------------------------------------------------------------------------
//  readMsg
//------------------------------------------------------------------------------

int CSocketcanObj::readMsg(canalMsg *pMsg)
{
	int rv = false;

	if ((NULL != m_socketcanobj.m_rcvList.pHead) &&

			(NULL != m_socketcanobj.m_rcvList.pHead->pObject)) {

		LOCK_MUTEX(m_socketcanObjMutex);

		memcpy(pMsg, m_socketcanobj.m_rcvList.pHead->pObject, sizeof( canalMsg));
		dll_removeNode(&m_socketcanobj.m_rcvList, m_socketcanobj.m_rcvList.pHead);

		UNLOCK_MUTEX(m_socketcanObjMutex);

		rv = true;
	}
	return rv;
}

//------------------------------------------------------------------------------
//	setFilter
//------------------------------------------------------------------------------

bool CSocketcanObj::setFilter(unsigned long filter, unsigned long mask)
{
	char buf[ 20 ];
	char szCmd[ 80 ];

	LOCK_MUTEX(m_socketcanObjMutex);

	// TODO

	UNLOCK_MUTEX(m_socketcanObjMutex);

	return true;
}

//------------------------------------------------------------------------------
//	setFilter
//------------------------------------------------------------------------------

bool CSocketcanObj::setFilter(unsigned long filter)
{
	char buf[ 20 ];
	char szCmd[ 80 ];

	LOCK_MUTEX(m_socketcanObjMutex);

	// TODO

	UNLOCK_MUTEX(m_socketcanObjMutex);

	return true;
}

//------------------------------------------------------------------------------
//	setMask
//------------------------------------------------------------------------------

bool CSocketcanObj::setMask(unsigned long mask)
{
	char buf[ 20 ];
	char szCmd[ 80 ];

	LOCK_MUTEX(m_socketcanObjMutex);

	// TODO

	UNLOCK_MUTEX(m_socketcanObjMutex);

	return true;
}

//------------------------------------------------------------------------------
//	dataAvailable
//------------------------------------------------------------------------------

int CSocketcanObj::dataAvailable(void)
{
	int cnt;

	LOCK_MUTEX(m_socketcanObjMutex);
	cnt = dll_getNodeCount(&m_socketcanobj.m_rcvList);
	UNLOCK_MUTEX(m_socketcanObjMutex);

	return cnt;
}

//------------------------------------------------------------------------------
//	getStatistics
//------------------------------------------------------------------------------

bool CSocketcanObj::getStatistics(PCANALSTATISTICS& pCanalStatistics)
{
	pCanalStatistics = &m_socketcanobj.m_stat;
	return true;
}

//------------------------------------------------------------------------------
//	getStatus
//------------------------------------------------------------------------------

bool CSocketcanObj::getStatus(PCANALSTATUS pCanalStatus)
{
	return true;
}

//------------------------------------------------------------------------------
// workThread
//
// The workThread do most of the actual work such as send and receive.
//------------------------------------------------------------------------------

void *workThread(void *pObject)
{
	int rv = 0;
	int sock;
	fd_set rdfs;
	struct timeval tv;
	struct sockaddr_can addr;
	struct ifreq ifr;
	struct cmsghdr *cmsg;
	struct canfd_frame frame;
	char ctrlmsg[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32))];
	const int canfd_on = 1;

	CSocketcanObj * psocketcanobj = (CSocketcanObj *) pObject;
	if (NULL == psocketcanobj) {
		pthread_exit(&rv);
	}
	
	sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (sock < 0) {
		syslog(LOG_ERR,
				"%s",
				(const char *) "CReadSocketCanTread: Error while opening socket. Terminating!");
		return NULL;
	}

	strcpy(ifr.ifr_name, psocketcanobj->m_socketcanobj.m_devname);
	ioctl(sock, SIOCGIFINDEX, &ifr);

	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

#ifdef DEBUG
	printf("using interface name '%s'.\n", ifr.ifr_name);
#endif

	// try to switch the socket into CAN FD mode 
	setsockopt(sock,
			SOL_CAN_RAW,
			CAN_RAW_FD_FRAMES,
			&canfd_on,
			sizeof(canfd_on));

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		syslog(LOG_ERR,
				"%s",
				(const char *) "CReadSocketCanTread: Error in socket bind. Terminating!");
		return NULL;
	}

	while (psocketcanobj->m_socketcanobj.m_bRun) {

		///////////////////////////////////////////////////////////////////////
		//                        Receive 
		///////////////////////////////////////////////////////////////////////

		LOCK_MUTEX(psocketcanobj->m_socketcanObjMutex);

		// Noting to do if we should end...
		if (!psocketcanobj->m_socketcanobj.m_bRun) continue;

		FD_ZERO(&rdfs);
		FD_SET(sock, &rdfs);

		tv.tv_sec = 0;
		tv.tv_usec = 5000; // 50ms timeout 

		int ret;
		if ((ret = select(sock+1, &rdfs, NULL, NULL, &tv)) < 0) {
			// Error
			psocketcanobj->m_socketcanobj.m_bRun = true;
			continue;
		}

		if (ret) {

			// There is data to read

			ret = read(sock, &frame, sizeof(struct can_frame));
			if (ret < 0) {
				psocketcanobj->m_socketcanobj.m_bRun = true;
				continue;
			}

			if (psocketcanobj->m_socketcanobj.m_rcvList.nCount < SOCKETCAN_MAX_RCVMSG) {
				
				PCANALMSG pMsg = new canalMsg;
				if (NULL != pMsg) {
                    pMsg->flags = 0;
					dllnode *pNode = new dllnode;
					if (NULL != pNode) {
							
						pNode->pObject = pMsg;
						
						// Assign the message
						pMsg->flags = 0;
						if (frame.can_id & CAN_RTR_FLAG) pMsg->flags |= CANAL_IDFLAG_RTR;
						if (frame.can_id & CAN_EFF_FLAG) {
							pMsg->id = frame.can_id & CAN_EFF_MASK;
							pMsg->flags |= CANAL_IDFLAG_EXTENDED;
						}
						else {
							pMsg->id = frame.can_id & CAN_SFF_MASK;
						}
						pMsg->sizeData = frame.len;
						memcpy(pMsg->data,frame.data,frame.len);
						dll_addNode(&psocketcanobj->m_socketcanobj.m_rcvList, pNode);
						
						// Update statistics
						psocketcanobj->m_socketcanobj.m_stat.cntReceiveData += pMsg->sizeData;
						psocketcanobj->m_socketcanobj.m_stat.cntReceiveFrames += 1;			
						
					} 
					else {
						delete pMsg;
					}
				}
			} // room in queue
						
			
		}

		UNLOCK_MUTEX(psocketcanobj->m_socketcanObjMutex);

		///////////////////////////////////////////////////////////////////////
		//                          Transmit
		///////////////////////////////////////////////////////////////////////

		LOCK_MUTEX(psocketcanobj->m_socketcanObjMutex);

		if ((NULL != psocketcanobj->m_socketcanobj.m_sndList.pHead) &&
				(NULL != psocketcanobj->m_socketcanobj.m_sndList.pHead->pObject)) {

			canalMsg msg;

			memcpy(&msg, psocketcanobj->m_socketcanobj.m_sndList.pHead->pObject, sizeof( canalMsg));
			dll_removeNode(&psocketcanobj->m_socketcanobj.m_sndList, psocketcanobj->m_socketcanobj.m_sndList.pHead);
			
			frame.can_id = msg.id;
			frame.len = msg.sizeData;
			memcpy(msg.data,frame.data,frame.len);
			if (msg.flags & CANAL_IDFLAG_EXTENDED ) frame.can_id |= CAN_EFF_FLAG;
			if (msg.flags & CANAL_IDFLAG_EXTENDED ) frame.can_id |= CAN_EFF_FLAG;
			
			// Write the data
			int nbytes = write(sock, &frame, sizeof(struct can_frame));

			// Update statistics
			psocketcanobj->m_socketcanobj.m_stat.cntTransmitData += msg.sizeData;
			psocketcanobj->m_socketcanobj.m_stat.cntTransmitFrames += 1;

		} // if there is something to transmit

		UNLOCK_MUTEX(psocketcanobj->m_socketcanObjMutex);

	} // while( psocketcanobj->m_socketcanobj.m_bRun )
	
	pthread_exit(&rv);
}
