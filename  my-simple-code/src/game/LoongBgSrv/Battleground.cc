/*
 * Battleground.cc
 *
 *  Created on: 2012-3-20
 *    
 */

#include <game/LoongBgSrv/Battleground.h>

#include <game/LoongBgSrv/BgPlayer.h>
#include <game/LoongBgSrv/BattlegroundState.h>
#include <game/LoongBgSrv/LoongBgSrv.h>
#include <game/LoongBgSrv/StateBattlegroundCountDown.h>
#include <game/LoongBgSrv/StateBattlegroundExit.h>
#include <game/LoongBgSrv/StateBattlegroundRun.h>
#include <game/LoongBgSrv/StateBattlegroundStart.h>
#include <game/LoongBgSrv/StateBattlegroundWait.h>
#include <game/LoongBgSrv/JoinTimesMgr.h>
#include <game/LoongBgSrv/base/PetBase.h>
#include <game/LoongBgSrv/Util.h>
#include <game/LoongBgSrv/protocol/GameProtocol.h>

Battleground::Battleground():
	id_(0),
	bFirst_(true),
	bgResult_(KNONE_BGRESULT),
	pState_(NULL)
{
	init();
}

Battleground::~Battleground()
{
	if (pState_)
	{
		delete pState_;
	}
}

void Battleground::setSrv(LoongBgSrv* srv)
{
	pSrv_ = srv;
}

void Battleground::init()
{
	bFirst_ = true;
	for (int i = 0; i < BgUnit::kCOUNT_TEAM; i++)
	{
		teamNum_[i] = 0;
	}
	teamNum_[0] = 99;
	bgResult_ = KNONE_BGRESULT;
	scene_.init();
}

void Battleground::shtudown()
{
	scene_.shutdown();
}

void Battleground::setId(int32 Id)
{
	id_ = Id;
}

int32 Battleground::getId()
{
	return id_;
}

bool Battleground::addBgPlayer(BgPlayer* player, BgUnit::TeamE team)
{
	if (!player) return false;

	LOG_DEBUG << " Battleground::addBgPlayer - playerId: " << player->getId()
							<< " team: " << team
							<< " num: " << static_cast<int>(teamNum_[team])
							<< " playerptr: " << player
							<< " bgid: " << this->getId();

	if (teamNum_[team] >= sMaxTeamNum)
	{
		//  告诉客户端, 这个队伍已经满人啦！不能进这个队伍啦
		return false;
	}

	if (bFirst_)
	{
		switchWaitState();
		bFirst_ = false;
	}
	teamNum_[team]++;
	// 如果他已经在莫个战场上了 就把他从莫个战场上移除
	if (player->isInBg())
	{
		player->close();
	}
	player->setBgId(getId());
	player->setTeam(team);

	char logcontent[128];
	snprintf(logcontent, sizeof(logcontent) - 1, "[30191003][%d][%d][%d]", player->getId(), this->getId(), team);
	Log(logcontent);

	TellPhpBattleInfo();
	// 在激战中算一个次 参加了战场
	if (this->getState() == BattlegroundState::BGSTATE_RUN)
	{
		sJoinTimesMgr.incJoinTimes(player->getId());
	}
	return scene_.addPlayer(player);
}

bool Battleground::removeBgPlayer(BgPlayer* player, BgUnit::TeamE team)
{
	if (!player) return false;

	LOG_DEBUG << " Battleground::removeBgPlayer - playerId: " << player->getId()
							<< " team: " << team
							<< " num: " << static_cast<int>(teamNum_[team])
							<< " playerbgId: " << player->getBgId()
							<< " playerptr: " << player
							<< " bgid: " << this->getId();

	teamNum_[team]--;
	player->setBgId(0);
	player->setTeam(BgUnit::kNONE_TEAM);

	bool res = scene_.removePlayer(player);
	if (res)
	{
		TellPhpBattleInfo();
	}
	return res;
}

void Battleground::run(int64 curTime)
{
	scene_.run(curTime);
	if (pState_)
	{
		pState_->run(curTime);
	}
}

bool Battleground::getBgInfo(PacketBase& op)
{
	op.putInt32(teamNum_[BgUnit::kBlack_TEAM]);
	op.putInt32(teamNum_[BgUnit::kWhite_TEAM]);
	op.putInt32(getState());
	op.putInt32(getLeftTime());
	scene_.serialize(op);
	//
	return true;
}

BattlegroundState::BgStateE Battleground::getState()
{
	if (pState_)
	{
		return pState_->getState();
	}
	return BattlegroundState::BGSTATE_NONE;
}

uint32 Battleground::getLeftTime()
{
	if (pState_)
	{
		return pState_->getLeftTime();
	}
	return 999999;
}

BgUnit* Battleground::getTargetUnit(int32 playerId, int32 uintType)
{
	enum
	{
		PLAYER_UNITTYPE	= 	0,
		BLACK_BUILDING_UNITTYPE = 1,
		WHITE_BUILDING_UNITTYPE = 2,
		FLOWER_UNITTYPE = 3,
	};

	BgUnit* target = NULL;
	if (uintType == PLAYER_UNITTYPE)
	{
		target = scene_.getPlayer(playerId);
	}
	else if (uintType == BLACK_BUILDING_UNITTYPE)
	{
		return &scene_.getBlackBuilding();
	}
	else if (uintType == WHITE_BUILDING_UNITTYPE)
	{
		return &scene_.getWhiteBuilding();
	}
	else if (uintType == FLOWER_UNITTYPE)
	{
		target = scene_.getFlower(playerId);
	}
	return target;
}

bool Battleground::isFull()
{
	if (teamNum_[BgUnit::kBlack_TEAM] >= sMaxTeamNum && teamNum_[BgUnit::kWhite_TEAM]  >= sMaxTeamNum)
	{
		return true;
	}
	return false;
}

bool Battleground::isEmpty()
{
	if (teamNum_[BgUnit::kBlack_TEAM] <= sMinTeamNum && teamNum_[BgUnit::kWhite_TEAM] <= sMinTeamNum)
	{
		return true;
	}
	return false;
}

void Battleground::TellClientCloseBg(int32 state)
{
	PacketBase op(client::OP_MEMBER_INSUFFICIENT, state); // state 0 代表正常退出战场  1 代表敌方人数不足退出战场
	scene_.broadMsg(op);
}

void Battleground::broadMsg(PacketBase& op)
{
	scene_.broadMsg(op);
}

bool Battleground::isGameOver()
{
	if (scene_.getBlackBuilding().isDead() && scene_.getWhiteBuilding().isDead())
	{
		bgResult_ = KDRAW_BGRESULT;
		return true;
	}

	if (scene_.getBlackBuilding().isDead())
	{
		bgResult_ = KWHITE_BGRESULT;
		return true;
	}

	if (scene_.getWhiteBuilding().isDead())
	{
		bgResult_ = KBLACK_BGRESULT;
		return true;
	}
	return false;
}

void Battleground::settlement()
{
	//统计战场结果 发放战斗奖励
	std::map<int32, BgPlayer*>& playerMgr = scene_.getPlayerMgr();
	std::map<int32, BgPlayer*>::iterator iter;

	PacketBase op(client::OP_SETTLEMENT, 0);
	op.putInt32(bgResult_);

	if (pState_)
	{
		char logcontent[128];
		snprintf(logcontent, sizeof(logcontent) - 1, "[30191004][%d][%d][%d]",this->getId(), static_cast<int>(bgResult_), pState_->getStateTimeLimit() - pState_->getLeftTime());
		Log(logcontent);
	}
	LOG_INFO << "[BattleResult] =========================== result: " << bgResult_
						<< " id: " << id_;
	int32 num = 0;
	PacketBase hotelop(hotel::OP_GET_BATTLEGROUND_AWARD, 0);
	for(iter = playerMgr.begin(); iter != playerMgr.end(); iter++)
	{
			BgPlayer* player = iter->second;
			if (player)
			{
				hotelop.putInt32(id_);
				hotelop.putInt32(id_);
				player->serializeResult(op, bgResult_, hotelop);
				num++;
			}
	}
	LOG_INFO << "[BattleResult] =========================== result: " << bgResult_
						<< " id: " << id_;
	hotelop.setParam(num);
	op.setParam(num);
	scene_.broadMsg(op);
	// 把战斗结果 通知给hotel 让hotel把相应的奖励加给玩家哦
	sendToHotel(hotelop);
}

void Battleground::incBgPlayerTimes()
{
	std::map<int32, BgPlayer*>& playerMgr = scene_.getPlayerMgr();
	std::map<int32, BgPlayer*>::iterator iter;
	for(iter = playerMgr.begin(); iter != playerMgr.end(); iter++)
	{
		BgPlayer* player = iter->second;
		if (player)
		{
			sJoinTimesMgr.incJoinTimes(player->getId());
		}
	}
}

void Battleground::tellClientBgState()
{
	PacketBase op(client::OP_TELLCLIENT_STATE, 0);
	op.putInt32(getState());
	op.putInt32(getLeftTime());
	scene_.broadMsg(op);

	TellPhpBattleInfo();
}

bool Battleground::haveOtherTeamEmpty()
{
	if (teamNum_[BgUnit::kBlack_TEAM] == sMinTeamNum || teamNum_[BgUnit::kWhite_TEAM] == sMinTeamNum)
	{
		return true;
	}
	return false;
}

void Battleground::TellPhpBattleInfo()
{
	if (pSrv_)
	{
		pSrv_->TellPhpBattleInfo(getId());
	}
}

void Battleground::sendToHotel(PacketBase& pb)
{
	if (pSrv_)
	{
		pSrv_->sendToHotel(pb);
	}
}

void Battleground::setBattlegroundState(BattlegroundState* state)
{
	if (pState_)
	{
		pState_->end();
		// 释放掉 前面状态的内存
		delete pState_;
		pState_ = NULL;
	}
	pState_ = state;
	if (pState_)
	{
		pState_->start();
	}
}

void Battleground::switchWaitState()
{
	BattlegroundState* state = new StateBattlegroundWait(this);
	setBattlegroundState(state);
}

void Battleground::switchCountDownState()
{
	BattlegroundState* state = new StateBattlegroundCountDown(this);
	setBattlegroundState(state);
}

void Battleground::switchStartState()
{
	BattlegroundState* state = new StateBattlegroundStart(this);
	setBattlegroundState(state);
}

void Battleground::switchRunState()
{
	BattlegroundState* state = new StateBattlegroundRun(this);
	setBattlegroundState(state);
}

void Battleground::switchExitState()
{
	BattlegroundState* state = new StateBattlegroundExit(this);
	setBattlegroundState(state);
}

void Battleground::closeBattleground()
{
	LOG_DEBUG << "Battleground::closeBattleground - battlegroundId: " << this->getId();
	if (pState_)
	{
		pState_->end();
		// 释放掉 前面状态的内存
		delete pState_;
		pState_ = NULL;
	}

	std::map<int32, BgPlayer*>& playerMgr = scene_.getPlayerMgr();
	std::map<int32, BgPlayer*>::iterator iter;
	for(iter = playerMgr.begin(); iter != playerMgr.end(); iter++)
	{
		BgPlayer* player = iter->second;
		if (player)
		{
			player->setBgId(0);
			player->setTeam(BgUnit::kNONE_TEAM);
		}
	}

	// 重新初始化一下战场哦
	init();

	TellPhpBattleInfo();
}
