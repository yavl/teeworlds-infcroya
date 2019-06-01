#include "boomer.h"
#include "base/system.h"
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>

CBoomer::CBoomer() : IClass()
{
	CSkin skin;
	skin.SetBodyColor(58, 200, 79);
	skin.SetMarkingName("saddo");
	skin.SetMarkingColor(0, 0, 0, 120);
	skin.SetFeetColor(0, 79, 70);
	SetSkin(skin);
	SetInfectedClass(true);
	SetName("Boomer");
}

void CBoomer::InitialWeaponsHealth(CCharacter* pChr)
{
	pChr->IncreaseHealth(10);
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->SetWeapon(WEAPON_HAMMER);
	pChr->SetNormalEmote(EMOTE_ANGRY);
}

void CBoomer::OnWeaponFire(vec2 Direction, vec2 ProjStartPos, int Weapon, CCharacter* pChr)
{
	int ClientID = pChr->GetPlayer()->GetCID();

	switch (Weapon) {
		case WEAPON_HAMMER:
		{
			BoomerExplosion(pChr);
			pChr->Die(ClientID, WEAPON_SELF);
		}
	}
}

int CBoomer::OnCharacterDeath(CCharacter* pVictim, CPlayer* pKiller, int Weapon)
{
	//BoomerExplosion(pVictim);
	return 0;
}

void CBoomer::BoomerExplosion(CCharacter* pChr)
{
	int ClientID = pChr->GetPlayer()->GetCID();
	CGameContext* pGameServer = pChr->GameServer();
	//if( !IsFrozen() && !IsInLove() ) not implemented yet
	//{

		pGameServer->CreateSound(pChr->GetPos(), SOUND_GRENADE_EXPLODE);
		pGameServer->CreateExplosionDisk(pChr->GetPos(), 80.0f, 107.5f, 30, 52.0f, ClientID, WEAPON_HAMMER);
	//}
}
