/*
===========================================================================
    Copyright (C) 1999-2005 Id Software, Inc.

    This file is part of CoD4X18-Server source code.

    CoD4X18-Server source code is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    CoD4X18-Server source code is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>
===========================================================================
*/

#include "../q_shared.h"
#include "../sys_main.h"
#include "../qcommon.h"
#include "../cmd.h"
#include "../sys_cod4defs.h"
#include "sys_win32.h"


#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

#define SYSCON_DEFAULT_WIDTH    540
#define SYSCON_DEFAULT_HEIGHT   450

#define COPY_ID         1
#define QUIT_ID         2
#define CLEAR_ID        3

#define ERRORBOX_ID     10
#define ERRORTEXT_ID    11

#define EDIT_ID         100
#define INPUT_ID        101



// Next default values for new objects
//
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NO_MFC                     1
#define _APS_NEXT_RESOURCE_VALUE        130
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1005
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif

//********************************************************


typedef struct
{
	HWND hWnd;
	HWND hwndBuffer;

	HWND hwndButtonClear;
	HWND hwndButtonCopy;
	HWND hwndButtonQuit;

	HWND hwndErrorBox;
	HWND hwndErrorText;

	HBITMAP hbmLogo;
	HBITMAP hbmClearBitmap;

	HBRUSH hbrEditBackground;

	HFONT hfBufferFont;
	HFONT hfButtonFont;

	HWND hwndInputLine;

	char errorString[80];

	char consoleText[512], returnedText[512];
	int visLevel;
	qboolean quitOnClose;
	int windowWidth, windowHeight;

	WNDPROC SysInputLineWndProc;

} WinConData;

static WinConData s_wcd;

#define CONSOLE_BUFFER_SIZE     16384
#define WM_SH1X_CONSOLE_FLUSH   ( WM_APP + 0x51 )
static void CON_DrainPending( void );

/* How the input message being handled arrived: GetCurrentInputMessageSource
 * (Windows 8+, resolved at run time; the build targets WINVER 0x501) tells a
 * hardware mouse click or key press from one injected with SendInput or
 * posted by another program. */
typedef struct { int deviceType; int originId; } sh1x_input_source_t;
typedef BOOL (WINAPI *GetCurrentInputMessageSource_t)( sh1x_input_source_t * );

static const char *Sys_ConsoleInputSource( void )
{
	static GetCurrentInputMessageSource_t fn;
	static int looked;
	static char out[64];
	sh1x_input_source_t src;
	const char *dev, *origin;

	if ( !looked ) {
		HMODULE u32 = GetModuleHandleA( "user32.dll" );
		fn = u32 ? (GetCurrentInputMessageSource_t)GetProcAddress( u32, "GetCurrentInputMessageSource" ) : NULL;
		looked = 1;
	}
	if ( !fn || !fn( &src ) )
		return "source unknown";
	switch ( src.deviceType ) {
	case 1: dev = "keyboard"; break;
	case 2: dev = "mouse"; break;
	case 4: dev = "touch"; break;
	case 8: dev = "pen"; break;
	case 16: dev = "touchpad"; break;
	default: dev = "no device (a posted/sent message)"; break;
	}
	switch ( src.originId ) {
	case 1: origin = "hardware"; break;
	case 2: origin = "injected"; break;
	case 4: origin = "system"; break;
	default: origin = "origin unknown"; break;
	}
	Com_sprintf( out, sizeof( out ), "%s/%s", dev, origin );
	return out;
}

static LONG WINAPI ConWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {

	static qboolean s_timePolarity;
	int cx, cy;
	float sx; /*, sy*/
	float x, y, w, h;

	switch ( uMsg )
	{
	case WM_SH1X_CONSOLE_FLUSH:
		CON_DrainPending();
		return 0;
	case WM_SIZE:
		// NERVE - SMF
		cx = LOWORD( lParam );
		cy = HIWORD( lParam );
/*
		if ( cx < SYSCON_DEFAULT_WIDTH )
			cx = SYSCON_DEFAULT_WIDTH;
		if ( cy < SYSCON_DEFAULT_HEIGHT )
			cy = SYSCON_DEFAULT_HEIGHT;
*/
		sx = (float)cx / SYSCON_DEFAULT_WIDTH;
		//sy = (float)cy / SYSCON_DEFAULT_HEIGHT;

		x = 8;
		y = 6;
		w = cx - 15;
		h = cy - 60;
		SetWindowPos( s_wcd.hwndBuffer, NULL, x, y, w, h, 0 );

		y = y + h + 4;
		h = 20;
		SetWindowPos( s_wcd.hwndInputLine, NULL, x, y, w, h, 0 );

		y = y + h + 4;
		w = 72 * sx;
		h = 24;
		SetWindowPos( s_wcd.hwndButtonCopy, NULL, x, y, w, h, 0 );

		x = x + w + 2;
		SetWindowPos( s_wcd.hwndButtonClear, NULL, x, y, w, h, 0 );

		x = cx - 6 - w;
		SetWindowPos( s_wcd.hwndButtonQuit, NULL, x, y, w, h, 0 );

		s_wcd.windowWidth = cx;
		s_wcd.windowHeight = cy;
		// -NERVE - SMF
		break;
	case WM_ACTIVATE:
		if ( LOWORD( wParam ) != WA_INACTIVE ) {
			SetFocus( s_wcd.hwndInputLine );
		}
		break;

	case WM_CLOSE:
		return 0;
	case WM_CTLCOLORSTATIC:
		if ( ( HWND ) lParam == s_wcd.hwndBuffer ) {
			SetBkColor( ( HDC ) wParam, RGB( 0, 0, 0 ) );
			SetTextColor( ( HDC ) wParam, RGB( 0xff, 0xff, 0xff ) );

#if 0   // this draws a background in the edit box, but there are issues with this
			if ( ( hdcScaled = CreateCompatibleDC( ( HDC ) wParam ) ) != 0 ) {
				if ( SelectObject( ( HDC ) hdcScaled, s_wcd.hbmLogo ) ) {
					StretchBlt( ( HDC ) wParam, 0, 0, 512, 384,
								hdcScaled, 0, 0, 512, 384,
								SRCCOPY );
				}
				DeleteDC( hdcScaled );
			}
#endif
			return ( long ) s_wcd.hbrEditBackground;
		}
		break;

	case WM_COMMAND:
		if ( wParam == COPY_ID ) {
			SendMessageA( s_wcd.hwndBuffer, EM_SETSEL, 0, -1 );
			SendMessageA( s_wcd.hwndBuffer, WM_COPY, 0, 0 );
		} else if ( LOWORD( wParam ) == QUIT_ID && HIWORD( wParam ) == BN_CLICKED )   {
			/* sh1x: this button ends a live match, and it used to do so with no
			 * console echo and no logfile line - the log simply stopped, which
			 * is exactly how three "the server died on a map change" reports
			 * looked.  Accept only a genuine BN_CLICKED from the quit button
			 * itself (a real click carries the control HWND in lParam; menu,
			 * accelerator and injected WM_COMMANDs carry 0), confirm while a
			 * map is loaded, and always record the raw message. */
			/* This window procedure runs on the message-pump thread, so
			 * format into a local buffer rather than va() (thread-local). */
			char detail[224];
			if ( (HWND)lParam != s_wcd.hwndButtonQuit )
			{
				Com_sprintf(detail, sizeof(detail),
					"wParam 0x%08x lParam 0x%08x expected button 0x%08x",
					(unsigned int)wParam, (unsigned int)lParam,
					(unsigned int)(size_t)s_wcd.hwndButtonQuit);
				Com_LogLifecycleEvent("console window quit button REJECTED", detail);
				break;
			}
			if ( s_wcd.quitOnClose ) {
				PostQuitMessage( 0 );
			} else
			{
				/* sh1x 2026-10-27: the live 17733 server was quit three times
				 * through this button (00:52, 01:09 and 20:54), each time while
				 * the user was closing or reloading the game client, and the
				 * old prompt ("A match is running on this server") under the
				 * title "sh1x server - Call of Duty Online ..." read like the
				 * game's own quit confirmation.  Name the server and its port,
				 * say what Yes costs, and record how the click arrived and how
				 * long the answer took, so a misclick and an injected click
				 * can be told apart in qconsole.log. */
				const char *source = Sys_ConsoleInputSource();
				DWORD askedAt = GetTickCount();
				if ( com_sv_running && com_sv_running->boolean )
				{
					char prompt[512];
					int port = Cvar_VariableIntegerValue("net_port");
					Com_sprintf(prompt, sizeof(prompt),
						"This window is the sh1x MATCH SERVER on udp/%d - not the game client.\n\n"
						"Yes shuts the SERVER down: every player is disconnected, and nobody\n"
						"(you included) can join a match on it until it is started again.\n\n"
						"To leave the game, close the game window instead.\n\n"
						"Shut down the match server?", port);
					if ( MessageBoxA( hWnd, prompt,
						"sh1x MATCH SERVER - confirm server shutdown",
						MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_SETFOREGROUND ) != IDYES )
					{
						Com_sprintf(detail, sizeof(detail), "click %s, No after %lu ms",
							source, (unsigned long)(GetTickCount() - askedAt));
						Com_LogLifecycleEvent("console window quit button declined", detail);
						break;
					}
				}
				Com_sprintf(detail, sizeof(detail), "wParam 0x%08x lParam 0x%08x click %s, Yes after %lu ms",
					(unsigned int)wParam, (unsigned int)lParam, source,
					(unsigned long)(GetTickCount() - askedAt));
				Com_LogLifecycleEvent("console window quit button", detail);
				Cbuf_AddText("quit\n");

			}
		} else if ( wParam == CLEAR_ID )   {
			SendMessageA( s_wcd.hwndBuffer, EM_SETSEL, 0, -1 );
			SendMessageA( s_wcd.hwndBuffer, EM_REPLACESEL, FALSE, ( LPARAM ) "" );
			UpdateWindow( s_wcd.hwndBuffer );
		}
		break;
	case WM_CREATE:
//		s_wcd.hbmLogo = LoadBitmap( g_wv.hInstance, MAKEINTRESOURCE( IDB_BITMAP1 ) );
//		s_wcd.hbmClearBitmap = LoadBitmap( g_wv.hInstance, MAKEINTRESOURCE( IDB_BITMAP2 ) );
		s_wcd.hbrEditBackground = CreateSolidBrush( RGB( 0, 0, 0 ) );
		SetTimer( hWnd, 1, 1000, NULL );
		break;
	case WM_ERASEBKGND:
#if 0
		HDC hdcScaled;
		HGDIOBJ oldObject;

#if 1   // a single, large image
		hdcScaled = CreateCompatibleDC( ( HDC ) wParam );
		assert( hdcScaled != 0 );

		if ( hdcScaled ) {
			oldObject = SelectObject( ( HDC ) hdcScaled, s_wcd.hbmLogo );
			assert( oldObject != 0 );
			if ( oldObject ) {
				StretchBlt( ( HDC ) wParam, 0, 0, s_wcd.windowWidth, s_wcd.windowHeight,
							hdcScaled, 0, 0, 512, 384,
							SRCCOPY );
			}
			DeleteDC( hdcScaled );
			hdcScaled = 0;
		}
#else   // a repeating brush
		{
			HBRUSH hbrClearBrush;
			RECT r;

			GetWindowRect( hWnd, &r );

			r.bottom = r.bottom - r.top + 1;
			r.right = r.right - r.left + 1;
			r.top = 0;
			r.left = 0;

			hbrClearBrush = CreatePatternBrush( s_wcd.hbmClearBitmap );

			assert( hbrClearBrush != 0 );

			if ( hbrClearBrush ) {
				FillRect( ( HDC ) wParam, &r, hbrClearBrush );
				DeleteObject( hbrClearBrush );
			}
		}
#endif
		return 1;
#endif
		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	case WM_TIMER:
		if ( wParam == 1 ) {
			s_timePolarity = !s_timePolarity;
			if ( s_wcd.hwndErrorBox ) {
				InvalidateRect( s_wcd.hwndErrorBox, NULL, FALSE );
			}
		}
		break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

LONG WINAPI InputLineWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
	char inputBuffer[1024];

	switch ( uMsg )
	{
	case WM_KILLFOCUS:
		if ( ( HWND ) wParam == s_wcd.hWnd ||
			 ( HWND ) wParam == s_wcd.hwndErrorBox ) {
			SetFocus( hWnd );
			return 0;
		}
		break;

	case WM_CHAR:
		if ( wParam == 13 ) {
			GetWindowText( s_wcd.hwndInputLine, inputBuffer, sizeof( inputBuffer ) );
			strncat( s_wcd.consoleText, inputBuffer, sizeof( s_wcd.consoleText ) - strlen( s_wcd.consoleText ) - 5 );
			strcat( s_wcd.consoleText, "\n" );
			SetWindowText( s_wcd.hwndInputLine, "" );

			//CON_Print( va( "]%s\n", inputBuffer ) );

			return 0;
		}
	}

	return CallWindowProc( s_wcd.SysInputLineWndProc, hWnd, uMsg, wParam, lParam );
}

/*
** Sys_CreateConsole
*/
void CON_InitInternal( void ) {
	HDC hDC;
	WNDCLASS wc;
	RECT rect;
	const char *DEDCLASS = CLIENT_WINDOW_TITLE " Console";
	const char *WINDOWNAME = CLIENT_WINDOW_TITLE " Console";

	int nHeight;
	int swidth, sheight;
	int DEDSTYLE = WS_POPUPWINDOW | WS_CAPTION | WS_MINIMIZEBOX | WS_SIZEBOX;

	if(Sys_IsHeadless()) return;

	memset( &wc, 0, sizeof( wc ) );

	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC) ConWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = g_wv.hInstance;
	wc.hIcon         = LoadIcon( g_wv.hInstance, MAKEINTRESOURCE( IDI_ICON1 ) );
	wc.hCursor       = LoadCursor( NULL,IDC_ARROW );
	wc.hbrBackground = (void *)COLOR_WINDOW;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = DEDCLASS;

	if ( !RegisterClass( &wc ) ) {
		return;
	}

	rect.left = 0;
	rect.right = SYSCON_DEFAULT_WIDTH;
	rect.top = 0;
	rect.bottom = SYSCON_DEFAULT_HEIGHT;
	AdjustWindowRect( &rect, DEDSTYLE, FALSE );

	hDC = GetDC( GetDesktopWindow() );
	swidth = GetDeviceCaps( hDC, HORZRES );
	sheight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	s_wcd.windowWidth = rect.right - rect.left + 1;
	s_wcd.windowHeight = rect.bottom - rect.top + 1;

	s_wcd.hWnd = CreateWindowEx( 0,
								 DEDCLASS,
								 WINDOWNAME,
								 DEDSTYLE,
								 ( swidth - 600 ) / 2, ( sheight - 450 ) / 2, rect.right - rect.left + 1, rect.bottom - rect.top + 1,
								 NULL,
								 NULL,
								 g_wv.hInstance,
								 NULL );

	if ( s_wcd.hWnd == NULL ) {
		return;
	}

	//
	// create fonts
	//
	hDC = GetDC( s_wcd.hWnd );
	nHeight = -MulDiv( 8, GetDeviceCaps( hDC, LOGPIXELSY ), 72 );

	s_wcd.hfBufferFont = CreateFont( nHeight,
									 0,
									 0,
									 0,
									 FW_LIGHT,
									 0,
									 0,
									 0,
									 DEFAULT_CHARSET,
									 OUT_DEFAULT_PRECIS,
									 CLIP_DEFAULT_PRECIS,
									 DEFAULT_QUALITY,
									 FF_MODERN | FIXED_PITCH,
									 "Courier New" );

	ReleaseDC( s_wcd.hWnd, hDC );

	//
	// create the input line
	//
	s_wcd.hwndInputLine = CreateWindow( "edit", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER |
										ES_LEFT | ES_AUTOHSCROLL,
										6, 400, 528, 20,
										s_wcd.hWnd,
										( HMENU ) INPUT_ID,         // child window ID
										g_wv.hInstance, NULL );

	//
	// create the buttons
	//
	s_wcd.hwndButtonCopy = CreateWindow( "button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
										 5, 425, 72, 24,
										 s_wcd.hWnd,
										 ( HMENU ) COPY_ID,         // child window ID
										 g_wv.hInstance, NULL );
	SendMessageA( s_wcd.hwndButtonCopy, WM_SETTEXT, 0, ( LPARAM ) "copy" );

	s_wcd.hwndButtonClear = CreateWindow( "button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
										  82, 425, 72, 24,
										  s_wcd.hWnd,
										  ( HMENU ) CLEAR_ID,       // child window ID
										  g_wv.hInstance, NULL );
	SendMessageA( s_wcd.hwndButtonClear, WM_SETTEXT, 0, ( LPARAM ) "clear" );

	s_wcd.hwndButtonQuit = CreateWindow( "button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
										 462, 425, 72, 24,
										 s_wcd.hWnd,
										 ( HMENU ) QUIT_ID,         // child window ID
										 g_wv.hInstance, NULL );
	SendMessageA( s_wcd.hwndButtonQuit, WM_SETTEXT, 0, ( LPARAM ) "quit" );


	//
	// create the scrollbuffer
	//
	s_wcd.hwndBuffer = CreateWindow( "edit", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER |
									 ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
									 6, 6, 526, 390,
									 s_wcd.hWnd,
									 ( HMENU ) EDIT_ID,             // child window ID
									 g_wv.hInstance, NULL );
	SendMessageA( s_wcd.hwndBuffer, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );

	s_wcd.SysInputLineWndProc = ( WNDPROC ) SetWindowLong( s_wcd.hwndInputLine, GWL_WNDPROC, ( long ) InputLineWndProc );
	SendMessageA( s_wcd.hwndInputLine, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );

	ShowWindow( s_wcd.hWnd, SW_SHOWDEFAULT );
	UpdateWindow( s_wcd.hWnd );
	SetForegroundWindow( s_wcd.hWnd );
	SetFocus( s_wcd.hwndInputLine );

	s_wcd.visLevel = 1;
}

/*
** Sys_DestroyConsole
*/
void CON_Shutdown( void ) {
	if ( s_wcd.hWnd ) {
		ShowWindow( s_wcd.hWnd, SW_HIDE );
		CloseWindow( s_wcd.hWnd );
		DestroyWindow( s_wcd.hWnd );
		s_wcd.hWnd = 0;
	}
}

/*
** Sys_ShowConsole
*/
void CON_Show( int visLevel, qboolean quitOnClose ) {
	if(Sys_IsHeadless()) return;
	s_wcd.quitOnClose = quitOnClose;
	mvabuf;

	if ( visLevel == s_wcd.visLevel ) {
		return;
	}

	s_wcd.visLevel = visLevel;

	if ( !s_wcd.hWnd ) {
		return;
	}

	switch ( visLevel )
	{
	case 0:
		ShowWindow( s_wcd.hWnd, SW_HIDE );
		break;
	case 1:
		ShowWindow( s_wcd.hWnd, SW_SHOWNORMAL );
		SendMessageA( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
		break;
	case 2:
		ShowWindow( s_wcd.hWnd, SW_MINIMIZE );
		break;
	default:
		Sys_Error( va("Invalid visLevel %d sent to Sys_ShowConsole\n", visLevel ));
		break;
	}
}

/*
** Sys_ConsoleInput
*/
char *CON_Input( void ) {
	if(Sys_IsHeadless()) return NULL;

	if ( s_wcd.consoleText[0] == 0 ) {
		return NULL;
	}

	strcpy( s_wcd.returnedText, s_wcd.consoleText );
	s_wcd.consoleText[0] = 0;

	return s_wcd.returnedText;
}

/*
** Conbuf_AppendText
*/
/* sh1x: console output no longer blocks the game thread.
 *
 * The console window lives on Sys_EventLoopThread, so each SendMessage the
 * game thread made here was a cross-thread round trip into the edit control,
 * four per line.  MEASURED 2026-10-21 (probe, zm_sumpf_sh, 24 zombies, trace
 * off): ~55 lines a second cost 40-74 ms a second, about 1 ms a line and up
 * to 6.9 ms for one, and the worst 60 Hz game frame of each second was
 * almost all printing (12.6 of 13.8 ms).  With ol_debugTrace on the
 * server printed several times as much and missed frames every second.
 * Lines now go into a pending buffer and one posted message has the window
 * thread append the lot.  The logfile is still written synchronously by
 * Com_PrintLogfile, so nothing is lost from qconsole.log; a console that
 * falls 64 KB behind drops text from its display only, and says so. */

static CRITICAL_SECTION s_pendingLock;
static volatile LONG s_pendingInit;
static char s_pending[65536];
static int s_pendingLen;
static int s_pendingPosted;
static unsigned long s_pendingDropped;
static unsigned long s_totalChars;

static void CON_PendingInit( void ) {
	if ( InterlockedCompareExchange( &s_pendingInit, 1, 0 ) == 0 ) {
		InitializeCriticalSection( &s_pendingLock );
		InterlockedExchange( &s_pendingInit, 2 );
	}
	while ( s_pendingInit != 2 ) {
		Sleep( 0 );
	}
}

/* Window thread: append everything pending to the edit control. */
static void CON_DrainPending( void ) {
	static char text[sizeof( s_pending ) + 128];
	int len;
	unsigned long dropped;
	const char *show;

	CON_PendingInit();
	EnterCriticalSection( &s_pendingLock );
	dropped = s_pendingDropped;
	len = 0;
	if ( dropped ) {
		len = Com_sprintf( text, sizeof( text ), "[console display fell behind; %lu characters not shown here, see qconsole.log]\r\n", dropped );
		if ( len < 0 ) {
			len = 0;
		}
	}
	memcpy( text + len, s_pending, s_pendingLen );
	len += s_pendingLen;
	text[len] = 0;
	s_pendingLen = 0;
	s_pendingDropped = 0;
	s_pendingPosted = 0;
	LeaveCriticalSection( &s_pendingLock );

	if ( !len || !s_wcd.hwndBuffer ) {
		return;
	}
	show = text;
	if ( len > CONSOLE_BUFFER_SIZE - 1 ) {
		show = text + len - ( CONSOLE_BUFFER_SIZE - 1 );
		len = CONSOLE_BUFFER_SIZE - 1;
	}

	s_totalChars += len;

	//
	// replace selection instead of appending if we're overflowing
	//
	if ( s_totalChars > CONSOLE_BUFFER_SIZE ) {
		SendMessageA( s_wcd.hwndBuffer, EM_SETSEL, 0, -1 );
		s_totalChars = len;
	} else {
		// NERVE - SMF - always append at the bottom of the textbox
		SendMessageA( s_wcd.hwndBuffer, EM_SETSEL, 0xFFFF, 0xFFFF );
	}

	//
	// put this text into the windows console
	//
	SendMessageA( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
	SendMessageA( s_wcd.hwndBuffer, EM_SCROLLCARET, 0, 0 );
	SendMessageA( s_wcd.hwndBuffer, EM_REPLACESEL, 0, (LPARAM) show );
}

void CON_Print( const char *pMsg ) {

	char buffer[CONSOLE_BUFFER_SIZE * 2];
	char *b = buffer;
	const char *msg;
	int bufLen;
	int i = 0;
	int post;
	HWND hWnd;

	/* Keep the ordinary asynchronous console intact; managed hosts have no
	 * window and must not format or enqueue display-only output. */
	if(Sys_IsHeadless() || !s_wcd.hwndBuffer) return;

	//
	// if the message is REALLY long, use just the last portion of it
	//
	if ( strlen( pMsg ) > CONSOLE_BUFFER_SIZE - 1 ) {
		msg = pMsg + strlen( pMsg ) - CONSOLE_BUFFER_SIZE + 1;
	} else
	{
		msg = pMsg;
	}

	//
	// copy into an intermediate buffer
	//
	while ( msg[i] && ( ( b - buffer ) < sizeof( buffer ) - 1 ) )
	{
		if ( msg[i] == '\n' && msg[i + 1] == '\r' ) {
			b[0] = '\r';
			b[1] = '\n';
			b += 2;
			i++;
		} else if ( msg[i] == '\r' )     {
			b[0] = '\r';
			b[1] = '\n';
			b += 2;
		} else if ( msg[i] == '\n' )     {
			b[0] = '\r';
			b[1] = '\n';
			b += 2;
		} else if ( Q_IsColorString( &msg[i] ) )   {
			i++;
		} else
		{
			*b = msg[i];
			b++;
		}
		i++;
	}
	*b = 0;
	bufLen = b - buffer;

	hWnd = s_wcd.hWnd;
	if ( !hWnd || !bufLen ) {
		return;
	}

	CON_PendingInit();
	EnterCriticalSection( &s_pendingLock );
	if ( s_pendingLen + bufLen < (int)sizeof( s_pending ) ) {
		memcpy( s_pending + s_pendingLen, buffer, bufLen );
		s_pendingLen += bufLen;
	} else {
		s_pendingDropped += bufLen;
	}
	post = !s_pendingPosted;
	s_pendingPosted = 1;
	LeaveCriticalSection( &s_pendingLock );

	if ( post && !PostMessageA( hWnd, WM_SH1X_CONSOLE_FLUSH, 0, 0 ) ) {
		EnterCriticalSection( &s_pendingLock );
		s_pendingPosted = 0;
		LeaveCriticalSection( &s_pendingLock );
	}
}

void CON_DisableDraw() 
{
	if(Sys_IsHeadless() || !s_wcd.hwndBuffer) return;
	SendMessage(  s_wcd.hwndBuffer, WM_SETREDRAW, (WPARAM) FALSE, 0);
}

void CON_EnableDraw() 
{
	if(Sys_IsHeadless() || !s_wcd.hwndBuffer) return;
	SendMessageA( s_wcd.hwndBuffer, WM_SETREDRAW,  (WPARAM) TRUE, 0);
}