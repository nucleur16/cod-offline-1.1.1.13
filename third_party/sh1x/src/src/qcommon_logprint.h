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



#ifndef __QCOMMON_LOGPRINT_H__
#define __QCOMMON_LOGPRINT_H__

#include "q_shared.h"

#ifdef __cplusplus
extern "C"{
#endif

void QDECL SV_EnterLeaveLog( const char *fmt, ... );
void QDECL Com_PrintAdministrativeLog( const char *msg );
void Com_PrintLogfile( const char *msg );
void Com_CloseLogFiles(void);
fileHandle_t Com_OpenGameLogfile(const char* name, char mode, qboolean sync);
void Com_CloseGameLogfile();
/* Saturation is explicit rejection, not successful persistence. This latched
 * snapshot survives close/reopen for operator health and postmortem inspection.
 * The first rejected mandatory record is retained up to this bounded capacity. */
#define COM_LOG_EMERGENCY_BYTES 16384
typedef struct {
    qboolean healthy;
    unsigned int writerStartFailures;
    unsigned int writerStopFailures;
    unsigned int filesystemWriteFailures;
    unsigned int mandatoryRejectedRecords, mandatoryRejectedBytes;
    unsigned int diagnosticDroppedRecords, diagnosticDroppedBytes;
    int emergencyLength, emergencyOriginalLength;
    char emergency[COM_LOG_EMERGENCY_BYTES];
} logWriterStatus_t;
void Com_GetLogWriterStatus(logWriterStatus_t *status);
int Com_WriteGameLogfile(const char* data, int len);

#ifdef __cplusplus
};
#endif


#endif

