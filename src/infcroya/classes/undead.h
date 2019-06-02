#pragma once

#include "class.h"

class CUndead : public IClass {
public:
	CUndead();

	void InitialWeaponsHealth(class CCharacter* pChr) override;

	void Tick(class CCharacter* pChr) override;
	
	int OnCharacterDeath(CCharacter* pVictim, CPlayer* pKiller, int Weapon) override;

	void OnWeaponFire(vec2 Direction, vec2 ProjStartPos, int Weapon, class CCharacter* pChr) override;
};
