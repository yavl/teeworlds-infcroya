#pragma once

#include "class.h"

class CHunter : public IClass {
public:
	CHunter();

	void InitialWeaponsHealth(class CCharacter* pChr) override;

	void Tick(class CCharacter* pChr) override;

	void OnWeaponFire(vec2 Direction, vec2 ProjStartPos, int Weapon, class CCharacter* pChr) override;
};
