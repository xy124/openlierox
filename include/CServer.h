/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Server class
// Created 28/6/02
// Jason Boettcher


#ifndef __CSERVER_H__
#define	__CSERVER_H__

#include <string>
#include "Networking.h"
#include "SmartPointer.h"
#include "CGameScript.h"
#include "CBonus.h"
#include "CShootList.h"
#include "HTTP.h"
#include "Timer.h"
#include "CBanList.h"
#include "CWpnRest.h"
#include "game/GameMode.h"

class CWorm;
class CServerConnection;
class CMap;
struct Version;
class CGameMode;
struct WormJoinInfo;
class FlagInfo;
struct weapon_t;

enum { 
	MAX_CHALLENGES = 1024,
	MAX_SERVER_SOCKETS = 4, // = max UDP masterservers
};


// Challenge structure
class challenge_t { public:
	NetworkAddr	Address;
	AbsTime		fTime;
	int			iNum;
	std::string	sClientVersion;
};

// Server state
enum {
	SVS_LOBBY=0,		// Lobby
	SVS_GAME,			// Game, waiting for players to load
	SVS_PLAYING			// Currently playing
};

// Client leaving reasons
enum {
	CLL_QUIT=0,
	CLL_TIMEOUT,
    CLL_KICK,
	CLL_BAN
};

class GameServer {
public:
	// Constructor
	GameServer();

	~GameServer();

public:
	// Handles information about a client connecting using NAT traversal
	struct NatConnection  {
		SmartPointer<NetworkSocket>	tTraverseSocket;
		SmartPointer<NetworkSocket>	tConnectHereSocket;
		NetworkAddr		tAddress;
		AbsTime			fLastUsed;
		bool			bClientConnected;
		
		NatConnection() : bClientConnected(false) { tTraverseSocket = new NetworkSocket(); tConnectHereSocket = new NetworkSocket(); }
	};

private:
	// Attributes

	// General
	int			iState;

	// Game rules
	bool		bGameOver;
	AbsTime		fGameOverTime;
	
	SmartPointer<CGameScript> cGameScript;
    CWpnRest    cWeaponRestrictions;
    
	// Clients
	CServerConnection *cClients;		// TODO: use std::list or vector

	// Worms
	int			iNumPlayers;
	CWorm		*cWorms;		// TODO: use std::list or vector

	// Projectiles
	//CProjectile	*cProjectiles;

	// Bonuses
	CBonus		cBonuses[MAX_BONUSES];  // TODO: use std::list or vector

	// Map
	bool		bRandomMap;		// TODO: what is this good for
	CMap		*cMap;

	FlagInfo*	m_flagInfo;
	
	// Simulation
	TimeDiff	fServertime;
	int			iServerFrame;	// TODO: what is this good for
	int			lastClientSendData;
	
	AbsTime		fLastBonusTime;

	int			iLastVictim;	// TODO: what is this good for

	// Network
	SmartPointer<NetworkSocket>	tSockets[MAX_SERVER_SOCKETS];
	int				nPort;
	typedef std::list< SmartPointer<NatConnection> > NatConnList;
	NatConnList	tNatClients;
	challenge_t		tChallenges[MAX_CHALLENGES]; // TODO: use std::list or vector
	CShootList		cShootList;
	CHttp			tHttp;
	CHttp			tHttp2;
	bool			bLocalClientConnected;
	int				iSuicidesInPacket;

	CBanList	cBanList;
	AbsTime		fLastUpdateSent;

	bool		bServerRegistered;
	AbsTime		fLastRegister;
	std::string sCurrentUrl;
	std::list<std::string>::iterator	tCurrentMasterServer;
	std::list<std::string>				tMasterServers;
	AbsTime		fRegisterUdpTime;
	std::vector<std::string>			tUdpMasterServers;
	int			iFirstUdpMasterServerNotRespondingCount;
	
	AbsTime		fWeaponSelectionTime;
	int			iWeaponSelectionTime_Warning;
	std::string	sExternalIP;
	
	bool		m_clientsNeedLobbyUpdate;
	AbsTime		m_clientsNeedLobbyUpdateTime;
	
	std::string	netError;

	friend class CServerNetEngine;
	friend class CServerNetEngineBeta7;
	friend class CServerNetEngineBeta9;
	
public:
	// Methods


	void		Clear();
	void		ResetSockets();
	void		SetSocketWithEvents(bool v);
	int			StartServer();
	void		Shutdown();

    void        notifyLog(const std::string& msg);

	// Game
	void		Frame();
	int			StartGame(std::string* errMsg = NULL);
	void		BeginMatch(CServerConnection* cl = NULL); // if NULL, begin match for everybody; or only for cl
	void		GameOver();

	void		SpawnWorm(CWorm *Worm, CVec * _pos = NULL, CServerConnection * client = NULL);
	void		SimulateGame();
	
	void		SpawnBonus();
	void		WormShoot(CWorm *w);
	void		WormShootEnd(CWorm* w, const weapon_t* weapon);
    void        RecheckGame();

	void		gotoLobby(bool alsoWithMenu, const std::string& reason);


	// Network
	bool		ReadPackets();
	void		SendPackets(bool sendPendingOnly = false);

	bool		ReadPacketsFromSocket(const SmartPointer<NetworkSocket>& sock);

	int			getPort() { return nPort; }
	bool		checkBandwidth(CServerConnection *cl);
	static bool	checkUploadBandwidth(float fCurUploadRate); // used by client/server to check upload
	static float getMaxUploadBandwidth();
	void		ObtainExternalIP();
	void		ProcessGetExternalIP();
	void		RegisterServer();
	void		RegisterServerUdp();
	void		ProcessRegister();
	void		CheckRegister();
	bool		DeRegisterServer();
	void		DeRegisterServerUdp();
	bool		ProcessDeRegister();
	void		CheckTimeouts();
	void		CheckWeaponSelectionTime();
	void		DropClient(CServerConnection *cl, int reason, const std::string& sReason, bool showReason = true);
	CWorm*		AddWorm(const WormJoinInfo& wormInfo);
	void		PrepareWorm(CWorm* worm);
	void		RemoveClient(CServerConnection *cl, const std::string& reason);
	void		RemoveClientWorms(CServerConnection* cl, const std::set<CWorm*>& worms, const std::string& reason);
	void		RemoveAllClientWorms(CServerConnection* cl, const std::string& reason);
	void		kickWorm(int wormID, const std::string& sReason, bool showReason = true);
    void        kickWorm(const std::string& szWormName, const std::string& sReason, bool showReason = true);
	void		banWorm(int wormID, const std::string& sReason, bool showReason = true);
	void		banWorm(const std::string& szWormName, const std::string& sReason, bool showReason = true);
	void		muteWorm(int wormID);
	void		muteWorm(const std::string& szWormName);
	void		unmuteWorm(int wormID);
	void		unmuteWorm(const std::string& szWormName);
	void		authorizeWorm(int wormID);
	void		killWorm(int victimID, int killerID, int suicidesCount = 0); // suicidesCount is ignored if victimID != killerID
    void		cloneWeaponsToAllWorms(CWorm* worm);
	
	void        CheckReadyClient();
	void		CheckForFillWithBots();
	
	float		GetDownload();
	float		GetUpload(float timeRange = 2.0f);

	void		checkVersionCompatibilities(bool dropOut);
	bool		checkVersionCompatibility(CServerConnection* cl, bool dropOut, bool makeMsg = true, std::string* msg = NULL);
	bool		isVersionCompatible(const Version& ver, std::string* incompReason = NULL);
	bool		clientsConnected_less(const Version& ver); // true if clients < ver are connected
	
	
	// Sending
	void		SendGlobalPacket(CBytestream *bs); // TODO: move this to CServerNetEngine
	void		SendGlobalPacket(CBytestream* bs, const Version& minVersion);
	void		SendGlobalText(const std::string& text, int type);
	void		SendWormsOut(const std::list<byte>& ids);
	void		SendDisconnect();
    void        SendWormLobbyUpdate(CServerConnection* receiver = NULL, CServerConnection *target = NULL); // if NULL, to everybody, or only to cl. If target is NULL send info about all worms
	void		UpdateGameLobby();
	void		UpdateWorms();
	void		UpdateWorm(CWorm* w);
#ifdef FUZZY_ERROR_TESTING
	void		SendRandomPacket();
#endif
	void		SendFiles();
	void		SendEmptyWeaponsOnRespawn( CWorm * Worm );
	bool		SendUpdate();
	void		SendWeapons(CServerConnection* cl = NULL); // if NULL, send globally, else only to that client
	void		SendWormTagged(CWorm *w);
	void		SendTeamScoreUpdate();
	void		SendPlaySound(const std::string& name);

	void		SetWormSpeedFactor(int wormID, float f);
	void		SetWormCanUseNinja(int wormID, bool b);
	void		SetWormDamageFactor(int wormID, float f);
	void		SetWormShieldFactor(int wormID, float f);
	void		SetWormCanAirJump(int wormID, bool b);

	
	// Connectionless packets only here
	void		ParseConnectionlessPacket(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs, const std::string& ip);
	void		ParseGetChallenge(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs);
	void		ParseConnect(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs);
	void		ParsePing(const SmartPointer<NetworkSocket>& tSocket);
	void		ParseTime(const SmartPointer<NetworkSocket>& tSocket);
	void		ParseQuery(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs, const std::string& ip);
    void        ParseGetInfo(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bsHeader = NULL);
	void		ParseWantsJoin(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs, const std::string& ip);
	void		ParseTraverse(const SmartPointer<NetworkSocket>& tSocket, CBytestream *bs, const std::string& ip);
	void		ParseServerRegistered(const SmartPointer<NetworkSocket>& tSocket);


	// Variables
	const SmartPointer<CGameScript>& getGameScript() { return cGameScript; }
	FlagInfo*		flagInfo() const	{ return m_flagInfo; }
	CWorm			*getWorms()			{ return cWorms; }
	CMap			*getMap()			{ return cMap; }
	void			resetMap()			{ cMap = NULL; }
	CMap*			getPreloadedMap(); // IMPORTANT: never ever keep this pointer! it's only temporarly! also don't modify the map!
	CBanList		*getBanList()		{ return &cBanList; }
	CServerConnection *getClient(int iWormID);
	bool			getGameOver()		{ return bGameOver; }
	AbsTime			getGameOverTime()	{ return fGameOverTime; }
	CHttp *getHttp()  { return &tHttp; }
	CServerConnection* getClients() { return cClients; }
	CServerConnection* localClientConnection();
	TimeDiff	getServerTime() { return fServertime; }
	bool		isServerRunning() const { return cWorms && cClients; }
	int		getState() const { return iState; }
	int		getNumPlayers() const		{ return iNumPlayers; }
	int		getNumBots() const;
	int		getLastBot() const;
	int		getFirstEmptyTeam() const; // -1 if there is no empty team; only possible teams by gamemode
	bool	isTeamEmpty(int t) const;
	int		getTeamWormNum(int t) const;
	bool	allWormsHaveFullLives() const;
	int		getAliveWormCount() const;
	int		getAliveTeamCount() const;
	CWorm	*getFirstAliveWorm() const;
	
	void	setWeaponRestFile(const std::string& fn);
	void	setDefaultWeaponRestFile();
	CWpnRest* getWeaponRestrictions() { return &cWeaponRestrictions; }
	
	bool	serverChoosesWeapons();
	bool	serverAllowsConnectDuringGame();
	
	void	DumpGameState(CmdLineIntf* caller);
	void	DumpConnections();
};

extern	GameServer		*cServer;

void SyncServerAndClient();

#endif  //  __CSERVER_H__
