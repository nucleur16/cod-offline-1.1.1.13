	// quick reject
	for (n=0, cl=svs.clients; n < sv_maxclients->integer; n++, cl++) {

		if ( NET_CompareBaseAdr( from, &cl->netchan.remoteAddress ) && (cl->netchan.qport == qport || from->port == cl->netchan.remoteAddress.port )){

			/*
			reconnectTime = (svs.time - cl->lastConnectTime);
			if (reconnectTime < (sv_reconnectlimit->integer * 1000)) {
				Com_Printf(CON_CHANNEL_SERVER,"%s -> reconnect rejected : too soon\n", NET_AdrToString (from));
				NET_OutOfBandPrint( NS_SERVER, from, "print\nReconnect limit in effect... (%i)\n",
							(sv_reconnectlimit->integer - (reconnectTime / 1000)));
				return;

			}else{
				*/
				if(cl->state == CS_CONNECTED)
				{
					Com_DPrintf(CON_CHANNEL_SERVER,"Rejected connection from %s. DoS attack detected by emulating client 1.8\n", NET_AdrToString(&cl->netchan.remoteAddress));
					return;
				}
				
#ifdef CODOL_SERVER
                if (!CODOL_MatchReportCanReuseSlot((unsigned)(cl-svs.clients))) {
                    NET_OutOfBandPrint(NS_SERVER, from, "error\nPrevious result persistence pending; retry later.\n");
                    return;
                }
#endif
				SV_DropClient( cl, "silent" );
				newcl = cl;
				Com_Printf(CON_CHANNEL_SERVER,"reconnected: %s\n", NET_AdrToString(from));
				cl->lastConnectTime = svs.time;
				break;
/*			}*/
		}else if ( NET_CompareBaseAdr( from, &cl->netchan.remoteAddress ) && (cl->state == CS_CONNECTED || cl->state == CS_ZOMBIE)){
			NET_OutOfBandPrint( NS_SERVER, from,
				"error\nConnection refused:\nAn uncompleted connection from %s has been detected\nPlease try again later\n",
				NET_AdrToString(&cl->netchan.remoteAddress));
			Com_DPrintf(CON_CHANNEL_SERVER,"Rejected connection from %s. This is a Fake-Player-DoS protection\n", NET_AdrToString(&cl->netchan.remoteAddress));
			return;
		}
	}

