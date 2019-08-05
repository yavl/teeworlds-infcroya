#include "croya-gamecontext.h"
#include "game/server/player.h"

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
