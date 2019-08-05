/* (c) InfCroya contributors. See licence.txt in the root of the distribution for more information. */
#ifndef INFCROYA_GAME_CROYA_GAMECONTEXT_H
#define INFCROYA_GAME_CROYA_GAMECONTEXT_H
#include <game/server/gamecontext.h>
#include <array>
#include <vector>
#include <unordered_map>

class CCroyaGameContext : public CGameContext
{

public:
	
	//constructor / destructor
	CCroyaGameContext();
	~CCroyaGameContext() override;
	
	//overrides
	void OnTick() override;
	
};

#endif
