package codo.controllers
{
   import codo.common.*;
   import codo.constants.*;
   import codo.constants.Table.*;
   import codo.objects.*;
   import codo.services.CharacterLoadoutService;
   import codo.services.MarketService;
   import codo.services.MatchService;
   import codo.services.WeaponLoadoutService;
   import codo.utils.ScaleformUtil;
   import codo.utils.SoundUtil;
   import codo.utils.TimeUtil;
   import components.CodoUIComponent;
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.events.TimerEvent;
   import flash.filters.GlowFilter;
   import flash.utils.Timer;
   
   public class LobbyController extends BaseController
   {
      private var _roomObj:RoomObj = new RoomObj();
      
      private var _roomMembers:Array = [];
      
      private var _dedicatedServers:Array = [];
      
      private var _ac130Types:Array;
      
      private var _currentSelectedWeaponSlot:Object = null;
      
      private var _currentSelectedCharacter:RoomMemberObj = null;
      
      private var _isCareerStatsSet:Boolean = false;
      
      private var _isRenewingAvatar:uint = 0;
      
      private var _parentGlowFilter:GlowFilter = new GlowFilter(16711680);
      
      private var _childGlowFilter:GlowFilter = new GlowFilter(16711680);
      
      private var _glowFilterTimers:Array = [];
      
      private var _stageMouseX:Number = 0;
      
      private var _stageMouseY:Number = 0;
      
      public function LobbyController()
      {
         super();
      }
      
      override protected function onInitialized() : void
      {
         if(ScaleformUtil.isTempContentEnabled_2())
         {
            CFunctionContainerCommon.setdvar("teamCameraDistance","160");
            CFunctionContainerCommon.setdvar("teamCameraMaxYaw","100");
         }
         this.addSyncDP(SyncDataID.IS_ROOM_HOST,this.isRoomHost);
         this.addSyncDP(SyncDataID.IS_IN_MY_ROOM,this.isInMyRoom);
         this.addSyncDP(SyncDataID.IS_LEAVING_ROOM,this.isLeavingRoom);
         this.addSyncDP(SyncDataID.IS_GAME_COUNTING_DOWN,this.isGameStartCountingDown);
         this.addSyncDP(SyncDataID.ROOM_INFO,this.getRoomInfo);
         this.addSyncDP(SyncDataID.ROOM_MEMBER_OBJ,this.getRoomMemberObj);
         this.addSyncDP(SyncDataID.ROOM_MEMBER_OBJ_BYID,this.getRoomMemberObjByID);
         this.bind(AsyncTaskID.ROOM_INFO,this.bind_roomInfo);
         this.bind(AsyncTaskID.ROOM_MEMBER_INFO,this.bind_roomMemberInfo);
         this.bind(AsyncTaskID.LEAVE_ROOM,this.bind_onRoomLeft);
         this.bind(AsyncTaskID.LIST_DEDICATED_SERVER,this.bind_dedicatedServerDP);
         this.listen(BCEvent.EVT_CHANGE_READY_STATE,this.onChangeReadyStateHandle);
         this.listen(BCEvent.EVT_LIST_DEDICATED_SERVER,this.onListDedicatedServerHandle);
         this.listen(BCEvent.EVT_RANKED_MATCH_CHANGE_STATUS,this.onRankedMatchStatusChangedHandle);
         this.listen(BCEvent.CHANGE_TEAM,this.onChangeTeamHandle);
         this.listen(BCEvent.CHANGE_ROOM_INFO,this.onChangeRoomInfoHandle);
         this.listen(BCEvent.VOTE_MAP,this.onVoteMapHandle);
         this.listen(BCEvent.SET_PILOT,this.onSetPilotHandle);
         this.listen(BCEvent.START_MATCH,this.onStartMatchHandle);
         this.listen(BCEvent.EVT_GOTO_AAR,this.onGotoAARHandle);
         this.listen(BCEvent.LOBBY_REFRESH_AVATAR,this.onRefreshAvatarHandle);
         this.listen(BCEvent.LOBBY_REFRESH_WEAPON,this.onRefreshWeaponHandle);
         this.listen(BCEvent.LOBBY_RENEW_AVATAR,this.onRenewAvatarHandle);
         this.listen(BCEvent.LOBBY_RENEW_WEAPON,this.onRenewWeaponHandle);
         this.listen(BCEvent.LOBBY_WEAPON_SLOT_CLICK,this.onWeaponSlotClickedHandle);
         this.listen(BCEvent.LOBBY_CHANGE_WEAPON_LOADOUT,this.onWeaponLoadoutChangedHandle);
         this.listen(BCEvent.EVT_CONTROLLER_CHECK_AVATAR_REFRESH_IN_TEAM,this.onControllerCheckAvatarRefreshInTeam);
         this.listen(BCEvent.EVT_REFRESH_UI_AFTER_INVENTORY_CHANGE,this.onInventoryChangeHandle);
      }
      
      public function cppCanSelectCharacter(character:String, xPos:int, yPos:int) : Boolean
      {
         if(this._currentSelectedCharacter != null && this._currentSelectedCharacter.id == character)
         {
            return false;
         }
         var isRoomPage:Boolean = this.getSyncDP(SyncDataID.CURRENT_PAGE_ID) == PageID.INROOM;
         var isBlur:Boolean = CFunctionContainerCommon.Dvar_GetFloat("r_blur") > 0;
         var canClose:Boolean = this.getSyncDP(SyncDataID.CAN_CLOSE_LOBBY_CHARACTER_VIEW);
         var isCyborg:Boolean = this.getSyncDP(SyncDataID.IS_CYBORG);
         var isCoop:Boolean = this._roomObj.currentMatchMode.id == "coop";
         var clicked:Boolean = false;
         var stage:DisplayObjectContainer = this.getSyncDP(SyncDataID.GET_STAGE);
         if(stage != null)
         {
            this._stageMouseX = stage.mouseX < 0 || stage.mouseY < 0 ? xPos : stage.mouseX;
            this._stageMouseY = stage.mouseX < 0 || stage.mouseY < 0 ? yPos : stage.mouseY;
            clicked = this.hitOnAUIComponent(stage);
         }
         var can:Boolean = isRoomPage && !isBlur && (this._currentSelectedCharacter == null || canClose) && !isCyborg && !clicked && !isCoop;
         Logger.print("LobbyController:::cppCanSelectCharacter can: " + can);
         return can;
      }
      
      public function cppSelectCharacterStart() : void
      {
         this.broadcast(BCEvent.LOBBY_TOGGLE_INROOM_ELEMENTS,false);
         PageManagerMain.getInstance().flyOffAllCurrentView([CodoPageViewDB.VIEW_LOBBY_CHARACTER,CodoPageViewDB.VIEW_IN_ROOM,CodoPageViewDB.VIEW_LOGO]);
      }
      
      public function cppSelectCharacter(character:String, forceHide:Boolean, selected:Boolean, xPos:int, yPos:int) : Boolean
      {
         if(selected && this._currentSelectedCharacter != null && this._currentSelectedCharacter.id == character)
         {
            return true;
         }
         var hasPopup:Boolean = this.getSyncDP(SyncDataID.HAS_POPUP);
         var hasSystemMsg:Boolean = this.getSyncDP(SyncDataID.IS_SYSTEM_MSG_SHOWING);
         var passToView:Boolean = (selected || this.getSyncDP(SyncDataID.CAN_CLOSE_LOBBY_CHARACTER_VIEW) || forceHide) && !hasPopup && !hasSystemMsg;
         if(passToView)
         {
            Logger.print("LobbyController:::cppSelectCharacter character/selected/xPos/yPos: " + character + "/" + selected + "/" + xPos + "/" + yPos);
            if(selected)
            {
               this._currentSelectedCharacter = this.getRoomMemberObjByID(character);
               if(Boolean(this._currentSelectedCharacter))
               {
                  this.refreshCareerStats(this._currentSelectedCharacter,true);
                  this.refreshMyWeaponLoadout(this._currentSelectedCharacter);
                  this.refreshLobbyCharacter(this._currentSelectedCharacter);
               }
            }
            else
            {
               this._currentSelectedCharacter = null;
               this.broadcast(BCEvent.LOBBY_TOGGLE_INROOM_ELEMENTS,true);
               PageManagerMain.getInstance().flyInAllCurrentView();
            }
            this.broadcast(BCEvent.LOBBY_SHOW_CHARACTER_VIEW,{
               "show":selected,
               "xPos":xPos,
               "yPos":yPos
            });
            SoundUtil.playSound(selected ? SE_ID.UI_3D_LOBBY_OPEN_MENU : SE_ID.UI_3D_LOBBY_CLOSE_MENU);
         }
         return passToView;
      }
      
      public function cppSetCharacterAvatarSlots(playerID:String, avatarSlots:Object, weaponSlots:Object) : void
      {
         var prop:String = null;
         var slotObj:Object = null;
         var itemID:int = 0;
         var roomMemberObj:RoomMemberObj = this.getRoomMemberObjByID(playerID);
         if(roomMemberObj != null)
         {
            if(roomMemberObj.isMe)
            {
               this.refreshMyAvatar(roomMemberObj);
               this.refreshMyWeaponLoadout(roomMemberObj);
            }
            else
            {
               roomMemberObj.avatarItemSlots = avatarSlots;
               for(prop in roomMemberObj.avatarItemSlots)
               {
                  slotObj = roomMemberObj.avatarItemSlots[prop];
                  itemID = !!slotObj.hasOwnProperty("item_id") ? int(slotObj.item_id) : 0;
                  if(itemID > 0)
                  {
                     slotObj.itemDat = MarketService.getInstance().getItemInfoObj(itemID);
                  }
                  if(slotObj.itemDat == null)
                  {
                     slotObj.itemDat = new ItemObj();
                  }
               }
               roomMemberObj.weaponItemSlots = weaponSlots;
               WeaponLoadoutService.getInstance().fillWeaponLoadout(roomMemberObj.weaponItemSlots);
            }
            if(this._currentSelectedCharacter != null && this._currentSelectedCharacter.id == roomMemberObj.id)
            {
               this.refreshLobbyCharacter(roomMemberObj);
            }
         }
      }
      
      public function cppRefreshRoomMemberStats(stats:Object) : void
      {
         var i:int = 0;
         if(stats == null)
         {
            return;
         }
         var roomMemberObj:RoomMemberObj = this.getRoomMemberObjByID(stats.id);
         if(roomMemberObj != null)
         {
            if(roomMemberObj.careerStats == null)
            {
               roomMemberObj.careerStats = new CareerStatsObj();
               roomMemberObj.careerStats.currentLevelInfo = new PlayerLevelObj();
            }
            roomMemberObj.careerStats.recentScores.splice(0);
            i = 0;
            while(i < stats.recentScores.length)
            {
               roomMemberObj.careerStats.recentScores.push(stats.recentScores[i] / 100);
               i++;
            }
            roomMemberObj.careerStats.averageScore = stats.averageScore;
            roomMemberObj.careerStats.matchResult = {
               "win":stats.matchResult[0],
               "lose":stats.matchResult[1],
               "draw":stats.matchResult[2],
               "flee":stats.matchResult[3]
            };
            roomMemberObj.careerStats.currentLevelInfo.currentLevel = roomMemberObj.level;
            roomMemberObj.careerStats.currentLevelInfo.currentLevelName = this.getSyncDP(SyncDataID.LEVEL_NAME,[roomMemberObj.level]);
            roomMemberObj.careerStats.customStats = [{
               "label":LocalizationCareer.CAREER_TIMEPLAYED,
               "value":TimeUtil.timeToString(stats.timePlayed)
            },{
               "label":LocalizationCareer.CAREER_SCORE,
               "value":stats.scoreEarned.toString()
            },{
               "label":LocalizationCareer.CAREER_HITRATE,
               "value":(stats.accuracy / 100).toFixed(2) + "%"
            },{
               "label":LocalizationCareer.CAREER_WINSTREAK,
               "value":stats.winStreak.toString()
            },{
               "label":LocalizationCareer.CAREER_KILLSTREAK,
               "value":stats.killStreak.toString()
            },{
               "label":LocalizationCareer.CAREER_KDRATIO,
               "value":(stats.kdRatio / 100).toFixed(2)
            }];
            roomMemberObj.careerStats.isBootCamp = roomMemberObj.level <= Common.BOOT_CAMP_THRESHOLD;
            roomMemberObj.careerStats.bootCampEXP = stats.totalEarnedXP;
            roomMemberObj.careerStats.bootCampScore = stats.score;
            this.broadcastCareerStats(roomMemberObj);
         }
      }
      
      public function cppSetRoomRTTParams(params:Object) : void
      {
         if(params == null)
         {
            return;
         }
         this.broadcast(BCEvent.LOBBY_RTT_PARAMS,params);
      }
      
      public function cppSetMemberVote(member:String, vote:int) : void
      {
         var roomMemberObj:RoomMemberObj = this.getRoomMemberObjByID(member);
         if(Boolean(roomMemberObj))
         {
            roomMemberObj.vote = vote;
            Logger.print("LobbyController:::cppSetMemberVote roomMemberObj.name/roomMemberObj.vote: " + roomMemberObj.name + "/" + roomMemberObj.vote);
            this.refreshRoomVoting();
         }
      }
      
      public function cppSetMemberReadyState(member:String, ready:Boolean) : void
      {
         var roomMemberObj:RoomMemberObj = this.getRoomMemberObjByID(member);
         if(Boolean(roomMemberObj))
         {
            // BEGIN shared Quick native Ready cycle evidence
            do
            {
               if(this._roomObj == null || this._roomObj["_localQuickSharedLineage"] != true || !CFunctionContainer.IsRankedMatch() || this._roomObj.name == null || String(this._roomObj.name).length == 0 || roomMemberObj == null || roomMemberObj["_localQuickNativeRoleKnown"] != true || roomMemberObj["_localQuickNativeMemberID"] != member || String(roomMemberObj.id) != member)
               {
                  break;
               }
               var gateHost:Object = null;
               var gateHostCount:int = 0;
               for each(var gateCandidate in this._roomMembers)
               {
                  if(gateCandidate != null && gateCandidate["_localQuickNativeRoleKnown"] == true && gateCandidate["_localQuickNativeMemberID"] == String(gateCandidate.id) && typeof gateCandidate.realName == "string" && gateCandidate.realName.length > 0 && gateCandidate.realName == String(this._roomObj.name))
                  {
                     gateHost = gateCandidate;
                     gateHostCount++;
                  }
               }
               if(gateHostCount != 1 || Boolean(this.getSyncDP(SyncDataID.IS_ME,[gateHost])) != Boolean(CFunctionContainer.IsRoomHost()))
               {
                  break;
               }
               if(String(gateHost.id) == member)
               {
                  if(!ready && (this._roomObj["_localQuickReadyCycleOpen"] != true || this._roomObj["_localQuickReadyCycleHost"] != member))
                  {
                     this._roomObj["_localQuickReadyCycle"] = uint(this._roomObj["_localQuickReadyCycle"]) + 1;
                     this._roomObj["_localQuickReadyCycleHost"] = member;
                     this._roomObj["_localQuickReadyCycleOpen"] = true;
                  }
                  else if(ready)
                  {
                     this._roomObj["_localQuickReadyCycleOpen"] = false;
                  }
               }
               var gateCycle:uint = uint(this._roomObj["_localQuickReadyCycle"]);
               if(gateCycle > 0 && this._roomObj["_localQuickReadyCycleHost"] == String(gateHost.id))
               {
                  // Only an actual native Ready callback stamps this cycle. A
                  // cached true in a room snapshot cannot acknowledge new results.
                  roomMemberObj["_localQuickReadyCycle"] = ready ? gateCycle : 0;
               }
            }
            while(false);
            // END shared Quick native Ready cycle evidence
            roomMemberObj.isReady = ready;
            if(roomMemberObj["_localQuickNativeRoleKnown"] == true && roomMemberObj["_localQuickNativeMemberID"] == member && String(roomMemberObj.id) == member)
            {
               roomMemberObj["_localQuickNativeReady"] = ready;
            }
            if(this._roomObj != null && this._roomObj["_localQuickSharedLineage"] == true && !ready && this._roomObj.name != null && String(this._roomObj.name).length > 0)
            {
               var canonicalHost:Object = null;
               var canonicalHostCount:int = 0;
               for each(var candidate in this._roomMembers)
               {
                  if(candidate != null && candidate["_localQuickNativeRoleKnown"] == true && candidate["_localQuickNativeMemberID"] == String(candidate.id) && typeof candidate.realName == "string" && candidate.realName.length > 0 && candidate.realName == String(this._roomObj.name))
                  {
                     canonicalHost = candidate;
                     canonicalHostCount++;
                  }
               }
               if(canonicalHostCount == 1 && String(canonicalHost.id) == member && Boolean(this.getSyncDP(SyncDataID.IS_ME,[canonicalHost])) == Boolean(CFunctionContainer.IsRoomHost()))
               {
                  this._roomObj["_localQuickHostFalseSeen"] = true;
                  // Keep the actual host false edge through a delayed results
                  // acknowledgement. A fresh return clears the previous edge.
                  var resultContext:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
                  var resultOwner:String = CFunctionContainerCommon.GetPlayerID();
                  var resultToken:String = resultContext.length == 3 && resultContext[0] == "v1" ? "v1|" + resultOwner + "|" + String(resultContext[2]) : "";
                  if(resultToken.length > 0 && CFunctionContainer.IsRankedMatch() && CFunctionContainerCommon.Dvar_GetString("codol_quick_return") == resultToken)
                  {
                     CFunctionContainerCommon.setdvar("codol_quick_return_host_false",resultToken);
                  }
               }
            }
            if(this.getSyncDP(SyncDataID.IS_ME,[roomMemberObj]))
            {
               this._roomObj.forces.isReady = ready;
            }
            if(this._roomObj.forces.showChangeForceBtn)
            {
               this._roomObj.forces.enableChangeForceBtn = !ready;
            }
            this.broadcast(BCEvent.ROOM_FORCES,{
               "team":roomMemberObj.team,
               "forces":this._roomObj.forces
            });
         }
      }
      
      private function onListDedicatedServerHandle(cEvt:ParaEvent) : void
      {
         this.request(AsyncTaskID.LIST_DEDICATED_SERVER,cEvt.myPara);
      }
      
      private function isInMyRoom(cUser:Object) : Boolean
      {
         var i:int = 0;
         while(i < this._roomMembers.length)
         {
            if(this._roomMembers[i].id == cUser.id)
            {
               return true;
            }
            i++;
         }
         return false;
      }
      
      private function isLeavingRoom() : Boolean
      {
         return MatchService.getInstance().isLeavingRoom();
      }
      
      private function isRoomHost() : Boolean
      {
         return Boolean(CFunctionContainer.IsRoomHost());
      }
      
      private function isGameStartCountingDown() : Boolean
      {
         return CFunctionContainerCommon.Dvar_GetFloat("dw_gameStartCountingDown") > 0;
      }
      
      private function onChangeReadyStateHandle(cEvt:ParaEvent) : void
      {
         if(!this.isRoomInfoReady())
         {
            return;
         }
         var readyToSet:Boolean = !this.isReady();
         if(this.isRoomHost())
         {
            this._roomObj.forces.enableChangeMapBtn = !this._roomObj.isPlayList && !readyToSet;
            this.broadcast(BCEvent.ROOM_STATE_CHANGED,this._roomObj);
         }
         CFunctionContainer.SetReadyState(readyToSet);
      }
      
      private function isReady() : Boolean
      {
         return Boolean(CFunctionContainer.GetReadyState());
      }
      
      private function listAC130Type() : Array
      {
         if(this._ac130Types == null)
         {
            this._ac130Types = [{
               "id":0,
               "name":LocalizationRoom.AC130_TYPE_GROUND
            },{
               "id":1,
               "name":LocalizationRoom.AC130_TYPE_PILOT
            }];
         }
         return this._ac130Types;
      }
      
      private function isAC130(map:Object) : Boolean
      {
         if(map == null || map.id.length <= 0)
         {
            return false;
         }
         return map.id == "coop_spec_mp_hunted_spring" || map.id == "coop_spec1_mp_suburbs";
      }
      
      private function getRoomInfo() : RoomObj
      {
         if(this.isRoomInfoReady() && this._roomObj["_localQuickSharedLineage"] == true && this._roomObj["_localQuickVoteFinal"] != null && int(this._roomObj["_localQuickVoteFinal"]) == -1 && CFunctionContainer.IsRankedMatch() && this._roomObj.forces != null && this._roomObj.forces.votes != null)
         {
            var quickCardPlaylist:PlayListObj = this.getCurrentPlayList();
            var quickCardVotes:RoomVoteObj = this._roomObj.forces.votes;
            if(quickCardPlaylist != null && quickCardPlaylist.name == "Quick Match" && quickCardPlaylist.type == 1 && quickCardPlaylist.minPartySize == 1 && quickCardPlaylist.maxPartySize == 6 && quickCardPlaylist.maxRoomSize == 12 && (quickCardVotes.previousMap == null || quickCardVotes.nextMap != null && quickCardVotes.previousMap.id == quickCardVotes.nextMap.id))
            {
               this.refreshRoomVoting(false);
            }
         }
         return this.isRoomInfoReady() ? this._roomObj : null;
      }
      
      private function getRoomMemberObj(name:String) : RoomMemberObj
      {
         var i:int = 0;
         while(i < this._roomMembers.length)
         {
            if(this._roomMembers[i].name == name)
            {
               return this._roomMembers[i];
            }
            i++;
         }
         return null;
      }
      
      private function getRoomMemberObjByID(id:String) : RoomMemberObj
      {
         var i:int = 0;
         while(i < this._roomMembers.length)
         {
            if(this._roomMembers[i].id == id)
            {
               return this._roomMembers[i];
            }
            i++;
         }
         return null;
      }
      
      private function isBootCampPlayList() : Boolean
      {
         var playListObj:PlayListObj = null;
         if(this._roomObj.isPlayList)
         {
            playListObj = this.getCurrentPlayList();
            return playListObj != null && playListObj.combatTrainingType == CombatType.CT && playListObj.leagueID == 0;
         }
         return false;
      }
      
      private function bind_roomInfo(cRes:Object) : void
      {
         this.initRoom(cRes);
      }
      
      private function bind_roomMemberInfo(cRes:Object) : void
      {
         this.initRoom(cRes,false);
      }
      
      private function bind_onRoomLeft(cRes:Object) : void
      {
         var directSurvival:Boolean = CFunctionContainerCommon.Dvar_GetString("codol_survival_direct_home") == "1";
         CFunctionContainerCommon.setdvar("codol_survival_direct_home","0");
         this.resetRoom();
         this._isRenewingAvatar = 0;
         this._roomMembers.splice(0);
         this._currentSelectedWeaponSlot = null;
         this._currentSelectedCharacter = null;
         MatchService.getInstance().isInRoom = false;
         this.broadcast(BCEvent.EVT_ON_ROOM_LEFT);
         this.broadcast(BCEvent.LOADOUT_TRANSITION);
         var currentPage:String = this.getSyncDP(SyncDataID.CURRENT_PAGE_ID);
         if(currentPage == null || currentPage == PageID.INROOM)
         {
            this.gotoPage(directSurvival ? CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW : (this.getSyncDP(SyncDataID.CURRENT_MATCH_TYPE) == QuickMatchType.COOP ? CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP : CodoPageViewDB.PAGE_MAIN_HOME));
         }
      }
      
      private function bind_dedicatedServerDP(cRes:Object) : void
      {
         this._dedicatedServers = cRes.data;
         this.broadcast(BCEvent.EVT_ON_DEDICATED_SERVER_LISTED,this._dedicatedServers);
      }
      
      private function onSetPilotHandle(cEvt:ParaEvent) : void
      {
         if(cEvt.myPara == null)
         {
            return;
         }
         var isPilot:Boolean = cEvt.myPara.id == 1;
         if(isPilot != CFunctionContainer.IsPilot())
         {
            CFunctionContainer.SetPilot(isPilot);
         }
      }
      
      private function onStartMatchHandle(cEvt:ParaEvent) : void
      {
         var msg:String = null;
         if(!this.isRoomInfoReady())
         {
            return;
         }
         var max:uint = this._roomObj.currentMaxPlayers;
         var min:uint = this._roomObj.currentMinPlayers;
         var current:uint = this._roomMembers.length;
         if(this.getSyncDP(SyncDataID.IS_SHIP_BUILD))
         {
            msg = current < min ? LocalizationRoom.ERROR_ROOM_NOT_ENOUGH : (current > max ? LocalizationRoom.ERROR_ROOM_TOO_MANY : "");
            if(msg.length > 0)
            {
               this.showMsg(msg,MsgBtnCount.ONE);
               return;
            }
         }
         this.request(AsyncTaskID.START_MATCH,cEvt.myPara);
      }
      
      private function onChangeTeamHandle(cEvt:ParaEvent) : void
      {
         if(this._roomObj != null && this._roomObj["_localQuickSharedLineage"] == true && this._roomObj.currentMaxPlayers == 12 && CFunctionContainer.IsRankedMatch())
         {
            return;
         }
         var team:uint = CFunctionContainerCommon.GetTeamType();
         team = uint(team == 0 ? 1 : 0);
         CFunctionContainer.ChangeTeam(team);
      }
      
      private function onVoteMapHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainer.VoteMap(int(cEvt.myPara));
      }
      
      private function onChangeRoomInfoHandle(cEvt:ParaEvent) : void
      {
         var mode:String = null;
         var isAvailableLevel:Boolean = false;
         var levels:Array = null;
         var i:int = 0;
         if(cEvt.myPara.data == null || !this.isRoomHost())
         {
            return;
         }
         var changedInfo:Object = {};
         if(cEvt.myPara.type == "map")
         {
            if(this._roomObj.currentMap.id == cEvt.myPara.data.id)
            {
               return;
            }
            mode = this._roomObj.currentMatchMode.id;
            changedInfo.map = cEvt.myPara.data.id;
            changedInfo.minPlayer = int(mode == "coop" || mode == "zm" ? cEvt.myPara.data.minPlayer : this._roomObj.currentMinPlayers);
            changedInfo.maxPlayer = int(mode == "coop" || mode == "zm" ? cEvt.myPara.data.maxPlayer : this._roomObj.currentMaxPlayers);
            isAvailableLevel = false;
            levels = this.getSyncDP(SyncDataID.LIST_DIFFICULTY,[{"id":changedInfo.map}]);
            i = 0;
            while(i < levels.length)
            {
               if(this._roomObj.currentLevel == levels[i].id)
               {
                  isAvailableLevel = true;
                  break;
               }
               i++;
            }
            isAvailableLevel ||= this._roomObj.currentMatchMode.id == "combat";
            changedInfo.difficulty = int(isAvailableLevel ? this._roomObj.currentLevel : (levels.length > 0 ? levels[0].id : 1));
         }
         else if(cEvt.myPara.type == "difficulty")
         {
            if(this._roomObj.forces.showDifficulty)
            {
               if(this._roomObj.currentLevel == cEvt.myPara.data)
               {
                  return;
               }
               changedInfo.difficulty = int(cEvt.myPara.data);
            }
         }
         else if(cEvt.myPara.type == "mode")
         {
            if(this._roomObj.forces.showMode)
            {
               if(this._roomObj.currentMatchMode.id == cEvt.myPara.data)
               {
                  return;
               }
               changedInfo.mode = cEvt.myPara.data;
            }
         }
         CFunctionContainer.ChangeRoomInfo(changedInfo,cEvt.myPara.type);
      }
      
      private function onRankedMatchStatusChangedHandle(cEvt:ParaEvent) : void
      {
         switch(cEvt.myPara.status)
         {
            case RankedMatchStatus.READY:
            case RankedMatchStatus.READY_IN_ROOM:
               this.setFinalVoting();
         }
      }
      
      private function onRefreshAvatarHandle(cEvt:ParaEvent) : void
      {
         Logger.print("LobbyController:::onRefreshAvatarHandle");
         this.refreshMyAvatar(this.me);
         this.broadcastAvatarRefreshInTeam();
      }
      
      private function onRefreshWeaponHandle(cEvt:ParaEvent) : void
      {
         this.refreshMyWeaponLoadout(this.me);
         this.broadcastAvatarRefreshInTeam();
      }
      
      private function onRenewAvatarHandle(cEvt:ParaEvent) : void
      {
         Logger.print("LobbyController:::onRenewAvatarHandle");
         this._isRenewingAvatar = 1;
      }
      
      private function onRenewWeaponHandle(cEvt:ParaEvent) : void
      {
         Logger.print("LobbyController:::onRenewWeaponHandle");
         this._isRenewingAvatar = 2;
      }
      
      private function onControllerCheckAvatarRefreshInTeam(cEvt:ParaEvent) : void
      {
         this.broadcastAvatarRefreshInTeam();
      }
      
      private function onInventoryChangeHandle(cEvt:ParaEvent) : void
      {
         if(this._isRenewingAvatar == 1)
         {
            Logger.print("LobbyController:::onRenewAvatarHandle");
            this._isRenewingAvatar = 0;
            this.refreshMyAvatar(this.me);
            this.broadcast(BCEvent.LOBBY_REFRESH_CHARACTER_SLOT);
         }
         else if(this._isRenewingAvatar == 2)
         {
            this._isRenewingAvatar = 0;
            this.refreshMyWeaponLoadout(this.me);
            this.broadcast(BCEvent.LOBBY_REFRESH_WEAPON);
         }
      }
      
      private function broadcastAvatarRefreshInTeam() : void
      {
         var faction:uint = Common.LOADOUT_TYPE_TF;
         if(this.getSyncDP(SyncDataID.IS_CYBORG))
         {
            faction = Common.LOADOUT_TYPE_ZM;
         }
         else
         {
            faction = Boolean(this.me) ? this.me.team : faction;
         }
         this.broadcast(BCEvent.EVT_CHECK_AVATAR_REFRESH_IN_TEAM,faction);
      }
      
      private function onWeaponSlotClickedHandle(cEvt:ParaEvent) : void
      {
         this._currentSelectedWeaponSlot = cEvt.myPara.slot;
         this.refreshMyWeaponItems(this._currentSelectedWeaponSlot);
      }
      
      private function onWeaponLoadoutChangedHandle(cEvt:ParaEvent) : void
      {
         this.refreshMyWeaponLoadout(this.me);
      }
      
      private function setFinalVoting() : void
      {
         if(this._roomObj.forces == null)
         {
            return;
         }
         var playListObj:PlayListObj = this.getCurrentPlayList();
         if(playListObj != null && playListObj.combatTrainingType > 0)
         {
            return;
         }
         this._roomObj.forces.votes.enable = false;
         this.broadcast(BCEvent.REFRESH_MAP_VOTING,this._roomObj);
         if(this.isRoomHost())
         {
            if(this._roomObj["_localQuickSharedLineage"] == true && this._roomObj["_localQuickVoteFinal"] != null && int(this._roomObj["_localQuickVoteFinal"]) >= 0 && playListObj != null && int(this._roomObj["_localQuickVoteFinal"]) < playListObj.entries.length)
            {
               CFunctionContainer.SetFinalVoting(int(this._roomObj["_localQuickVoteFinal"]));
            }
            else
            {
               CFunctionContainer.SetFinalVoting(this._roomObj.forces.votes.entry);
            }
         }
      }
      
      private function isRoomInfoReady() : Boolean
      {
         return this._roomObj.name.length > 0 && this._roomMembers.length > 0;
      }
      
      private function initRoom(res:Object, isRoom:Boolean = true) : void
      {
         if(res == null || res.data == null)
         {
            return;
         }
         if(this._roomObj.forces == null)
         {
            this._roomObj.forces = new RoomInfoObj();
            this._roomObj.forces.votes = new RoomVoteObj();
         }
         if(isRoom)
         {
            this.initRoomInfo(res.data);
         }
         else
         {
            this.initRoomMembers(res.data);
         }
         this.refreshRoomMaps();
         this.refreshRoomStatus();
         this.dispatchRoom();
      }
      
      private function initRoomInfo(obj:Object) : void
      {
         this._roomObj.name = obj.name.length > 0 ? obj.name : " ";
         this._roomObj.index = obj.index;
         this._roomObj.password = obj.password;
         this._roomObj.isPlayList = obj.isPlayList;
         this._roomObj.matchType = obj.matchType;
         this._roomObj.currentLevel = obj.currentLevel;
         this._roomObj.currentMatchMode.id = obj.currentMatchMode == null ? "" : obj.currentMatchMode.id || "";
         this._roomObj.currentMinPlayers = this._roomObj.currentMatchMode.id == "combat" ? 1 : uint(obj.currentMinPlayers);
         this._roomObj.currentMaxPlayers = obj.currentMaxPlayers;
         this._roomObj.mapType = MapType.NONE;
         if(Boolean(obj.isPlayList))
         {
            this._roomObj.leagueID = uint(obj.leagueID) || 0;
            this._roomObj.playListID = int(obj.playListID) || 0;
            this._roomObj.playListName = obj.playListName || "";
            this._roomObj.playListDescription = obj.playListDescription || "";
            this._roomObj.playListNextEntry = int(obj.playListNextEntry) || 0;
            this._roomObj.playListLastEntry = int(obj.playListLastEntry) || 0;
         }
         else
         {
            this._roomObj.currentMap.name = obj.currentMap.name || "";
            this._roomObj.currentMap.id = obj.currentMap.id || "";
            this._roomObj.currentMap.description = obj.currentMap.description || "";
            this._roomObj.currentMap.imageURL = obj.currentMap.imageURL || "";
            this._roomObj.currentMatchMode.name = obj.currentMatchMode.name || "";
            this._roomObj.currentMatchMode.description = obj.currentMatchMode.description || "";
            this._roomObj.mapType = this.getSyncDP(SyncDataID.GET_MAP_TYPE,[this._roomObj.currentMap.id]);
         }
         this.broadcast(BCEvent.ROOM_INFO_ONLY,this._roomObj);
      }
      
      private function initRoomMembers(members:Array) : void
      {
         var isRankedMatch:Boolean = false;
         var oldRoomMembers:Array = null;
         var roomMemberObj:RoomMemberObj = null;
         var i:int = 0;
         var j:int = 0;
         var k:int = 0;
         if(ScaleformUtil.isScaleform)
         {
            isRankedMatch = this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH);
            oldRoomMembers = this._roomMembers.concat();
            this._roomMembers.splice(0);
            i = 0;
            while(i < members.length)
            {
               roomMemberObj = null;
               j = 0;
               while(j < oldRoomMembers.length)
               {
                  if(oldRoomMembers[j].id == members[i].id)
                  {
                     roomMemberObj = oldRoomMembers[j];
                     this.refreshRoomMember(roomMemberObj,members[i]);
                     break;
                  }
                  j++;
               }
               if(roomMemberObj == null)
               {
                  roomMemberObj = new RoomMemberObj(members[i]);
               }
               roomMemberObj["_localQuickNativeMemberID"] = String(members[i].id);
               roomMemberObj["_localQuickNativeRoleKnown"] = typeof members[i].isHost == "boolean" && typeof members[i].isReady == "boolean" && String(members[i].id).length > 0 && String(roomMemberObj.id) == String(members[i].id);
               roomMemberObj["_localQuickNativeHost"] = roomMemberObj["_localQuickNativeRoleKnown"] == true && members[i].isHost == true;
               roomMemberObj["_localQuickNativeReady"] = roomMemberObj["_localQuickNativeRoleKnown"] == true && members[i].isReady == true;
               if(roomMemberObj["_localQuickNativeReady"] != true)
               {
                  roomMemberObj["_localQuickReadyCycle"] = 0;
               }
               if(isRankedMatch)
               {
                  roomMemberObj.isHost = false;
                  roomMemberObj.isReady = false;
               }
               roomMemberObj.isOnline = true;
               roomMemberObj.isInMyParty = this.getSyncDP(SyncDataID.IS_IN_MY_PARTY,[roomMemberObj]);
               roomMemberObj.isMe = this.getSyncDP(SyncDataID.IS_ME,[roomMemberObj]);
               if(roomMemberObj.isMe)
               {
                  this.refreshMyAvatar(roomMemberObj);
                  this.refreshCareerStats(roomMemberObj);
               }
               this.setMenuSelections(roomMemberObj);
               this._roomMembers.push(roomMemberObj);
               i++;
            }
         }
         else if(this._roomMembers.length <= 0)
         {
            k = 1;
            while(k <= 18)
            {
               this._roomMembers.push(new RoomMemberObj({
                  "id":k,
                  "name":"player " + k,
                  "team":k % 2,
                  "isOnline":Math.round(k + Math.random()) % 2 == 0
               }));
               k++;
            }
         }
      }
      
      private function refreshRoomMaps() : void
      {
         var map:MapObj = null;
         var i:int = 0;
         if(this._roomObj.name.length <= 0)
         {
            return;
         }
         if(this._roomObj.isPlayList)
         {
            this._roomObj.forces.maps.splice(0);
            if(this._roomObj.forces.votes.votedMap != null)
            {
               this._roomObj.forces.maps.push(this._roomObj.forces.votes.votedMap);
            }
            else
            {
               map = this.getPlayListEntryMap(this._roomObj.playListID,this._roomObj.playListNextEntry);
               if(map != null)
               {
                  this._roomObj.forces.maps.push(map);
               }
            }
         }
         else if(this.isRoomHost())
         {
            this._roomObj.forces.maps.splice(0);
            this._roomObj.forces.maps = this.getSyncDP(SyncDataID.LIST_SAME_TYPE_MAP,[this._roomObj.currentMap.id]);
            i = 0;
            while(i < this._roomObj.forces.maps.length)
            {
               if(this._roomObj.currentMap.id == this._roomObj.forces.maps[i].id)
               {
                  this._roomObj.forces.selectedMapIndex = i;
                  break;
               }
               i++;
            }
         }
         else
         {
            this._roomObj.forces.maps.splice(0);
            this._roomObj.forces.maps.push(this.getSyncDP(SyncDataID.MAP_OBJ,[this._roomObj.currentMap.id]));
            this._roomObj.forces.selectedMapIndex = 0;
         }
      }
      
      private function refreshRoomStatus() : void
      {
         var j:int = 0;
         var isSpecOps:Boolean = false;
         var isCyborgs:Boolean = false;
         if(!this.isRoomInfoReady())
         {
            return;
         }
         var mode:String = this._roomObj.currentMatchMode != null ? this._roomObj.currentMatchMode.id : "";
         var isCoop:Boolean = mode == "coop";
         var isZm:Boolean = mode == "zm";
         var isDm:Boolean = mode == "dm" || mode == "gun" || mode == "oic" || mode == "shrp" || mode == "infect";
         var isCT:Boolean = mode == "combat";
         var forceInfo:RoomInfoObj = this._roomObj.forces;
         var playListObj:PlayListObj = this.getCurrentPlayList();
         var showBoth:Boolean = mode != "gun" && mode != "oic" && mode != "shrp" && mode != "infect" && (this._roomObj.isPlayList && (!isCT || playListObj != null && playListObj.combatTrainingType > 0) || !isCT && !isCoop && !isZm && !isDm);
         var party:Boolean = !this.getSyncDP(SyncDataID.IS_IN_PARTY) || this.getSyncDP(SyncDataID.IS_PARTY_HOST);
         forceInfo.force0.splice(0);
         forceInfo.force1.splice(0);
         forceInfo.forceMaxPlayer = isCoop || isZm || isDm || isCT ? int(this._roomObj.currentMaxPlayers) : int(this._roomObj.currentMaxPlayers / 2);
         var i:int = 0;
         while(i < this._roomMembers.length)
         {
            if(mode == "gun" || mode == "oic" || mode == "shrp" || mode == "infect" || this._roomMembers[i].team == 0)
            {
               forceInfo.force0.push(this._roomMembers[i]);
            }
            else
            {
               forceInfo.force1.push(this._roomMembers[i]);
            }
            if(this.getSyncDP(SyncDataID.IS_ME,[this._roomMembers[i]]))
            {
               forceInfo.isInForce0 = mode == "gun" || mode == "oic" || mode == "shrp" || mode == "infect" || this._roomMembers[i].team == 0;
               forceInfo.isHost = this.isRoomHost();
               forceInfo.isReady = this._roomMembers[i].isReady;
               isSpecOps = this.getSyncDP(SyncDataID.GET_MAP_COOP_TYPE,[this._roomObj.currentMap]) == CoopMapType.SPEC_OPS;
               isCyborgs = this.getSyncDP(SyncDataID.IS_CYBORG);
               forceInfo.showPilot = !this._roomObj.isPlayList && forceInfo.isHost && isSpecOps && this.isAC130(this._roomObj.currentMap);
               if(forceInfo.showPilot)
               {
                  if(forceInfo.pilots.length <= 0)
                  {
                     forceInfo.pilots = this.listAC130Type();
                     forceInfo.selectedPilotIndex = 0;
                  }
                  j = 0;
                  while(j < forceInfo.pilots.length)
                  {
                     if(forceInfo.pilots[j].id == this._roomMembers[i].pilot)
                     {
                        forceInfo.selectedPilotIndex = j;
                        break;
                     }
                     j++;
                  }
               }
               forceInfo.showDifficulty = !this._roomObj.isPlayList && forceInfo.isHost && (isSpecOps || isCyborgs);
               if(forceInfo.showDifficulty)
               {
                  forceInfo.difficulties = this.getSyncDP(SyncDataID.LIST_DIFFICULTY,[this._roomObj.currentMap]);
                  forceInfo.selectedDifficultyIndex = 0;
                  j = 0;
                  while(j < forceInfo.difficulties.length)
                  {
                     if(forceInfo.difficulties[j].id == this._roomObj.currentLevel)
                     {
                        forceInfo.selectedDifficultyIndex = j;
                        break;
                     }
                     j++;
                  }
               }
               forceInfo.showMode = !this._roomObj.isPlayList && forceInfo.isHost && !isCT && !isZm && !isCoop;
               if(forceInfo.showMode)
               {
                  forceInfo.modes = this.getSyncDP(SyncDataID.LIST_MODE,[MatchType.PVP]);
                  forceInfo.selectedModeIndex = 0;
                  j = 0;
                  while(j < forceInfo.modes.length)
                  {
                     if(forceInfo.modes[j].id == this._roomObj.currentMatchMode.id)
                     {
                        forceInfo.selectedModeIndex = j;
                        break;
                     }
                     j++;
                  }
               }
            }
            i++;
         }
         forceInfo.showMapAlbum = true;
         forceInfo.enableChangeMapBtn = !this._roomObj.isPlayList && forceInfo.isHost && !forceInfo.isReady;
         this.refreshRoomVoting(false);
         if(forceInfo.force0.length > 1)
         {
            forceInfo.force0.sortOn("name",Array.CASEINSENSITIVE);
         }
         if(forceInfo.force1.length > 1)
         {
            forceInfo.force1.sortOn("name",Array.CASEINSENSITIVE);
         }
         forceInfo.showForce0 = forceInfo.force0.length > 0 || showBoth;
         forceInfo.showForce1 = forceInfo.force1.length > 0 || showBoth;
         forceInfo.showReadyBtn = ScaleformUtil.isScaleform ? !(this._roomObj.isPlayList && !isCT) : true;
         forceInfo.showChangeForceBtn = forceInfo.showForce0 && forceInfo.showForce1 && !this._roomObj.isPlayList;
         forceInfo.enableChangeForceBtn = !forceInfo.isReady && (forceInfo.isInForce0 ? forceInfo.force1.length < forceInfo.forceMaxPlayer : forceInfo.force0.length < forceInfo.forceMaxPlayer) && party;
         if(isDm && forceInfo.showForce0 && forceInfo.showForce1)
         {
            forceInfo.forceMaxPlayer = this._roomObj.currentMaxPlayers / 2;
         }
         this._roomObj.currentPlayers = this._roomMembers.length;
         if(this._roomObj.currentPlayers > this._roomObj.currentMaxPlayers)
         {
            this.showMsg(LocalizationRoom.ERROR_ROOM_TOO_MANY,MsgBtnCount.ONE);
            if(forceInfo.showReadyBtn)
            {
               forceInfo.enableReadyBtn = false;
            }
         }
      }
      
      private function refreshRoomVoting(fromVote:Boolean = true) : void
      {
         if(this._roomObj.forces == null)
         {
            return;
         }
         var playListObj:PlayListObj = this.getCurrentPlayList();
         if(playListObj == null)
         {
            return;
         }
         var voteObj:RoomVoteObj = this._roomObj.forces.votes;
         var voteSecondEntry:int = this._roomObj.playListLastEntry;
         var voteNextEntry:int = this._roomObj.playListNextEntry;
         var managedTwoMaps:Boolean = playListObj.name == "Quick Match" && playListObj.type == 1 && playListObj.minPartySize == 1 && playListObj.maxPartySize == 6 && playListObj.maxRoomSize == 12 && this._roomObj["_localQuickSharedLineage"] == true && CFunctionContainer.IsRankedMatch();
         if(managedTwoMaps && playListObj.entries != null && voteNextEntry >= 0 && voteNextEntry < playListObj.entries.length)
         {
            var voteNextMap:String = String(playListObj.entries[voteNextEntry].map);
            if(voteSecondEntry < 0 || voteSecondEntry >= playListObj.entries.length || String(playListObj.entries[voteSecondEntry].map) == voteNextMap)
            {
               voteSecondEntry = -1;
               var voteScan:int = 1;
               while(voteScan < playListObj.entries.length && voteSecondEntry < 0)
               {
                  var voteCandidate:int = (voteNextEntry + voteScan) % playListObj.entries.length;
                  if(String(playListObj.entries[voteCandidate].map) != voteNextMap)
                  {
                     voteSecondEntry = voteCandidate;
                  }
                  voteScan++;
               }
            }
         }
         var managedNewBallot:Boolean = playListObj.name == "Quick Match" && playListObj.type == 1 && playListObj.minPartySize == 1 && playListObj.maxPartySize == 6 && playListObj.maxRoomSize == 12 && this._roomObj["_localQuickSharedLineage"] == true && this._roomObj["_localQuickVoteFinal"] != null && int(this._roomObj["_localQuickVoteFinal"]) == -1 && CFunctionContainer.IsRankedMatch() && !this.getSyncDP(SyncDataID.IS_RANKED_MATCH_READY);
         if(!fromVote && voteObj.show && !managedNewBallot)
         {
            return;
         }
         this.resetRoomVoting();
         voteObj.show = this._roomObj.isPlayList && this._roomObj.currentMatchMode.id != "combat" && playListObj.combatTrainingType == 0;
         Logger.print("LobbyController:::refreshRoomVoting voteObj.show: " + voteObj.show);
         if(!voteObj.show)
         {
            return;
         }
         voteObj.enable = !this.getSyncDP(SyncDataID.IS_RANKED_MATCH_READY);
         var i:int = 0;
         while(i < this._roomMembers.length)
         {
            if(this._roomMembers[i].vote == VoteMapType.NEXT)
            {
               ++voteObj.next;
            }
            else if(this._roomMembers[i].vote == VoteMapType.PREVIOUS)
            {
               ++voteObj.previous;
            }
            else if(this._roomMembers[i].vote == VoteMapType.RANDOM)
            {
               ++voteObj.random;
            }
            i++;
         }
         var next:int = voteObj.next;
         var previous:int = voteObj.previous;
         var random:int = voteObj.random;
         var max:int = Math.max(next,previous,random);
         if(max < playListObj.minMapValidVotes)
         {
            voteObj.entry = this._roomObj.playListNextEntry;
         }
         else if(previous > random)
         {
            voteObj.entry = previous > next ? voteSecondEntry : this._roomObj.playListNextEntry;
         }
         else
         {
            voteObj.entry = random > next ? -1 : this._roomObj.playListNextEntry;
         }
         Logger.print("LobbyController:::refreshRoomVoting entry: " + voteObj.entry);
         voteObj.nextMap = this.getPlayListEntryMap(this._roomObj.playListID,this._roomObj.playListNextEntry);
         if(voteObj.nextMap != null)
         {
            voteObj.nextMap.voted = false;
            Logger.print("LobbyController:::refreshRoomVoting next map: " + voteObj.nextMap.id);
         }
         voteObj.previousMap = this.getPlayListEntryMap(this._roomObj.playListID,voteSecondEntry);
         if(voteObj.previousMap != null)
         {
            voteObj.previousMap.voted = false;
            Logger.print("LobbyController:::refreshRoomVoting previous map: " + voteObj.previousMap.id);
         }
         if(voteObj.randomMap == null)
         {
            voteObj.randomMap = new MapObj({"name":LocalizationRoom.MAP_VOTING_RANDOM});
         }
         voteObj.randomMap.voted = false;
         if(voteObj.entry == this._roomObj.playListNextEntry && voteObj.nextMap != null)
         {
            voteObj.votedMap = voteObj.nextMap;
         }
         else if(voteObj.entry == voteSecondEntry && voteObj.previousMap != null)
         {
            voteObj.votedMap = voteObj.previousMap;
         }
         else
         {
            voteObj.votedMap = voteObj.randomMap;
         }
         Logger.print("LobbyController:::refreshRoomVoting vote map: " + voteObj.votedMap);
         if(voteObj.votedMap != null)
         {
            voteObj.votedMap.voted = true;
            this._roomObj.forces.maps.splice(0);
            this._roomObj.forces.maps.push(voteObj.votedMap);
            if(fromVote)
            {
               this.broadcast(BCEvent.REFRESH_MAP_VOTING,this._roomObj);
            }
         }
      }
      
      private function getPlayListEntryMap(playListID:int, idxEntry:int) : MapObj
      {
         var playListObj:PlayListObj = this.getCurrentPlayList();
         if(playListObj == null)
         {
            return null;
         }
         if(idxEntry >= 0 && idxEntry < playListObj.entries.length)
         {
            return this.getSyncDP(SyncDataID.MAP_OBJ,[playListObj.entries[idxEntry].map]);
         }
         return null;
      }
      
      private function getCurrentPlayList() : PlayListObj
      {
         return this.getSyncDP(SyncDataID.PLAY_LIST_OBJ,[this._roomObj.playListID]);
      }
      
      private function resetRoom() : void
      {
         this._roomObj.name = "";
         this._roomObj.index = 0;
         this._roomObj.currentMap.id = "";
         this._roomObj.currentMatchMode.id = "";
         if(this._roomObj.forces != null)
         {
            this._roomObj.forces.maps.splice(0);
            this._roomObj.forces.difficulties.splice(0);
            this._roomObj.forces.pilots.splice(0);
            this.resetRoomVoting();
         }
      }
      
      private function resetRoomVoting() : void
      {
         if(this._roomObj.forces != null)
         {
            this._roomObj.forces.votes.show = false;
            this._roomObj.forces.votes.enable = false;
            this._roomObj.forces.votes.next = 0;
            this._roomObj.forces.votes.previous = 0;
            this._roomObj.forces.votes.random = 0;
            this._roomObj.forces.votes.entry = 0;
            this._roomObj.forces.votes.previousMap = null;
            this._roomObj.forces.votes.nextMap = null;
            this._roomObj.forces.votes.votedMap = null;
         }
      }
      
      private function get me() : RoomMemberObj
      {
         var i:int = 0;
         while(i < this._roomMembers.length)
         {
            if(Boolean(this._roomMembers[i].isMe))
            {
               return this._roomMembers[i];
            }
            i++;
         }
         return null;
      }
      
      private function refreshRoomMember(dest:RoomMemberObj, src:Object) : void
      {
         dest.name = src.name;
         dest.realName = src.realName;
         dest.team = src.team;
         dest.vote = src.vote;
         dest.pilot = src.pilot;
         dest.isReady = src.isReady;
         dest.isHost = src.isHost;
         if(src.badgeObj != null)
         {
            dest.badgeObj.id = src.badgeObj.id;
            dest.badgeObj.name = src.badgeObj.name;
            dest.badgeObj.description = src.badgeObj.description;
            dest.badgeObj.imageURL = src.badgeObj.imageURL;
         }
         if(src.callSignObj != null)
         {
            dest.callSignObj.cardTitle = src.callSignObj.cardTitle;
            dest.callSignObj.cardIconURL = src.callSignObj.cardIconURL;
            dest.callSignObj.cardBackgroundURL = src.callSignObj.cardBackgroundURL;
         }
      }
      
      private function filterOnTeam(dp:Array, faction:int) : void
      {
         var i:int = dp.length - 1;
         while(i >= 0)
         {
            switch(faction)
            {
               case 0:
                  if(dp[i].enableInTF141 == false)
                  {
                     dp.splice(i,1);
                  }
                  break;
               case 1:
                  if(dp[i].enableInSC == false)
                  {
                     dp.splice(i,1);
                  }
                  break;
            }
            i--;
         }
      }
      
      private function refreshMyAvatar(roomMemberObj:RoomMemberObj) : void
      {
         var prop:String = null;
         var slotObj:Object = null;
         var slotName:String = null;
         if(roomMemberObj == null || !roomMemberObj.isMe)
         {
            return;
         }
         CFunctionContainer.CL_SetCurrentLoadout(int(roomMemberObj.team));
         roomMemberObj.avatarItemSlots = CharacterLoadoutService.getInstance().getActiveCharacterLoadout();
         roomMemberObj.avatarInventoryItems = {};
         var headID:int = roomMemberObj.avatarItemSlots.heaSlot == null ? 0 : int(roomMemberObj.avatarItemSlots.heaSlot.item_id);
         roomMemberObj.isCustomizable = headID > 0 ? CFunctionContainer.CL_IsCharacterCustomizable(headID) as Boolean : true;
         for(prop in roomMemberObj.avatarItemSlots)
         {
            slotObj = roomMemberObj.avatarItemSlots[prop];
            slotName = !!slotObj.hasOwnProperty("nameid") ? slotObj.nameid : "";
            if(slotName.length > 0)
            {
               roomMemberObj.avatarInventoryItems[slotName] = MarketService.getInstance().GetProductList(slotName,"inventory",true);
               this.filterOnTeam(roomMemberObj.avatarInventoryItems[slotName],roomMemberObj.team);
            }
         }
      }
      
      private function refreshMyWeaponLoadout(roomMemberObj:RoomMemberObj) : void
      {
         if(roomMemberObj == null || !roomMemberObj.isMe)
         {
            return;
         }
         roomMemberObj.weaponItemSlots = WeaponLoadoutService.getInstance().getCurrentWeaponLoadout();
         this.refreshMyWeaponItems(this._currentSelectedWeaponSlot);
         if(this._currentSelectedCharacter != null && this._currentSelectedCharacter.id == roomMemberObj.id)
         {
            this.broadcast(BCEvent.LOBBY_SET_WEAPON_LOADOUT,roomMemberObj);
         }
      }
      
      private function refreshMyWeaponItems(weaponSlot:Object) : void
      {
         var category:String = null;
         var values:Array = null;
         if(weaponSlot == null)
         {
            return;
         }
         var roomMemberObj:RoomMemberObj = this.me;
         if(roomMemberObj != null)
         {
            category = weaponSlot.nameid;
            if(category.length <= 0)
            {
               return;
            }
            values = category.split("_");
            if(values[1] == "primary")
            {
               WeaponLoadoutService.getInstance().currentWeaponID = roomMemberObj.weaponItemSlots.primaryWeapSlot.item_id;
            }
            else if(values[1] == "secondary")
            {
               WeaponLoadoutService.getInstance().currentWeaponID = roomMemberObj.weaponItemSlots.secondaryWeapSlot.item_id;
            }
            roomMemberObj.weaponInventoryItems = MarketService.getInstance().getItemListByCategory(values[0],"inventory",true);
            if(roomMemberObj.weaponInventoryItems == null)
            {
               roomMemberObj.weaponInventoryItems = [];
            }
            roomMemberObj.weaponInventoryItems.sortOn(["equippable","equipLevel","requirementLevel","name"],[Array.DESCENDING,Array.NUMERIC,Array.NUMERIC,Array.CASEINSENSITIVE]);
         }
      }
      
      private function refreshCareerStats(roomMemberObj:RoomMemberObj, select:Boolean = false) : void
      {
         if(roomMemberObj == null)
         {
            return;
         }
         if(roomMemberObj.isMe)
         {
            if(roomMemberObj.careerStats == null)
            {
               roomMemberObj.careerStats = this.getSyncDP(SyncDataID.CAREER_STATS);
               if(!this._isCareerStatsSet)
               {
                  this._isCareerStatsSet = true;
                  CFunctionContainer.SetStatsForRoom(roomMemberObj.careerStats);
               }
            }
         }
         else if(select && roomMemberObj.careerStats == null)
         {
            CFunctionContainer.QueryRoomTeammateStats(roomMemberObj.id);
         }
      }
      
      private function refreshLobbyCharacter(roomMemberObj:RoomMemberObj) : void
      {
         if(roomMemberObj != null)
         {
            this.setBindData(AsyncTaskID.LOBBY_CHARACTER_DATA,roomMemberObj);
         }
      }
      
      private function broadcastCareerStats(roomMemberObj:RoomMemberObj) : void
      {
         if(roomMemberObj != null)
         {
            this.broadcast(BCEvent.LOBBY_SET_CHARACTER_STATS,roomMemberObj);
         }
      }
      
      private function dispatchRoom() : void
      {
         if(!this.isRoomInfoReady())
         {
            return;
         }
         this.broadcast(BCEvent.ROOM_INFO,this._roomObj);
      }
      
      private function onGotoAARHandle() : void
      {
         this.gotoPage(CodoPageViewDB.PAGE_POPUP_AAR);
      }
      
      private function setMenuSelections(roomMemberObj:RoomMemberObj) : void
      {
         if(roomMemberObj == null)
         {
            return;
         }
         var isFriend:Boolean = this.getSyncDP(SyncDataID.IS_FRIEND,[roomMemberObj]);
         roomMemberObj.menuSelections.splice(0);
         if(!this.isBootCampPlayList())
         {
            if(roomMemberObj.isMe ? this.getSyncDP(SyncDataID.IS_NOTIFIED,[UINotifications.WEAPON_CUSTOMIZATION]) : true)
            {
               roomMemberObj.menuSelections.push({
                  "label":LocalizationRoom.LOBBY_CHARACTER_MENU_WEAPON_LOADOUT,
                  "enable":true,
                  "needStandBy":false,
                  "action":LobbyConstants.ACTION_WEAPON_LOADOUT,
                  "reserve":{}
               });
            }
            if(roomMemberObj.isMe ? this.getSyncDP(SyncDataID.IS_NOTIFIED,[UINotifications.AVATAR_CUSTOMIZATION]) : true)
            {
               roomMemberObj.menuSelections.push({
                  "label":LocalizationRoom.LOBBY_CHARACTER_OUTFIT,
                  "enable":true,
                  "needStandBy":false,
                  "action":LobbyConstants.ACTION_CHARACTER_OUTFIT,
                  "reserve":{}
               });
            }
         }
         roomMemberObj.menuSelections.push({
            "label":LocalizationRoom.LOBBY_CHARACTER_PLAYER_STATS,
            "enable":true,
            "needStandBy":false,
            "action":LobbyConstants.ACTION_PLAYER_STATS,
            "reserve":{}
         });
         roomMemberObj.menuSelections.push({
            "label":LocalizationRoom.LOBBY_CHARACTER_ADD_FRIEND,
            "enable":!roomMemberObj.isMe && !isFriend,
            "needStandBy":true,
            "action":LobbyConstants.ACTION_ADD_FRIEND,
            "reserve":roomMemberObj
         });
         roomMemberObj.menuSelections.push({
            "label":LocalizationRoom.LOBBY_CHARACTER_SEND_MESSAGE,
            "enable":!roomMemberObj.isMe,
            "needStandBy":false,
            "action":LobbyConstants.ACTION_SEND_MESSAGE,
            "reserve":roomMemberObj
         });
      }
      
      private function isBaseView(container:DisplayObjectContainer) : Boolean
      {
         return container is BaseView || container is BaseSubElement;
      }
      
      private function hasBaseView(container:DisplayObjectContainer) : Boolean
      {
         return this.isBaseView(container) || this.hasClass(container,BaseView,BaseSubElement);
      }
      
      private function hasUIComponent(container:DisplayObjectContainer) : Boolean
      {
         return this.hasClass(container,CodoUIComponent);
      }
      
      private function hasClass(container:DisplayObjectContainer, class1:Class, class2:Class = null) : Boolean
      {
         var i:int = 0;
         var child:DisplayObject = null;
         var found:Boolean = false;
         if(container != null)
         {
            i = 0;
            while(i < container.numChildren)
            {
               child = container.getChildAt(i++);
               if(class1 != null && child is class1 || class2 != null && child is class2)
               {
                  found = true;
                  break;
               }
            }
            if(!found)
            {
               i = 0;
               while(i < container.numChildren)
               {
                  child = container.getChildAt(i++);
                  if(child is DisplayObjectContainer)
                  {
                     found = this.hasClass(child as DisplayObjectContainer,class1,class2);
                     if(found)
                     {
                        break;
                     }
                  }
               }
            }
         }
         return found;
      }
      
      private function hasDisplayObjectContainer(container:DisplayObjectContainer) : Boolean
      {
         var child:DisplayObject = null;
         var i:int = 0;
         var found:Boolean = false;
         if(container != null)
         {
            i = 0;
            while(i < container.numChildren)
            {
               child = container.getChildAt(i++);
               if(child is DisplayObjectContainer)
               {
                  found = true;
                  break;
               }
            }
         }
         return found;
      }
      
      private function hitOnAUIComponent(container:DisplayObjectContainer) : Boolean
      {
         var i:int = 0;
         var child:DisplayObject = null;
         var grandchild:DisplayObject = null;
         var j:int = 0;
         var hit:Boolean = false;
         if(container != null)
         {
            i = 0;
            while(i < container.numChildren)
            {
               child = container.getChildAt(i++);
               if(child.visible)
               {
                  if(child.hitTestPoint(this._stageMouseX,this._stageMouseY))
                  {
                     if(child is CodoUIComponent)
                     {
                        if(!this.hasBaseView(child as DisplayObjectContainer))
                        {
                           if(!this.hasUIComponent(child as DisplayObjectContainer))
                           {
                              j = 0;
                              while(j < (child as DisplayObjectContainer).numChildren)
                              {
                                 grandchild = (child as DisplayObjectContainer).getChildAt(j);
                                 if(grandchild.hitTestPoint(this._stageMouseX,this._stageMouseY))
                                 {
                                    if(grandchild.visible)
                                    {
                                       hit = true;
                                       if(this.getSyncDP(SyncDataID.IS_DEV_BUILD))
                                       {
                                          this.addGlowFilterTimer(child);
                                       }
                                       break;
                                    }
                                 }
                                 j++;
                              }
                              if(hit)
                              {
                                 break;
                              }
                           }
                           else
                           {
                              hit = this.hitOnAUIComponent(child as DisplayObjectContainer);
                              if(hit)
                              {
                                 break;
                              }
                           }
                        }
                        else
                        {
                           hit = this.hitOnAUIComponent(child as DisplayObjectContainer);
                           if(hit)
                           {
                              break;
                           }
                        }
                     }
                     else if(child is DisplayObjectContainer)
                     {
                        hit = this.hitOnAUIComponent(child as DisplayObjectContainer);
                        if(hit)
                        {
                           break;
                        }
                     }
                  }
               }
            }
         }
         return hit;
      }
      
      private function addGlowFilterTimer(container:*, child:* = null) : void
      {
         if(container == null && child == null)
         {
            return;
         }
         var i:int = 0;
         while(i < this._glowFilterTimers.length)
         {
            if(this._glowFilterTimers[i].container == container && this._glowFilterTimers[i].child == child)
            {
               this._glowFilterTimers[i].timer.reset();
               this._glowFilterTimers[i].timer.start();
               return;
            }
            i++;
         }
         var containerFilters:Array = container && Boolean(container.filters) ? container.filters : [];
         var containerHasFilter:Boolean = containerFilters.length > 0;
         containerFilters.push(this._parentGlowFilter);
         if(container)
         {
            container.filters = containerFilters;
         }
         var childFilters:Array = child && Boolean(child.filters) ? child.filters : [];
         var childHasFilter:Boolean = childFilters.length > 0;
         childFilters.push(this._childGlowFilter);
         if(child)
         {
            child.filters = childFilters;
         }
         var timer:Timer = new Timer(500,1);
         timer.addEventListener(TimerEvent.TIMER_COMPLETE,this.onGlowFilterTimerComplete);
         timer.start();
         this._glowFilterTimers.push({
            "container":container,
            "containerHasFilter":containerHasFilter,
            "child":child,
            "childHasFilter":childHasFilter,
            "timer":timer
         });
      }
      
      private function onGlowFilterTimerComplete(event:TimerEvent) : void
      {
         var j:int = 0;
         var filters:Array = null;
         var glowFilterTimer:Object = null;
         var i:int = this._glowFilterTimers.length - 1;
         while(i >= 0)
         {
            if(this._glowFilterTimers[i].timer == event.target)
            {
               glowFilterTimer = this._glowFilterTimers[i];
               if(Boolean(glowFilterTimer.container))
               {
                  if(Boolean(glowFilterTimer.containerHasFilter))
                  {
                     filters = glowFilterTimer.container.filters;
                     j = filters.length - 1;
                     while(j >= 0)
                     {
                        if(filters[j] == this._parentGlowFilter)
                        {
                           filters.splice(j,1);
                           glowFilterTimer.container.filters = filters;
                           break;
                        }
                        j--;
                     }
                  }
                  else
                  {
                     glowFilterTimer.container.filters = null;
                  }
               }
               if(Boolean(glowFilterTimer.child))
               {
                  if(Boolean(glowFilterTimer.childHasFilter))
                  {
                     filters = glowFilterTimer.child.filters;
                     j = filters.length - 1;
                     while(j >= 0)
                     {
                        if(filters[j] == this._childGlowFilter)
                        {
                           filters.splice(j,1);
                           glowFilterTimer.child.filters = filters;
                           break;
                        }
                        j--;
                     }
                  }
                  else
                  {
                     glowFilterTimer.child.filters = null;
                  }
               }
               this._glowFilterTimers.splice(i,1);
               break;
            }
            i--;
         }
      }
   }
}

