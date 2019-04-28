#pragma once

#include "class.h"

class CSmoker : public IClass {
private:
	CSkin m_Skin;
	bool m_InfectedClass;
public:
	CSmoker();
	~CSmoker() override;
	const CSkin& GetSkin() const override;

	void OnCharacterSpawn(CCharacter* pChr) override;
	int OnCharacterDeath(class CCharacter* pVictim, class CPlayer* pKiller, int Weapon) override;

	void OnWeaponFire(vec2 Direction, int Weapon) override;

	bool IsInfectedClass() const override;
};