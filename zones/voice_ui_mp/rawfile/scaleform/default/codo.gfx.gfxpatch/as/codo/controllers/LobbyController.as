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
   import flash.utils.getTimer;
   
   public class LobbyController extends BaseController
   {
      private var _roomObj:RoomObj = new RoomObj();
      
      // Matchmaking owns training map/mode choices even after native commit
      // changes isPlayList to false. The room-scoped, unarchived dvar also
      // survives the movie being unloaded for a match. Keep Ready unchanged.
      private var _trainingRoomIndex:String = "";
      private var _trainingPlaylist:int = -1;

      private var _roomMembers:Array = [];

      private var _campaignHostTimer:Timer;
      private var _campaignHostMap:String = "";
      private var _campaignHostSelectedMap:String = "";
      private var _campaignHostContext:String = "";
      private var _campaignHostRequestedMap:String = "";
      private var _campaignHostStartPara:Object;
      private var _campaignHostWaitStart:int = 0;
      private var _localHostPendingAction:String = "";
      
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
               if(this._roomObj == null || this._roomObj["_localQuickSharedLineage"] != true || !CFunctionContainer.IsRankedMatch() || !CFunctionContainer.IsRoomHost() && (this._roomObj.name == null || String(this._roomObj.name).length == 0) || roomMemberObj == null || roomMemberObj["_localQuickNativeRoleKnown"] != true || roomMemberObj["_localQuickNativeMemberID"] != member || String(roomMemberObj.id) != member)
               {
                  break;
               }
               var gateHost:Object = null;
               var gateHostCount:int = 0;
               var gateLocalHost:Boolean = Boolean(CFunctionContainer.IsRoomHost());
               var gateLocalOwner:String = gateLocalHost ? String(CFunctionContainerCommon.GetPlayerID()) : "";
               for each(var gateCandidate in this._roomMembers)
               {
                  if(gateCandidate != null && gateCandidate["_localQuickNativeRoleKnown"] == true && gateCandidate["_localQuickNativeMemberID"] == String(gateCandidate.id) && (gateLocalHost ? String(gateCandidate.id) == gateLocalOwner && Boolean(this.getSyncDP(SyncDataID.IS_ME,[gateCandidate])) : typeof gateCandidate.realName == "string" && gateCandidate.realName.length > 0 && gateCandidate.realName == String(this._roomObj.name)))
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
            if(this._roomObj != null && this._roomObj["_localQuickSharedLineage"] == true && !ready && (CFunctionContainer.IsRoomHost() || this._roomObj.name != null && String(this._roomObj.name).length > 0))
            {
               var canonicalHost:Object = null;
               var canonicalHostCount:int = 0;
               var canonicalLocalHost:Boolean = Boolean(CFunctionContainer.IsRoomHost());
               var canonicalLocalOwner:String = canonicalLocalHost ? String(CFunctionContainerCommon.GetPlayerID()) : "";
               for each(var candidate in this._roomMembers)
               {
                  if(candidate != null && candidate["_localQuickNativeRoleKnown"] == true && candidate["_localQuickNativeMemberID"] == String(candidate.id) && (canonicalLocalHost ? String(candidate.id) == canonicalLocalOwner && Boolean(this.getSyncDP(SyncDataID.IS_ME,[candidate])) : typeof candidate.realName == "string" && candidate.realName.length > 0 && candidate.realName == String(this._roomObj.name)))
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
      
      private function isManagedTrainingRoom() : Boolean
      {
         return this._trainingPlaylist >= 0 && this._roomObj != null &&
            String(this._roomObj.index) == this._trainingRoomIndex;
      }

      private function isGameStartCountingDown() : Boolean
      {
         return CFunctionContainerCommon.Dvar_GetFloat("dw_gameStartCountingDown") > 0;
      }

      private function isSupportedMapMode(mapID:String, modeID:String) : Boolean
      {
         return mapID != "mp_sd_jardin" || modeID == "sd";
      }
      
      private function onChangeReadyStateHandle(cEvt:ParaEvent) : void
      {
         if(!this.isRoomInfoReady())
         {
            return;
         }
         var readyToSet:Boolean = !this.isReady();
         if(readyToSet && !this.isSupportedMapMode(this._roomObj.currentMap.id,this._roomObj.currentMatchMode.id))
         {
            this.showMsg("This map supports Search & Destroy only.",MsgBtnCount.ONE);
            return;
         }
         if(!readyToSet)
         {
            this.cancelCampaignHostStart();
         }
         else if(!this._roomObj.isPlayList && this.isRoomHost() && (this.isLocalCampaignRoom() || CFunctionContainerCommon.Dvar_GetString("ui_p2p_enabled") == "1"))
         {
            if(!this.isLocalHostRoom())
            {
               this.showMsg("Localhost hosting requires a valid selected room map and game type.",MsgBtnCount.ONE);
               return;
            }
            this.beginCampaignLocalHost(null,true,"ready");
            return;
         }
         this.applyRoomReadyState(readyToSet);
      }

      private function applyRoomReadyState(readyToSet:Boolean) : void
      {
