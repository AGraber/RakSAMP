/*
	Updated to 0.3z by P3ti
*/

#include "main.h"

RakClientInterface *pRakClient = NULL;
int iAreWeConnected = 0, iConnectionRequested = 0, iSpawned = 0, iGameInited = 0;
int iReconnectTime = 2 * 1000;
PLAYERID g_myPlayerID;
char g_szNickName[32];

struct stPlayerInfo playerInfo[MAX_PLAYERS];
struct stVehiclePool vehiclePool[MAX_VEHICLES];

FILE *flLog = NULL;

PLAYERID normalMode_goto = -1;
DWORD dwAutoRunTick = GetTickCount();
int dd = 0;

SAMP * pSamp = NULL;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	srand((unsigned int)GetTickCount());

	// load up settings
	if(!LoadSettings())
	{
		Log("Failed to load settings");
		getchar();
		return 0;
	}

	if(settings.iConsole)
		SetUpConsole();
	else
	{
		SetUpWindow(hInstance);
		Sleep(500); // wait a bit for the dialog to create
	}

	pSamp = new SAMP("samp.dll");

	if (pSamp->GetHMODULE() == NULL)
	{
		Log("Copy samp.dll AND bass.dll to the RakSAMP directory.");

		if(flLog != NULL)
		{
			fclose(flLog);
			flLog = NULL;
		}
		Sleep(8000);
		
		return 0;
	}

	// RCON mode
	if(settings.runMode == RUNMODE_RCON)
	{
		if(RCONReceiveLoop())
		{
			if(flLog != NULL)
			{
				fclose(flLog);
				flLog = NULL;
			}

			return 0;
		}
	}	

	// set up networking
	pRakClient = RakNetworkFactory::GetRakClientInterface();
	if(pRakClient == NULL)
		return 0;

	pRakClient->SetMTUSize(576);

	resetPools(1, 0);
	RegisterRPCs(pRakClient);

	SYSTEMTIME time;
	GetLocalTime(&time);
	if(settings.iConsole)
	{
		Log(" ");
		Log("* ===================================================== *");
		Log("  RakSAMP " RAKSAMP_VERSION " initialized on %02d/%02d/%02d %02d:%02d:%02d",
			time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);
		Log("  Authors: " AUTHOR "");
		Log("* ===================================================== *");
		Log(" ");
	}

	char szInfo[400];
	while(1)
	{
		UpdateNetwork(pRakClient);

		if(settings.ispam) sampSpam();

		if (settings.fakeKill) {
			for(int a=0;a<46;a++){
				for(int b=0;b<getPlayerCount();b++){
					if(settings.fakeKill){
						SendWastedNotification(a, b);
					}
				}
			}
		}

		if (settings.lag) {
				RakNet::BitStream bsDeath;
				bsDeath.Write(dd++);
				pRakClient->RPC(&RPC_ClickPlayer, &bsDeath, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
				pRakClient->RPC(&RPC_EnterVehicle, &bsDeath, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
				pRakClient->RPC(&RPC_ExitVehicle, &bsDeath, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
				pRakClient->RPC(&RPC_PickedUpPickup, &bsDeath, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
				pRakClient->RPC(&RPC_RequestSpawn, &bsDeath, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
				sendDialogResponse(sampDialog.wDialogID, 1, 1, "");
		}

		if(!iConnectionRequested)
		{
			if(!iGettingNewName)
				sampConnect(settings.server.szAddr, settings.server.iPort, settings.server.szNickname, settings.server.szPassword, pRakClient);
			else
				sampConnect(settings.server.szAddr, settings.server.iPort, g_szNickName, settings.server.szPassword, pRakClient);

			iConnectionRequested = 1;
		}

		if (iAreWeConnected && iGameInited)
		{
			static DWORD dwLastInfoUpdate = GetTickCount();
			if(dwLastInfoUpdate && dwLastInfoUpdate < (GetTickCount() - 1000))
			{
				sprintf(szInfo, "Hostname: %s     Players: %d     Ping: %d\nAuthors: %s",
					g_szHostName, getPlayerCount(), playerInfo[g_myPlayerID].dwPing,AUTHOR);
				SetWindowText(texthwnd, szInfo);
			}

			if(settings.runMode == RUNMODE_BARE)
				goto bare;

			if(!iSpawned)
			{
				sampRequestClass(settings.iClassID);
				sampSpawn();

				iSpawned = 1;
			}
			else
			{
				if(settings.runMode == RUNMODE_STILL)
				{
					//Nothing left to do :-)
				}

				if(settings.runMode == RUNMODE_NORMAL)
				{
					if(normalMode_goto == (PLAYERID)-1)
						onFootUpdateAtNormalPos();
				}

				// Have to teleport to play_pos so that we can get vehicles streamed in.
				/*if(settings.runMode == RUNMODE_PLAYROUTES)
				{
					if(rec_state == RECORDING_OFF)
						onFootUpdateAtNormalPos();
				}*/

				// Run autorun commands
				if(settings.iAutorun)
				{
					if(dwAutoRunTick && dwAutoRunTick < (GetTickCount() - 2000))
					{
						static int autorun;
						if(!autorun)
						{
							Log("Loading autorun...");
							for(int i = 0; i < MAX_AUTORUN_CMDS; i++)
								if(settings.autoRunCMDs[i].iExists)
									RunCommand(settings.autoRunCMDs[i].szCMD, 1);

							autorun = 1;
						}
					}
				}

				// Play routes handler
				/*if(settings.runMode == RUNMODE_PLAYROUTES)
					vehicle_playback_handler();*/

				// Following player mode.
				if(settings.runMode == RUNMODE_FOLLOWPLAYER)
				{
					PLAYERID copyingID = getPlayerIDFromPlayerName(settings.szFollowingPlayerName);
					if(copyingID != (PLAYERID)-1)
						onFootUpdateFollow(copyingID);
				}

				// Following a player with a vehicle mode.
				if(settings.runMode == RUNMODE_FOLLOWPLAYERSVEHICLE)
				{
					PLAYERID copyingID = getPlayerIDFromPlayerName(settings.szFollowingPlayerName);
					if(copyingID != (PLAYERID)-1)
						inCarUpdateFollow(copyingID, (VEHICLEID)settings.iFollowingWithVehicleID);
				}

			}
		}

bare:;
		Sleep(30);
	}

	if(flLog != NULL)
	{
		fclose(flLog);
		flLog = NULL;
	}

	return 0;
}

void Log ( char *fmt, ... )
{
	SYSTEMTIME	time;
	va_list		ap;

	if ( flLog == NULL )
	{
		flLog = fopen( "RakSAMPClient.log", "a" );
		if ( flLog == NULL )
			return;
	}

	GetLocalTime( &time );
	fprintf( flLog, "[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds );
	if(settings.iPrintTimestamps)
	{
		if(settings.iConsole)
			printf("[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds );
	}

	va_start( ap, fmt );
	vfprintf( flLog, fmt, ap );
	if(settings.iConsole)
		vprintf(fmt, ap);
	else
	{
		int lbCount = SendMessage(loghwnd, LB_GETCOUNT, 0, 0);
		LPTSTR buf = new TCHAR[512];
		wvsprintf(buf, fmt, ap);

		WPARAM idx = SendMessage(loghwnd, LB_ADDSTRING, 0, (LPARAM)buf);
		SendMessage(loghwnd, LB_SETCURSEL, lbCount - 1, 0);
		SendMessage(loghwnd, LB_SETTOPINDEX, idx, 0);
	}
	va_end( ap );

	fprintf( flLog, "\n" );
	if(settings.iConsole)
		printf("\n");
	fflush( flLog );
}

void gen_random(char *s, const int len)
{
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	for (int i = 0; i < len; ++i)
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];

	s[len] = 0;
}
