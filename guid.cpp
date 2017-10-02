///////////////////////////////////////////////////////////////////////////////
// guid.cpp: 
//
// This file is part is part of CANAL (CAN Abstraction Layer)
// http://www.vscp.org)
//
// Copyright (C) 2000-2012 Ake Hedman, Grodans Paradis AB, <akhe@grodansparadis.com>
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
// $RCSfile: canaltcpif.cpp,v $                                       
// $Date: 2005/08/24 21:56:55 $                                  
// $Author: akhe $                                              
// $Revision: 1.4 $ 
///////////////////////////////////////////////////////////////////////////////

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

#ifdef __WXMSW__
    #include  "wx/ownerdrw.h"
#endif

#include <wx/tokenzr.h>
#include "guid.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

cguid::cguid()
{	
	clear();
}


cguid::~cguid()
{	
	;
}

///////////////////////////////////////////////////////////////////////////////
// getFromString
//
 
 void cguid::getFromString( const wxString& strGUID )
 {
    unsigned long val;

    wxStringTokenizer tkz( strGUID, wxT ( ":" ) );
    for ( int i=0; i<16; i++ ) {
        tkz.GetNextToken().ToULong ( &val, 16 );
        m_id[ 15-i ] = ( uint8_t ) val;
        // If no tokens left no use to continue
        if ( !tkz.HasMoreTokens() ) break;
    }
 }

///////////////////////////////////////////////////////////////////////////////
// getFromString
//

 void cguid::getFromString( char *pszGUID )
 {
    wxString str;
    str.FromAscii( pszGUID );
    getFromString( str );
 }


///////////////////////////////////////////////////////////////////////////////
// toString
//

void cguid::toString( wxString& strGUID  )
{
    strGUID.Printf( _( "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X" ),
                    m_id[15], m_id[14], m_id[13], m_id[12],
                    m_id[11], m_id[10], m_id[9], m_id[8],
                    m_id[7], m_id[6], m_id[5], m_id[4],
                    m_id[3], m_id[2], m_id[1], m_id[0] );
}


///////////////////////////////////////////////////////////////////////////////
// isSameGUID
//

bool cguid::isSameGUID( const unsigned char *pguid )
{
    if ( NULL == pguid ) return false;

    if ( 0 != memcmp ( m_id, pguid, 16 ) ) return false;

    return true;
}