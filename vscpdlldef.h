// vscpdlldef.h
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
// 
// This file is part of the VSCP (http://www.vscp.org) 
//
// Copyright (C) 2000-2011 Ake Hedman, eurosource, <akhe@eurosource.se>
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
// $RCSfile: vscpdlldef.h,v $                                       
// $Date: 2005/01/05 12:50:32 $                                  
// $Author: akhe $                                              
// $Revision: 1.3 $ 

#ifndef ___VSCPDLLDEF_H___
#define ___VSCPDLLDEF_H___


#include "vscp.h"

#ifdef WIN32
typedef long ( __stdcall * LPFNDLL_VSCPOPEN) ( const char *pUsername,
                                                const char *pPassword,
                                                const char *pHost,
                                                short port,
                                                const char *pPrefix,
                                                const char *pParameter, 
                                                unsigned long flags );
typedef int ( __stdcall * LPFNDLL_VSCPCLOSE) ( long handle );
typedef unsigned long ( __stdcall * LPFNDLL_VSCPGETLEVEL) ( long handle  );
typedef unsigned long ( __stdcall * LPFNDLL_VSCPGETVERSION) ( void );
typedef unsigned long ( __stdcall * LPFNDLL_VSCPGETDLLVERSION) ( void );
typedef const char * ( __stdcall * LPFNDLL_VSCPGETVENDORSTRING) ( void );
typedef const char * ( __stdcall * LPFNDLL_VSCPGETDRIVERINFO) ( void );

#else

typedef long ( *LPFNDLL_VSCPOPEN ) ( const char *pUsername,
                                        const char *pPassword,
                                        const char *pHost,
                                        short port,
                                        const char *pPrefix,
                                        const char *pParameter, 
                                        unsigned long flags );
typedef int ( *LPFNDLL_VSCPCLOSE ) ( long handle );
typedef unsigned long ( *LPFNDLL_VSCPGETLEVEL ) ( long handle  );
typedef unsigned long ( *LPFNDLL_VSCPGETVERSION ) (  void );
typedef unsigned long ( *LPFNDLL_VSCPGETDLLVERSION ) (  void );
typedef const char *( *LPFNDLL_VSCPGETVENDORSTRING ) (  void );
typedef const char *( *LPFNDLL_VSCPGETDRIVERINFO ) (  void );

#endif


#endif // ___VSCPDLLDEF_H__



