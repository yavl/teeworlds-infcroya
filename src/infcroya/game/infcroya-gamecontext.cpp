/* (c) InfCroya contributors. See licence.txt in the root of the distribution for more information. */
#include "infcroya-gamecontext.h"

CCroyaGameContext::CCroyaGameContext() : CGameContext() {}

CCroyaGameContext::~CCroyaGameContext(){}

void CCroyaGameContext::OnTick()
{
	// check tuning
	CheckPureTuning();

	// copy tuning
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();

	//if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->Tick();
			m_apPlayers[i]->PostTick();
		}
	}

	// INFCROYA BEGIN ------------------------------------------------------------
	// Clean old dots
	int DotIter;

	DotIter = 0;
	while (DotIter < m_LaserDots.size())
	{
		m_LaserDots[DotIter].m_LifeSpan--;
		if (m_LaserDots[DotIter].m_LifeSpan <= 0)
		{
			Server()->SnapFreeID(m_LaserDots[DotIter].m_SnapID);
			m_LaserDots.remove_index(DotIter);
		}
		else
			DotIter++;
	}
	// INFCROYA END ------------------------------------------------------------//

	// update voting
	if(m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if(m_VoteCloseTime == -1)
			EndVote(VOTE_END_ABORT, false);
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
				for(int i = 0; i < MAX_CLIENTS; i++)
					if(m_apPlayers[i])
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
				bool aVoteChecked[MAX_CLIENTS] = {0};
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!m_apPlayers[i] || m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || aVoteChecked[i])	// don't count in votes by spectators
						continue;

					int ActVote = m_apPlayers[i]->m_Vote;
					int ActVotePos = m_apPlayers[i]->m_VotePos;

					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i+1; j < MAX_CLIENTS; ++j)
					{
						if(!m_apPlayers[j] || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]))
							continue;

						aVoteChecked[j] = true;
						if(m_apPlayers[j]->m_Vote && (!ActVote || ActVotePos > m_apPlayers[j]->m_VotePos))
						{
							ActVote = m_apPlayers[j]->m_Vote;
							ActVotePos = m_apPlayers[j]->m_VotePos;
						}
					}

					Total++;
					if(ActVote > 0)
						Yes++;
					else if(ActVote < 0)
						No++;
				}
			}

			if(m_VoteEnforce == VOTE_ENFORCE_YES || (m_VoteUpdate && Yes >= Total/2+1))
			{
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				if(m_VoteCreator != -1 && m_apPlayers[m_VoteCreator])
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;

				EndVote(VOTE_END_PASS, m_VoteEnforce==VOTE_ENFORCE_YES);
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || (m_VoteUpdate && No >= (Total+1)/2) || time_get() > m_VoteCloseTime)
				EndVote(VOTE_END_FAIL, m_VoteEnforce==VOTE_ENFORCE_NO);
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}


#ifdef CONF_DEBUG
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->IsDummy())
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i&1)?-1:1;
			m_apPlayers[i]->OnPredictedInput(&Input);
		}
	}
#endif
}

void CCroyaGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, int MaxDamage, bool MercBomb) // INFCROYA RELATED, (bool MercBomb)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	// deal damage
	CCharacter *apEnts[MAX_CLIENTS];
	float Radius = g_pData->m_Explosion.m_Radius;
	float InnerRadius = 48.0f;
	float MaxForce = g_pData->m_Explosion.m_MaxForce;
	int Num = m_World.FindEntities(Pos, Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		// INFCROYA BEGIN ------------------------------------------------------------
		if (MercBomb && m_apPlayers[Owner]->GetCroyaPlayer()->IsZombie()) {
			break;
		}
		// INFCROYA END ------------------------------------------------------------//
		vec2 Diff = apEnts[i]->GetPos() - Pos;
		vec2 Force(0, MaxForce);
		float l = length(Diff);
		if(l)
			Force = normalize(Diff) * MaxForce;
		float Factor = 1 - clamp((l-InnerRadius)/(Radius-InnerRadius), 0.0f, 1.0f);
		if((int)(Factor * MaxDamage))
			apEnts[i]->TakeDamage(Force * Factor, Diff*-1, (int)(Factor * MaxDamage), Owner, Weapon);
	}
}

void CCroyaGameContext::CreateExplosionDisk(vec2 Pos, float InnerRadius, float DamageRadius, int Damage, float Force, int Owner, int Weapon)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
	if(Damage > 0)
	{
		// deal damage
		CCharacter *apEnts[MAX_CLIENTS];
		int Num = m_World.FindEntities(Pos, DamageRadius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++)
		{
			vec2 Diff = apEnts[i]->GetPos() - Pos;
			if (Diff.x == 0.0f && Diff.y == 0.0f)
				Diff.y = -0.5f;
			vec2 ForceDir(0,1);
			float len = length(Diff);
			len = 1-clamp((len-InnerRadius)/(DamageRadius-InnerRadius), 0.0f, 1.0f);
			
			if(len)
				ForceDir = normalize(Diff);
			
			float DamageToDeal = 1 + ((Damage - 1) * len);
			if (apEnts[i]->IsZombie())
				apEnts[i]->IncreaseOverallHp(DamageToDeal);
			apEnts[i]->TakeDamage(ForceDir * Force * len, Diff * -1, DamageToDeal, Owner, Weapon);
		}
	}
	
	float CircleLength = 2.0*pi*max(DamageRadius-135.0f, 0.0f);
	int NumSuroundingExplosions = CircleLength/32.0f;
	float AngleStart = frandom()*pi*2.0f;
	float AngleStep = pi*2.0f/static_cast<float>(NumSuroundingExplosions);
	for(int i=0; i<NumSuroundingExplosions; i++)
	{
		CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion));
		if(pEvent)
		{
			pEvent->m_X = (int)Pos.x + (DamageRadius-135.0f) * cos(AngleStart + i*AngleStep);
			pEvent->m_Y = (int)Pos.y + (DamageRadius-135.0f) * sin(AngleStart + i*AngleStep);
		}
	}
}

// CCroyaGameContext::SendCommand() copied from github.com/AssassinTee/catch64
void CCroyaGameContext::SendCommand(int ChatterClientID, const std::string& command)
{
	std::vector<std::string> messageList;
	if (command == "cmdlist")
	{
		messageList.push_back("-- Commands --");
		messageList.push_back("'/cmdlist'- show commands");
		messageList.push_back("'/help' - show help");
		messageList.push_back("'/info' - show mod information");
	}
	else if (command == "help")
	{
		messageList.push_back("-- Help --");
		messageList.push_back("Infection mod");
		messageList.push_back("Run away from zombies (greens) as a human");
		messageList.push_back("Infect humans as a zombie");
		messageList.push_back("More fun to play with friends ;)");
	}
	else if (command == "info")
	{
		messageList.push_back("-- Info --");
		messageList.push_back("InfCroya");
		messageList.push_back("InfClass with battle royale circles");
		messageList.push_back("Thanks to: All InfClass & InfClassR contributors and Assa for chat commands");
		messageList.push_back("Sources: https://github.com/yavl/teeworlds-infcroya");
		std::stringstream ss;
		ss << "Teeworlds version: '" << GAME_RELEASE_VERSION << "', Compiled: '" << __DATE__ << "'";
		messageList.push_back(ss.str());
	}
	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = CHAT_ALL;
	Msg.m_ClientID = -1;

	Msg.m_TargetID = ChatterClientID;
	for (auto it = messageList.begin(); it != messageList.end(); ++it)
	{
		Msg.m_pMessage = it->c_str();
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ChatterClientID);
	}
}

void CCroyaGameContext::CreateLaserDotEvent(vec2 Pos0, vec2 Pos1, int LifeSpan)
{
	CCroyaGameContext::LaserDotState State;
	State.m_Pos0 = Pos0;
	State.m_Pos1 = Pos1;
	State.m_LifeSpan = LifeSpan;
	State.m_SnapID = Server()->SnapNewID();

	m_LaserDots.add(State);
}

void CCroyaGameContext::SendChatTarget(int To, const char* pText)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = MSGFLAG_VITAL;
	Msg.m_ClientID = -1;
	Msg.m_TargetID = To;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
}

int CCroyaGameContext::GetHumanCount() const
{
	int HumanCount = 0;
	for (CPlayer* each : m_apPlayers) {
		if (each) {
			CCharacter* pChr = each->GetCharacter();
			if (pChr && pChr->IsHuman())
				HumanCount++;
		}
	}
	return HumanCount;
}

int CCroyaGameContext::GetZombieCount() const
{
	int ZombiesCount = 0;
	for (CPlayer* each : m_apPlayers) {
		if (each) {
			CCharacter* pChr = each->GetCharacter();
			if (pChr && pChr->IsZombie())
				ZombiesCount++;
		}
	}
	return ZombiesCount;
}
