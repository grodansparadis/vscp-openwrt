/////////////////////////////////////////////////////////////////////////////
// Name:        vscpclientthreads.h
// 
// This file is part of the VSCP (http://www.vscp.org) 
//
// Copyright (C) 2000-2011 Ake Hedman, eurosource, <ake@eurosource.se>
// 
// This file is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this file see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.
// 
//  As a special exception, if other files instantiate templates or use macros
//  or inline functions from this file, or you compile this file and link it
//  with other works to produce a work based on this file, this file does not
//  by itself cause the resulting work to be covered by the GNU General Public
//  License. However the source code for this file must still be made available
//  in accordance with section (3) of the GNU General Public License.
// 
//  This exception does not invalidate any other reasons why a work based on
//  this file might be covered by the GNU General Public License.
// 
//  Alternative licenses for VSCP & Friends may be arranged by contacting 
//  eurosource at info@eurosource.se, http://www.eurosource.se
/////////////////////////////////////////////////////////////////////////////


#if !defined(AFX_VSCPCLIENTTHREADS_H__C2A773AD_8886_40F0_96C4_4DCA663402B2__INCLUDED_)
#define AFX_VSCPCLIENTTHREADS_H__C2A773AD_8886_40F0_96C4_4DCA663402B2__INCLUDED_

#include <wx/list.h>
#include <wx/ffile.h>
#include <wx/dynlib.h>

#include "../common/canal.h"
#include "../common/vscp.h"
#include "../common/canaldlldef.h"
#include "../common/canalsuperwrapper.h"

#define INTERFACE_CANAL   0
#define INTERFACE_VSCP    1

// List with transmission objects
WX_DECLARE_LIST( vscpEvent, TXLIST );

// Outqueue
WX_DECLARE_LIST( vscpEvent, eventOutQueue );


DECLARE_EVENT_TYPE(wxVSCP_IN_EVENT, wxID_ANY)					// Received event
DECLARE_EVENT_TYPE(wxVSCP_OUT_EVENT, wxID_ANY)				// Transmitted event
DECLARE_EVENT_TYPE(wxVSCP_CTRL_LOST_EVENT, wxID_ANY)	// Control thread connection lost
DECLARE_EVENT_TYPE(wxVSCP_RCV_LOST_EVENT, wxID_ANY)		// Receive thread connection lost

// Error constants for worker threads
#define VSCP_SESSION_ERROR_UNABLE_TO_CONNECT  -1
#define VSCP_SESSION_ERROR_TERMINATED         -99


// Structure for CANAL drivers
typedef struct {
    wxString m_strDescription;	// Decription of driver
    wxString m_strPath;					// Path to driver
    wxString m_strConfig;				// Driver configuration string
    unsigned long m_flags;			// Driver flags
		unsigned char m_GUID[16];		// GUID for interface
} canal_interface;

// Structure for VSCP drivers
typedef struct {	
    wxString m_strDescription;	// Description of VSCP interface
    wxString m_strHost;					// Host where server lives
    wxString m_strUser;					// Username
    wxString m_strPassword;			// Password
    unsigned long m_port;				// Port to use on server
} vscp_interface;

/*!
  Shared object among threads.
*/
class ctrlObj
{
  public:
  
  ctrlObj();
   ~ctrlObj();
  
  /// Thread run control
  bool m_bQuit;

  /*!
    Start worker threads
    @param pWnd Pointer to owenr window that wilol receive evenst
    @param id ID for owner window.
  */
  void startWorkerThreads( wxWindow *pWnd, unsigned long id );

  /*!
    Terminate the worker threads.
   */
  void stopWorkerThreads( void );

  /*!
    Pointer to VSCP session window
  */
  wxWindow *m_pWnd;

  /*!
    Id for the above window
  */
  unsigned long m_windowID;
  
  /// Thread error return code for control thread
  long m_errorControl;

  /// Thread error return code for receive thread
  long m_errorReceive;
  
  /// Chennel ID for tx channel 
  unsigned long m_txChannelID;
  
  /// Chennel ID for rx channel 
  unsigned long m_rxChannelID;
  
  // Event output queue
  eventOutQueue m_outQueue;
  
  /// Protection for output queue
  wxMutex m_mutexOutQueue;
  wxSemaphore m_semOutQue;
  
  /// Table for transmission objects
  TXLIST m_txList;
  
  /// Interface type
  int m_interfaceType;

	/*!
		CANAL driver level
	*/
	unsigned char m_driverLevel;
  
  /// Interface information CANAL interface type
  canal_interface m_ifCANAL;
  
  /// Interface information VSCP interface type
  vscp_interface m_ifVSCP;

	/*!
		Mutex handle that is used for sharing of the device.
	*/
	wxMutex m_deviceMutex;

  /*!
    GUID for CANAL device
  */
  unsigned char m_GUID[16];


	// Handle for dll/dl driver interface
	long m_openHandle;

	// CANAL methods
	LPFNDLL_CANALOPEN				      m_proc_CanalOpen;
	LPFNDLL_CANALCLOSE				    m_proc_CanalClose;
	LPFNDLL_CANALGETLEVEL			    m_proc_CanalGetLevel;
	LPFNDLL_CANALSEND				      m_proc_CanalSend;
	LPFNDLL_CANALRECEIVE			    m_proc_CanalReceive;
	LPFNDLL_CANALDATAAVAILABLE		m_proc_CanalDataAvailable;
	LPFNDLL_CANALGETSTATUS			  m_proc_CanalGetStatus;
	LPFNDLL_CANALGETSTATISTICS		m_proc_CanalGetStatistics;
	LPFNDLL_CANALSETFILTER			  m_proc_CanalSetFilter;
	LPFNDLL_CANALSETMASK			    m_proc_CanalSetMask;
	LPFNDLL_CANALSETBAUDRATE		  m_proc_CanalSetBaudrate;
	LPFNDLL_CANALGETVERSION			  m_proc_CanalGetVersion;
	LPFNDLL_CANALGETDLLVERSION		m_proc_CanalGetDllVersion;
	LPFNDLL_CANALGETVENDORSTRING	m_proc_CanalGetVendorString;
	// Generation 2
	LPFNDLL_CANALBLOCKINGSEND		  m_proc_CanalBlockingSend;
	LPFNDLL_CANALBLOCKINGRECEIVE	m_proc_CanalBlockingReceive;
	LPFNDLL_CANALGETDRIVERINFO		m_proc_CanalGetdriverInfo;
  
};


/*!
	This class implement a thread that handles
	transmit events
*/

class TXWorkerThread : public wxThread
{

public:
	
	/// Constructor
	TXWorkerThread();

	/// Destructor
	virtual ~TXWorkerThread();

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
		Pointer to control object.
	*/
	ctrlObj *m_pCtrlObject;

};

/*!
	This class implement a thread that handles
	client receive events
*/

class RXWorkerThread : public wxThread
{

public:
	
	/// Constructor
	RXWorkerThread();

	/// Destructor
	virtual ~RXWorkerThread();

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
		Pointer to control object.
	*/
	ctrlObj *m_pCtrlObject;
  


};






/////////////////////////////////////////////////////////////////////



class deviceThread;




/*!
	This class implement a thread that write data
	to a blocking driver
*/

class deviceWriteThread : public wxThread
{

public:
	
	/// Constructor
	deviceWriteThread();

	/// Destructor
	virtual ~deviceWriteThread();

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
		Pointer to master thread.
	*/
	deviceThread *m_pMainThreadObj;
  

  /*!
    Termination control
  */
  bool m_bQuit;


};


/*!
	This class implement a thread that read data
	from a blocking driver
*/

class deviceReceiveThread : public wxThread
{

public:
	
	/// Constructor
	deviceReceiveThread();

	/// Destructor
	virtual ~deviceReceiveThread();

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
		Pointer to master thread.
	*/
	deviceThread *m_pMainThreadObj;
  

  /*!
    Termination control
  */
  bool m_bQuit;

};

/*!
	This class implement a one of thread that look
	for specific events and react on them appropriatly.

*/

class deviceThread : public wxThread
{

public:
	
	/// Constructor
	deviceThread();

	/// Destructor
	virtual ~deviceThread();

	/*!
		Thread code entry point
	*/
	virtual void *Entry();


	/*! 
		called when the thread exits - whether it terminates normally or is
		stopped with Delete() (but not when it is Kill()ed!)
	*/
  virtual void OnExit();


	/// dl/dll handler
	wxDynamicLibrary m_wxdll;


	/*!
		Pointer to control object.
	*/
	ctrlObj *m_pCtrlObject;
  

	/*!
		Holder for receive thread
	*/
	deviceReceiveThread *m_preceiveThread;

	/*!
		Holder for write thread
	*/
	deviceWriteThread *m_pwriteThread;

		/*!
			Check filter/mask to see if filter should be delivered

			The filter have the following matrix

			mask bit n	|	filter bit n	| msg id bit	|	result
			===========================================================
				0				X					X			Accept
				1				0					0			Accept
				1				0					1			Reject
				1				1					0			Reject
				1				1					1			Accept

				Formula is !( ( filter ?d ) & mask )

			@param pclientItem Pointer to client item
			@param pcanalMsg Pointer to can message
			@return True if message is accepted false if rejected
			TODO
		*/
		
};



#endif // #if !defined(AFX_VSCPCLIENTTHREADS_H__C2A773AD_8886_40F0_96C4_4DCA663402B2__INCLUDED_)