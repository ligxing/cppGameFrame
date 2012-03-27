/*
 * Scene.h
 *
 *  Created on: 2012-3-20
 *    
 */

#ifndef GAME_SCENE_H_
#define GAME_SCENE_H_

#include <mysdk/base/Common.h>
#include <mysdk/protocol/kabu/codec/PacketBase.h>
#include <map>

class BgPlayer;
class Scene
{
public:
	Scene();
	~Scene();

	bool addPlayer(BgPlayer* player);
	bool removePlayer(BgPlayer* player);

	void broadMsg(PacketBase& op);

	bool init();
	void run(uint32 curTime);
	void shutdown();

	std::map<int32, BgPlayer*>& getPlayerMgr()
	{
		return playerMgr_;
	}
	BgPlayer* getPlayer(int playerId);
private:
	int32 sceneId_;
	std::map<int32, BgPlayer*> playerMgr_;
private:
	DISALLOW_COPY_AND_ASSIGN(Scene);
};
#endif /* SCENE_H_ */
