#include "class.h"

class CBoomer : public IClass {
public:
	CBoomer();

	void InitialWeaponsHealth(class CCharacter* pChr) override;

	void OnWeaponFire(vec2 Direction, vec2 ProjStartPos, int Weapon, class CCharacter* pChr) override;
	
	int OnCharacterDeath(class CCharacter* pVictim, class CPlayer* pKiller, int Weapon) override;
	
	
private:
	void BoomerExplosion(class CCharacter* pChr);
	
};
