/*
===========================================================================
    Copyright (C) 2010-2013  Ninja and TheKelm
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



#include "q_shared.h"
#include "qcommon.h"
#include "qcommon_logprint.h"
#include "filesystem.h"
#include "sys_thread.h"
#include "sys_main.h"

#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <limits.h>

#ifdef MAXPRINTMSG
#undef MAXPRINTMSG
#endif

#define MAXPRINTMSG 8*1024

struct logfileInfo_t
{
    fileHandle_t fh;
    qboolean wantclose;
};

static volatile fileHandle_t logfile;
static volatile fileHandle_t adminlogfile;
static volatile fileHandle_t debuglogfile;
static volatile fileHandle_t enterleavelogfile;
static volatile fileHandle_t gamelogfile; //copy of level.logfile


fileHandle_t Com_OpenLogfile(const char* name, char mode);
int Com_WriteLog(const char* data, int len, fileHandle_t f);


void QDECL SV_EnterLeaveLog( const char *fmt, ... ) {

	va_list		argptr;
	char		msg[MAXPRINTMSG];
	char		inputmsg[MAXPRINTMSG];
	struct tm 	*newtime;
	char*		ltime;
	time_t		realtime;

	Sys_EnterCriticalSection(CRITSECT_LOGFILE);

        // logfile
	if ( com_logfile && com_logfile->integer ) {
        // TTimo: only open the qconsole.log if the filesystem is in an initialized state
        // also, avoid recursing in the qconsole.log opening (i.e. if fs_debug is on)

	    va_start (argptr,fmt);
	    Q_vsnprintf (inputmsg, sizeof(inputmsg), fmt, argptr);
	    va_end (argptr);

	    Com_UpdateRealtime();
	    realtime = Com_GetRealtime();
	    newtime = localtime( &realtime );
	    ltime = asctime( newtime );
	    ltime[strlen(ltime)-1] = 0;

	    if ( !enterleavelogfile && FS_Initialized()) {

				enterleavelogfile = Com_OpenLogfile( "enterleave.log", 'a' );
				// force it to not buffer so we get valid
				if ( enterleavelogfile )
				{
					FS_ForceFlush(enterleavelogfile);
#ifdef _WIN32
					Com_sprintf(msg, sizeof(msg), "\r\nLogfile opened on %s\r\n\r\n", ltime);
#else
					Com_sprintf(msg, sizeof(msg), "\nLogfile opened on %s\n\n", ltime);
#endif
				}
	    }

	    if ( enterleavelogfile && FS_Initialized()) {
				Com_sprintf(msg, sizeof(msg), "%s: %s\n", ltime, inputmsg);
#ifdef _WIN32
				char outstring[2* MAXPRINTMSG];
				int stringlen = Q_strLF2CRLF(msg, outstring, sizeof(outstring) );
				Com_WriteLog( outstring, stringlen, enterleavelogfile );
#else
				Com_WriteLog( msg, strlen(msg), enterleavelogfile);
#endif
	    }

	}
	Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
}


void QDECL Com_PrintAdministrativeLog( const char *msg ) {

	Sys_EnterCriticalSection(CRITSECT_LOGFILE);

	struct tm 	*newtime;
	char*		ltime;
	time_t		realtime;
	char		logwritestart[256];

        // logfile
	if ( com_logfile && com_logfile->integer ) {
        // TTimo: only open the qconsole.log if the filesystem is in an initialized state
        //   also, avoid recursing in the qconsole.log opening (i.e. if fs_debug is on)


	    if ( !adminlogfile && FS_Initialized()) {

				Com_UpdateRealtime();
				realtime = Com_GetRealtime();
				newtime = localtime( &realtime );
				ltime = asctime( newtime );
				ltime[strlen(ltime)-1] = 0;


				adminlogfile = Com_OpenLogfile( "adminactions.log", 'a' );
				// force it to not buffer so we get valid
				if ( adminlogfile ){
					FS_ForceFlush(adminlogfile);
#ifdef _WIN32
					Com_sprintf(logwritestart, sizeof(logwritestart), "\r\nLogfile opened on %s\r\n\r\n", ltime);
#else
					Com_sprintf(logwritestart, sizeof(logwritestart), "\nLogfile opened on %s\n\n", ltime);
#endif
					Com_WriteLog(logwritestart, strlen(logwritestart), adminlogfile);
				}
	    }

	    if ( adminlogfile && FS_Initialized())
	    {
#ifdef _WIN32
				char outstring[2* MAXPRINTMSG];
				int stringlen = Q_strLF2CRLF(msg, outstring, sizeof(outstring) );
				Com_WriteLog( outstring, stringlen, adminlogfile );
#else
				Com_WriteLog( msg, strlen(msg), adminlogfile);
#endif
	    }

	}
	Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
}

void Com_PrintLogfile( const char *msg )
{

	char	logwritestart[256];
	Sys_EnterCriticalSection(CRITSECT_LOGFILE);


	if ( com_logfile && com_logfile->integer ) {
        // TTimo: only open the qconsole.log if the filesystem is in an initialized state
        // also, avoid recursing in the qconsole.log opening (i.e. if fs_debug is on)
	    if ( !logfile && FS_Initialized()) {
				struct tm *newtime;
				time_t aclock;

				time( &aclock );
				newtime = localtime( &aclock );

				/* 1st try to delete any existing old backup logfile */
				FS_HomeRemove( "qconsole.log.old" );
				/* Now try to rename it */
				FS_Rename( "qconsole.log", "qconsole.log.old" );

				logfile = Com_OpenLogfile( "qconsole.log", 'w' );

				if ( com_logfile->integer > 1 && logfile ) {
					// force it to not buffer so we get valid
					// data even if we are crashing
					FS_ForceFlush(logfile);
				}
				if ( logfile )
				{
#ifdef _WIN32
					Com_sprintf(logwritestart, sizeof(logwritestart), "\r\nLogfile opened on %s\r\n\r\n", asctime( newtime ));
#else
					Com_sprintf(logwritestart, sizeof(logwritestart), "\nLogfile opened on %s\n\n", asctime( newtime ));
#endif
					Com_WriteLog(logwritestart, strlen(logwritestart), logfile);
				}
	    }
	    if ( logfile && FS_Initialized())
	    {
#ifdef _WIN32
				char outstring[2* MAXPRINTMSG];
				int stringlen = Q_strLF2CRLF(msg, outstring, sizeof(outstring) );
				Com_WriteLog( outstring, stringlen, logfile );
#else
				Com_WriteLog( msg, strlen(msg), logfile);
#endif
			}
	}
	Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
}


void Com_DPrintLogfile( const char *msg )
{

	char	logwritestart[256];
	Sys_EnterCriticalSection(CRITSECT_LOGFILE);


	if ( com_logfile && com_logfile->integer ) {
        // TTimo: only open the qconsole.log if the filesystem is in an initialized state
        // also, avoid recursing in the qconsole.log opening (i.e. if fs_debug is on)
	    if ( !debuglogfile && FS_Initialized()) {
				struct tm *newtime;
				time_t aclock;

				time( &aclock );
				newtime = localtime( &aclock );

				/* 1st try to delete any existing old backup logfile */
				FS_HomeRemove( "qconsole.log.old" );
				/* Now try to rename it */
				FS_Rename( "qconsole.log", "qconsole.log.old" );

				debuglogfile = Com_OpenLogfile( "debug_qconsole.log", 'w' );

				if ( com_logfile->integer > 1 && debuglogfile ) {
					// force it to not buffer so we get valid
					// data even if we are crashing
					FS_ForceFlush(debuglogfile);
				}
				if ( debuglogfile )
				{
#ifdef _WIN32
					Com_sprintf(logwritestart, sizeof(logwritestart), "\r\nLogfile opened on %s\r\n\r\n", asctime( newtime ));
#else
					Com_sprintf(logwritestart, sizeof(logwritestart), "\nLogfile opened on %s\n\n", asctime( newtime ));
#endif
					Com_WriteLog(logwritestart, strlen(logwritestart), debuglogfile);
				}
	    }
	    if ( debuglogfile && FS_Initialized())
	    {
				char outstring[2* MAXPRINTMSG];
				Com_sprintf(outstring, sizeof(outstring), "Time=%ud ", Sys_Milliseconds());
				Com_WriteLog( outstring, strlen(outstring), debuglogfile );
#ifdef _WIN32
				int stringlen = Q_strLF2CRLF(msg, outstring, sizeof(outstring) );
				Com_WriteLog( outstring, stringlen, debuglogfile );
#else
				Com_WriteLog( msg, strlen(msg), debuglogfile);
#endif
		}
	}
	Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
}


void QDECL Com_DPrintfLogfile( const char *fmt, ... ) {
	va_list		argptr;
	char		msg[0x10000];

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

    Com_DPrintLogfile( msg );
}


/* Handles and start/stop belong to CRITSECT_LOGFILE. The worker owns no
 * generic filesystem state: only the serialized log-ring/stdio flush seam.
 * CRITSECT_LOGFILETHREAD guards enabled/status and short queue operations;
 * neither queue admission nor overload reporting waits on disk/console I/O. */
static HANDLE wakelogfilewriter;
static HANDLE logfilewriterstopped;
static qboolean logwriterenabled, logwriterstarting, logwriterstopping, logopening;
static threadid_t logthreadid;
static logWriterStatus_t logwriterstatus = { .healthy = qtrue };

void Com_GetLogWriterStatus(logWriterStatus_t *status)
{
	if (!status) return;
	Sys_EnterCriticalSection(CRITSECT_LOGFILETHREAD);
	*status = logwriterstatus;
	Sys_LeaveCriticalSection(CRITSECT_LOGFILETHREAD);
	status->filesystemWriteFailures = FS_LogWriteFailureCount();
	if (status->filesystemWriteFailures) status->healthy = qfalse;
}

static unsigned int Com_LogCountAdd(unsigned int value, unsigned int add)
{
	return value > UINT_MAX - add ? UINT_MAX : value + add;
}

static void Com_LogRejected(const char *data, int len, qboolean diagnostic)
{
	Sys_EnterCriticalSection(CRITSECT_LOGFILETHREAD);
	if (diagnostic)
	{
		logwriterstatus.diagnosticDroppedRecords = Com_LogCountAdd(logwriterstatus.diagnosticDroppedRecords, 1);
		logwriterstatus.diagnosticDroppedBytes = Com_LogCountAdd(logwriterstatus.diagnosticDroppedBytes, len);
	}
	else
	{
		logwriterstatus.healthy = qfalse;
		logwriterstatus.mandatoryRejectedRecords = Com_LogCountAdd(logwriterstatus.mandatoryRejectedRecords, 1);
		logwriterstatus.mandatoryRejectedBytes = Com_LogCountAdd(logwriterstatus.mandatoryRejectedBytes, len);
		if (!logwriterstatus.emergencyOriginalLength)
		{
			int n = len < COM_LOG_EMERGENCY_BYTES ? len : COM_LOG_EMERGENCY_BYTES;
			memcpy(logwriterstatus.emergency, data, n);
			logwriterstatus.emergencyLength = n;
			logwriterstatus.emergencyOriginalLength = len;
		}
	}
	Sys_LeaveCriticalSection(CRITSECT_LOGFILETHREAD);
}

static void Com_LogReportOverload(void)
{
	static unsigned int mandatory, diagnostic;
	logWriterStatus_t status;
	char warning[256];
	Com_GetLogWriterStatus(&status);
	if (mandatory == status.mandatoryRejectedRecords && diagnostic == status.diagnosticDroppedRecords)
		return;
	mandatory = status.mandatoryRejectedRecords;
	diagnostic = status.diagnosticDroppedRecords;
	snprintf(warning, sizeof(warning), "ERROR: log writer overload: mandatory rejected %u records/%u bytes; diagnostic dropped %u/%u; first rejected mandatory record retained in health state\n",
		mandatory, status.mandatoryRejectedBytes, diagnostic, status.diagnosticDroppedBytes);
	/* This may block in the existing console implementation, but runs only
	 * on the writer. It cannot turn a full queue into producer-side I/O. */
	Sys_Print(warning);
}


void* Com_WriteLogThread(void* null)
{
	for (;;)
	{
		qboolean enabled;
		Sys_WaitForObject(wakelogfilewriter);
		FS_WriteLogFlushAll();
		Com_LogReportOverload();
		Sys_EnterCriticalSection(CRITSECT_LOGFILETHREAD);
		enabled = logwriterenabled;
		Sys_LeaveCriticalSection(CRITSECT_LOGFILETHREAD);
		if (!enabled) break;
	}
	/* Last access by this detached worker. The owner closes both events only
	 * after this acknowledgement; a reopened generation gets fresh events. */
	Sys_SetEvent(logfilewriterstopped);
	return NULL;
}

static qboolean Com_StartLogWriter(void)
{
	threadid_t tid;
	if (wakelogfilewriter) return !logwriterstarting && !logwriterstopping;
	logwriterstarting = qtrue;
	wakelogfilewriter = Sys_CreateEvent(qfalse, qfalse, "wakelogfilewriter");
	logfilewriterstopped = Sys_CreateEvent(qtrue, qfalse, "logfilewriterstopped");
	Sys_EnterCriticalSection(CRITSECT_LOGFILETHREAD);
	logwriterenabled = qtrue;
	Sys_LeaveCriticalSection(CRITSECT_LOGFILETHREAD);
	if (!wakelogfilewriter || !logfilewriterstopped ||
		!Sys_CreateNewThread(Com_WriteLogThread, &tid, NULL))
	{
		Sys_EnterCriticalSection(CRITSECT_LOGFILETHREAD);
		logwriterenabled = qfalse;
		logwriterstatus.healthy = qfalse;
		logwriterstatus.writerStartFailures = Com_LogCountAdd(logwriterstatus.writerStartFailures, 1);
		Sys_LeaveCriticalSection(CRITSECT_LOGFILETHREAD);
		if (wakelogfilewriter) _CloseHandle(wakelogfilewriter);
		if (logfilewriterstopped) _CloseHandle(logfilewriterstopped);
		wakelogfilewriter = logfilewriterstopped = 0;
		logwriterstarting = qfalse;
		Sys_Print("ERROR: logfile writer startup failed; health latched, no dead queue accepted\n");
		return qfalse;
	}
	logthreadid = tid;
	Sys_SetThreadName(logthreadid, "LogfileWriter");
	logwriterstarting = qfalse;
	return qtrue;
}

static qboolean Com_StopLogWriter(void)
{
	if (!wakelogfilewriter) return qtrue;
	logwriterstopping = qtrue;
	Sys_EnterCriticalSection(CRITSECT_LOGFILETHREAD);
	logwriterenabled = qfalse;
	Sys_LeaveCriticalSection(CRITSECT_LOGFILETHREAD);
	Sys_SetEvent(wakelogfilewriter);
	/* Explicit close may wait for outstanding OS I/O. Producers never wait
	 * for free ring space. Existing shutdown watchdog still owns stuck I/O. */
	if (Sys_WaitForObject(logfilewriterstopped) != 0)
	{
		Sys_EnterCriticalSection(CRITSECT_LOGFILETHREAD);
		logwriterstatus.healthy = qfalse;
		logwriterstatus.writerStopFailures = Com_LogCountAdd(logwriterstatus.writerStopFailures, 1);
		Sys_LeaveCriticalSection(CRITSECT_LOGFILETHREAD);
		Sys_Print("ERROR: logfile writer stop acknowledgement failed; events/files retained, admission closed\n");
		return qfalse;
	}
	_CloseHandle(wakelogfilewriter);
	_CloseHandle(logfilewriterstopped);
	wakelogfilewriter = logfilewriterstopped = 0;
	logthreadid = 0;
	logwriterstopping = qfalse;
	return qtrue;
}


void Com_CloseLogFile(volatile fileHandle_t* f)
{
	Sys_EnterCriticalSection(CRITSECT_LOGFILE);
	if (*f && FS_CloseLogFile(*f)) *f = 0;
	Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
}

fileHandle_t Com_OpenLogfile(const char* name, char mode)
{
	fileHandle_t f = 0;
	Sys_EnterCriticalSection(CRITSECT_LOGFILE);
	if (!logopening && !logwriterstarting)
	{
		logopening = qtrue; /* Also guards recursive FS/startup error printing. */
		f = FS_OpenLogfile(name, mode);
		if (f && !Com_StartLogWriter()) { FS_CloseLogFile(f); f = 0; }
		logopening = qfalse;
	}
	Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
	return f;
}


int Com_WriteLog(const char* data, int ilen, fileHandle_t f)
{
	int written;
	if (ilen <= 0 || !data) return 0;
	Sys_EnterCriticalSection(CRITSECT_LOGFILE);
	written = Com_StartLogWriter() ? FS_WriteLog(data, ilen, f) : 0;
	if (written != ilen)
		Com_LogRejected(data, ilen, f != 0 && f == debuglogfile);
	if (wakelogfilewriter) Sys_SetEvent(wakelogfilewriter);
	Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
	return written > 0 ? written : 0;
}

void Com_CloseLogFiles()
{
	Sys_EnterCriticalSection(CRITSECT_LOGFILE);

	Cvar_SetInt(com_logfile, 0);
	if (!Com_StopLogWriter())
	{
		Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
		return;
	}

	Com_CloseLogFile( &adminlogfile );

	Com_CloseLogFile( &logfile );

	Com_CloseLogFile( &debuglogfile );

	Com_CloseLogFile( &enterleavelogfile );

	Com_CloseLogFile( &gamelogfile ); //possible duplicate because this happens in G_ShutdownGame

	Sys_LeaveCriticalSection(CRITSECT_LOGFILE);

}

fileHandle_t Com_OpenGameLogfile(const char* name, char mode, qboolean sync)
{
	if ( !FS_Initialized()) {
		return 0;
	}

	Sys_EnterCriticalSection(CRITSECT_LOGFILE);
	if (gamelogfile) Com_CloseLogFile(&gamelogfile);
	if (gamelogfile)
	{
		Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
		return 0; /* A failed close still owns its unread data. */
	}
	fileHandle_t f = Com_OpenLogfile(name, mode);
	if(f > 0)
	{
		if(sync)
		{
			FS_ForceFlush(f);
		}
		gamelogfile = f;
	}
	Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
	return f;
}

void Com_CloseGameLogfile()
{
	Com_CloseLogFile( &gamelogfile ); //possible duplicate because this happens in G_ShutdownGame
}

int Com_WriteGameLogfile(const char* data, int ilen)
{
    int written;
    Sys_EnterCriticalSection(CRITSECT_LOGFILE);
    written = gamelogfile ? Com_WriteLog(data, ilen, gamelogfile) : 0;
    Sys_LeaveCriticalSection(CRITSECT_LOGFILE);
    return written;

}
