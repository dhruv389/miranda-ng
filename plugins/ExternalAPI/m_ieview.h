/*

IEView Plugin for Miranda IM
Copyright (C) 2005  Piotr Piastucki

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef M_IEVIEW_INCLUDED
#define M_IEVIEW_INCLUDED

/* IEView window commands */
#define IEW_CREATE  1               // create new window (control)
#define IEW_DESTROY 2               // destroy control
#define IEW_SETPOS  3               // set window position and size
#define IEW_SCROLLBOTTOM 4          // scroll text to bottom

/* IEView window type/mode */
#define IEWM_TABSRMM  1             // TabSRMM-compatible HTML builder
#define IEWM_SCRIVER  3             // Scriver-compatible HTML builder
#define IEWM_HISTORY  6             // history viewer
#define IEWM_BROWSER  256           // empty browser window

struct IEVIEWWINDOW
{
	int      iType;                 // one of IEW_* values
	uint32_t dwMode;                // compatibility mode - one of IEWM_* values
	uint32_t dwFlags;               // flags, one of IEWF_* values
	HWND     parent;                // parent window HWND
	HWND     hwnd;                  // IEW_CREATE returns WebBrowser control's HWND here
	int      x;                     // IE control horizontal position
	int      y;                     // IE control vertical position
	int      cx;                    // IE control horizontal size
	int      cy;                    // IE control vertical size
};

#define MS_IEVIEW_WINDOW   "IEVIEW/NewWindow"

/////////////////////////////////////////////////////////////////////////////////////////
// IEView events

#define IEEDF_UNICODE        0x00000001  // if set pszText is a pointer to wchar_t string instead of char string
#define IEEDF_UNICODE_TEXT   0x00000001  // if set pszText is a pointer to wchar_t string instead of char string
#define IEEDF_UNICODE_NICK   0x00000002  // if set pszNick is a pointer to wchar_t string instead of char string

/* The following flags are valid only for message events (IEED_EVENT_MESSAGE) */
#define IEEDF_FORMAT_FONT    0x00000100  // if set pszFont (font name) is valid and should be used
#define IEEDF_FORMAT_SIZE    0x00000200  // if set fontSize is valid and should be used
#define IEEDF_FORMAT_COLOR   0x00000400  // if set color is valid and should be used
#define IEEDF_FORMAT_STYLE   0x00000800  // if set fontSize is valid and should be used

#define IEEDF_READ           0x00001000  // if set
#define IEEDF_SENT           0x00002000  // if set
#define IEEDF_RTL            0x00004000  // if set

#define IEED_EVENT_MESSAGE      0x0001   // message
#define IEED_EVENT_STATUSCHANGE 0x0002   // status change
#define IEED_EVENT_FILE         0x0003   // file
#define IEED_EVENT_ERRMSG       0x0005   // error message
#define IEED_EVENT_SYSTEM       0x0006   // system event
												     
#define IEED_MUCC_EVENT_MESSAGE 0x0001   // message
#define IEED_MUCC_EVENT_TOPIC   0x0002   // topic change
#define IEED_MUCC_EVENT_JOINED  0x0003   // user joined
#define IEED_MUCC_EVENT_LEFT    0x0004   // user left
#define IEED_MUCC_EVENT_ERROR   0x0005   // error

/* MUCC-related dwData bit flags */
#define IEEDD_MUCC_SHOW_NICK       0x00000001
#define IEEDD_MUCC_MSG_ON_NEW_LINE 0x00000002
#define IEEDD_MUCC_SHOW_DATE       0x00000010
#define IEEDD_MUCC_SHOW_TIME       0x00000020
#define IEEDD_MUCC_SECONDS         0x00000040
#define IEEDD_MUCC_LONG_DATE       0x00000080

#define IEED_GC_EVENT_HIGHLIGHT    0x8000
#define IEED_GC_EVENT_MESSAGE      0x0001
#define IEED_GC_EVENT_TOPIC        0x0002
#define IEED_GC_EVENT_JOIN         0x0003
#define IEED_GC_EVENT_PART         0x0004
#define IEED_GC_EVENT_QUIT         0x0006
#define IEED_GC_EVENT_NICK         0x0007
#define IEED_GC_EVENT_ACTION       0x0008
#define IEED_GC_EVENT_KICK         0x0009
#define IEED_GC_EVENT_NOTICE       0x000A
#define IEED_GC_EVENT_INFORMATION  0x000B
#define IEED_GC_EVENT_ADDSTATUS    0x000C
#define IEED_GC_EVENT_REMOVESTATUS 0x000D

/* GC-related dwData bit flags */
#define IEEDD_GC_SHOW_NICK         0x00000001
#define IEEDD_GC_SHOW_TIME         0x00000002
#define IEEDD_GC_SHOW_ICON         0x00000004
#define IEEDD_GC_MSG_ON_NEW_LINE   0x00001000

#define IE_FONT_BOLD               0x000100  // Bold font flag
#define IE_FONT_ITALIC             0x000200  // Italic font flag
#define IE_FONT_UNDERLINE          0x000400  // Underlined font flags

struct IEVIEWEVENTDATA
{
	int          iType;             // Event type, one of MUCC_EVENT_* values
	uint32_t     dwFlags;           // Event flags - IEEF_*
	const char  *fontName;          // Text font name
	int          fontSize;          // Text font size (in pixels)
	int          fontStyle;         // Text font style (combination of IE_FONT_* flags)
	COLORREF     color;             // Text color
	MAllCStrings szNick;            // Nick, usage depends on type of event
	MAllCStrings szText;            // Text, usage depends on type of event
	uint32_t     dwData;            // DWORD data e.g. status see IEEDD_* values
	int          bIsMe;             // TRUE if the event is related to the user
	uint32_t     time;              // Time of the event
	IEVIEWEVENTDATA *next;
};

/* IEView events */
#define IEE_LOG_DB_EVENTS  1       // log specified number of DB events
#define IEE_CLEAR_LOG      2       // clear log
#define IEE_GET_SELECTION  3       // get selected text
#define IEE_SAVE_DOCUMENT  4       // save current document
#define IEE_LOG_MEM_EVENTS 5       // log specified number of IEView events

/* IEView event flags */
#define IEEF_RTL           1       // turn on RTL support
#define IEEF_NO_UNICODE    2       // disable Unicode support - valid for IEE_LOG_DB_EVENTS and IEE_GET_SELECTION events

struct IEVIEWEVENT
{
	int      iType;                 // one of IEE_* values
	uint32_t dwFlags;               // one of IEEF_* values
	HWND     hwnd;                  // HWND returned by IEW_CREATE
	MCONTACT hContact;              // contact
	union {
		MEVENT hDbEventFirst;        // first event to log, when IEE_LOG_EVENTS returns it will contain
		                             // the last event actually logged or NULL if no event was logged (IEE_LOG_EVENTS)
		IEVIEWEVENTDATA *eventData;  // the pointer to an array of IEVIEWEVENTDATA objects (IEE_LOG_IEV_EVENTS)
	};
	int count;                      // number of events to log
	int codepage;                   // ANSI codepage
};

#define MS_IEVIEW_EVENT    "IEVIEW/Event"

/////////////////////////////////////////////////////////////////////////////////////////

#define IEN_NAVIGATE 1             // navigate to the given destination
#define IENF_UNICODE 1             // if set urlW is used instead of urlW

struct IEVIEWNAVIGATE
{
	int          iType;            // one of IEN_* values
	uint32_t     dwFlags;          // one of IEEF_* values
	HWND         hwnd;             // HWND returned by IEW_CREATE
	MAllCStrings url;              // Text, usage depends on type of event
};

#define MS_IEVIEW_NAVIGATE "IEVIEW/Navigate"

/////////////////////////////////////////////////////////////////////////////////////////

#define ME_IEVIEW_OPTIONSCHANGED  "IEVIEW/OptionsChanged"

#endif

