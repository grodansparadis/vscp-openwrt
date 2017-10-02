// udpthread.h
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
// 
// This file is part of the VSCP (http://www.vscp.org) 
//
// Copyright (C) 2000-2012 Ake Hedman, Grodans Paradis AB, <akhe@grodansparadis.com>
// 
// This file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this file see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
//
// $RCSfile: daemon_VSCP.h,v $                                       
// $Date: 2005/09/15 16:26:43 $                                  
// $Author: akhe $                                              
// $Revision: 1.2 $ 

#if !defined(UDPTHREAD_H__7D80016B_5EFD_40D5_94E3_6FD9C324CC7B__INCLUDED_)
#define UDPTHREAD_H__7D80016B_5EFD_40D5_94E3_6FD9C324CC7B__INCLUDED_


#include <wx/thread.h>
#include <wx/socket.h>

//#include <strings.h>


class ClientList;
class CControlObject;



/*!
	This class implement a one of thread that look
	for specific events and react on them appropriatly.

*/

class UDPSendThread : public wxThread
{

public:
	
	/// Constructor
	UDPSendThread();

	/// Destructor
	~UDPSendThread();

	/*!
		Thread code entry point
	*/
	virtual void *Entry();

	/*!
		Send the UDP event
		@param pMsg Event to send.
		@return True on success.
	*/
	bool sendUdpEvent( vscpEvent *pEvent );

	/*! 
		called when the thread exits - whether it terminates normally or is
		stopped with Delete() (but not when it is Kill()ed!)
	*/
 	virtual void OnExit();

	/*!
		Termination control
	*/
	bool m_bQuit;

	/*!
		UDP Socket
	*/
	int	m_sendsock;
	wxDatagramSocket *m_pSocket;


	/*!
		Pointer to the control object
	*/
	CControlObject *m_pCtrlObject;

};


/*!
	This class implement a thread that handles
	incoming UDP datagrams
*/

class UDPReceiveThread : public wxThread
{

public:
	
	/// Constructor
	UDPReceiveThread();

	/// Destructor
	virtual ~UDPReceiveThread();

	/*!
		Thread code entry point
	*/
	virtual void *Entry();


	/*! 
		called when the thread exits - whether it terminates normally or is
		stopped with Delete() (but not when it is Kill()ed!)
	*/
	virtual void OnExit();

	/*!
		Termination control
	*/
	bool m_bQuit;

	/*!
		Pointer to clientitem for this object
	*/
	//CClientItem *m_pClientItem;

	/*!
		Pointer to control object.
	*/
	CControlObject *m_pCtrlObject;

};



#endif



