/* (c) InfCroya contributors. See licence.txt in the root of the distribution for more information. */
#ifndef INFCROYA_GAME_CROYA_GAMECONTEXT_H
#define INFCROYA_GAME_CROYA_GAMECONTEXT_H
#include <game/server/gamecontext.h>
#include <array>
#include <vector>
#include <unordered_map>
#include "game/server/player.h"
#include "game/server/entities/character.h"
#include <sstream>
#include <game/version.h>
#include <generated/server_data.h>
#include <infcroya/localization/localization.h>

class CCroyaGameContext : public CGameContext
{

public:
	
	//constructor / destructor
	CCroyaGameContext();
	~CCroyaGameContext() override;
	
	//overrides
	void OnTick() override;
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, int MaxDamage, bool MercBomb = false) override; // INFCROYA RELATED, (bool MercBomb)
	void CreateExplosionDisk(vec2 Pos, float InnerRadius, float DamageRadius, int Damage, float Force, int Owner, int Weapon) override;
	
	// CGameContext::SendCommand() copied from github.com/AssassinTee/catch64
	void SendCommand(int ChatterClientID, const std::string& command) override;
	void CreateLaserDotEvent(vec2 Pos0, vec2 Pos1, int LifeSpan) override;
	void SendChatTarget(int To, const char* pText) override;
	int GetHumanCount() const override;
	int GetZombieCount() const override;
	

private:

	struct LaserDotState
	{
		vec2 m_Pos0;
		vec2 m_Pos1;
		int m_LifeSpan;
		int m_SnapID;
	};
	
	array<LaserDotState> m_LaserDots;

};

#endif
