#pragma once

#include <base/vmath.h>
#include <unordered_map>
#include <string>

class IClass;
class CPlayer;
class CCharacter;
class CGameContext;
class CGameControllerMOD;

class CroyaPlayer {
private:
	IClass* m_pClass;
	CPlayer* m_pPlayer;
	CCharacter* m_pCharacter;
	CGameContext* m_pGameServer;
	CGameControllerMOD* m_pGameController;
	int m_ClientID;
	bool m_Infected;
	bool m_HookProtected;
	std::unordered_map<int, class IClass*> m_Classes;
	std::string m_Language;
	int m_OldClassNum; // set to old class on medic revive

	int m_RespawnPointsNum;
	int m_RespawnPointsDefaultNum;
	bool m_RespawnPointPlaced;
	vec2 m_RespawnPointPos;
	int m_RespawnPointDefaultCooldown; // in seconds
	int m_RespawnPointCooldown; // in seconds
	int m_AirJumpCounter;

	bool m_InsideInfectionZone;
public:
	CroyaPlayer(int ClientID, CPlayer* pPlayer, CGameContext* pGameServer, CGameControllerMOD* pGameController, std::unordered_map<int, class IClass*> Classes);
	~CroyaPlayer();

	void Tick();

	class CCircle* GetClosestCircle();
	class CInfCircle* GetClosestInfCircle();

	int GetClassNum();
	void SetClassNum(int Class, bool DrawPurpleThing = false);

	CCharacter* GetCharacter();
	void SetCharacter(CCharacter* pCharacter);

	CPlayer* GetPlayer();
	int GetClientID() const;

	void OnCharacterSpawn(CCharacter* pChr);
	void OnCharacterDeath(CCharacter* pVictim, CPlayer* pKiller, int Weapon);
	void OnKill(int Victim);

	void OnWeaponFire(vec2 Direction, vec2 ProjStartPos, int Weapon, CCharacter* pChr); // called in CCharacter::FireWeapon
	void OnButtonF3(); // (... and OnButtonF4) called in CGameContext::OnMessage { ... else if(MsgID == NETMSGTYPE_CL_VOTE) }
	void OnMouseWheelDown(CCharacter* pChr); // called in CCharacter::HandleWeaponSwitch
	void OnMouseWheelUp(CCharacter* pChr); // called in CCharacter::HandleWeaponSwitch

	vec2 GetRespawnPointPos() const;
	int GetRespawnPointsNum() const;
	void SetRespawnPointsNum(int Num);
	bool IsRespawnPointPlaced() const;
	void SetRespawnPointPlaced(bool Placed);
	int GetRespawnPointDefaultCooldown() const;
	int GetRespawnPointCooldown();
	void SetRespawnPointCooldown(int Cooldown);

	bool IsHuman() const;
	bool IsZombie() const;

	std::unordered_map<int, class IClass*>& GetClasses();

	void TurnIntoNextHumanClass();
	void TurnIntoPrevHumanClass();
	void TurnIntoRandomZombie();
	void TurnIntoRandomHuman();

	void TurnIntoPrevZombieClass();
	bool IsInsideInfectionZone() const;
	void SetInsideInfectionZone(bool InsideInfectionZone);

	int GetAirJumpCounter() const;
	void SetAirJumpCounter(int AirJumpCounter);

	bool IsHookProtected() const;
	void SetHookProtected(bool HookProtected);

	int GetOldClassNum() const;
	void SetOldClassNum(int Class);

	const char* GetLanguage() const;
	void SetLanguage(const char* Language);

	class CGameControllerMOD* GetGameControllerMOD();

	IClass* GetClass();
	void SetClass(IClass* pClass, bool DrawPurpleThing = false);
};
