// ControlObject.cpp: implementation of the CControlObject class.
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
// As a special exception, if other files instantiate templates or use macros
// or inline functions from this file, or you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on
// this file might be covered by the GNU General Public License.
//
// Alternative licenses for VSCP & Friends may be arranged by contacting
// Grodans Paradis AB at http://www.grodansparadis.com
//
//
// TraceMasks:
// ===========
//
//   wxTRACE_doWorkLoop - Workloop messages 
//   wxTRACE_vscpd_Msg - Received messages.
//   wxTRACE_vscpd_ReceiveMutex  - Mutex lock
//   wxTRACE_VSCP_Msg - VSCP message mechanism

#ifdef __GNUG__
//#pragma implementation
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif


#include "wx/defs.h"
#include "wx/app.h"
#include <wx/xml/xml.h>

#ifdef WIN32

//#include <winsock.h>
#include "canal_win32_ipc.h"

#else 	// UNIX

#define _POSIX
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <sys/msg.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <syslog.h>
#include <netdb.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "wx/wx.h"
#include "wx/defs.h"
#include "wx/log.h"
#include "wx/socket.h"

#endif

#include <wx/config.h>
#include <wx/wfstream.h>
#include <wx/fileconf.h>
#include <wx/tokenzr.h>
#include <wx/listimpl.cpp>
#include <wx/xml/xml.h>

#include "canal_macro.h"
#include "../common/vscp.h"
#include "../common/vscphelper.h"
#include "../../common/configfile.h"
#include "../../common/crc.h"
#include "../../common/md5.h"
#include "../../common/randPassword.h"
#include "../common/version.h"
#include "variablecodes.h"
#include "actioncodes.h"
#include "devicelist.h"
#include "devicethread.h"
#include "dm.h"
#include "controlobject.h"


//#define DEBUGPRINT

// For MAC address

#ifdef WIN32

typedef struct _ASTAT_
{
    ADAPTER_STATUS adapt;
    NAME_BUFFER    NameBuff [30];

}ASTAT, * PASTAT;

ASTAT Adapter;

#endif


#ifdef WIN32

WORD wVersionRequested = MAKEWORD ( 1,1 );   // WSA functions
WSADATA wsaData;                             // WSA functions

#endif

///////////////////////////////////////////////////
//					WEBSOCKETS
///////////////////////////////////////////////////

#ifdef WIN32
static int close_testing;

struct per_session_data__dumb_increment {
	int number;
};

/* lws-mirror_protocol */

#define MAX_MESSAGE_QUEUE 64

struct per_session_data__lws_mirror {
	struct libwebsocket *wsi;
	int ringbuffer_tail;
};

struct a_message {
	void *payload;
	size_t len;
};

static struct a_message ringbuffer[ MAX_MESSAGE_QUEUE ];
static int ringbuffer_head;


// list of supported websocket protocols and callbacks 

static struct libwebsocket_protocols protocols[] = {
	
	// first protocol must always be HTTP handler 

	{
		"http-only",						// name 
		CControlObject::callback_http,		// callback 
		0									// per_session_data_size 
	},
	{
		"dumb-increment-protocol",
		CControlObject::callback_dumb_increment,
		sizeof( struct per_session_data__dumb_increment ),
	},
	{
		"lws-mirror-protocol",
		CControlObject::callback_lws_mirror,
		sizeof( struct per_session_data__lws_mirror )
	},
	{
		NULL, NULL, 0		// End of list 
	}
};

#endif

WX_DEFINE_LIST ( CanalMsgList );
WX_DEFINE_LIST ( VSCPEventList );



// Initialize statics
wxString CControlObject::m_pathRoot = _("");


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CControlObject::CControlObject()
{
    int i;
    m_bQuit = false;						// true if we should quit

    m_maxItemsInClientReceiveQueue = MAX_ITEMS_CLIENT_RECEIVE_QUEUE;

    // Nill the GUID
    memset ( m_GUID, 0, 16 );

    // Initialize the client map
    // to all unused
    for ( i=0; i<VSCP_MAX_CLIENTS; i++ ) {
        m_clientMap[ i ] = 0;
    }


    // Set default TCP Port
    m_tcpport = VSCP_LEVEL2_TCP_PORT;

    // Set default UDP port
    m_UDPPort = VSCP_LEVEL2_UDP_PORT;

    // Set Default Log Level
    m_logLevel = 0;


    // Control TCP/IP Interface
    m_bTCPInterface = true;

	// Default TCP/IP interface
	m_strTcpInterfaceAddress = _("");

    // Control UDP Interface 
    m_bUDPInterface = true;

    // Default is UDP send/receive
    m_bUDPInterfaceNoSend = false;

	// Default UDP send interface
	m_strUdpInterfaceAddress = _("");

	// Default UDP bind interface
	m_strUdpBindInterfaceAddress = _("");

    // Canaldriver
    m_bCanalDrivers = true;

    // Control VSCP
    m_bVSCPDaemon = true;

    // Control DM
    m_bDM = true;

    // Use variables
    m_bVariables = true;

    m_pclientMsgWorkerThread = NULL;
    m_pTcpClientListenThread = NULL;
    m_pdaemonVSCPThread = NULL;
    m_pudpSendThread = NULL;
    m_pudpReceiveThread = NULL;
	m_portWebsockets = 7681;
	m_pathCert.Empty();
	m_pathKey.Empty();

    // Set control object
    m_dm.setControlObject( this );

#ifdef WIN32

    // Initialize winsock layer
    WSAStartup ( wVersionRequested, &wsaData );

    // Also for wx
    wxSocketBase::Initialize();

#ifdef BUILD_VSCPD_SERVICE
    if ( !m_hEventSource ) {
        m_hEvntSource = ::RegisterEventSource ( NULL,				// local machine
                                                _("vscpservice") );	// source name
    }

#endif // windows service

	CControlObject::m_pathRoot = _("c:\\temp");

#else

	CControlObject::m_pathRoot = _("/var/vscp");

#endif

    // Initialize the CRC
    crcInit();

}


CControlObject::~CControlObject()
{
    // Remove objects in Client send queue
    VSCPEventList::iterator iterVSCP;

    m_mutexClientOutputQueue.Lock();
    for ( iterVSCP = m_clientOutputQueue.begin(); 
            iterVSCP != m_clientOutputQueue.end(); ++iterVSCP ) {
        vscpEvent *pEvent = *iterVSCP;
        deleteVSCPevent( pEvent );
    }
  
    m_clientOutputQueue.Clear();
    m_mutexClientOutputQueue.Unlock();

}

/////////////////////////////////////////////////////////////////////////////
// logMsg
//

void CControlObject::logMsg( const wxString& wxstr, unsigned char level )
{
    
  wxString wxdebugmsg = (wxDateTime::Now()).FormatISOTime() + _(" vscpd: ") + wxstr;
    
#ifdef WIN32
#ifdef BUILD_VSCPD_SERVICE

    const char* ps[3];
    ps[ 0 ] = wxstr;
    ps[ 1 ] = NULL;
    ps[ 2 ] = NULL;

    int iStr = 0;
    for ( int i = 0; i < 3; i++ ) {
        if ( ps[i] != NULL ) {
            iStr++;
        }
    }

    ::ReportEvent( m_hEventSource,
                        EVENTLOG_INFORMATION_TYPE,
                        0,
                        (1L << 30),
                        NULL, // sid
                        iStr,
                        0,
                        ps,
                        NULL );
#else
    //printf( wxdebugmsg.mb_str( wxConvUTF8 ) );
	if ( m_logLevel >= level ) {
		wxPrintf( wxdebugmsg );				
	}
#endif
#else
    
    ::wxLogDebug ( wxdebugmsg );

	if ( m_logLevel >= level ) {
		wxPrintf( wxdebugmsg );				
	}

    switch ( level ) {
        case DAEMON_LOGMSG_DEBUG:
            syslog( LOG_DEBUG, "%s", (const char *)wxdebugmsg.ToAscii() );
            break;

        case DAEMON_LOGMSG_INFO:
            syslog( LOG_INFO, "%s", (const char *)wxdebugmsg.ToAscii() );
            break;

        case DAEMON_LOGMSG_NOTICE:
            syslog( LOG_NOTICE, "%s", (const char *)wxdebugmsg.ToAscii() );
            break;

        case DAEMON_LOGMSG_WARNING:
            syslog( LOG_WARNING, "%s", (const char *)wxdebugmsg.ToAscii() );
            break;

        case DAEMON_LOGMSG_ERROR:
            syslog( LOG_ERR, "%s", (const char *)wxdebugmsg.ToAscii() );
            break;

        case DAEMON_LOGMSG_CRITICAL:
            syslog( LOG_CRIT, "%s", (const char *)wxdebugmsg.ToAscii() );
            break;

        case DAEMON_LOGMSG_ALERT:
            syslog( LOG_ALERT, "%s", (const char *)wxdebugmsg.ToAscii() );
            break;

        case DAEMON_LOGMSG_EMERGENCY:
            syslog( LOG_EMERG, "%s", (const char *)wxdebugmsg.ToAscii() );
            break;

    };
#endif
}


/////////////////////////////////////////////////////////////////////////////
// init

bool CControlObject::init( wxString& strcfgfile )
{
    //wxLog::AddTraceMask( "wxTRACE_doWorkLoop" );
    wxLog::AddTraceMask( _( "wxTRACE_vscpd_receiveQueue" ) ); // Receive queue
    wxLog::AddTraceMask( _( "wxTRACE_vscpd_Msg" ) );
    wxLog::AddTraceMask( _( "wxTRACE_VSCP_Msg" ) );
    wxLog::AddTraceMask( _( "wxTRACE_vscpd_ReceiveMutex" ) );
    wxLog::AddTraceMask( _( "wxTRACE_vscpd_sendMutexLevel1" ) );
    wxLog::AddTraceMask( _( "wxTRACE_vscpd_LevelII" ) );
    //wxLog::AddTraceMask( _( "wxTRACE_vscpd_dm" ) );

    wxString str =  _("VSCP Daemon started\n");
    str += _("Version: ");
    str += VSCPD_DISPLAY_VERSION;
    str += _("\n");
    str += VSCPD_COPYRIGHT;
    str += _("\n");
#ifdef BUILD_VSCPD_SERVICE
    logMsg( str );
#else
    printf( "%s", (const char *)str.ToAscii() );
#endif

    // A configuration file must be available
    if ( !wxFile::Exists( strcfgfile ) ) {
        logMsg( _("No configuration file given. Can't initialize!).\n"), DAEMON_LOGMSG_CRITICAL );
        logMsg( _("Path = .") + strcfgfile + _("\n"), DAEMON_LOGMSG_CRITICAL );
        return FALSE;
    }

    ::wxLogDebug ( _("Using configuration file: \n\t") + strcfgfile );

    // Generate username and password for drivers
    char buf[ 64 ];
    randPassword pw( 3 );

    pw.generatePassword( 32, buf );
    m_driverUsername = wxString::FromAscii( buf );
    pw.generatePassword( 32, buf );
    Cmd5 md5( (unsigned char *)buf );
    m_driverPassword = wxString::FromAscii( buf );
    
    m_userList.addUser( m_driverUsername, 
                            wxString::FromAscii( md5.getDigest() ), 
                            _T("admin"), 
                            NULL, 
                            _T(""),
                            _T("") );

    // Read configuration
    readConfiguration ( strcfgfile );
    
    // Get GUID
    if ( isGUIDEmpty( m_GUID ) ) {
        if ( !getMacAddress ( m_GUID ) ) {
            // We failed to create GUID from MAC address use
            // 'localhost' IP instead as the base.
            getIPAddress( m_GUID );
        }
    }

    // Load decision matrix if mechanism is enabled
    if ( m_bDM ) {
        m_dm.load();
    }

    // Load variables if mechanism is enabled
    if ( m_bVariables ) {
        m_VSCP_Variables.load();
    }
  
    startClientWorkerThread();

    if ( m_bCanalDrivers ) startDeviceWorkerThreads();

    if ( m_bTCPInterface ) startTcpWorkerThread();

    if ( m_bUDPInterface ) {
        startUdpWorkerThreads();
    }
 
    startDaemonWorkerThread();
    
    return true;

}


/////////////////////////////////////////////////////////////////////////////
// run - Program main loop
//
// Most work is done in the threads at the moment
//

bool CControlObject::run ( void )
{
    CLIENTEVENTLIST::compatibility_iterator nodeClient;

    vscpEvent EventLoop;
    EventLoop.vscp_class = VSCP_CLASS2_VSCPD;
    EventLoop.vscp_type = VSCP2_TYPE_VSCPD_LOOP;

    vscpEvent EventStartUp;
    EventLoop.vscp_class = VSCP_CLASS2_VSCPD;
    EventLoop.vscp_type = VSCP2_TYPE_VSCPD_STARTING_UP;

    vscpEvent EventShutDown;
    EventLoop.vscp_class = VSCP_CLASS2_VSCPD;
    EventLoop.vscp_type = VSCP2_TYPE_VSCPD_SHUTTING_DOWN;

    // We need to create a clientItem and add this object to the list
    CClientItem *pClientItem = new CClientItem;
    if ( NULL == pClientItem ) {
        wxLogDebug ( _ ( "ControlObject: Unable to allocate Client item, Ending" ) );
        logMsg( _("Unable to allocate Client item, Ending."), DAEMON_LOGMSG_CRITICAL );
        return false;
    }

    // Save a pointer to the client item
    m_dm.m_pClientItem = pClientItem;

    // Set Filter/Mask for full DM table
    memcpy( &pClientItem->m_filterVSCP, &m_dm.m_DM_Table_filter, sizeof( vscpEventFilter ) );

    // This is an active client
    pClientItem->m_bOpen = true;
    pClientItem->m_type =  CLIENT_ITEM_INTERFACE_TYPE_CLIENT_INTERNAL;
    pClientItem->m_strDeviceName = _("Internal Daemon DM Client. Started at ");
    wxDateTime now = wxDateTime::Now(); 
    pClientItem->m_strDeviceName += now.FormatISODate();
    pClientItem->m_strDeviceName += _(" ");
    pClientItem->m_strDeviceName += now.FormatISOTime();

    // Add the client to the Client List
    m_wxClientMutex.Lock();
    addClient ( pClientItem );
    m_wxClientMutex.Unlock();

    // Feed startup event
    m_dm.feed( &EventStartUp );

#ifdef WIN32
	// Initialize websockets
	int opts = 0;
	unsigned int oldus = 0;
	char interface_name[128] = "";
	const char *websockif = NULL;
	struct libwebsocket_context *context;
	unsigned char buf[ LWS_SEND_BUFFER_PRE_PADDING + 1024 +
						  LWS_SEND_BUFFER_POST_PADDING ];

	context = libwebsocket_create_context( m_portWebsockets, 
											websockif, 
											protocols,
											libwebsocket_internal_extensions,
											NULL, 
											NULL, 
											-1, 
											-1, 
											opts );

#endif

    // DM Loop
    while ( !m_bQuit ) {

		struct timeval tv;
		gettimeofday( &tv, NULL );

        // Feed possible perodic event
        m_dm.feedPeriodicEvent();

		// Put the LOOP event on the queue
        // Garanties at least one lop event between every other
        // event feed to the queue
        m_dm.feed( &EventLoop );

		// tcp/ip clients uses joinable treads and therefor does not
		// delete themseves.  This is a garbage collect for unterminated 
		// tcp/ip connection threads.
		TCPCLIENTS::iterator iter;
		for (iter = m_pTcpClientListenThread->m_tcpclients.begin(); 
				iter != m_pTcpClientListenThread->m_tcpclients.end(); ++iter) {
			TcpClientThread *pThread = *iter;
			if ( ( NULL != pThread )  ) {
				if ( pThread->m_bQuit ) {
					pThread->Wait();
					m_pTcpClientListenThread->m_tcpclients.remove( pThread );
					delete pThread;
					break;
				}
			}
		}

#ifdef WIN32
		/*
		 * This broadcasts to all dumb-increment-protocol connections
		 * at 20Hz.
		 *
		 * We're just sending a character 'x', in these examples the
		 * callbacks send their own per-connection content.
		 *
		 * You have to send something with nonzero length to get the
		 * callback actions delivered.
		 *
		 * We take care of pre-and-post padding allocation.
		 */

		if ( ( (unsigned int)tv.tv_usec - oldus) > 50000) {
			libwebsockets_broadcast( &protocols[ PROTOCOL_DUMB_INCREMENT ],
										&buf[ LWS_SEND_BUFFER_PRE_PADDING ], 
										1 );
			oldus = tv.tv_usec;
		}


		/*
		 * This example server does not fork or create a thread for
		 * websocket service, it all runs in this single loop.  So,
		 * we have to give the websockets an opportunity to service
		 * "manually".
		 *
		 * If no socket is needing service, the call below returns
		 * immediately and quickly.
		 */

		libwebsocket_service( context, 50 );
#endif		

        // Wait for event
        if ( wxSEMA_TIMEOUT == pClientItem->m_semClientInputQueue.WaitTimeout ( 10 ) ) {

            // Put the LOOP event on the queue
            m_dm.feed( &EventLoop );
            continue;

        }

		//---------------------------------------------------------------------------
		//                         Event received here
		//---------------------------------------------------------------------------

        if ( pClientItem->m_clientInputQueue.GetCount() ) {

            vscpEvent *pEvent;

            pClientItem->m_mutexClientInputQueue.Lock();
            nodeClient = pClientItem->m_clientInputQueue.GetFirst();
            pEvent = nodeClient->GetData();
            pClientItem->m_clientInputQueue.DeleteNode ( nodeClient );
            pClientItem->m_mutexClientInputQueue.Unlock();

            if ( NULL != pEvent ) {
                
                if ( doLevel2Filter( pEvent, &m_dm.m_DM_Table_filter ) ) {
                    // Feed event through matrix
                    m_dm.feed( pEvent );
                }

                // Remove the event
                deleteVSCPevent( pEvent );

            } // Valid pEvent pointer

        } // Event in queue
        
    }

    // Do shutdown event
    m_dm.feed( &EventShutDown );

    // Remove messages in the client queues
    m_wxClientMutex.Lock();
    removeClient ( pClientItem );
    m_wxClientMutex.Unlock();

#ifdef WIN32
	libwebsocket_context_destroy( context );
#endif	

    wxLogDebug ( _ ( "ControlObject: Done" ) );
    return true;
}


/////////////////////////////////////////////////////////////////////////////
// cleanup

bool CControlObject::cleanup ( void )
{
    stopDeviceWorkerThreads();
    stopUdpWorkerThreads();
    stopTcpWorkerThread();
    stopClientWorkerThread();
    stopDaemonWorkerThread();

    wxLogDebug ( _ ( "ControlObject: Cleanup done" ) );
    return true;
}



/////////////////////////////////////////////////////////////////////////////
// startClientWorkerThread
//

bool CControlObject::startClientWorkerThread( void )
{
    /////////////////////////////////////////////////////////////////////////////
    // Load controlobject client message handler
    /////////////////////////////////////////////////////////////////////////////
    m_pclientMsgWorkerThread = new clientMsgWorkerThread;

    if ( NULL != m_pclientMsgWorkerThread ) {
        m_pclientMsgWorkerThread->m_pCtrlObject = this;
        wxThreadError err;
        if ( wxTHREAD_NO_ERROR == ( err = m_pclientMsgWorkerThread->Create() ) ) {
            //m_ptcpListenThread->SetPriority( WXTHREAD_DEFAULT_PRIORITY );
            if ( wxTHREAD_NO_ERROR != ( err = m_pclientMsgWorkerThread->Run() ) ) {
                logMsg( _("Unable to run controlobject client thread."), DAEMON_LOGMSG_CRITICAL );
            }
        }
        else {
            logMsg( _("Unable to create controlobject client thread."), DAEMON_LOGMSG_CRITICAL );
        }
    }
    else {
        logMsg( _("Unable to allocate memory for controlobject client thread."), DAEMON_LOGMSG_CRITICAL );
    }	

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// stopTcpWorkerThread
//

bool CControlObject::stopClientWorkerThread( void )
{
    if ( NULL != m_pclientMsgWorkerThread ) {
        m_mutexclientMsgWorkerThread.Lock();    
        m_pclientMsgWorkerThread->m_bQuit = true;
        m_pclientMsgWorkerThread->Wait();
        delete m_pclientMsgWorkerThread;
        m_mutexclientMsgWorkerThread.Unlock();
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// startTcpWorkerThread
//

bool CControlObject::startTcpWorkerThread( void )
{
    /////////////////////////////////////////////////////////////////////////////
    // Run the TCP server thread   --   TODO - multiport
    /////////////////////////////////////////////////////////////////////////////
    if ( m_bTCPInterface ) {
        m_pTcpClientListenThread = new TcpClientListenThread;

        if ( NULL != m_pTcpClientListenThread ) {
            m_pTcpClientListenThread->m_pCtrlObject = this;
            wxThreadError err;
            if ( wxTHREAD_NO_ERROR == ( err = m_pTcpClientListenThread->Create() ) ) {
                //m_ptcpListenThread->SetPriority( WXTHREAD_DEFAULT_PRIORITY );
                if ( wxTHREAD_NO_ERROR != ( err = m_pTcpClientListenThread->Run() ) ) {
                    logMsg( _("Unable to run TCP thread."), DAEMON_LOGMSG_CRITICAL );
                }
            }
            else {
                logMsg( _("Unable to create TCP thread."), DAEMON_LOGMSG_CRITICAL );
            }
        }
        else {
            logMsg( _("Unable to allocate memory for TCP thread."), DAEMON_LOGMSG_CRITICAL );
        }
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
// stopTcpWorkerThread
//

bool CControlObject::stopTcpWorkerThread( void )
{
    if ( NULL != m_pTcpClientListenThread ) {
        m_mutexTcpClientListenThread.Lock();
        m_pTcpClientListenThread->m_bQuit = true;
        m_pTcpClientListenThread->Wait();
        delete m_pTcpClientListenThread;
        m_mutexTcpClientListenThread.Unlock();
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// startUdpWorkerThreads
//

bool CControlObject::startUdpWorkerThreads( void )
{
    /////////////////////////////////////////////////////////////////////////////
    // Run the UDP send thread   --   TODO - multiport
    /////////////////////////////////////////////////////////////////////////////
    if ( m_bUDPInterface ) {

        if ( !m_bUDPInterfaceNoSend ) {

            m_pudpSendThread = new UDPSendThread;

            if ( m_pudpSendThread ) {
                m_pudpSendThread->m_pCtrlObject = this;
                wxThreadError err;
                if ( wxTHREAD_NO_ERROR == ( err = m_pudpSendThread->Create() ) ) {
                    //m_ptcpListenThread->SetPriority( WXTHREAD_DEFAULT_PRIORITY );
                    if ( wxTHREAD_NO_ERROR != ( err = m_pudpSendThread->Run() ) ) {
                        logMsg( _("Unable to run UDP send thread."), DAEMON_LOGMSG_CRITICAL );	
                    }
                }
                else {
                    logMsg( _("Unable to create UDP send thread."), DAEMON_LOGMSG_CRITICAL );	
                }
            }
            else {
                logMsg( _("Unable to allocate memory for UDP send thread."), DAEMON_LOGMSG_CRITICAL );
            }

        }

        // =========================================

        m_pudpReceiveThread = new UDPReceiveThread;

        if ( m_pudpReceiveThread ) {
            m_pudpReceiveThread->m_pCtrlObject = this;
            wxThreadError err;
            if ( wxTHREAD_NO_ERROR == ( err = m_pudpReceiveThread->Create() ) ) {
                //m_ptcpListenThread->SetPriority( WXTHREAD_DEFAULT_PRIORITY );
                if ( wxTHREAD_NO_ERROR != ( err = m_pudpReceiveThread->Run() ) ) {
                    logMsg( _("Unable to run UDP receive thread."), DAEMON_LOGMSG_CRITICAL );	
                }
            }
            else {
                logMsg( _("Unable to create UDP receve thread."), DAEMON_LOGMSG_CRITICAL );	
            }
        }
        else {
            logMsg( _("Unable to allocate memory for UDP receive thread."), DAEMON_LOGMSG_CRITICAL );
        }

    } // udp i/f

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// stopUdpWorkerThreads
//

bool CControlObject::stopUdpWorkerThreads( void )
{
    // send Thread
    if ( !m_bUDPInterfaceNoSend ) {
        if ( NULL != m_pudpSendThread ) {
            m_mutexudpSendThread.Lock();
            m_pudpSendThread->m_bQuit = true;
            m_pudpSendThread->Wait();
            delete m_pudpSendThread;
            m_mutexudpSendThread.Unlock();
        }
    }


    // Receive Thread
    if ( NULL != m_pudpReceiveThread ) {
        m_mutexudpReceiveThread.Lock();
        m_pudpReceiveThread->m_bQuit = true;
        m_pudpReceiveThread->Wait();
        delete m_pudpReceiveThread;
        m_mutexudpReceiveThread.Unlock();
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// startDaemonWorkerThread
//

bool CControlObject::startDaemonWorkerThread( void )
{
    /////////////////////////////////////////////////////////////////////////////
    // Run the VSCP daemon thread
    /////////////////////////////////////////////////////////////////////////////
    if ( m_bVSCPDaemon ) {

        m_pdaemonVSCPThread = new daemonVSCPThread;
    
        if ( NULL != m_pdaemonVSCPThread ) {
            m_pdaemonVSCPThread->m_pCtrlObject = this;

            wxThreadError err;
            if ( wxTHREAD_NO_ERROR == ( err = m_pdaemonVSCPThread->Create() ) ) {
                m_pdaemonVSCPThread->SetPriority( WXTHREAD_DEFAULT_PRIORITY );
                if ( wxTHREAD_NO_ERROR != ( err = m_pdaemonVSCPThread->Run() ) ) {
                    logMsg( _("Unable to start TCP VSCP daemon thread."), DAEMON_LOGMSG_CRITICAL );
                }
            }
            else {
                logMsg( _("Unable to create TCP VSCP daemon thread."), DAEMON_LOGMSG_CRITICAL );
            }
        }
        else {
            logMsg( _("Unable to start VSCP daemon thread."), DAEMON_LOGMSG_CRITICAL );
        }

    } // daemon enabled

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// stopDaemonWorkerThread
//

bool CControlObject::stopDaemonWorkerThread( void )
{
    if ( NULL != m_pdaemonVSCPThread ) {
        m_mutexdaemonVSCPThread.Lock();
        m_pdaemonVSCPThread->m_bQuit = true;
        m_pdaemonVSCPThread->Wait();
        delete m_pdaemonVSCPThread;
        m_mutexdaemonVSCPThread.Unlock();
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//  
//

bool CControlObject::startDeviceWorkerThreads( void )
{
    CDeviceItem *pDeviceItem;

    VSCPDEVICELIST::iterator iter;
    for ( iter = m_deviceList.m_devItemList.begin(); 
            iter != m_deviceList.m_devItemList.end(); 
            ++iter ) {
    
        pDeviceItem = *iter;
        if ( NULL != pDeviceItem ) {

            // *****************************************
            //  Create the worker thread for the device
            // *****************************************
                
            pDeviceItem->m_pdeviceThread = new deviceThread();
            if ( NULL != pDeviceItem->m_pdeviceThread ) {

                pDeviceItem->m_pdeviceThread->m_pCtrlObject = this;
                pDeviceItem->m_pdeviceThread->m_pDeviceItem = pDeviceItem;

                wxThreadError err;
                if ( wxTHREAD_NO_ERROR == ( err = pDeviceItem->m_pdeviceThread->Create() ) ) {
                    if ( wxTHREAD_NO_ERROR != ( err = pDeviceItem->m_pdeviceThread->Run() ) ) {
                        logMsg( _("Unable to create DeviceThread."), DAEMON_LOGMSG_CRITICAL );
                    }
                }
                else {
                    logMsg( _("Unable to run DeviceThread."), DAEMON_LOGMSG_CRITICAL );
                }

            }
            else {
                logMsg( _("Unable to allocate memory for DeviceThread."), DAEMON_LOGMSG_CRITICAL );
            }

        } // Valid device item
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// stopDeviceWorkerThreads
//

bool CControlObject::stopDeviceWorkerThreads( void )
{
    CDeviceItem *pDeviceItem;

    VSCPDEVICELIST::iterator iter;
    for ( iter = m_deviceList.m_devItemList.begin(); 
            iter != m_deviceList.m_devItemList.end(); 
            ++iter ) {
    
        pDeviceItem = *iter;
        if ( NULL != pDeviceItem ) {

            if ( NULL != pDeviceItem->m_pdeviceThread ) {
                pDeviceItem->m_mutexdeviceThread.Lock();
                pDeviceItem->m_bQuit = true;
                pDeviceItem->m_pdeviceThread->Wait();
                pDeviceItem->m_mutexdeviceThread.Unlock();
                delete pDeviceItem->m_pdeviceThread;
            }

        }

    }

    return true;
}


///////////////////////////////////////////////////////////////////////////////
// sendEventToClient
//

void CControlObject::sendEventToClient ( CClientItem *pClientItem, 
                                            vscpEvent *pEvent )
{
    // Must be valid pointers
    if ( NULL == pClientItem ) return;
    if ( NULL == pEvent ) return;

    // Check if filtered out
    if ( !doLevel2Filter( pEvent, &pClientItem->m_filterVSCP ) ) return;

    // If the client queue is full for this client then the
    // client will not receive the message
    if ( pClientItem->m_clientInputQueue.GetCount() >
        m_maxItemsInClientReceiveQueue ) {
        // Overun
        pClientItem->m_statistics.cntOverruns++;
        return;
    }

    // Statistics 1 received frame,+ received data
    pClientItem->m_statistics.cntReceiveFrames++;
    pClientItem->m_statistics.cntReceiveData += pEvent->sizeData;

    // Create an event
    vscpEvent *pnewvscpEvent = new vscpEvent;
    if ( NULL != pnewvscpEvent ) {
        // Copy in the new message
        memcpy ( pnewvscpEvent, pEvent, sizeof ( vscpEvent ) );

        // And data...
        if ( ( pEvent->sizeData > 0 ) && ( NULL != pEvent->pdata ) ) {
            // Copy in data
            pnewvscpEvent->pdata = new uint8_t[ pEvent->sizeData ];
            memcpy ( pnewvscpEvent->pdata, pEvent->pdata, pEvent->sizeData );
        }
        else {
            // No data
            pnewvscpEvent->pdata = NULL;
        }

        // Add the new event to the inputqueue
        pClientItem->m_mutexClientInputQueue.Lock();
        pClientItem->m_clientInputQueue.Append ( pnewvscpEvent );
        pClientItem->m_semClientInputQueue.Post();
        pClientItem->m_mutexClientInputQueue.Unlock();

    }
}

///////////////////////////////////////////////////////////////////////////////
// sendEventAllClients
//

void CControlObject::sendEventAllClients ( vscpEvent *pEvent, uint32_t excludeID )
{
    CClientItem *pClientItem;
    VSCPCLIENTLIST::iterator it;

    if ( NULL == pEvent ) return;

    wxLogTrace( _("wxTRACE_vscpd_receiveQueue"), 
                  _(" ControlObject: event %d, excludeid = %d"), 
                  pEvent->obid, excludeID ); 
    
    m_wxClientMutex.Lock();
    for ( it = m_clientList.m_clientItemList.begin(); it != m_clientList.m_clientItemList.end(); ++it ) {
        pClientItem = *it;

        if (NULL != pClientItem)
        {
            wxLogTrace( _("wxTRACE_vscpd_receiveQueue"),
                          _(" ControlObject: clientid = %d"), 
                          pClientItem->m_clientID ); 
        }

        if ( ( NULL != pClientItem ) && ( excludeID != pClientItem->m_clientID ) ) {
          sendEventToClient ( pClientItem, pEvent );
          wxLogTrace( _("wxTRACE_vscpd_receiveQueue"),
                        _(" ControlObject: Sent to client %d"),
                        pClientItem->m_clientID );
        }
    }

    m_wxClientMutex.Unlock();

}



//
// The clientmap holds free client id's in an array
// They are aquired when a client connects and released when a
// client disconnects.
//
// Interfaces can be fetched by investigating the map. 
//
// Not used at the moment.


///////////////////////////////////////////////////////////////////////////////
//  getClientMapFromId
//

uint32_t CControlObject::getClientMapFromId ( uint32_t clid )
{
    for ( uint32_t i=0; i<VSCP_MAX_CLIENTS; i++ ) {
        if ( clid == m_clientMap[ i ] ) return i;
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//  getClientMapFromIndex
//

uint32_t CControlObject::getClientMapFromIndex ( uint32_t idx )
{
    return m_clientMap[ idx ];
}


///////////////////////////////////////////////////////////////////////////////
//  addIdToClientMap
//

uint32_t CControlObject::addIdToClientMap ( uint32_t clid )
{
    for ( uint32_t i=1; i<VSCP_MAX_CLIENTS; i++ ) {
        if ( 0 == m_clientMap[ i ] ) {
            m_clientMap[ i ]= clid;
            return clid;
        }
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//  removeIdFromClientMap
//

bool CControlObject::removeIdFromClientMap ( uint32_t clid )
{
    for ( uint32_t i=0; i<VSCP_MAX_CLIENTS; i++ )
    {
        if ( clid == m_clientMap[ i ] )
        {
            m_clientMap[ i ] = 0;
            return true;
        }
    }

    return false;
}


//////////////////////////////////////////////////////////////////////////////
// addClient
//

void CControlObject::addClient ( CClientItem *pClientItem, uint32_t id )
{
    // Add client to client list
    m_clientList.addClient ( pClientItem, id );

    // Add mapped item
    addIdToClientMap ( pClientItem->m_clientID );

    // Set GUID for interface
    //getIPAddress ( pClientItem->m_GUID );
    memcpy( pClientItem->m_GUID, m_GUID, 16 );
    pClientItem->m_GUID[ 0 ] = 0x00;
    pClientItem->m_GUID[ 1 ] = 0x00;
    pClientItem->m_GUID[ 2 ] = pClientItem->m_clientID & 0xff;
    pClientItem->m_GUID[ 3 ] = ( pClientItem->m_clientID >> 8 ) & 0xff;
}


//////////////////////////////////////////////////////////////////////////////
// removeClient
//

void CControlObject::removeClient ( CClientItem *pClientItem )
{
    // Remove the mapped item
    removeIdFromClientMap ( pClientItem->m_clientID );

    // Remove the client
    m_clientList.removeClient ( pClientItem );
}


///////////////////////////////////////////////////////////////////////////////
//  getMacAddress
//

bool CControlObject::getMacAddress ( unsigned char *pGUID )
{
    // Check pointer
    if ( NULL == pGUID ) return false;

#ifdef WIN32

    bool rv = false;
    NCB Ncb;
    UCHAR uRetCode;
    LANA_ENUM lenum;
    int i;

    // Clear the GUID
    memset ( pGUID, 0, 16 );

    memset ( &Ncb, 0, sizeof ( Ncb ) );
    Ncb.ncb_command = NCBENUM;
    Ncb.ncb_buffer = ( UCHAR * ) &lenum;
    Ncb.ncb_length = sizeof ( lenum );
    uRetCode = Netbios ( &Ncb );
    //printf( "The NCBENUM return code is: 0x%x \n", uRetCode );

    for ( i=0; i < lenum.length ;i++ )
    {
        memset ( &Ncb, 0, sizeof ( Ncb ) );
        Ncb.ncb_command = NCBRESET;
        Ncb.ncb_lana_num = lenum.lana[i];

        uRetCode = Netbios ( &Ncb );

        memset ( &Ncb, 0, sizeof ( Ncb ) );
        Ncb.ncb_command = NCBASTAT;
        Ncb.ncb_lana_num = lenum.lana[i];

        strcpy ( ( char * ) Ncb.ncb_callname,  "*               " );
        Ncb.ncb_buffer = ( unsigned char * ) &Adapter;
        Ncb.ncb_length = sizeof ( Adapter );

        uRetCode = Netbios ( &Ncb );

        if ( uRetCode == 0 )
        {
            pGUID[ 15 ] = 0xff;	// Ethernet assigned group
            pGUID[ 14 ] = 0xff;
            pGUID[ 13 ] = 0xff;
            pGUID[ 12 ] = 0xff;
            pGUID[ 11 ] = 0xff;
            pGUID[ 10 ] = 0xff;
            pGUID[ 9 ] = 0xff;
            pGUID[ 8 ] = 0xfe;
            pGUID[ 7 ] = Adapter.adapt.adapter_address[ 0 ];
            pGUID[ 6 ] = Adapter.adapt.adapter_address[ 1 ];
            pGUID[ 5 ] = Adapter.adapt.adapter_address[ 2 ];
            pGUID[ 4 ] = Adapter.adapt.adapter_address[ 3 ];
            pGUID[ 3 ] = Adapter.adapt.adapter_address[ 4 ];
            pGUID[ 2 ] = Adapter.adapt.adapter_address[ 5 ];
            pGUID[ 1 ] = 0;
            pGUID[ 0 ] = 0;
#ifdef __WXDEBUG__
            char buf[256];
            sprintf ( buf, "The Ethernet MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                        pGUID[ 2 ],
                        pGUID[ 3 ],
                        pGUID[ 4 ],
                        pGUID[ 5 ],
                        pGUID[ 6 ],
                        pGUID[ 7 ] );

            wxString str = wxString::FromUTF8( buf );
            wxLogDebug ( str );
#endif

            rv = true;
        }
    }

    return rv;

#else

    bool rv = true;
    struct ifreq ifr;
    int fd;

    // Clear the GUID
    memset ( pGUID, 0, 16 );

    fd = socket ( PF_INET, SOCK_RAW, htons ( ETH_P_ALL ) );
    memset ( &ifr, 0, sizeof ( ifr ) );
    strncpy ( ifr.ifr_name, "eth0", sizeof ( ifr.ifr_name ) );
    if ( ioctl ( fd, SIOCGIFHWADDR, &ifr ) >= 0 )
    {
        unsigned char *ptr;
        ptr = ( unsigned char * ) &ifr.ifr_ifru.ifru_hwaddr.sa_data[ 0 ];
        logMsg( wxString::Format ( _ ( " Ethernet MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n" ),
                    *ptr,
                    *( ptr + 1 ),
                    *( ptr + 2 ),
                    *( ptr + 3 ),
                    *( ptr + 4 ),
                    *( ptr + 5 ) ), DAEMON_LOGMSG_INFO );
        pGUID[ 15 ] = 0xff;	// Ethernet assigned group
        pGUID[ 14 ] = 0xff;
        pGUID[ 13 ] = 0xff;
        pGUID[ 12 ] = 0xff;
        pGUID[ 11 ] = 0xff;
        pGUID[ 10 ] = 0xff;
        pGUID[ 9 ] = 0xff;
        pGUID[ 8 ] = 0xfe;
        pGUID[ 7 ] = *ptr;
        pGUID[ 6 ] = * ( ptr + 1 );
        pGUID[ 5 ] = * ( ptr + 2 );
        pGUID[ 4 ] = * ( ptr + 3 );
        pGUID[ 3 ] = * ( ptr + 4 );
        pGUID[ 2 ] = * ( ptr + 5 );
        pGUID[ 1 ] = 0;
        pGUID[ 0 ] = 0;
    }
    else
    {
        logMsg( _ ( "Failed to get hardware address (must be root?).\n" ), DAEMON_LOGMSG_WARNING );
        rv = false;
    }

    return rv;


#endif

}



///////////////////////////////////////////////////////////////////////////////
//  getIPAddress
//

bool CControlObject::getIPAddress ( unsigned char *pGUID )
{
    // Clear the GUID
    memset ( pGUID, 0, 16 );

    pGUID[15] = 0xff;
    pGUID[14] = 0xff;
    pGUID[13] = 0xff;
    pGUID[12] = 0xff;
    pGUID[11] = 0xff;
    pGUID[10] = 0xff;
    pGUID[9] = 0xff;
    pGUID[8] = 0xfd;

    char szName[ 128 ];
    gethostname ( szName, sizeof ( szName ) );
#ifdef WIN32
    LPHOSTENT lpLocalHostEntry;
#else
    struct hostent *lpLocalHostEntry;
#endif
    lpLocalHostEntry = gethostbyname ( szName );
    if ( NULL == lpLocalHostEntry )
    {
        return false;
    }

    // Get all local addresses
    int idx = -1;
    void *pAddr;
    unsigned long localaddr[ 16 ]; // max 16 local addresses
    do
    {
        idx++;
        localaddr[ idx ] = 0;
        pAddr = lpLocalHostEntry->h_addr_list[ idx ];
        if ( NULL != pAddr ) localaddr[ idx ] = * ( ( unsigned long * ) pAddr );
    }
    while ( ( NULL != pAddr ) && ( idx < 16 ) );


    pGUID[7] = ( localaddr[ 0 ] >> 24 ) & 0xff;
    pGUID[6] = ( localaddr[ 0 ] >> 16 ) & 0xff;
    pGUID[5] = ( localaddr[ 0 ] >> 8 ) & 0xff;
    pGUID[4] = localaddr[ 0 ] & 0xff;

    return true;
}


///////////////////////////////////////////////////////////////////////////////
// readConfiguration
//
// Read the configuration XML file
//

bool CControlObject::readConfiguration ( wxString& strcfgfile )
{
	unsigned long val;
    wxXmlDocument doc;
    if ( !doc.Load ( strcfgfile ) )
    {
        return false;
    }

    // start processing the XML file
    if ( doc.GetRoot()->GetName() != wxT ( "vscpconfig" ) ) {
        return false;
    }

    wxXmlNode *child = doc.GetRoot()->GetChildren();
    while ( child )
    {

        if ( child->GetName() == wxT ( "general" ) )
        {

            wxXmlNode *subchild = child->GetChildren();
            while ( subchild )
            {

				// Depricated <==============
                if ( subchild->GetName() == wxT ( "tcpport" ) )
                {
                    wxString str = subchild->GetNodeContent();
                    m_TCPPort = readStringValue( str );
                }
				// Depricated <==============
                else if ( subchild->GetName() == wxT ( "udpport" ) )
                {
                    wxString str = subchild->GetNodeContent();
                    m_UDPPort = readStringValue( str );
                }
                else if ( subchild->GetName() == wxT ( "loglevel" ) )
                {
                    wxString str = subchild->GetNodeContent();
                    m_logLevel = readStringValue( str );
                }
                else if ( subchild->GetName() == wxT ( "tcpif" ) )
                {
                    wxString property = subchild->GetPropVal ( wxT ( "enabled" ), wxT ( "true" ) );
                    if ( property.IsSameAs ( _ ( "false" ), false ) )
                    {
                        m_bTCPInterface = false;
                    }

					property = subchild->GetPropVal ( wxT ( "port" ), wxT ( "9598" ) );
                    if ( property.IsNumber() )
                    {
                        m_TCPPort = readStringValue( property );
                    }

					m_strTcpInterfaceAddress = subchild->GetPropVal ( wxT ( "ifaddress" ), wxT ( "" ) );
                 
                }
                else if ( subchild->GetName() == wxT ( "udpif" ) )
                {
                    wxString property = subchild->GetPropVal ( wxT ( "enabled" ), wxT ( "true" ) );
                    if ( property.IsSameAs ( _ ( "false" ), false ) )
                    {
                        m_bUDPInterface = false;
                    }

                    property = subchild->GetPropVal ( wxT ( "onlyincoming" ), wxT ( "false" ) );
                    if ( property.IsSameAs ( _ ( "true" ), false ) )
                    {
                        m_bUDPInterfaceNoSend = true;
                    }

					property = subchild->GetPropVal ( wxT ( "port" ), wxT ( "9598" ) );
                    if ( property.IsNumber() )
                    {
                        m_UDPPort = readStringValue( property );
                    }

					m_strUdpInterfaceAddress = subchild->GetPropVal ( wxT ( "ifaddress" ), wxT ( "" ) );

					m_strUdpBindInterfaceAddress = subchild->GetPropVal ( wxT ( "ifaddress" ), wxT ( "" ) );


                }
                else if ( subchild->GetName() == wxT ( "canaldriver" ) )
                {
                    wxString property = subchild->GetPropVal ( wxT ( "enabled" ), wxT ( "true" ) );
                    if ( property.IsSameAs ( _ ( "false" ), false ) )
                    {
                        m_bCanalDrivers = false;
                    }
                }
                else if ( subchild->GetName() == wxT ( "dm" ) )
                {
                    // Should the internal DM be disabled
                    wxString property = subchild->GetPropVal ( wxT ( "enabled" ), wxT ( "true" ) );
                    if ( property.IsSameAs ( _ ( "false" ), false ) )
                    {
                        m_bDM = false;
                    }
                    
                    // Get the path to the DM file
                    m_dm.m_configPath = subchild->GetPropVal ( wxT ( "path" ), wxT ( "" ) );

                }
                else if ( subchild->GetName() == wxT ( "variables" ) )
                {
                    // Should the internal DM be disabled
                    wxString property = subchild->GetPropVal ( wxT ( "enabled" ), wxT ( "true" ) );
                    if ( property.IsSameAs ( _ ( "false" ), false ) )
                    {
                        m_bVariables = false;
                    }
                    
                    // Get the path to the DM file
                    m_VSCP_Variables.m_configPath = subchild->GetPropVal ( wxT ( "path" ), wxT ( "" ) );

                }
                else if ( subchild->GetName() == wxT ( "vscp" ) )
                {
                    wxString property = subchild->GetPropVal ( wxT ( "enabled" ), wxT ( "true" ) );
                    if ( property.IsSameAs ( _ ( "false" ), false ) )
                    {
                        m_bVSCPDaemon = false;
                    }
                }
                else if ( subchild->GetName() == wxT ( "guid" ) )
                {
                    wxString str = subchild->GetNodeContent();
                    getGuidFromStringToArray( m_GUID, str );
                }
                else if ( subchild->GetName() == wxT ( "clientbuffersize" ) )
                {
                    wxString str = subchild->GetNodeContent();
                    m_maxItemsInClientReceiveQueue = readStringValue( str );
                }
				else if ( subchild->GetName() == wxT ( "pathroot" ) )
                {
					CControlObject::m_pathRoot = subchild->GetNodeContent();
				}
				else if ( subchild->GetName() == wxT ( "pathcert" ) )
                {
					m_pathCert = subchild->GetNodeContent();
				}
				else if ( subchild->GetName() == wxT ( "pathkey" ) )
                {
					m_pathKey = subchild->GetNodeContent();
				}
				else if ( subchild->GetName() == wxT ( "websocket" ) )
				{
					wxString property = subchild->GetPropVal ( wxT ( "enabled" ), wxT ( "true" ) );
                    if ( property.IsSameAs ( _ ( "false" ), false ) )
                    {
                        m_bWebsocketif = false;
                    }
					
					property = subchild->GetPropVal ( wxT ( "port" ), wxT ( "7681" ) );
                    if ( property.IsNumber() )
                    {
                        m_portWebsockets = readStringValue( property );
                    }
                }

                subchild = subchild->GetNext();
            }

            wxString content = child->GetNodeContent();

        }
        else if ( child->GetName() == wxT ( "remoteuser" ) )
        {

            wxXmlNode *subchild = child->GetChildren();
            while ( subchild )
            {
                vscpEventFilter VSCPFilter;
                bool bFilterPresent = false;
                bool bMaskPresent = false;
                wxString name;
                wxString md5;
                wxString privilege;
                wxString allowfrom;
                wxString allowevent;
                bool bUser = false;

                clearVSCPFilter( &VSCPFilter );	// Allow all frames

                if ( subchild->GetName() == wxT ( "user" ) )
                {

                    wxXmlNode *subsubchild = subchild->GetChildren();

                    while ( subsubchild )
                    {
                        if ( subsubchild->GetName() == wxT ( "name" ) )
                        {
                            name = subsubchild->GetNodeContent();
                            bUser = true;
                        }
                        else if ( subsubchild->GetName() == wxT ( "password" ) )
                        {
                            md5 = subsubchild->GetNodeContent();
                        }
                        else if ( subsubchild->GetName() == wxT ( "privilege" ) )
                        {
                            privilege = subsubchild->GetNodeContent();
                        }
                            else if ( subsubchild->GetName() == wxT ( "filter" ) )
                        {
                            bFilterPresent = true;
                            wxString str_vscp_priority = subchild->GetPropVal ( wxT ( "priority" ), wxT ( "0" ) );
                            val = 0;
                            str_vscp_priority.ToULong( &val );
                            VSCPFilter.filter_priority = val;
                            wxString str_vscp_class = subchild->GetPropVal ( wxT ( "class" ), wxT ( "0" ) );
                            val = 0;
                            str_vscp_class.ToULong( &val );
                            VSCPFilter.filter_class = val;
                            wxString str_vscp_type = subchild->GetPropVal ( wxT ( "type" ), wxT ( "0" ) );
                            val = 0;
                            str_vscp_type.ToULong( &val );
                            VSCPFilter.filter_type = val;
                            wxString str_vscp_guid = subchild->GetPropVal ( wxT ( "guid" ), 
                                                                wxT ( "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00" ) );
                            getGuidFromStringToArray( VSCPFilter.filter_GUID, str_vscp_guid );
                        }
                        else if ( subsubchild->GetName() == wxT ( "mask" ) )
                        {
                            bMaskPresent = true;
                            wxString str_vscp_priority = subchild->GetPropVal ( wxT ( "priority" ), wxT ( "0" ) );
                            val = 0;
                            str_vscp_priority.ToULong( &val );
                            VSCPFilter.mask_priority = val;
                            wxString str_vscp_class = subchild->GetPropVal ( wxT ( "class" ), wxT ( "0" ) );
                            val = 0;
                            str_vscp_class.ToULong( &val );
                            VSCPFilter.mask_class = val;
                            wxString str_vscp_type = subchild->GetPropVal ( wxT ( "type" ), wxT ( "0" ) );
                            val = 0;
                            str_vscp_type.ToULong( &val );
                            VSCPFilter.mask_type = val;
                            wxString str_vscp_guid = subchild->GetPropVal ( wxT ( "guid" ), 
                                                                    wxT ( "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00" ) );
                            getGuidFromStringToArray( VSCPFilter.mask_GUID, str_vscp_guid );
                        }
                        else if ( subsubchild->GetName() == wxT ( "allowfrom" ) )
                        {
                            allowfrom = subsubchild->GetNodeContent();
                        }
                        else if ( subsubchild->GetName() == wxT ( "allowevent" ) )
                        {
                            allowevent = subsubchild->GetNodeContent();
                        }

                        subsubchild = subsubchild->GetNext();

                    }

                }

                // Add user
                if ( bUser ) {

                    if ( bFilterPresent && bMaskPresent) {
                        m_userList.addUser ( name, md5, privilege, &VSCPFilter, allowfrom, allowevent );
                    }
                    else {
                        m_userList.addUser ( name, md5, privilege, NULL, allowfrom, allowevent );
                    }

                    bUser = false;
                    bFilterPresent = false;
                    bMaskPresent = false;

                }

                subchild = subchild->GetNext();

            }

        }
        else if ( child->GetName() == wxT ( "interfaces" ) )
        {

            wxXmlNode *subchild = child->GetChildren();
            while ( subchild )
            {

                wxString ip;
                wxString mac;
                wxString guid;
                bool bInterface = false;

                if ( subchild->GetName() == wxT ( "interface" ) )
                {
                    wxXmlNode *subsubchild = subchild->GetChildren();

                    while ( subsubchild )
                    {

                        if ( subsubchild->GetName() == wxT ( "ipaddress" ) )
                        {
                            ip = subsubchild->GetNodeContent();
                            bInterface = true;
                        }
                        else if ( subsubchild->GetName() == wxT ( "macaddress" ) )
                        {
                            mac = subsubchild->GetNodeContent();
                        }
                        else if ( subsubchild->GetName() == wxT ( "guid" ) )
                        {
                            guid = subsubchild->GetNodeContent();
                        }

                        subsubchild = subsubchild->GetNext();

                    }

                }

                // Add interface
                if ( bInterface) {
                    m_interfaceList.addInterface ( ip, mac, guid );
                    bInterface = false;
                }

                subchild = subchild->GetNext();

            }

        }

        // Level I driver
        else if ( child->GetName() == wxT ( "canaldriver" ) )
        {
            wxXmlNode *subchild = child->GetChildren();
            while ( subchild )
            {
                wxString strName;
                wxString strParameter;
                wxString strPath;
                unsigned long flags;
                wxString strGUID;
                bool bCanalDriver = false;

                if ( subchild->GetName() == wxT ( "driver" ) )
                {
                    wxXmlNode *subsubchild = subchild->GetChildren();
                    while ( subsubchild )
                    {
                        if ( subsubchild->GetName() == wxT ( "name" ) )
                        {
                            strName = subsubchild->GetNodeContent();
                            bCanalDriver = true;
                        }
                        else if ( subsubchild->GetName() == wxT ( "parameter" ) )
                        {
                            strParameter = subsubchild->GetNodeContent();
                        }
                        else if ( subsubchild->GetName() == wxT ( "path" ) )
                        {
                            strPath = subsubchild->GetNodeContent();
                        }
                        else if ( subsubchild->GetName() == wxT ( "flags" ) )
                        {
                            wxString str = subsubchild->GetNodeContent();
                            flags = readStringValue( str );
                        }
                        else if ( subsubchild->GetName() == wxT ( "guid" ) )
                        {
                            strGUID = subsubchild->GetNodeContent();
                        }

                        // Next driver item
                        subsubchild = subsubchild->GetNext();

                    }

                }

                // Configuration data for one driver loaded
                uint8_t GUID[ 16 ];

                // Nill the GUID
                memset( GUID, 0, 16 );

                if ( strGUID.Length() ) {
                    getGuidFromStringToArray( GUID, strGUID );
                }

                // Add the device
                if ( bCanalDriver ) {  
 
                    if ( !m_deviceList.addItem ( strName,
                                                    strParameter,
                                                    strPath,
                                                    flags,
                                                    GUID ) ) {
                        wxString errMsg = _("Driver not added. Path does not exist. - \n\t[ ") + 
                        strPath + _(" ]\n");
                        logMsg( errMsg, DAEMON_LOGMSG_INFO );
                    }

                    bCanalDriver = false;

                }

                // Next driver
                subchild = subchild->GetNext();

            }

        }

        // Level II driver
        else if ( child->GetName() == wxT ( "vscpdriver" ) )
        {
            wxXmlNode *subchild = child->GetChildren();
            while ( subchild )
            {
                wxString strName;
                wxString strParameter;
                wxString strPath;
                unsigned long flags;
                wxString strGUID;
                bool bCanalDriver = false;

                if ( subchild->GetName() == wxT ( "driver" ) )
                {
                    wxXmlNode *subsubchild = subchild->GetChildren();
                    while ( subsubchild )
                    {
                        if ( subsubchild->GetName() == wxT ( "name" ) )
                        {
                            strName = subsubchild->GetNodeContent();
                            bCanalDriver = true;
                        }
                        else if ( subsubchild->GetName() == wxT ( "parameter" ) )
                        {
                            strParameter = subsubchild->GetNodeContent();
                        }
                        else if ( subsubchild->GetName() == wxT ( "path" ) )
                        {
                            strPath = subsubchild->GetNodeContent();
                        }
                        else if ( subsubchild->GetName() == wxT ( "flags" ) )
                        {
                            wxString str = subsubchild->GetNodeContent();
                            flags = readStringValue( str );
                        }
                        else if ( subsubchild->GetName() == wxT ( "guid" ) )
                        {
                            strGUID = subsubchild->GetNodeContent();
                        }

                        // Next driver item
                        subsubchild = subsubchild->GetNext();

                    }

                }

                // Configuration data for one driver loaded
                uint8_t GUID[ 16 ];

                // Nill the GUID
                memset( GUID, 0, 16 );

                if ( strGUID.Length() ) {
                    getGuidFromStringToArray( GUID, strGUID );
                }

                // Add the device
                if ( bCanalDriver ) {  
 
                    if ( !m_deviceList.addItem ( strName,
                                                    strParameter,
                                                    strPath,
                                                    flags,
                                                    GUID,
                                                    VSCP_DRIVER_LEVEL2 ) ) {
                        wxString errMsg = _("Driver not added. Path does not exist. - \n\t[ ") + 
                        strPath + _(" ]\n");
                        logMsg( errMsg, DAEMON_LOGMSG_INFO );
                    }

                    bCanalDriver = false;

                }

                // Next driver
                subchild = subchild->GetNext();

            }

        }
        

        child = child->GetNext();

    }

    return true;

}

#ifdef WIN32


////////////////////////
// websocket callbaks
///////////////////////


///////////////////////////////////////////////////////////////////////////////
// callback_http
//
// this protocol server (always the first one) just knows how to do HTTP 

int 
CControlObject::callback_http( struct libwebsocket_context *context,
								struct libwebsocket *wsi,
								enum libwebsocket_callback_reasons reason, 
								void *user,
								void *in, 
								size_t len )
{
	char client_name[128];
	char client_ip[128];
	wxString path;

	switch (reason) {
	
	case LWS_CALLBACK_HTTP:

		fprintf( stderr, "serving HTTP URI %s\n", (char *)in);

		if ( in && strcmp( (char *)in, "/favicon.ico") == 0) {

			path = CControlObject::m_pathRoot + _("/favicon.ico");
			if (libwebsockets_serve_http_file( wsi,
												path.ToAscii(), 
												"image/x-icon" ) ) {
				fprintf(stderr, "Failed to send favicon\n");
			}
			break;
		}

		// send the script... when it runs it'll start websockets 
		path = CControlObject::m_pathRoot + _("/test.html");
		if ( libwebsockets_serve_http_file( wsi,
											path.ToAscii(), 
											"text/html" ) )  {
			fprintf( stderr, "Failed to send HTTP file\n" );
		}
		break;

	/*
	 * callback for confirming to continue with client IP appear in
	 * protocol 0 callback since no websocket protocol has been agreed
	 * yet.  You can just ignore this if you won't filter on client IP
	 * since the default uhandled callback return is 0 meaning let the
	 * connection continue.
	 */

	case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:

		libwebsockets_get_peer_addresses( (int)(long)user, 
											client_name,
											sizeof( client_name ), 
											client_ip, 
											sizeof( client_ip ) );

		fprintf( stderr, 
					"Received network connect from %s (%s)\n",
					client_name, client_ip);

		/* if we returned non-zero from here, we kill the connection */
		break;

	default:
		break;
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
// dump_handshake_info
//

/*
 * this is just an example of parsing handshake headers, you don't need this
 * in your code unless you will filter allowing connections by the header
 * content
 */

void
CControlObject::dump_handshake_info( struct lws_tokens *lwst )
{
	int n;
	static const char *token_names[ WSI_TOKEN_COUNT ] = {
		/*[WSI_TOKEN_GET_URI]		=*/ "GET URI",
		/*[WSI_TOKEN_HOST]			=*/ "Host",
		/*[WSI_TOKEN_CONNECTION]	=*/ "Connection",
		/*[WSI_TOKEN_KEY1]			=*/ "key 1",
		/*[WSI_TOKEN_KEY2]			=*/ "key 2",
		/*[WSI_TOKEN_PROTOCOL]		=*/ "Protocol",
		/*[WSI_TOKEN_UPGRADE]		=*/ "Upgrade",
		/*[WSI_TOKEN_ORIGIN]		=*/ "Origin",
		/*[WSI_TOKEN_DRAFT]			=*/ "Draft",
		/*[WSI_TOKEN_CHALLENGE]		=*/ "Challenge",

		/* new for 04 */
		/*[WSI_TOKEN_KEY]			=*/ "Key",
		/*[WSI_TOKEN_VERSION]		=*/ "Version",
		/*[WSI_TOKEN_SWORIGIN]		=*/ "Sworigin",

		/* new for 05 */
		/*[WSI_TOKEN_EXTENSIONS]	=*/ "Extensions",

		/* client receives these */
		/*[WSI_TOKEN_ACCEPT]		=*/ "Accept",
		/*[WSI_TOKEN_NONCE]			=*/ "Nonce",
		/*[WSI_TOKEN_HTTP]			=*/ "Http",
		/*[WSI_TOKEN_MUXURL]		=*/ "MuxURL",
	};

	for (n = 0; n < WSI_TOKEN_COUNT; n++) {

		if ( lwst[n].token == NULL ) continue;

		fprintf(stderr, "    %s = %s\n", token_names[n], lwst[n].token);
	}
}

///////////////////////////////////////////////////////////////////////////////
// callback_dumb_increment
//

int 
CControlObject::callback_dumb_increment( struct libwebsocket_context *context,
											struct libwebsocket *wsi,
											enum libwebsocket_callback_reasons reason,
											void *user, 
											void *in, 
											size_t len )
{
	int n;
	unsigned char buf[ LWS_SEND_BUFFER_PRE_PADDING + 512 +
						LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *p = &buf[ LWS_SEND_BUFFER_PRE_PADDING ];
	struct per_session_data__dumb_increment *pss = 
					(struct per_session_data__dumb_increment *)user;

	switch ( reason ) {

	case LWS_CALLBACK_ESTABLISHED:
		fprintf( stderr, "callback_dumb_increment: "
						 "LWS_CALLBACK_ESTABLISHED\n");
		pss->number = 0;
		break;

	/*
	 * in this protocol, we just use the broadcast action as the chance to
	 * send our own connection-specific data and ignore the broadcast info
	 * that is available in the 'in' parameter
	 */

	case LWS_CALLBACK_BROADCAST:
		
		n = sprintf((char *)p, "%d", pss->number++);
		n = libwebsocket_write(wsi, p, n, LWS_WRITE_TEXT);
		
		if (n < 0) {
			fprintf(stderr, "ERROR writing to socket");
			return 1;
		}

		if ( close_testing && pss->number == 50 ) {
			fprintf(stderr, "close tesing limit, closing\n");
			libwebsocket_close_and_free_session(context, wsi,
						       LWS_CLOSE_STATUS_NORMAL);
		}
		break;

	case LWS_CALLBACK_RECEIVE:

		fprintf(stderr, "rx %d\n", (int)len);

		if (len < 6)
			break;

		if ( strcmp( (char *)in, "reset\n" ) == 0 )
			pss->number = 0;

		break;

	/*
	 * this just demonstrates how to use the protocol filter. If you won't
	 * study and reject connections based on header content, you don't need
	 * to handle this callback
	 */

	case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:

		dump_handshake_info((struct lws_tokens *)(long)user);
		/* you could return non-zero here and kill the connection */
		break;

	default:
		break;
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
// callback_lws_mirror
//

int
CControlObject::callback_lws_mirror( struct libwebsocket_context *context,
										struct libwebsocket *wsi,
										enum libwebsocket_callback_reasons reason,
										void *user, 
										void *in, 
										size_t len )
{
	int n;
	struct per_session_data__lws_mirror *pss = (per_session_data__lws_mirror *)user;

	switch (reason) {

	case LWS_CALLBACK_ESTABLISHED:
		fprintf(stderr, "callback_lws_mirror: "
						 "LWS_CALLBACK_ESTABLISHED\n");
		pss->ringbuffer_tail = ringbuffer_head;
		pss->wsi = wsi;
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		if (close_testing)
			break;
		if (pss->ringbuffer_tail != ringbuffer_head) {

			n = libwebsocket_write(wsi, (unsigned char *)
				   ringbuffer[pss->ringbuffer_tail].payload +
				   LWS_SEND_BUFFER_PRE_PADDING,
				   ringbuffer[pss->ringbuffer_tail].len,
								LWS_WRITE_TEXT);
			if (n < 0) {
				fprintf(stderr, "ERROR writing to socket");
				exit(1);
			}

			if (pss->ringbuffer_tail == (MAX_MESSAGE_QUEUE - 1))
				pss->ringbuffer_tail = 0;
			else
				pss->ringbuffer_tail++;

			if (((ringbuffer_head - pss->ringbuffer_tail) %
				  MAX_MESSAGE_QUEUE) < (MAX_MESSAGE_QUEUE - 15))
				libwebsocket_rx_flow_control(wsi, 1);

			libwebsocket_callback_on_writable(context, wsi);

		}
		break;

	case LWS_CALLBACK_BROADCAST:
		n = libwebsocket_write(wsi, (unsigned char *)in, len, LWS_WRITE_TEXT);
		if (n < 0)
			fprintf(stderr, "mirror write failed\n");
		break;

	case LWS_CALLBACK_RECEIVE:

		if (ringbuffer[ringbuffer_head].payload)
			free(ringbuffer[ringbuffer_head].payload);

		ringbuffer[ringbuffer_head].payload =
				malloc(LWS_SEND_BUFFER_PRE_PADDING + len +
						  LWS_SEND_BUFFER_POST_PADDING);
		ringbuffer[ringbuffer_head].len = len;
		memcpy((char *)ringbuffer[ringbuffer_head].payload +
					  LWS_SEND_BUFFER_PRE_PADDING, in, len);
		if (ringbuffer_head == (MAX_MESSAGE_QUEUE - 1))
			ringbuffer_head = 0;
		else
			ringbuffer_head++;

		if (((ringbuffer_head - pss->ringbuffer_tail) %
				  MAX_MESSAGE_QUEUE) > (MAX_MESSAGE_QUEUE - 10))
			libwebsocket_rx_flow_control(wsi, 0);

		libwebsocket_callback_on_writable_all_protocol(
					       libwebsockets_get_protocol(wsi));
		break;
	/*
	 * this just demonstrates how to use the protocol filter. If you won't
	 * study and reject connections based on header content, you don't need
	 * to handle this callback
	 */

	case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
		dump_handshake_info((struct lws_tokens *)(long)user);
		/* you could return non-zero here and kill the connection */
		break;

	default:
		break;
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


#endif // WIN32



///////////////////////////////////////////////////////////////////////////////
// clientMsgWorkerThread
//

clientMsgWorkerThread::clientMsgWorkerThread()
            : wxThread( wxTHREAD_JOINABLE )
{
    m_bQuit = false;
    m_pCtrlObject = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// deviceWorkerThread
//

clientMsgWorkerThread::~clientMsgWorkerThread()
{
    ;
}

///////////////////////////////////////////////////////////////////////////////
// Entry
//
// Is there any messages to send from Level II clients. Send it/them to all
// devices/clients except for itself.
//

void *clientMsgWorkerThread::Entry()
{
    VSCPEventList::compatibility_iterator nodeVSCP;
    vscpEvent *pvscpEvent = NULL;

    // Must be a valid control object pointer
    if ( NULL == m_pCtrlObject ) return NULL;

    while ( !TestDestroy() && !m_bQuit )
    {
        // Wait for event
        if ( wxSEMA_TIMEOUT == 
            m_pCtrlObject->m_semClientOutputQueue.WaitTimeout( 500 ) ) continue;

        if ( m_pCtrlObject->m_clientOutputQueue.GetCount() )
        {
			{
				wxString dbgStr = 
					wxString::Format( _("SendqueCount  = %d\r\n"), 
								         m_pCtrlObject->m_clientOutputQueue.GetCount() ); 
				m_pCtrlObject->logMsg( dbgStr, DAEMON_LOGMSG_INFO );
			}

            m_pCtrlObject->m_mutexClientOutputQueue.Lock();
            nodeVSCP = m_pCtrlObject->m_clientOutputQueue.GetFirst();
            pvscpEvent = nodeVSCP->GetData();
            m_pCtrlObject->m_clientOutputQueue.DeleteNode( nodeVSCP );
            m_pCtrlObject->m_mutexClientOutputQueue.Unlock();

            if ( ( NULL != pvscpEvent ) )
            {
                // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
                // * * * * Send event to all Level II clients (not to ourself )  * * * *
                // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

                m_pCtrlObject->sendEventAllClients ( pvscpEvent, pvscpEvent->obid );

            } // Valid event

        // Delete the event
        if ( NULL != pvscpEvent ) deleteVSCPevent( pvscpEvent ); 
 
        }  // while

    } // while

    return NULL;

}


///////////////////////////////////////////////////////////////////////////////
// OnExit
//

void clientMsgWorkerThread::OnExit()
{
    ;
}


