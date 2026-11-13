#ifndef CODOL_P2P_PRODUCTION_KERNEL_H
#define CODOL_P2P_PRODUCTION_KERNEL_H
#include "production/p2p_production_receiver.h"
static int P2PProductionMode(void);
static int P2PSelectedProtocolMode(void);
static int P2PProductionStart(void);
static void P2PProductionTick(void);
static void P2PProductionOwnerTick(int);
static int P2PProductionLoadHost(HMODULE);
static const wchar_t *P2PProductionHostPath(void);
static int P2PProductionHostInstalled(void);
static int P2PProductionHostCurrent(void);
static void P2PProductionCancel(void);
static int P2PProductionInstallSocketHooks(void);
static void P2PProductionBorrowSend(SOCKET);
static int P2PProductionGuestAddress(DWORD,unsigned);
#endif
