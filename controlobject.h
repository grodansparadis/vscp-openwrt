// ControlObject.h: interface for the CControlObject class.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
//
// This file is part of the VSCP (http://www.vscp.org)
//
// Copyright (C) 2000-2012 Ake Hedman, Grodans Paradis AB, 
// <akhe@grodansparadis.com>
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


#if !defined(AFX_CONTROLOBJECT_H__7D80016B_5EFD_40D5_94E3_6FD9C324CC7B__INCLUDED_)
#define AFX_CONTROLOBJECT_H__7D80016B_5EFD_40D5_94E3_6FD9C324CC7B__INCLUDED_

#ifdef WIN32
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#endif

#include <wx/thread.h>

#include "devicelist.h"
#include "clientlist.h"
#include "interfacelist.h"
#include "userlist.h"
#include "../../vscp/common/vscp.h"
#include "vscpvariable.h"
#include "tcpipclientthread.h"
#include "daemonvscp.h"
#include "udpthread.h"
#include "dm.h"
#include "vscp.h"

#ifdef WIN32
extern "C" {
#include <libwebsockets.h>
}
#endif

#define DAEMON_LOGMSG_NONE                      0
#define DAEMON_LOGMSG_INFO                      1		
#define DAEMON_LOGMSG_NOTICE                    2
#define DAEMON_LOGMSG_WARNING                   3
#define DAEMON_LOGMSG_ERROR                     4
#define DAEMON_LOGMSG_CRITICAL                  5
#define DAEMON_LOGMSG_ALERT                     6
#define DAEMON_LOGMSG_EMERGENCY                 7
#define DAEMON_LOGMSG_DEBUG                     8


#define MAX_ITEMS_RECEIVE_QUEUE                 1021
#define MAX_ITEMS_SEND_QUEUE                    1021
#define MAX_ITEMS_CLIENT_RECEIVE_QUEUE          8191

/*
 * The websocket server shows how to use libwebsockets for one or more
 * websocket protocols in the same server
 *
 * It defines the following websocket protocols:
 *
 *  dumb-increment-protocol:  once the socket is opened, an incrementing
 *				ascii string is sent down it every 50ms.
 *				If you send "reset\n" on the websocket, then
 *				the incrementing number is reset to 0.
 *
 *  lws-mirror-protocol: copies any received packet to every connection also
 *				using this protocol, including the sender
 */

enum websocket_protocols {
	/* always first */
	PROTOCOL_HTTP = 0,

	PROTOCOL_DUMB_INCREMENT,
	PROTOCOL_LWS_MIRROR,

	/* always last */
	DEMO_PROTOCOL_COUNT
};


WX_DECLARE_LIST ( canalMsg, CanalMsgList );
WX_DECLARE_LIST ( vscpEvent, VSCPEventList );


/*!
	This class implement a thread that handles
	client receive events
*/

class clientMsgWorkerThread : public wxThread
{

public:
	
    /// Constructor
    clientMsgWorkerThread();

    /// Destructor
    virtual ~clientMsgWorkerThread();

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
        Pointer to control object.
    */
    CControlObject *m_pCtrlObject;

};



class CControlObject
{

public:

    /*!
        Constructor
    */
    CControlObject ( void );

    /*!
        Destructor
    */
    virtual ~CControlObject ( void );

    /*!
      logMsg
      write log message
    */
    void logMsg( const wxString& wxstr, unsigned char level=DAEMON_LOGMSG_WARNING );

    /*!
        General initialization
    */
		bool init ( wxString& strcfgfile );

    /*!
        Clean up used resources
    */
    bool cleanup ( void );

    /*!
        The main worker thread
    */
    bool run ( void );

    /*!
        Start worker threads for devices
        @return true on success
    */
    bool startDeviceWorkerThreads( void );

    /*!
        Stop worker threads for devices
        @return true on success	
    */
    bool stopDeviceWorkerThreads( void );

    /*!
        Starting daemon worker thread
        @return true on success
    */
    bool startDaemonWorkerThread( void );

    /*!
        Stop daemon worker thread
        @return true on success
    */
    bool stopDaemonWorkerThread( void );

    /*!
        Starting UDP worker threads
        @return true on success
    */
    bool startUdpWorkerThreads( void );

    /*!
        Stop the UDP worker threads
        @return true on success
    */
    bool stopUdpWorkerThreads( void );

    /*!
        Starting TCP/IP worker thread
        @return true on success
    */
    bool startTcpWorkerThread( void );

    /*!
        Stop the TCP/IP worker thread
        @return true on success
    */
    bool stopTcpWorkerThread( void );

    /*!
        Starting Client worker thread
        @return true on success
    */
    bool startClientWorkerThread( void );

    /*!
        Stop Client worker thread
        @return true on success
    */
    bool stopClientWorkerThread( void );
	
    /*!
        Save persistent data
    */
    void saveRegistryData ( void );


    /*!
        Add a new client to the clinet list

        @param Pointer to client that should be added.
        @param Normally not used but can be used to set a special 
        client id.
    */
    void addClient ( CClientItem *pClientItem, uint32_t id = 0 );

    /*!
        Remove a new client from the clinet list

        @param pClientItem Pointer to client that should be added.
    */
    void removeClient ( CClientItem *pClientItem );


    /*!
        Get device address for primary etehernet adapter

        @param pGUID Pointer to GUID array (16 bytes)
    */
    bool getMacAddress ( uint8_t *pGUID );


    /*!
        Get the first IP address computer is known under

        @param pGUID Pointer to GUID array (16 bytes)
    */
    bool getIPAddress ( uint8_t *pGUID );

    /*!
        Read configuration data
        @param strcfgfile path to configuration file.
        @return Returns true on success false on failure.
    */
    bool readConfiguration ( wxString& strcfgfile );

    /*!
        send level II message to all clients
    */
    void sendEventToClient ( CClientItem *pClientItem, vscpEvent *pEvent );

    /*!
        Send Level II event to all clients witch exception
    */
    void sendEventAllClients ( vscpEvent *pEvent, uint32_t excludeID = 0 );


    /*!
        Get clientmap index from a client id
    */
    uint32_t getClientMapFromId ( uint32_t clid );

    /*!
        Get a client id from a clinet map index
    */
    uint32_t getClientMapFromIndex ( uint32_t idx );

    /*!
        Add a client id to the clientmap
    */
    uint32_t addIdToClientMap ( uint32_t clid );

    /*!
        Remove a client id to the clientmap
    */
    bool removeIdFromClientMap ( uint32_t clid );
   
#ifdef WIN32   

	/////////////////////////////////////////////////
	//               WEBSOCKET STATICS
	////////////////////////////////////////////////

	static void 
	dump_handshake_info( struct lws_tokens *lwst );

	static int 
	callback_http( struct libwebsocket_context *context,
							struct libwebsocket *wsi,
							enum libwebsocket_callback_reasons reason, 
							void *user,
							void *in, 
							size_t len );

	static int 
	callback_dumb_increment( struct libwebsocket_context *context,
								struct libwebsocket *wsi,
								enum libwebsocket_callback_reasons reason,
								void *user, 
								void *in, 
								size_t len );


	static int 
	callback_lws_mirror( struct libwebsocket_context *context,
								struct libwebsocket *wsi,
								enum libwebsocket_callback_reasons reason,
								void *user, 
								void *in, 
								size_t len );
#endif								

public:

#ifdef BUILD_VSCPD_SERVICE
    HANDLE m_hEvntSource;
#endif

    /*!
        true if we should quit
    */
    bool m_bQuit;



    /*!
        Maximum number of items in receivequeue for clients
    */
    uint32_t m_maxItemsInClientReceiveQueue;


    /*!
        Server GUID
        This is the GUID for the server
    */
    uint8_t m_GUID[ 16 ];

    /*!
        ClientMap
        This structure maps client id's and
        The client map structure
        Maps unsigned log client id's to
        unsigned char id's for the
        GUID
    */
    uint32_t m_clientMap[ VSCP_MAX_CLIENTS ];


    /*!
        TCP Port for TCP Interface
    */
    unsigned short m_TCPPort;

    /*!
        UDP Port 
    */
    unsigned short m_UDPPort;


    /*!
        Log Level
    */
    uint8_t m_logLevel;

    /*!
        Enable control (TCP/IP) interface
    */
    bool m_bTCPInterface;

	/*!
		Interface used for TCP/IP connection
		Default: empty
	*/
	wxString m_strTcpInterfaceAddress;

    /*!
        Enable control UDP interface
    */  
    bool m_bUDPInterface;

    /*!
        UDP interface - just receive no send
    */
    bool m_bUDPInterfaceNoSend;

	/*!
		Interface used for UDP send 
		Default: empty
	*/
	wxString m_strUdpInterfaceAddress;

	/*!
		Interface used for UDP listen
		Default: empty
	*/
	wxString m_strUdpBindInterfaceAddress;

    /*!
        Enable CANAL Driver functionality
    */
    bool m_bCanalDrivers;

    /*!
        Enable VSCP Daemon functionality
    */
    bool m_bVSCPDaemon;

    /*!
        Enable variable usage
    */
    bool m_bVariables;

    /*!
        Hash table for variables
    */
    CVariableStorage m_VSCP_Variables;

    /*!
        Mutex to protect variables
    */
    wxMutex m_variableMutex;

    /*!
        Event source for NT event reporting
    */
#ifdef BUILD_VSCPD_SERVICE
    HANDLE m_hEventSource;
#endif

// *************************************************************************

    /*!
        Mutex for client queue
    */
    wxMutex m_wxClientMutex;

// *************************************************************************

    /*!
        Mutex for device queue
    */
    wxMutex m_wxDeviceMutex;

// *************************************************************************

    /*!
        Enable DM functionality
    */
    bool m_bDM;

    // Daemon Decision Matrix
    CDM m_dm;

    /*!
        TCP Port
    */
    short m_tcpport;

    /*!
        TCP Username
    */
    wxString m_strTcpUserName;

    /*!
        TCP Password
        This is 32 hex characters as MD5 sum of password.
    */
    wxString m_strTcpPassword;


    /*!
        Username for level II divers
    */
    wxString m_driverUsername;

    /*!
        Password for Level II drivers
    */
    wxString m_driverPassword;

	//*****************************************************
    //                   websocket interface
    //*****************************************************

	// Path to filesystem root
	static wxString m_pathRoot;

	// Enable disable web socket interface
	bool m_bWebsocketif;	

	// websocket port
	uint16_t m_portWebsockets;  // 7681

	//*****************************************************
    //                         Security
    //*****************************************************

	// Path to SSL certificate
	wxString m_pathCert;

	// Path to SSL key
	wxString m_pathKey;


    //*****************************************************
    //                      Lists
    //*****************************************************
    /*!
        The list with available devices.
    */
    CDeviceList m_deviceList;
    wxMutex m_mutexDeviceList;

    /*!
        The list with active clients.
    */
    CClientList m_clientList;
    wxMutex m_mutexClientList;

    /*!
        The list of active interfaces
    */
    CInterfaceList m_interfaceList;
    wxMutex m_mutexInterfaceList;

    /*!
        The list of users
    */
    CUserList m_userList;
    wxMutex m_mutexUserList;


    // *************************************************************************


    /*!
        Send queue

        This is the send queue for all clients attached to the system
    */
    VSCPEventList m_clientOutputQueue;

    /*!
        Event object to indicate that there is an event in the client output queue
    */
    wxSemaphore m_semClientOutputQueue;

    /*!
        Mutex for Level II message send queue
    */
    wxMutex m_mutexClientOutputQueue;

    // *************************************************************************


private:

    //*****************************************************
    //						        Threads
    //*****************************************************

    /*!
        controlobject device thread
    */
    clientMsgWorkerThread *m_pclientMsgWorkerThread;

    wxMutex m_mutexclientMsgWorkerThread;

    /*!
        The server thread for the TCP connection interface
    */
    TcpClientListenThread *m_pTcpClientListenThread;

    wxMutex m_mutexTcpClientListenThread;

    /*!
        The server thread for the VSCP daemon
    */
    daemonVSCPThread *m_pdaemonVSCPThread;

    wxMutex m_mutexdaemonVSCPThread;

    /*!
        UDP Worker threads
    */
    UDPSendThread *m_pudpSendThread;
    UDPReceiveThread *m_pudpReceiveThread;

    wxMutex m_mutexudpSendThread;
    wxMutex m_mutexudpReceiveThread;
};




#endif // !defined(AFX_CONTROLOBJECT_H__7D80016B_5EFD_40D5_94E3_6FD9C324CC7B__INCLUDED_)
