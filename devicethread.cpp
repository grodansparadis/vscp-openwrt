// devicethread.cpp
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
// $RCSfile: daemon_VSCP.cpp,v $
// $Date: 2005/09/15 16:26:43 $
// $Author: akhe $
// $Revision: 1.5 $

#include "wx/wxprec.h"
#include "wx/wx.h"
#include "wx/defs.h"
#include "wx/app.h"

#ifdef WIN32
#else

#define _POSIX

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#endif

#ifndef DWORD
#define DWORD unsigned long
#endif

#include "daemonvscp.h"
#include "canal_win32_ipc.h"
#include "canal_macro.h"
#include "vscp.h"
#include "vscpdlldef.h"
#include "vscphelper.h"
#include "../../common/dllist.h"
#include "../../common/md5.h"
#include "controlobject.h"
#include "devicethread.h"


///////////////////////////////////////////////////////////////////////////////
// deviceThread
//


deviceThread::deviceThread()
		: wxThread( wxTHREAD_JOINABLE )
{
	m_pDeviceItem = NULL;
	m_pCtrlObject = NULL;
	m_preceiveThread = NULL;
	m_pwriteThread = NULL;
}


deviceThread::~deviceThread()
{
	;
}


///////////////////////////////////////////////////////////////////////////////
// Entry
//

void *deviceThread::Entry()
{
    // Must have a valid pointer to the device item
    if ( NULL == m_pDeviceItem ) return NULL;

    // Must have a valid pointer to the control object
    if ( NULL == m_pCtrlObject ) return NULL;

    // We need to create a clientobject and add this object to the list
    m_pDeviceItem->m_pClientItem = new CClientItem;
    if ( NULL == m_pDeviceItem->m_pClientItem ) {
        return NULL;
    }

    // This is now an active Client
    m_pDeviceItem->m_pClientItem->m_bOpen = true;
    m_pDeviceItem->m_pClientItem->m_type = 	CLIENT_ITEM_INTERFACE_TYPE_DRIVER_CANAL;
    m_pDeviceItem->m_pClientItem->m_strDeviceName = m_pDeviceItem->m_strName;
    m_pDeviceItem->m_pClientItem->m_strDeviceName += _(" Started at ");
    wxDateTime now = wxDateTime::Now(); 
    m_pDeviceItem->m_pClientItem->m_strDeviceName += now.FormatISODate();
    m_pDeviceItem->m_pClientItem->m_strDeviceName += _(" ");
    m_pDeviceItem->m_pClientItem->m_strDeviceName += now.FormatISOTime();

    // Add the client to the Client List
    m_pCtrlObject->m_wxClientMutex.Lock();
    m_pCtrlObject->addClient ( m_pDeviceItem->m_pClientItem );
    m_pCtrlObject->m_wxClientMutex.Unlock();


    // Load dynamic library
    if ( ! m_wxdll.Load ( m_pDeviceItem->m_strPath, wxDL_LAZY ) )
    {
        m_pCtrlObject->logMsg ( _T ( "Unable to load dynamic library." ), DAEMON_LOGMSG_CRITICAL );
        return NULL;
    }

    if ( VSCP_DRIVER_LEVEL1 == m_pDeviceItem->m_driverLevel ) {

        // Now find methods in library

        // * * * * CANAL OPEN * * * *
        if ( NULL == ( m_pDeviceItem->m_proc_CanalOpen =
                ( LPFNDLL_CANALOPEN ) m_wxdll.GetSymbol ( _T( "CanalOpen" ) ) ) ) {
            // Free the library
            m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalOpen." ), DAEMON_LOGMSG_CRITICAL );
            return NULL;
        }

        // * * * * CANAL CLOSE * * * *
        if ( NULL == ( m_pDeviceItem->m_proc_CanalClose =
                ( LPFNDLL_CANALCLOSE ) m_wxdll.GetSymbol ( _T( "CanalClose" ) ) ) ) {
            // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalClose." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * CANAL GETLEVEL * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalGetLevel =
	                       ( LPFNDLL_CANALGETLEVEL ) m_wxdll.GetSymbol ( _T( "CanalGetLevel" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalGetLevel." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * CANAL SEND * * * *
        if ( NULL == ( m_pDeviceItem->m_proc_CanalSend =
	                           ( LPFNDLL_CANALSEND ) m_wxdll.GetSymbol ( _T( "CanalSend" ) ) ) ) {
            // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalSend." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * CANAL DATA AVAILABLE * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalDataAvailable =
	                       ( LPFNDLL_CANALDATAAVAILABLE ) m_wxdll.GetSymbol ( _T( "CanalDataAvailable" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalDataAvailable." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }


	    // * * * * CANAL RECEIVE * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalReceive =
	                       ( LPFNDLL_CANALRECEIVE ) m_wxdll.GetSymbol ( _T( "CanalReceive" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalReceive." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * CANAL GET STATUS * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalGetStatus =
	                       ( LPFNDLL_CANALGETSTATUS ) m_wxdll.GetSymbol ( _T( "CanalGetStatus" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalGetStatus." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * CANAL GET STATISTICS * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalGetStatistics =
	                       ( LPFNDLL_CANALGETSTATISTICS ) m_wxdll.GetSymbol ( _T( "CanalGetStatistics" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalGetStatistics." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * CANAL SET FILTER * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalSetFilter =
	                       ( LPFNDLL_CANALSETFILTER ) m_wxdll.GetSymbol ( _T( "CanalSetFilter" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalSetFilter." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * CANAL SET MASK * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalSetMask =
	                       ( LPFNDLL_CANALSETMASK ) m_wxdll.GetSymbol ( _T( "CanalSetMask" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalSetMask." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * CANAL GET VERSION * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalGetVersion =
	                       ( LPFNDLL_CANALGETVERSION ) m_wxdll.GetSymbol ( _T( "CanalGetVersion" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalGetVersion." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * CANAL GET DLL VERSION * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalGetDllVersion =
	                       ( LPFNDLL_CANALGETDLLVERSION ) m_wxdll.GetSymbol ( _T( "CanalGetDllVersion" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalGetDllVersion." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * CANAL GET VENDOR STRING * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalGetVendorString =
	                       ( LPFNDLL_CANALGETVENDORSTRING ) m_wxdll.GetSymbol ( _T( "CanalGetVendorString" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalGetVendorString." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }


	    // ******************************
	    //     Generation 2 Methods
	    // ******************************


        // * * * * CANAL BLOCKING SEND * * * *
        m_pDeviceItem->m_proc_CanalBlockingSend = NULL;
        if ( m_wxdll.HasSymbol( _T("CanalBlockingSend" ) ) ) {
            if ( NULL == ( m_pDeviceItem->m_proc_CanalBlockingSend =
                ( LPFNDLL_CANALBLOCKINGSEND ) m_wxdll.GetSymbol ( _T( "CanalBlockingSend" ) ) ) ) {
                m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalBlockingSend. Probably Generation 1 driver." ),
                                            DAEMON_LOGMSG_CRITICAL );
                m_pDeviceItem->m_proc_CanalBlockingSend = NULL;
            }  
        }
        else {
            m_pCtrlObject->logMsg ( _T( "CanalBlockingSend not available. \n\tNon blocking operations set.\n" ),
                                    DAEMON_LOGMSG_WARNING );	
        }

        // * * * * CANAL BLOCKING RECEIVE * * * *
        m_pDeviceItem->m_proc_CanalBlockingReceive = NULL;
        if ( m_wxdll.HasSymbol( _T("CanalBlockingReceive" ) ) ) {
            if ( NULL == ( m_pDeviceItem->m_proc_CanalBlockingReceive =
                    ( LPFNDLL_CANALBLOCKINGRECEIVE ) m_wxdll.GetSymbol ( _T( "CanalBlockingReceive" ) ) ) ) {
                m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalBlockingReceive. Probably Generation 1 driver." ),
                                        DAEMON_LOGMSG_CRITICAL );
                m_pDeviceItem->m_proc_CanalBlockingReceive = NULL;
            }
        }
        else {
            m_pCtrlObject->logMsg( _T( "CanalBlockingReceive not available. \n\tNon blocking operations set.\n" ),
                                    DAEMON_LOGMSG_WARNING );	
        }

	    // * * * * CANAL GET DRIVER INFO * * * *
        m_pDeviceItem->m_proc_CanalGetdriverInfo = NULL;
        if ( m_wxdll.HasSymbol( _T("CanalGetDriverInfo" ) ) ) {
            if ( NULL == ( m_pDeviceItem->m_proc_CanalGetdriverInfo =
                ( LPFNDLL_CANALGETDRIVERINFO ) m_wxdll.GetSymbol ( _T( "CanalGetDriverInfo" ) ) ) ) {
                m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for CanalGetDriverInfo. Probably Generation 1 driver." ),
                                        DAEMON_LOGMSG_CRITICAL );
                m_pDeviceItem->m_proc_CanalGetdriverInfo = NULL;
            }
        }


        // Open the device
	    m_pDeviceItem->m_openHandle =
	    m_pDeviceItem->m_proc_CanalOpen( ( const char * ) m_pDeviceItem->m_strParameter.mb_str ( wxConvUTF8 ),
	                                      m_pDeviceItem->m_DeviceFlags );

	    // Check if the driver opened properly
	    if ( m_pDeviceItem->m_openHandle <= 0 ) {
            wxString errMsg = _T ( "Failed to open driver. Will not use it! \n\t[ ") 
                                    + m_pDeviceItem->m_strName + _T(" ]\n");
		    m_pCtrlObject->logMsg ( errMsg, DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // Get Driver Level
	    m_pDeviceItem->m_driverLevel = m_pDeviceItem->m_proc_CanalGetLevel ( m_pDeviceItem->m_openHandle );


        //  * * * Level I Driver * * *

        // Check if blocking driver is available
        if ( NULL != m_pDeviceItem->m_proc_CanalBlockingReceive ) {

			// * * * * Blocking version * * * *

			/////////////////////////////////////////////////////////////////////////////
			// Device write worker thread
			/////////////////////////////////////////////////////////////////////////////
			m_pwriteThread = new deviceWriteThread;

			if ( m_pwriteThread ) {
				m_pwriteThread->m_pMainThreadObj = this;
				wxThreadError err;
				if ( wxTHREAD_NO_ERROR == ( err = m_pwriteThread->Create() ) ) {
					m_pwriteThread->SetPriority( WXTHREAD_MAX_PRIORITY );
					if ( wxTHREAD_NO_ERROR != ( err = m_pwriteThread->Run() ) ) {
						m_pCtrlObject->logMsg ( _("Unable to run device write worker thread."), DAEMON_LOGMSG_CRITICAL );	
					}
				}
				else {
					m_pCtrlObject->logMsg ( _("Unable to create device write worker thread."), DAEMON_LOGMSG_CRITICAL );	
				}
			}
			else {
				m_pCtrlObject->logMsg ( _("Unable to allocate memory for device write worker thread."), DAEMON_LOGMSG_CRITICAL );
			}

			/////////////////////////////////////////////////////////////////////////////
			// Device read worker thread
			/////////////////////////////////////////////////////////////////////////////
			m_preceiveThread = new deviceReceiveThread;

			if ( m_preceiveThread ) {
				m_preceiveThread->m_pMainThreadObj = this;
				wxThreadError err;
				if ( wxTHREAD_NO_ERROR == ( err = m_preceiveThread->Create() ) ) {
					m_preceiveThread->SetPriority( WXTHREAD_MAX_PRIORITY );
					if ( wxTHREAD_NO_ERROR != ( err = m_preceiveThread->Run() ) ) {
						m_pCtrlObject->logMsg ( _("Unable to run device receive worker thread."), DAEMON_LOGMSG_CRITICAL );	
					}
				}
				else {
					m_pCtrlObject->logMsg ( _("Unable to create device receive worker thread."), DAEMON_LOGMSG_CRITICAL );	
				}
			}
			else {
				m_pCtrlObject->logMsg ( _("Unable to allocate memory for device receive worker thread."), DAEMON_LOGMSG_CRITICAL );
			}

			// Just sit and wait until the end of the world as we know it...
			while ( !m_pDeviceItem->m_bQuit ) {
				wxSleep ( 1 );
			}

            m_preceiveThread->m_bQuit = true;
            m_pwriteThread->m_bQuit = true;
            m_preceiveThread->Wait();
            m_pwriteThread->Wait();
		}
		else {

			// * * * * Non blocking version * * * *

            bool bActivity;
			while ( !TestDestroy() && !m_pDeviceItem->m_bQuit ) {

                bActivity = false;
                /////////////////////////////////////////////////////////////////////////////
                //                           Receive from device						   //
                /////////////////////////////////////////////////////////////////////////////
                canalMsg msg;
                if ( m_pDeviceItem->m_proc_CanalDataAvailable( m_pDeviceItem->m_openHandle ) ) {

                    if ( CANAL_ERROR_SUCCESS == 
                            m_pDeviceItem->m_proc_CanalReceive( m_pDeviceItem->m_openHandle, &msg ) ) {

                        bActivity = true;

                        // There must be room in the receive queue
                        if ( m_pCtrlObject->m_maxItemsInClientReceiveQueue >
                            m_pCtrlObject->m_clientOutputQueue.GetCount() ) {

                            vscpEvent *pvscpEvent = new vscpEvent;
                            if ( NULL != pvscpEvent ) {

                                // Convert CANAL message to VSCP event
                                convertCanalToEvent ( pvscpEvent,
                                                        &msg,
                                                        m_pDeviceItem->m_pClientItem->m_GUID );

                                pvscpEvent->obid = m_pDeviceItem->m_pClientItem->m_clientID;

                                m_pCtrlObject->m_mutexClientOutputQueue.Lock();
                                m_pCtrlObject->m_clientOutputQueue.Append ( pvscpEvent );
                                m_pCtrlObject->m_semClientOutputQueue.Post();
                                m_pCtrlObject->m_mutexClientOutputQueue.Unlock();

                            }
                        }
                    }
                } // data available


                // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
                //             Send messages (if any) in the outqueue
                // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

                // Check if there is something to send
                if ( m_pDeviceItem->m_pClientItem->m_clientInputQueue.GetCount() ) {
			
                    bActivity = true;

                    CLIENTEVENTLIST::compatibility_iterator nodeClient;

                    m_pDeviceItem->m_pClientItem->m_mutexClientInputQueue.Lock();
                    nodeClient = m_pDeviceItem->m_pClientItem->m_clientInputQueue.GetFirst();
                    vscpEvent *pqueueEvent = nodeClient->GetData();
                    m_pDeviceItem->m_pClientItem->m_mutexClientInputQueue.Unlock();
				
                    canalMsg canalMsg;
                    convertEventToCanal ( &canalMsg, pqueueEvent );
                    if ( CANAL_ERROR_SUCCESS == 
                        m_pDeviceItem->m_proc_CanalSend ( m_pDeviceItem->m_openHandle, &canalMsg ) ) {
                        // Remove the node
                        delete pqueueEvent;
                        m_pDeviceItem->m_pClientItem->m_clientInputQueue.DeleteNode ( nodeClient );  
                    }
                    else {
                        // Another try
                        //m_pCtrlObject->m_semClientOutputQueue.Post();
                        deleteVSCPevent( pqueueEvent );
                        m_pDeviceItem->m_pClientItem->m_clientInputQueue.DeleteNode ( nodeClient );
                    }

                } // events

                if ( !bActivity ) {
                    ::wxMilliSleep( 100 );
                }

                bActivity = false;

            } // while working - non blocking

        } // if blocking/non blocking


	    // Close CANAL channel
	    m_pDeviceItem->m_proc_CanalClose ( m_pDeviceItem->m_openHandle );

	    // Library is unloaded in destructor

		// Remove messages in the client queues
        m_pCtrlObject->m_wxClientMutex.Lock();
	    m_pCtrlObject->removeClient ( m_pDeviceItem->m_pClientItem );
	    m_pCtrlObject->m_wxClientMutex.Unlock();

	    if ( NULL != m_preceiveThread ) {
		    m_preceiveThread->Wait();
		    delete m_preceiveThread;
	    }

	    if ( NULL != m_pwriteThread ) {
		    m_pwriteThread->Wait();
		    delete m_pwriteThread;
	    }

    }
    else if ( VSCP_DRIVER_LEVEL2 == m_pDeviceItem->m_driverLevel ) {
    
        // Now find methods in library

        // * * * * VSCP OPEN * * * *
        if ( NULL == ( m_pDeviceItem->m_proc_VSCPOpen =
                ( LPFNDLL_VSCPOPEN ) m_wxdll.GetSymbol ( _T( "VSCPOpen" ) ) ) ) {
            // Free the library
            m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for VSCPOpen." ), DAEMON_LOGMSG_CRITICAL );
            return NULL;
        }

        // * * * * VSCP CLOSE * * * *
        if ( NULL == ( m_pDeviceItem->m_proc_VSCPClose =
                ( LPFNDLL_VSCPCLOSE ) m_wxdll.GetSymbol ( _T( "VSCPClose" ) ) ) ) {
            // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for VSCPClose." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * VSCP GETLEVEL * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_VSCPGetLevel =
	                       ( LPFNDLL_VSCPGETLEVEL ) m_wxdll.GetSymbol ( _T( "VSCPGetLevel" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for VSCPGetLevel." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

        // * * * * VSCP GET DLL VERSION * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_VSCPGetDllVersion =
	                       ( LPFNDLL_VSCPGETDLLVERSION ) m_wxdll.GetSymbol ( _T( "VSCPGetDllVersion" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for VSCPGetDllVersion." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

	    // * * * * VSCP GET VENDOR STRING * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_VSCPGetVendorString =
	                       ( LPFNDLL_VSCPGETVENDORSTRING ) m_wxdll.GetSymbol ( _T( "VSCPGetVendorString" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for VSCPGetVendorString." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

        // * * * * VSCP GET DRIVER INFO * * * *
	    if ( NULL == ( m_pDeviceItem->m_proc_CanalGetdriverInfo =
	                       ( LPFNDLL_VSCPGETVENDORSTRING ) m_wxdll.GetSymbol ( _T( "VSCPGetDriverInfo" ) ) ) ) {
		    // Free the library
		    m_pCtrlObject->logMsg ( _T ( "Unable to get dl entry for VSCPGetDriverInfo." ), DAEMON_LOGMSG_CRITICAL );
		    return NULL;
	    }

        // Username, password, host and port can be set in configuration file. Read in them here
        // if they are.
        wxString strHost( _("localhost") );
        short port = 9598;

        wxStringTokenizer tkz( m_pDeviceItem->m_strParameter, _(";") );
        if ( tkz.HasMoreTokens() ) {
            
            CVSCPVariable *pVar;

            // Get prefix
            wxString prefix = tkz.GetNextToken();

            // Check if username is specified in the configuration file
            pVar = m_pCtrlObject->m_VSCP_Variables.find( m_pDeviceItem->m_strName + _("_username") );
            if ( NULL != pVar ) {
                wxString str;
                if ( VSCP_DAEMON_VARIABLE_CODE_STRING == pVar->getType() ) {
                    pVar->getValue( &str );
                    m_pCtrlObject->m_driverUsername = str;
                }
            }

            // Check if password is specified in the configuration file
            pVar = m_pCtrlObject->m_VSCP_Variables.find( m_pDeviceItem->m_strName + _("_password") );
            if ( NULL != pVar ) {
                wxString str;
                if ( VSCP_DAEMON_VARIABLE_CODE_STRING == pVar->getType() ) {
                    pVar->getValue( &str );
                    m_pCtrlObject->m_driverPassword = str;
                }
            }

            // Check if host is specified in the configuration file
            pVar = m_pCtrlObject->m_VSCP_Variables.find( m_pDeviceItem->m_strName + _("_host") );
            if ( NULL != pVar ) {
                wxString str;
                if ( VSCP_DAEMON_VARIABLE_CODE_STRING == pVar->getType() ) {
                    pVar->getValue( &str );
                    strHost = str;
                }
            }

            // Check if port is specified in the configuration file
            pVar = m_pCtrlObject->m_VSCP_Variables.find( m_pDeviceItem->m_strName + _("_port") );
            if ( NULL != pVar ) {
                int cfgport;
                if ( VSCP_DAEMON_VARIABLE_CODE_INTEGER == pVar->getType() ) {
                    pVar->getValue( (int *)&cfgport );
                    port = cfgport;
                }
            }

        }

        m_pDeviceItem->m_pClientItem->m_type = 	CLIENT_ITEM_INTERFACE_TYPE_DRIVER_TCPIP;

        // Open up the driver
        // Open the device
	    m_pDeviceItem->m_openHandle =
            m_pDeviceItem->m_proc_VSCPOpen( m_pCtrlObject->m_driverUsername.mb_str( wxConvUTF8 ),
                                                m_pCtrlObject->m_driverPassword.mb_str( wxConvUTF8 ),
                                                strHost.mb_str( wxConvUTF8 ),
                                                port,
                                                ( const char * )m_pDeviceItem->m_strName.mb_str( wxConvUTF8 ),
                                                ( const char * )m_pDeviceItem->m_strParameter.mb_str( wxConvUTF8 ),
	                                            m_pDeviceItem->m_DeviceFlags );


		// Just sit and wait until the end of the world as we know it...
		while ( !TestDestroy() && !m_pDeviceItem->m_bQuit )
		{
			wxSleep ( 200 );
		}
        
    }



	//
	// =====================================================================================
	//

		
	

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// OnExit
//

void deviceThread::OnExit()
{

}


// ****************************************************************************


///////////////////////////////////////////////////////////////////////////////
// deviceReceiveThread
//



deviceReceiveThread::deviceReceiveThread()
		: wxThread( wxTHREAD_JOINABLE )
{
	m_pMainThreadObj = NULL;
  m_bQuit = false;
}


deviceReceiveThread::~deviceReceiveThread()
{
	;
}

///////////////////////////////////////////////////////////////////////////////
// Entry
//

void *deviceReceiveThread::Entry()
{
	canalMsg msg;
	CanalMsgOutList::compatibility_iterator nodeCanal;

	// Must be a valid main object pointer
	if ( NULL == m_pMainThreadObj ) return NULL;

  // Blocking receive method must have been found
  if ( NULL == m_pMainThreadObj->m_pDeviceItem->m_proc_CanalBlockingReceive ) return NULL;

  int rv;
	while ( !TestDestroy() && !m_bQuit )
	{
		if ( CANAL_ERROR_SUCCESS == 
        ( rv = m_pMainThreadObj->m_pDeviceItem->m_proc_CanalBlockingReceive ( 
                              m_pMainThreadObj->m_pDeviceItem->m_openHandle, &msg, 500 ) ) )
		{
			// There must be room in the receive queue
			if ( m_pMainThreadObj->m_pCtrlObject->m_maxItemsInClientReceiveQueue >
			        m_pMainThreadObj->m_pCtrlObject->m_clientOutputQueue.GetCount() )
			{

				vscpEvent *pvscpEvent = new vscpEvent;
				if ( NULL != pvscpEvent ) {

					// Convert CANAL message to VSCP event
					convertCanalToEvent ( pvscpEvent,
											&msg,
        									m_pMainThreadObj->m_pDeviceItem->m_pClientItem->m_GUID );

          pvscpEvent->obid = m_pMainThreadObj->m_pDeviceItem->m_pClientItem->m_clientID;

					m_pMainThreadObj->m_pCtrlObject->m_mutexClientOutputQueue.Lock();
					m_pMainThreadObj->m_pCtrlObject->m_clientOutputQueue.Append ( pvscpEvent );
					m_pMainThreadObj->m_pCtrlObject->m_semClientOutputQueue.Post();
					m_pMainThreadObj->m_pCtrlObject->m_mutexClientOutputQueue.Unlock();

				}
			}
		}
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// OnExit
//

void deviceReceiveThread::OnExit()
{
	;
}


// ****************************************************************************


///////////////////////////////////////////////////////////////////////////////
// deviceWriteThread
//

deviceWriteThread::deviceWriteThread()
		: wxThread(wxTHREAD_JOINABLE)
{
	m_pMainThreadObj = NULL;
  m_bQuit = false;
}


deviceWriteThread::~deviceWriteThread()
{
	;
}


///////////////////////////////////////////////////////////////////////////////
// Entry
//

void *deviceWriteThread::Entry()
{

	CanalMsgOutList::compatibility_iterator nodeCanal;

	// Must be a valid main object pointer
	if ( NULL == m_pMainThreadObj ) return NULL;

   // Blocking send method must have been found
  if ( NULL == m_pMainThreadObj->m_pDeviceItem->m_proc_CanalBlockingSend ) return NULL;

	while ( !TestDestroy() && !m_bQuit )
	{
		// Wait until there is something to send
		if ( wxSEMA_TIMEOUT == 
      								m_pMainThreadObj->m_pDeviceItem->m_pClientItem->m_semClientInputQueue.WaitTimeout( 500 ) ) {
			continue;
		}

		CLIENTEVENTLIST::compatibility_iterator nodeClient;

		if ( m_pMainThreadObj->m_pDeviceItem->m_pClientItem->m_clientInputQueue.GetCount() ) {

			m_pMainThreadObj->m_pDeviceItem->m_pClientItem->m_mutexClientInputQueue.Lock();
			nodeClient = m_pMainThreadObj->m_pDeviceItem->m_pClientItem->m_clientInputQueue.GetFirst();
			vscpEvent *pqueueEvent = nodeClient->GetData();

			m_pMainThreadObj->m_pDeviceItem->m_pClientItem->m_mutexClientInputQueue.Unlock();
				
			canalMsg canalMsg;
			convertEventToCanal( &canalMsg, pqueueEvent );
			if ( CANAL_ERROR_SUCCESS == 
							m_pMainThreadObj->m_pDeviceItem->m_proc_CanalBlockingSend ( m_pMainThreadObj->m_pDeviceItem->m_openHandle, &canalMsg, 300 ) ) {
				// Remove the node
				deleteVSCPevent( pqueueEvent );
				m_pMainThreadObj->m_pDeviceItem->m_pClientItem->m_clientInputQueue.DeleteNode ( nodeClient );  
			}
			else {
				// Give it another try
				m_pMainThreadObj->m_pCtrlObject->m_semClientOutputQueue.Post();
			}

		} // events in queue 

	} // while

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// OnExit
//

void deviceWriteThread::OnExit()
{

}
