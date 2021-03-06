/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

#include "character.h"
#include "laser.h"
#include "projectile.h"

// INFCROYA BEGIN ------------------------------------------------------------
#include <infcroya/croyaplayer.h>
#include <infcroya/classes/class.h>
#include <game/server/eventhandler.h>
#include <infcroya/entities/engineer-wall.h>
#include <infcroya/entities/biologist-mine.h>
#include <infcroya/entities/soldier-bomb.h>
#include <infcroya/entities/scientist-mine.h>
#include <infcroya/entities/medic-grenade.h>
#include <infcroya/entities/merc-bomb.h>
#include <infcroya/entities/scatter-grenade.h>
#include <game/server/gamemodes/mod.h>
// INFCROYA END ------------------------------------------------------------//

//input count
struct CInputCount
{
	int m_Presses;
	int m_Releases;
};

CInputCount CountInput(int Prev, int Cur)
{
	CInputCount c = {0, 0};
	Prev &= INPUT_STATE_MASK;
	Cur &= INPUT_STATE_MASK;
	int i = Prev;

	while(i != Cur)
	{
		i = (i+1)&INPUT_STATE_MASK;
		if(i&1)
			c.m_Presses++;
		else
			c.m_Releases++;
	}

	return c;
}


MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld)
: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), ms_PhysSize)
{
	m_Health = 0;
	m_Armor = 0;
	m_TriggeredEvents = 0;

	// INFCROYA BEGIN ------------------------------------------------------------
	if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
		m_Infected = false;
		m_HeartID = Server()->SnapNewID();
		m_FirstShot = true;
		m_BarrierHintID = Server()->SnapNewID();
		m_BarrierHintIDs.set_size(2);
		for (int i = 0; i < 2; i++)
		{
			m_BarrierHintIDs[i] = Server()->SnapNewID();
		}
		m_RespawnPointID = Server()->SnapNewID();
	}
	
	m_IsFrozen = false;
	m_FrozenTime = -1;
	m_PoisonTick = 0;
	m_InAirTick = 0;
	// INFCROYA END ------------------------------------------------------------//
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	// INFCROYA BEGIN ------------------------------------------------------------
	SetNormalEmote(EMOTE_NORMAL);
	m_IsFrozen = false;
	m_FrozenTime = -1;
	m_Poison = 0;
	SetHookProtected(true); // both humans and zombies hook protected by default
	// INFCROYA END ------------------------------------------------------------//
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	m_ActiveWeapon = WEAPON_GUN;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;

	m_pPlayer = pPlayer;
	m_Pos = Pos;

	m_Core.Reset();
	m_Core.Init(&GameWorld()->m_Core, GameServer()->Collision());
	m_Core.m_Pos = m_Pos;
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameWorld()->InsertEntity(this);
	m_Alive = true;

	GameServer()->m_pController->OnCharacterSpawn(this);

	return true;
}

void CCharacter::Destroy()
{
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
	// INFCROYA BEGIN ------------------------------------------------------------
	if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
		if (m_HeartID >= 0) {
			Server()->SnapFreeID(m_HeartID);
			m_HeartID = -1;
		}
		if (m_BarrierHintID >= 0) {
			Server()->SnapFreeID(m_BarrierHintID);
			m_BarrierHintID = -1;
		}
		if (m_RespawnPointID >= 0) {
			Server()->SnapFreeID(m_RespawnPointID);
			m_RespawnPointID = -1;
		}

		if (m_BarrierHintIDs[0] >= 0) {
			for (int i = 0; i < 2; i++) {
				Server()->SnapFreeID(m_BarrierHintIDs[i]);
				m_BarrierHintIDs[i] = -1;
			}
		}
	}
	DestroyChildEntities();
	// INFCROYA END ------------------------------------------------------------//
}

void CCharacter::SetWeapon(int W)
{
	if(W == m_ActiveWeapon)
		return;

	m_LastWeapon = m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_ActiveWeapon = W;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);

	if(m_ActiveWeapon < 0 || m_ActiveWeapon >= NUM_WEAPONS)
		m_ActiveWeapon = 0;
	m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
}

bool CCharacter::IsGrounded()
{
	if(GameServer()->Collision()->CheckPoint(m_Pos.x+GetProximityRadius()/2, m_Pos.y+GetProximityRadius()/2+5))
		return true;
	if(GameServer()->Collision()->CheckPoint(m_Pos.x-GetProximityRadius()/2, m_Pos.y+GetProximityRadius()/2+5))
		return true;
	return false;
}


void CCharacter::HandleNinja()
{
	if(m_ActiveWeapon != WEAPON_NINJA)
		return;

	if ((Server()->Tick() - m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000))
	{
		// time's up, return
		m_aWeapons[WEAPON_NINJA].m_Got = false;
		m_ActiveWeapon = m_LastWeapon;

		// reset velocity
		if(m_Ninja.m_CurrentMoveTime > 0)
			m_Core.m_Vel = m_Ninja.m_ActivationDir*m_Ninja.m_OldVelAmount;

		SetWeapon(m_ActiveWeapon);
		return;
	}

	// force ninja Weapon
	SetWeapon(WEAPON_NINJA);

	m_Ninja.m_CurrentMoveTime--;

	if (m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir*m_Ninja.m_OldVelAmount;
	}

	if (m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;
		GameServer()->Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CCharacter *aEnts[MAX_CLIENTS];
			vec2 Dir = m_Pos - OldPos;
			float Radius = GetProximityRadius() * 2.0f;
			vec2 Center = OldPos + Dir * 0.5f;
			int Num = GameWorld()->FindEntities(Center, Radius, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			for (int i = 0; i < Num; ++i)
			{
				if (aEnts[i] == this)
					continue;

				// make sure we haven't Hit this object before
				bool bAlreadyHit = false;
				for (int j = 0; j < m_NumObjectsHit; j++)
				{
					if (m_apHitObjects[j] == aEnts[i])
						bAlreadyHit = true;
				}
				if (bAlreadyHit)
					continue;

				// check so we are sufficiently close
				if (distance(aEnts[i]->m_Pos, m_Pos) > (GetProximityRadius() * 2.0f))
					continue;

				// Hit a player, give him damage and stuffs...
				GameServer()->CreateSound(aEnts[i]->m_Pos, SOUND_NINJA_HIT);
				// set his velocity to fast upward (for now)
				if(m_NumObjectsHit < 10)
					m_apHitObjects[m_NumObjectsHit++] = aEnts[i];

				aEnts[i]->TakeDamage(vec2(0, -10.0f), m_Ninja.m_ActivationDir*-1, g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, m_pPlayer->GetCID(), WEAPON_NINJA);
			}
		}

		return;
	}

	return;
}


void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_aWeapons[WEAPON_NINJA].m_Got)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if (m_aWeapons[WantedWeapon].m_Got) {
				Next--;
				// INFCROYA BEGIN ------------------------------------------------------------
				if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
					GetCroyaPlayer()->OnMouseWheelDown(this);
				}
				// INFCROYA END ------------------------------------------------------------//
			}
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if (m_aWeapons[WantedWeapon].m_Got) {
				Prev--;
				// INFCROYA BEGIN ------------------------------------------------------------
				if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
					GetCroyaPlayer()->OnMouseWheelUp(this);
				}
				// INFCROYA END ------------------------------------------------------------//
			}
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon-1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_ActiveWeapon && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::FireWeapon()
{
	if(m_ReloadTimer != 0)
		return;

	DoWeaponSwitch();
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if (m_ActiveWeapon == WEAPON_GRENADE || m_ActiveWeapon == WEAPON_SHOTGUN || m_ActiveWeapon == WEAPON_LASER)
		FullAuto = true;
	// INFCROYA BEGIN ------------------------------------------------------------
	if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
		if (m_ActiveWeapon == WEAPON_GRENADE || m_ActiveWeapon == WEAPON_SHOTGUN || m_ActiveWeapon == WEAPON_LASER || m_ActiveWeapon == WEAPON_GUN) // INFCROYA RELATED
			FullAuto = true;
	}
	// INFCROYA END ------------------------------------------------------------//


	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire&1) && m_aWeapons[m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_aWeapons[m_ActiveWeapon].m_Ammo)
	{
		// 125ms is a magical limit of how fast a human can click
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		if(m_LastNoAmmoSound+Server()->TickSpeed() <= Server()->Tick())
		{
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);
			m_LastNoAmmoSound = Server()->Tick();
		}
		return;
	}

	vec2 ProjStartPos = m_Pos+Direction*GetProximityRadius()*0.75f;

	if(g_Config.m_Debug)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "shot player='%d:%s' team=%d weapon=%d", m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), m_pPlayer->GetTeam(), m_ActiveWeapon);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	switch(m_ActiveWeapon)
	{
		case WEAPON_HAMMER:
		{
			// INFCROYA BEGIN ------------------------------------------------------------
			if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
				m_pCroyaPlayer->OnWeaponFire(Direction, ProjStartPos, WEAPON_HAMMER, this);
			}
			else {
				// reset objects Hit
				m_NumObjectsHit = 0;
				GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE);

			CCharacter *apEnts[MAX_CLIENTS];
			int Hits = 0;
			int Num = GameWorld()->FindEntities(ProjStartPos, GetProximityRadius()*0.5f, (CEntity**)apEnts,
														MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

				for (int i = 0; i < Num; ++i)
				{
					CCharacter* pTarget = apEnts[i];

					if ((pTarget == this) || GameServer()->Collision()->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL))
						continue;

					// set his velocity to fast upward (for now)
					if (length(pTarget->m_Pos - ProjStartPos) > 0.0f)
						GameServer()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos) * GetProximityRadius() * 0.5f);
					else
						GameServer()->CreateHammerHit(ProjStartPos);

					vec2 Dir;
					if (length(pTarget->m_Pos - m_Pos) > 0.0f)
						Dir = normalize(pTarget->m_Pos - m_Pos);
					else
						Dir = vec2(0.f, -1.f);

					pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, Dir * -1, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
						m_pPlayer->GetCID(), m_ActiveWeapon);
					Hits++;
				}

				// if we Hit anything, we have to wait for the reload
				if (Hits)
					m_ReloadTimer = Server()->TickSpeed() / 3;
			}
			// INFCROYA END ------------------------------------------------------------//

		} break;

		case WEAPON_GUN:
		{
			// INFCROYA BEGIN ------------------------------------------------------------
			if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
				m_pCroyaPlayer->OnWeaponFire(Direction, ProjStartPos, WEAPON_GUN, this);
			}
			else {
				new CProjectile(GameWorld(), WEAPON_GUN,
					m_pPlayer->GetCID(),
					ProjStartPos,
					Direction,
					(int)(Server()->TickSpeed() * GameServer()->Tuning()->m_GunLifetime),
					g_pData->m_Weapons.m_Gun.m_pBase->m_Damage, false, 0, -1, WEAPON_GUN);

				GameServer()->CreateSound(m_Pos, SOUND_GUN_FIRE);
			}
			// INFCROYA END ------------------------------------------------------------//
		} break;

		case WEAPON_SHOTGUN:
		{
			// INFCROYA BEGIN ------------------------------------------------------------
			if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
				m_pCroyaPlayer->OnWeaponFire(Direction, ProjStartPos, WEAPON_SHOTGUN, this);
			}
			else {
				int ShotSpread = 2;

				for (int i = -ShotSpread; i <= ShotSpread; ++i)
				{
					float Spreading[] = { -0.185f, -0.070f, 0, 0.070f, 0.185f };
					float a = angle(Direction);
					a += Spreading[i + 2];
					float v = 1 - (absolute(i) / (float)ShotSpread);
					float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
					new CProjectile(GameWorld(), WEAPON_SHOTGUN,
						m_pPlayer->GetCID(),
						ProjStartPos,
						vec2(cosf(a), sinf(a)) * Speed,
						(int)(Server()->TickSpeed() * GameServer()->Tuning()->m_ShotgunLifetime),
						g_pData->m_Weapons.m_Shotgun.m_pBase->m_Damage, false, 0, -1, WEAPON_SHOTGUN);
				}

				GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
			}
			// INFCROYA END ------------------------------------------------------------//
		} break;

		case WEAPON_GRENADE:
		{
			// INFCROYA BEGIN ------------------------------------------------------------
			if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
				m_pCroyaPlayer->OnWeaponFire(Direction, ProjStartPos, WEAPON_GRENADE, this);
			}
			else {
				new CProjectile(GameWorld(), WEAPON_GRENADE,
					m_pPlayer->GetCID(),
					ProjStartPos,
					Direction,
					(int)(Server()->TickSpeed() * GameServer()->Tuning()->m_GrenadeLifetime),
					g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);

				GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE);
			}
			// INFCROYA END ------------------------------------------------------------//
		} break;

		case WEAPON_LASER:
		{
			// INFCROYA BEGIN ------------------------------------------------------------
			if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
				m_pCroyaPlayer->OnWeaponFire(Direction, ProjStartPos, WEAPON_LASER, this);
			}
			else {
				new CLaser(GameWorld(), m_Pos, Direction, GameServer()->Tuning()->m_LaserReach, m_pPlayer->GetCID());
				GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE);
			}
			// INFCROYA END ------------------------------------------------------------//
		} break;

		case WEAPON_NINJA:
		{
			// INFCROYA BEGIN ------------------------------------------------------------
			if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
				m_pCroyaPlayer->OnWeaponFire(Direction, ProjStartPos, WEAPON_NINJA, this);
			}
			else {
				// reset Hit objects
				m_NumObjectsHit = 0;

				m_Ninja.m_ActivationDir = Direction;
				m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
				m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);

				GameServer()->CreateSound(m_Pos, SOUND_NINJA_FIRE);
			}
			// INFCROYA END ------------------------------------------------------------//
		} break;

	}

	m_AttackTick = Server()->Tick();

	if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0) // -1 == unlimited
		m_aWeapons[m_ActiveWeapon].m_Ammo--;

	// INFCROYA BEGIN ------------------------------------------------------------

	if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
		m_ReloadTimer = Server()->GetFireDelay(GetInfWeaponID(m_ActiveWeapon)) * Server()->TickSpeed() / 1000;
	}
	else {
		if (!m_ReloadTimer)
			m_ReloadTimer = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Firedelay * Server()->TickSpeed() / 1000;
	}
	// INFCROYA END ------------------------------------------------------------//
}

void CCharacter::HandleWeapons()
{
	//ninja
	HandleNinja();

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();

	// ammo regen
	// INFCROYA BEGIN ------------------------------------------------------------
	if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
		for (int i = WEAPON_GUN; i <= WEAPON_LASER; i++)
		{
			int InfWID = GetInfWeaponID(i);
			int AmmoRegenTime = Server()->GetAmmoRegenTime(InfWID);
			int MaxAmmo = Server()->GetMaxAmmo(InfWID);

			if (InfWID == INFWEAPON_MERCENARY_GUN)
			{
				if (m_InAirTick > Server()->TickSpeed() * 4)
				{
					AmmoRegenTime = 0;
				}
			}

			if (AmmoRegenTime)
			{
				if (m_ReloadTimer <= 0)
				{
					if (m_aWeapons[i].m_AmmoRegenStart < 0)
						m_aWeapons[i].m_AmmoRegenStart = Server()->Tick();

					if ((Server()->Tick() - m_aWeapons[i].m_AmmoRegenStart) >= AmmoRegenTime * Server()->TickSpeed() / 1000)
					{
						// Add some ammo
						m_aWeapons[i].m_Ammo = min(m_aWeapons[i].m_Ammo + 1, MaxAmmo);
						m_aWeapons[i].m_AmmoRegenStart = -1;
					}
				}
			}
		}
	}
	else {
		int AmmoRegenTime = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Ammoregentime;
		if (AmmoRegenTime && m_aWeapons[m_ActiveWeapon].m_Ammo >= 0)
		{
			// If equipped and not active, regen ammo?
			if (m_ReloadTimer <= 0)
			{
				if (m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart < 0)
					m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick();

				if ((Server()->Tick() - m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart) >= AmmoRegenTime * Server()->TickSpeed() / 1000)
				{
					// Add some ammo
					m_aWeapons[m_ActiveWeapon].m_Ammo = min(m_aWeapons[m_ActiveWeapon].m_Ammo + 1,
						g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Maxammo);
					m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
				}
			}
			else
			{
				m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
			}
		}
	}

	// not in the right place, but still in Tick()
	if (!IsGrounded() && (m_Core.m_HookState != HOOK_GRABBED || m_Core.m_HookedPlayer != -1))
		m_InAirTick++;
	else
		m_InAirTick = 0;
	// INFCROYA END ------------------------------------------------------------//

	return;
}

bool CCharacter::GiveWeapon(int Weapon, int Ammo)
{
	if(m_aWeapons[Weapon].m_Ammo < g_pData->m_Weapons.m_aId[Weapon].m_Maxammo || !m_aWeapons[Weapon].m_Got)
	{
		m_aWeapons[Weapon].m_Got = true;
		m_aWeapons[Weapon].m_Ammo = min(g_pData->m_Weapons.m_aId[Weapon].m_Maxammo, Ammo);
		return true;
	}
	return false;
}

void CCharacter::GiveNinja()
{
	m_Ninja.m_ActivationTick = Server()->Tick();
	m_Ninja.m_CurrentMoveTime = -1;
	m_aWeapons[WEAPON_NINJA].m_Got = true;
	m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	if (m_ActiveWeapon != WEAPON_NINJA)
		m_LastWeapon = m_ActiveWeapon;
	m_ActiveWeapon = WEAPON_NINJA;

	GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA);
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

// INFCROYA BEGIN ------------------------------------------------------------
void CCharacter::SetNormalEmote(int Emote)
{
	m_NormalEmote = Emote;
}

bool CCharacter::IsHuman() const {
	return !m_Infected;
}

bool CCharacter::IsZombie() const {
	return m_Infected;
}

void CCharacter::SetInfected(bool Infected) {
	m_Infected = Infected;
	m_Core.m_Infected = Infected;
}

void CCharacter::SetCroyaPlayer(CroyaPlayer* CroyaPlayer) {
	m_pCroyaPlayer = CroyaPlayer;
}

CroyaPlayer* CCharacter::GetCroyaPlayer() {
	return m_pCroyaPlayer;
}

void CCharacter::ResetWeaponsHealth()
{
	m_Health = 0;
	m_Armor = 0;
	for (auto& each : m_aWeapons) {
		each.m_Got = false;
	}
}

int CCharacter::GetActiveWeapon() const
{
	return m_ActiveWeapon;
}

void CCharacter::SetReloadTimer(int ReloadTimer)
{
	m_ReloadTimer = ReloadTimer;
}

void CCharacter::SetNumObjectsHit(int NumObjectsHit)
{
	m_NumObjectsHit = NumObjectsHit;
}

void CCharacter::Infect(int From)
{
	if (From >= 0) { // -1 and below is a special case (e.g infect when inside infection zone)
		// Kill message (copypasted from CCharacter::TakeDamage)
		CNetMsg_Sv_KillMsg Msg;
		Msg.m_Killer = From;
		Msg.m_Victim = m_pPlayer->GetCID();
		Msg.m_Weapon = WEAPON_HAMMER;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

		// BEGIN // do that before you actually turn someone into a zombie
		GetCroyaPlayer()->SetOldClassNum(GetCroyaPlayer()->GetClassNum());
		GameServer()->m_apPlayers[From]->GetCroyaPlayer()->OnKill(GetPlayer()->GetCID());
		// END //   do that before you actually turn someone into a zombie
	}
	GetCroyaPlayer()->SetOldClassNum(GetCroyaPlayer()->GetClassNum());
	GetCroyaPlayer()->TurnIntoRandomZombie();
}

bool CCharacter::IncreaseOverallHp(int Amount)
{
	bool success = false;
	if (m_Health < 10)
	{
		int healthDiff = 10 - m_Health;
		IncreaseHealth(Amount);
		success = true;
		Amount = Amount - healthDiff;
	}
	if (Amount > 0)
	{
		if (IncreaseArmor(Amount))
			success = true;
	}
	return success;
}

int CCharacter::GetArmor() const
{
	return m_Armor;
}

int CCharacter::GetHealthArmorSum() const
{
	return m_Health + m_Armor;
}

void CCharacter::SetHealthArmor(int Health, int Armor)
{
	m_Health = Health;
	m_Armor = Armor;
}

CCharacterCore& CCharacter::GetCharacterCore()
{
	return m_Core;
}

void CCharacter::Freeze(float Time, int Player, int Reason)
{
	//if (m_IsFrozen && m_FreezeReason == FREEZEREASON_UNDEAD)
	//	return;

	m_IsFrozen = true;
	m_FrozenTime = Server()->TickSpeed() * Time;
	m_FreezeReason = Reason;

	m_LastFreezer = Player;
}
void CCharacter::Unfreeze()
{
	m_IsFrozen = false;
	m_FrozenTime = -1;

	if (m_FreezeReason == FREEZEREASON_UNDEAD)
	{
		m_Health = 10.0;
	}

	GameServer()->CreatePlayerSpawn(m_Pos);
}
void CCharacter::Poison(int Count, int From)
{
	if (m_Poison <= 0)
	{
		m_PoisonTick = 0;
		m_Poison = Count;
		m_PoisonFrom = From;
	}
}

void CCharacter::DestroyChildEntities()
{
	for (CEngineerWall* pWall = (CEngineerWall*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_ENGINEER_WALL); pWall; pWall = (CEngineerWall*)pWall->TypeNext())
	{
		if (pWall->m_Owner != m_pPlayer->GetCID()) continue;
		GameServer()->m_World.DestroyEntity(pWall);
	}
	for (CSoldierBomb* pBomb = (CSoldierBomb*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_SOLDIER_BOMB); pBomb; pBomb = (CSoldierBomb*)pBomb->TypeNext())
	{
		if (pBomb->m_Owner != m_pPlayer->GetCID()) continue;
		GameServer()->m_World.DestroyEntity(pBomb);
	}
	for (CScientistMine* pMine = (CScientistMine*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_SCIENTIST_MINE); pMine; pMine = (CScientistMine*)pMine->TypeNext())
	{
		if (pMine->m_Owner != m_pPlayer->GetCID()) continue;
		GameServer()->m_World.DestroyEntity(pMine);
	}
	for (CBiologistMine* pMine = (CBiologistMine*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_BIOLOGIST_MINE); pMine; pMine = (CBiologistMine*)pMine->TypeNext())
	{
		if (pMine->m_Owner != m_pPlayer->GetCID()) continue;
		GameServer()->m_World.DestroyEntity(pMine);
	}
	for (CMedicGrenade* pGrenade = (CMedicGrenade*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_MEDIC_GRENADE); pGrenade; pGrenade = (CMedicGrenade*)pGrenade->TypeNext())
	{
		if (pGrenade->m_Owner != m_pPlayer->GetCID()) continue;
		GameServer()->m_World.DestroyEntity(pGrenade);
	}
	for (CMercenaryBomb* pBomb = (CMercenaryBomb*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_MERCENARY_BOMB); pBomb; pBomb = (CMercenaryBomb*)pBomb->TypeNext())
	{
		if (pBomb->m_Owner != m_pPlayer->GetCID()) continue;
		GameServer()->m_World.DestroyEntity(pBomb);
	}
	for (CScatterGrenade* pGrenade = (CScatterGrenade*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_SCATTER_GRENADE); pGrenade; pGrenade = (CScatterGrenade*)pGrenade->TypeNext())
	{
		if (pGrenade->m_Owner != m_pPlayer->GetCID()) continue;
		GameServer()->m_World.DestroyEntity(pGrenade);
	}

	m_FirstShot = true;
}

bool CCharacter::IsHookProtected() const
{
	return m_HookProtected;
}

void CCharacter::SetHookProtected(bool HookProtected)
{
	m_HookProtected = HookProtected;
	m_Core.m_HookProtected = HookProtected;
}

CNetObj_PlayerInput& CCharacter::GetInput()
{
	return m_Input;
}

bool CCharacter::FindPortalPosition(vec2 Pos, vec2& Res)
{
	vec2 PortalShift = Pos - m_Pos;
	vec2 PortalDir = normalize(PortalShift);
	if (length(PortalShift) > 500.0f)
		PortalShift = PortalDir * 500.0f;

	float Iterator = length(PortalShift);
	while (Iterator > 0.0f)
	{
		PortalShift = PortalDir * Iterator;
		vec2 PortalPos = m_Pos + PortalShift;

		if (GameServer()->m_pController->IsSpawnable(PortalPos))
		{
			Res = PortalPos;
			return true;
		}

		Iterator -= 4.0f;
	}

	return false;
}

void CCharacter::SaturateVelocity(vec2 Force, float MaxSpeed)
{
	if (length(Force) < 0.00001)
		return;

	float Speed = length(m_Core.m_Vel);
	vec2 VelDir = normalize(m_Core.m_Vel);
	if (Speed < 0.00001)
	{
		VelDir = normalize(Force);
	}
	vec2 OrthoVelDir = vec2(-VelDir.y, VelDir.x);
	float VelDirFactor = dot(Force, VelDir);
	float OrthoVelDirFactor = dot(Force, OrthoVelDir);

	vec2 NewVel = m_Core.m_Vel;
	if (Speed < MaxSpeed || VelDirFactor < 0.0f)
	{
		NewVel += VelDir * VelDirFactor;
		float NewSpeed = length(NewVel);
		if (NewSpeed > MaxSpeed)
		{
			if (VelDirFactor > 0.f)
				NewVel = VelDir * MaxSpeed;
			else
				NewVel = -VelDir * MaxSpeed;
		}
	}

	NewVel += OrthoVelDir * OrthoVelDirFactor;

	m_Core.m_Vel = NewVel;
}

int CCharacter::GetInfWeaponID(int WID)
{
	if (WID == WEAPON_HAMMER)
	{
	}
	else if (WID == WEAPON_GUN)
	{
		switch (GetCroyaPlayer()->GetClassNum())
		{
		case Class::MERCENARY:
			return INFWEAPON_MERCENARY_GUN;
		default:
			return INFWEAPON_GUN;
		}
		return INFWEAPON_GUN;
	}
	else if (WID == WEAPON_SHOTGUN)
	{
		switch (GetCroyaPlayer()->GetClassNum())
		{
		case Class::MEDIC:
			return INFWEAPON_MEDIC_SHOTGUN;
		case Class::BIOLOGIST:
			return INFWEAPON_BIOLOGIST_SHOTGUN;
		default:
			return INFWEAPON_SHOTGUN;
		}
	}
	else if (WID == WEAPON_GRENADE)
	{
		switch (GetCroyaPlayer()->GetClassNum())
		{
		case Class::MERCENARY:
			return INFWEAPON_MERCENARY_GRENADE;
		case Class::MEDIC:
			return INFWEAPON_MEDIC_GRENADE;
		case Class::SOLDIER:
			return INFWEAPON_SOLDIER_GRENADE;
		case Class::SCIENTIST:
			return INFWEAPON_SCIENTIST_GRENADE;
		default:
			return INFWEAPON_GRENADE;
		}
	}
	else if (WID == WEAPON_LASER)
	{
		switch (GetCroyaPlayer()->GetClassNum())
		{
		case Class::ENGINEER:
			return INFWEAPON_ENGINEER_RIFLE;
		case Class::SCIENTIST:
			return INFWEAPON_SCIENTIST_RIFLE;
		case Class::BIOLOGIST:
			return INFWEAPON_BIOLOGIST_RIFLE;
		case Class::MEDIC:
			return INFWEAPON_MEDIC_RIFLE;
		default:
			return INFWEAPON_RIFLE;
		}
	}
	else if (WID == WEAPON_NINJA)
	{
		return INFWEAPON_NINJA;
	}
	return INFWEAPON_NONE;
}
int CCharacter::GetLastNoAmmoSound() const
{
	return m_LastNoAmmoSound;
}
void CCharacter::SetLastNoAmmoSound(int LastNoAmmoSound)
{
	m_LastNoAmmoSound = LastNoAmmoSound;
}
// INFCROYA END ------------------------------------------------------------//

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire&1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Tick()
{
	m_Core.m_Input = m_Input;
	m_Core.Tick(true);

	// handle leaving gamelayer
	if(GameLayerClipped(m_Pos))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}

	// handle Weapons
	HandleWeapons();

	// INFCROYA BEGIN ------------------------------------------------------------
	if (m_Poison > 0)
	{
		if (m_PoisonTick == 0)
		{
			m_Poison--;
			TakeDamage(vec2(0.0f, 0.0f), vec2(0, 0), 1, m_PoisonFrom, WEAPON_HAMMER);
			if (m_Poison > 0)
			{
				m_PoisonTick = Server()->TickSpeed() / 2;
			}
		}
		else
		{
			m_PoisonTick--;
		}
	}
	// INFCROYA END ------------------------------------------------------------//
}

void CCharacter::TickDefered()
{
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision());
		m_ReckoningCore.Tick(false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));

	m_Core.Move();

	bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Core.Quantize();
	bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		}StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	m_TriggeredEvents |= m_Core.m_TriggeredEvents;

	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}
	else if(m_Core.m_Death)
	{
		// handle death-tiles
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_ReckoningTick+Server()->TickSpeed()*3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_Ninja.m_ActivationTick;
	++m_ReckoningTick;
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart > -1)
		++m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart;
	if(m_EmoteStop > -1)
		++m_EmoteStop;
	// INFCROYA BEGIN ------------------------------------------------------------
	++m_HookDmgTick;
	// INFCROYA END ------------------------------------------------------------//
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= 10)
		return false;
	m_Health = clamp(m_Health+Amount, 0, 10);
	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= 10)
		return false;
	m_Armor = clamp(m_Armor+Amount, 0, 10);
	return true;
}

void CCharacter::Die(int Killer, int Weapon)
{
	// we got to wait 0.5 secs before respawning
	m_Alive = false;
	m_pPlayer->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	// INFCROYA BEGIN ------------------------------------------------------------
	int ModeSpecial = 0;
	if (str_comp_nocase(g_Config.m_SvGametype, "mod") != 0) {
		ModeSpecial = GameServer()->m_pController->OnCharacterDeath(this, (Killer < 0) ? 0 : GameServer()->m_apPlayers[Killer], Weapon);
	}
	// INFCROYA END ------------------------------------------------------------//

	char aBuf[256];
	if (Killer < 0)
		str_format(aBuf, sizeof(aBuf), "kill killer='%d:%d:' victim='%d:%d:%s' weapon=%d special=%d",
			Killer, - 1 - Killer,
			m_pPlayer->GetCID(), m_pPlayer->GetTeam(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial
		);
	else
		str_format(aBuf, sizeof(aBuf), "kill killer='%d:%d:%s' victim='%d:%d:%s' weapon=%d special=%d",
			Killer, GameServer()->m_apPlayers[Killer]->GetTeam(), Server()->ClientName(Killer),
			m_pPlayer->GetCID(), m_pPlayer->GetTeam(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial
		);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_ModeSpecial = ModeSpecial;
	for(int i = 0 ; i < MAX_CLIENTS; i++)
	{
		if(!Server()->ClientIngame(i))
			continue;
	// INFCROYA BEGIN ------------------------------------------------------------
	if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
		GameServer()->m_pController->OnCharacterDeath(this, GameServer()->m_apPlayers[Killer], Weapon);
	}
	// INFCROYA END ------------------------------------------------------------//

		if(Killer < 0 && Server()->GetClientVersion(i) < MIN_KILLMESSAGE_CLIENTVERSION)
		{
			Msg.m_Killer = 0;
			Msg.m_Weapon = WEAPON_WORLD;
		}
		else
		{
			Msg.m_Killer = Killer;
			Msg.m_Weapon = Weapon;
		}
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}

	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

	// this is for auto respawn after 3 secs
	m_pPlayer->m_DieTick = Server()->Tick();

	GameWorld()->RemoveEntity(this);
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID());

	// INFCROYA BEGIN ------------------------------------------------------------
	DestroyChildEntities();
	Destroy();
	// INFCROYA END ------------------------------------------------------------//
}

bool CCharacter::TakeDamage(vec2 Force, vec2 Source, int Dmg, int From, int Weapon)
{
	m_Core.m_Vel += Force;

	if(GameServer()->m_pController->IsFriendlyFire(m_pPlayer->GetCID(), From))
		return false;
	if(From >= 0)
	{
		if(GameServer()->m_pController->IsFriendlyFire(m_pPlayer->GetCID(), From))
			return false;
	}
	else
	{
		int Team = TEAM_RED;
		if(From == PLAYER_TEAM_BLUE)
			Team = TEAM_BLUE;
		if(GameServer()->m_pController->IsFriendlyTeamFire(m_pPlayer->GetTeam(), Team))
			return false;
	}

	// m_pPlayer only inflicts half damage on self
	if(From == m_pPlayer->GetCID())
		Dmg = max(1, Dmg/2);

	// INFCROYA BEGIN ------------------------------------------------------------
	// search tags: no self harm no selfharm selfhurt self hurt
	if (From == m_pPlayer->GetCID() && Weapon != WEAPON_WORLD) {
		int ClassNum = GetCroyaPlayer()->GetClassNum();
		if ((ClassNum == Class::SOLDIER && m_ActiveWeapon == WEAPON_GRENADE) || (ClassNum == Class::SCIENTIST && m_ActiveWeapon == WEAPON_LASER)) {
			return false;
		}
		if (ClassNum == Class::BOOMER) {
			return false;
		}
	}
	// INFCROYA END ------------------------------------------------------------//

	int OldHealth = m_Health, OldArmor = m_Armor;
	if(Dmg)
	{
		if(m_Armor)
		{
			if(Dmg > 1 && str_comp(g_Config.m_SvGametype, "mod") != 0) // INFCROYA RELATED
			{
				m_Health--;
				Dmg--;
			}

			if(Dmg > m_Armor)
			{
				Dmg -= m_Armor;
				m_Armor = 0;
			}
			else
			{
				m_Armor -= Dmg;
				Dmg = 0;
			}
		}

		m_Health -= Dmg;
	}

	// create healthmod indicator
	GameServer()->CreateDamage(m_Pos, m_pPlayer->GetCID(), Source, OldHealth-m_Health, OldArmor-m_Armor, From == m_pPlayer->GetCID());

	// do damage Hit sound
	if(From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
	{
		int64 Mask = CmaskOne(From);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS ||  GameServer()->m_apPlayers[i]->m_DeadSpecMode) &&
				GameServer()->m_apPlayers[i]->GetSpectatorID() == From)
				Mask |= CmaskOne(i);
		}
		GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
	}

	// check for death
	if(m_Health <= 0)
	{
		// INFCROYA BEGIN ------------------------------------------------------------
		if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0 && IsHuman() && From != GetPlayer()->GetCID()) {
			Infect(From);
		}
		else {
			Die(From, Weapon);
		}
		// INFCROYA END ------------------------------------------------------------//

		// set attacker's face to happy (taunt!)
		if (From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
		{
			CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
			if (pChr)
			{
				pChr->m_EmoteType = EMOTE_HAPPY;
				pChr->m_EmoteStop = Server()->Tick() + Server()->TickSpeed();
				// INFCROYA BEGIN ------------------------------------------------------------
				if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0 && GameServer()->m_apPlayers[From]) {
					pChr->GetCroyaPlayer()->OnKill(GetPlayer()->GetCID());
				}
				// INFCROYA END ------------------------------------------------------------//
			}
		}

		return false;
	}

	if (Dmg > 2)
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
	else
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);

	m_EmoteType = EMOTE_PAIN;
	m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;

	return true;
}

void CCharacter::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	// write down the m_Core
	if(!m_ReckoningTick || GameWorld()->m_Paused)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	// set emote
	if (m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = m_NormalEmote; // INFCROYA RELATED
		m_EmoteStop = -1;
	}

	pCharacter->m_Emote = m_EmoteType;

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_TriggeredEvents = m_TriggeredEvents;

	pCharacter->m_Weapon = m_ActiveWeapon;
	pCharacter->m_AttackTick = m_AttackTick;

	pCharacter->m_Direction = m_Input.m_Direction;

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
		(!g_Config.m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID()))
	{
		pCharacter->m_Health = m_Health;
		pCharacter->m_Armor = m_Armor;
		if(m_ActiveWeapon == WEAPON_NINJA)
			pCharacter->m_AmmoCount = m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000;
		else if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0)
			pCharacter->m_AmmoCount = m_aWeapons[m_ActiveWeapon].m_Ammo;
	}

	if(pCharacter->m_Emote == EMOTE_NORMAL)
	{
		if(250 - ((Server()->Tick() - m_LastAction)%(250)) < 5)
			pCharacter->m_Emote = EMOTE_BLINK;
	}

	// INFCROYA BEGIN ------------------------------------------------------------
	// Heart displayed on top of injured tees
	if (str_comp_nocase(g_Config.m_SvGametype, "mod") == 0) {
		CPlayer* pClient = GameServer()->m_apPlayers[SnappingClient];
		if (IsZombie() && GetHealthArmorSum() < 10 && SnappingClient != m_pPlayer->GetCID() && pClient->GetCroyaPlayer()->IsZombie()) {
			CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_HeartID, sizeof(CNetObj_Pickup)));
			if (!pP)
				return;

			pP->m_X = (int)m_Pos.x;
			pP->m_Y = (int)m_Pos.y - 60.0;
			pP->m_Type = PICKUP_HEALTH;
		}
		if (IsHuman() && m_Armor < 10 && SnappingClient != m_pPlayer->GetCID() && pClient->GetCroyaPlayer()->GetClassNum() == Class::MEDIC) {
			CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_HeartID, sizeof(CNetObj_Pickup)));
			if (!pP)
				return;

			pP->m_X = (int)m_Pos.x;
			pP->m_Y = (int)m_Pos.y - 60.0;
			if (m_Health < 10 && m_Armor == 0)
				pP->m_Type = PICKUP_HEALTH;
			else
				pP->m_Type = PICKUP_ARMOR;
		}
		if (!m_FirstShot)
		{
			CNetObj_Laser* pObj = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_BarrierHintID, sizeof(CNetObj_Laser)));
			if (!pObj)
				return;

			pObj->m_X = (int)m_FirstShotCoord.x;
			pObj->m_Y = (int)m_FirstShotCoord.y;
			pObj->m_FromX = (int)m_FirstShotCoord.x;
			pObj->m_FromY = (int)m_FirstShotCoord.y;
			pObj->m_StartTick = Server()->Tick();
		}
	}
	// INFCROYA END ------------------------------------------------------------//
}

void CCharacter::PostSnap()
{
	m_TriggeredEvents = 0;
}
