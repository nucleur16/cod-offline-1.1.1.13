package codo.controllers
{
   import codo.common.*;
   import codo.constants.*;
   import codo.constants.Table.UINotifications;
   import codo.objects.*;
   import codo.services.MatchService;
   import codo.utils.*;
   import flash.events.TimerEvent;
   import flash.utils.Timer;
   
import flash.utils.getTimer;
   public class MatchController extends BaseController
   {
      private static const AUTO_LIST_ROOM_TIMER:Number = 90;
      
      private var _autoListRoomTime:Number = 0;
      
      private var _autoListRoomTimeEnabled:Boolean = false;
      
      private var _isListingRoom:Boolean = false;
      
      private var _currentRoomListType:uint = RoomListType.NONE;
      
      private var _previousRoomListType:uint = RoomListType.NONE;
      
      private var _roomListDP:Array = [];
      
      private var _roomListFilter:RoomListFilterObj = new RoomListFilterObj();
      
      private var _roomListSortingType:uint = RoomListSortType.NONE;
      
      private var _roomListSortingOptions:uint = 0;
      
      private var _currentMatchType:uint = QuickMatchType.NONE;
      
      private var _currentQuickMatchFilter:QuickMatchFilterObj = null;
      
      private var _currentChannel:uint = ServerChannel.NONE;
      
      private var _currentRankedMatchStatus:String = "idle";
      
      private var _currentSelectedLeague:uint = 1;
      
      private var _currentTutorialObj:Object = null;
      
      private var _rankedMatchGoRoomTime:int = 0;
      
      private var _rankedMatchGoGameTime:int = 0;
      
      private var _rankedMatchGoRoomTimer:Timer = null;
      
      private var _rankedMatchGoGameTimer:Timer = null;
      
      private var _rankedMatchPassedTimeTimer:Timer = null;
      
      private var _unrankedMatchGoGameTime:int = 0;
      
      private var _unrankedMatchGoGameTimer:Timer = null;
      
      private var _fetchGroupCountTimer:Timer = null;
      
      private var _showLobbySummaryTimer:Timer = null;
      
      private var _isRankedMatchBeingLocked:Boolean = false;
      
      private var _isRankedMatchInRoomDone:Boolean = false;
      
      private var _isPlayerStatsReady:Boolean = false;
      
      private var _tryJoinRoomPara:Object;
      
      private var _roomToCreate:Object;
      
      private var _groupCounts:Object = new Object();
      
      private var _currentTerrain:String = "";
      
      private var _fakeElapsedTime:Number = 0;
      
      public function MatchController()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.enableRoomListEvent(true);
         this.cppRefreshGroupCount();
         var interval:int = CFunctionContainerCommon.Dvar_GetInt("dw_refreshGroupCountTime");
         interval = interval > 0 ? interval * 1000 : 120 * 1000;
         this._fetchGroupCountTimer = new Timer(interval);
         this._fetchGroupCountTimer.addEventListener(TimerEvent.TIMER,this.onFetchGroupCountTimerHandle);
         this._fetchGroupCountTimer.start();
      }
      
      override protected function onInitialized() : void
      {
         this.bind(AsyncTaskID.QUICK_MATCH_DEBUG,this.onQuickMatchDebug);
         this.bind(AsyncTaskID.LIST_ROOM_PAGE,this.bind_roomListDP);
         this.bind(AsyncTaskID.JOIN_ROOM,this.onRoomJoined);
         this.bind(AsyncTaskID.QUICK_MATCH_INTERMISSION,this.bind_rankedMatchIntermission);
         this.bind(AsyncTaskID.QUICK_MATCH_IN_PROGRESS_IN_ROOM,this.bind_rankedMatchInProgressInRoom);
         this.bind(AsyncTaskID.START_MATCH_COUNTDOWN,this.bind_startMatchCountingDown);
         this.bind(AsyncTaskID.START_MATCH,this.bind_startMatchComplete);
         this.bind(AsyncTaskID.QUICK_MATCH,this.bind_quickMatchReady);
         this.bind(AsyncTaskID.GROUP_COUNTS,this.bind_groupCounts);
         this.addSyncDP(SyncDataID.DEFAULT_ROOM_NAME,this.getDefaultRoomName);
         this.addSyncDP(SyncDataID.CURRENT_MATCH_TYPE,this.getCurrentMatchType);
         this.addSyncDP(SyncDataID.CURRENT_RANKED_MATCH_STATUS,this.getCurrentRankedMatchStatus);
         this.addSyncDP(SyncDataID.CURRENT_LEAGUE,this.getCurrentLeague);
         this.addSyncDP(SyncDataID.CURRENT_LEAGUE_NAME,this.getCurrentLeagueName);
         this.addSyncDP(SyncDataID.GET_MATCH_TYPE_BY_MAP,this.getMatchTypeByMap);
         this.addSyncDP(SyncDataID.IS_CYBORG,this.isCyborgMode);
         GameInfo.currentMatchType = this.isCyborgMode;
         this.addSyncDP(SyncDataID.IS_IN_RANKED_MATCH,this.isInRankedMatch);
         this.addSyncDP(SyncDataID.IS_RANKED_MATCH_IN_PROGRESS,this.isRankedMatchInProgress);
         this.addSyncDP(SyncDataID.IS_RANKED_MATCH_READY,this.isRankedMatchReady);
         this.addSyncDP(SyncDataID.IS_BOOT_CAMP,this.isBootCamp);
         this.addSyncDP(SyncDataID.IS_CYBORG_ENABLED,this.isCyborgEnabled);
         this.addSyncDP(SyncDataID.IS_SURVIVAL_ENABLED,this.isSurvivalEnabled);
         this.addSyncDP(SyncDataID.IS_MARKET_ENABLED,this.isMarketEnabled);
         this.addSyncDP(SyncDataID.IS_MATCH_BUTTON_POS_ENABLED,this.isMatchButtonPosEnabled);
         this.addSyncDP(SyncDataID.IS_CYBORG_TUTORIAL_COMPLETE,this.isCyborgTutorialComplete);
         this.addSyncDP(SyncDataID.IS_IN_ROOM,this.isInRoom);
         this.addSyncDP(SyncDataID.IS_ROOM_FULL,this.isRoomFull);
         this.addSyncDP(SyncDataID.IS_ON_BOARD_COMPLETE,this.isOnBoardComplete);
         this.addSyncDP(SyncDataID.IS_ACT_COMPLETE,this.isActComplete);
         this.addSyncDP(SyncDataID.LIST_SAME_TYPE_MAP,this.listSameTypeMap);
         this.addSyncDP(SyncDataID.LIST_MAP,this.listMap);
         this.addSyncDP(SyncDataID.LIST_MODE,this.listMode);
         this.addSyncDP(SyncDataID.LIST_LEAGUE,this.listLeague);
         this.addSyncDP(SyncDataID.LIST_DIFFICULTY,this.listDifficulty);
         this.addSyncDP(SyncDataID.LIST_DIFFICULTY_CT_CREATOR,this.listCTDifficulty);
         this.addSyncDP(SyncDataID.LIST_PLAY_LIST_NEW,this.listPlayListNew);
         this.addSyncDP(SyncDataID.LIST_PLAY_LIST,this.listPlayList);
         this.addSyncDP(SyncDataID.LIST_MATCH_QUICK_MP_PLAY_LIST,this.listQuickMPPlayList);
         this.addSyncDP(SyncDataID.LIST_COOP_TYPE,this.listCoopType);
         this.addSyncDP(SyncDataID.LIST_MAX_PLAYER,this.listMaxPlayer);
         this.addSyncDP(SyncDataID.LIST_MAX_PLAYER_CT,this.listCTMaxPlayer);
         this.addSyncDP(SyncDataID.LIST_ROOM_LIST_FILTER_STATUS,this.listRoomListFilterStatus);
         this.addSyncDP(SyncDataID.IS_COOP_MAP,this.isCoopMap);
         this.addSyncDP(SyncDataID.IS_MP_MAP,this.isMpMap);
         this.addSyncDP(SyncDataID.IS_ZM_MAP,this.isZmMap);
         this.addSyncDP(SyncDataID.GET_MAP_TYPE,this.getMapType);
         this.addSyncDP(SyncDataID.GET_MAP_COOP_TYPE,this.getMapCoopType);
         this.addSyncDP(SyncDataID.GET_GAME_RULES_BY_MODE_ID,this.getGameRulesByModeID);
         this.addSyncDP(SyncDataID.GET_SPECTATING_BY_MODE_ID,this.getSpectatingByModeID);
         this.addSyncDP(SyncDataID.GET_GAME_PLAY_OPTIONS,this.getGamePlayOptions);
         this.addSyncDP(SyncDataID.MAP_OBJ,this.getMapObj);
         this.addSyncDP(SyncDataID.MATCH_MODE_OBJ,this.getMatchModeObj);
         this.addSyncDP(SyncDataID.PLAY_LIST_OBJ,this.getPlayListObj);
         this.addSyncDP(SyncDataID.BONUS_CT_PER_MIN,this.getCTBonus);
         this.addSyncDP(SyncDataID.BONUS_MP_PER_MIN,this.getMPBonus);
         this.addSyncDP(SyncDataID.XP_LEAGUE_FACTOR,this.getXPLeagueFactor);
         this.addSyncDP(SyncDataID.XP_LEAGUE_FACTORS,this.getXPLeagueFactors);
         this.addSyncDP(SyncDataID.GP_LEAGUE_FACTOR,this.getGPLeagueFactor);
         this.addSyncDP(SyncDataID.GP_LEAGUE_FACTORS,this.getGPLeagueFactors);
         this.addSyncDP(SyncDataID.DELAY_TO_SHOW_BUTTON,this.getDelayToShowButton);
         this.listen(BCEvent.EVT_SET_MATCH_TYPE,this.onSetQuickMatchTypeHandle);
         this.listen(BCEvent.EVT_CLOSE_COOP_ROOM_LIST,this.onCloseCoopRoomListHandle);
         this.listen(BCEvent.EVT_CLOSE_ROOM_CREATOR,this.onCloseRoomCreatorHandle);
         this.listen(BCEvent.EVT_ENABLE_LIST_ROOM,this.onEnableAutoListRoomHandle);
         this.listen(BCEvent.EVT_LIST_ROOM,this.onListRoomHandle);
         this.listen(BCEvent.EVT_CREATE_ROOM,this.onCreateRoomHandle);
         this.listen(BCEvent.EVT_TRY_JOIN_ROOM,this.tryJoinRoomHandle);
         this.listen(BCEvent.EVT_JOIN_ROOM,this.joinRoomHandle);
         this.listen(BCEvent.EVT_BACK_TO_ROOM,this.onBackToRoomHandle);
         this.listen(BCEvent.LOBBY_CLOSE_SUMMARY,this.onBackToRoomHandle);
         this.listen(BCEvent.ROOM_INFO_ONLY,this.onOnlyRoomInfoHandle);
         this.listen(BCEvent.ROOM_INFO,this.onOnlyRoomInfoHandle);
         this.listen(BCEvent.EVT_CONFIRM_ENTER_JOIN_ROOM_PASSWORD,this.onConfirmEnterJoinRoomPasswordHandle);
         this.listen(BCEvent.EVT_CANCEL_ENTER_JOIN_ROOM_PASSWORD,this.onCancelEnterJoinRoomPasswordHandle);
         this.listen(BCEvent.EVT_PROPOSE_ROOM_MEMBERSHIP,this.onProposeRoomMembershipHandle);
         this.listen(BCEvent.EVT_REJECT_ROOM_MEMBERSHIP,this.onRejectRoomMembershipHandle);
         this.listen(BCEvent.RETRY_ON_BOARDING,this.onRetryTutorialHandle);
         this.listen(BCEvent.SKIP_ON_BOARDING,this.onSkipTutorialHandle);
         this.listen(BCEvent.START_ON_BOARDING,this.onStartTutorialHandle);
         this.listen(BCEvent.EVT_QUICK_MATCH,this.onQuickMatchHandle);
         this.listen(BCEvent.EVT_START_QUICK_MATCH_MP,this.onQuickMatchMpHandle);
         this.listen(BCEvent.EVT_ON_ROOM_LEFT,this.onRoomLeftHandle);
         this.listen(BCEvent.PARTY_EVERYONE_IS_READY,this.onPartyEveryoneIsReadyHandle);
         this.listen(BCEvent.EVT_LEAVE_ROOM,this.onLeaveRoomHandle);
         this.listen(BCEvent.MATCH_ROOM_LIST_FILTER_CHANGED,this.onMatchRoomListFilterChangedHandle);
         this.listen(BCEvent.MATCH_ROOM_LIST_SORTING_TYPE_CHANGED,this.onMatchRoomListSortingTypeChangedHandle);
         this.listen(BCEvent.MATCH_QUICK_MP_CHANGE_LEAGUE,this.onMatchQuickMpLeagueChangedHandle);
         this.listen(BCEvent.EVT_GOTO_CYBORG_QUICK_MATCH,this.gotoCyborgQuickMatchHandle);
         this.listen(BCEvent.EVT_GOTO_SURVIVAL_QUICK_MATCH,this.gotoSurvivalQuickMatchHandle);
         this.listen(BCEventCommon.EVT_ON_STRING_SANITIZED,this.onRoomNameSanitizedHandle);
         this.listen(BCEvent.EVT_GOTO_MATCH_MPCT_PLAY_LIST,this.onGotoMPCTChildPageHandle);
         this.listen(BCEvent.EVT_GOTO_MATCH_MPCT_ROOM_LIST,this.onGotoMPCTChildPageHandle);
         this.listen(BCEvent.EVT_GOTO_MATCH_MPCT_ROOM_CREATOR,this.onGotoMPCTChildPageHandle);
         this.listen(BCEvent.CHANGE_ROOM_INFO,this.onChangeRoomInfoHandle);
         this.listen(BCEvent.EVT_PLAY_MAINMENU_SOUND,this.onPlayMainMenuSound);
      }
      
      override public function update(deltaTime:Number) : void
      {
         this.autoListRoom(deltaTime);
         if(!this._isPlayerStatsReady)
         {
            if(ScaleformUtil.isScaleform)
            {
               this._isPlayerStatsReady = CFunctionContainer.IsPlayerStatsReady() as Boolean;
            }
            else
            {
               this._fakeElapsedTime += deltaTime;
               if(this._fakeElapsedTime >= 0.3)
               {
                  this._isPlayerStatsReady = true;
               }
            }
            if(this._isPlayerStatsReady)
            {
               this.broadcast(BCEvent.EVT_PLAYER_STATS_READY);
               this.broadcast(BCEvent.EVT_HIDE_SYSTEM_CONNECT);
            }
         }
      }
      
      private function autoListRoom(deltaTime:Number) : void
      {
         if(!this._autoListRoomTimeEnabled)
         {
            return;
         }
         this._autoListRoomTime += deltaTime;
         if(this._autoListRoomTime >= AUTO_LIST_ROOM_TIMER && this._currentRoomListType != RoomListType.NONE)
         {
            this._autoListRoomTime = 0;
            this.listRoom(true);
         }
      }
      
      private function cancelListRoom() : void
      {
         MatchService.getInstance().cancelListRoom();
      }
      
      private function cancelRankedMatch() : void
      {
         MatchService.getInstance().cancelRankedMatch();
      }
      
      private function onCloseCoopRoomListHandle(cEvt:ParaEvent) : void
      {
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP);
      }
      
      private function onCloseRoomCreatorHandle(cEvt:ParaEvent) : void
      {
         this.gotoPage(this._currentMatchType == QuickMatchType.MP ? CodoPageViewDB.PAGE_MAIN_MATCH_MPCT_PLAYLIST : CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP);
      }
      
      private function onSetQuickMatchTypeHandle(cEvt:ParaEvent) : void
      {
         this.setCurrentMatchType(cEvt.myPara);
         Logger.print("MatchController::onSetQuickMatchTypeHandle this._currentMatchType: " + this._currentMatchType);
         var channel:uint = ServerChannel.NONE;
         switch(this._currentMatchType)
         {
            case QuickMatchType.MP:
               channel = ServerChannel.CHANNEL_0;
               break;
            case QuickMatchType.COOP:
               channel = ServerChannel.CHANNEL_5;
               break;
            case QuickMatchType.CYBORG:
               channel = ServerChannel.CHANNEL_6;
               break;
            case QuickMatchType.TRAINING:
               channel = ServerChannel.CHANNEL_7;
         }
         this.joinChannel(channel);
      }
      
      private function gotoCyborgQuickMatchHandle(cEvt:ParaEvent) : void
      {
         var maps:Array = null;
         var filter:QuickMatchFilterObj = null;
         var mapObj:MapObj = null;
         if(cEvt.myPara == null || cEvt.myPara.length <= 0)
         {
            maps = this.listMap(MatchType.ZM);
            if(maps.length > 0)
            {
               mapObj = maps[0];
            }
         }
         else
         {
            mapObj = this.getMapObj(cEvt.myPara);
         }
         if(Boolean(mapObj))
         {
            filter = new QuickMatchFilterObj();
            filter.type = RoomListType.CYBORG;
            filter.league = 0;
            filter.map = mapObj.id;
            filter.mode = "zm";
            filter.minPlayer = mapObj.minPlayer;
            filter.maxPlayer = mapObj.maxPlayer;
            this.joinChannel(ServerChannel.CHANNEL_6);
            this.broadcast(BCEvent.EVT_QUICK_MATCH,{"filter":filter});
         }
      }
      
      private function gotoSurvivalQuickMatchHandle(cEvt:ParaEvent) : void
      {
         var filter:QuickMatchFilterObj = new QuickMatchFilterObj();
         filter.type = RoomListType.COOP;
         filter.league = 1;
         filter.map = TutorialMap.SURVIVAL;
         filter.mode = "coop";
         filter.minPlayer = 1;
         filter.maxPlayer = 2;
         this.joinChannel(ServerChannel.CHANNEL_9);
         this.broadcast(BCEvent.EVT_QUICK_MATCH,{"filter":filter});
      }
      
      private function onRoomNameSanitizedHandle(cEvt:ParaEvent) : void
      {
         if(cEvt.myPara.type == SanitizedType.ROOM_NAME && !cEvt.myPara.sanitized)
         {
            if(this.getSyncDP(SyncDataID.IS_IN_ROOM))
            {
               return;
            }
            this.request(AsyncTaskID.CREATE_ROOM,this._roomToCreate,this.onRoomCreated);
         }
      }
      
      private function getDefaultRoomName() : String
      {
         return ScaleformUtil.isScaleform ? this.getSyncDP(SyncDataID.REAL_PLAYER_NAME) : LocalizationRoom.ROOM_DEFAULT_NAME;
      }
      
      private function setMatchTypeByRoom() : void
      {
         var matchType:uint = 0;
         var roomInfo:Object = CFunctionContainer.GetRoomInfo();
         if(roomInfo != null)
         {
            matchType = QuickMatchType.MP;
            if(roomInfo.playListID <= 0)
            {
               switch(this.getMapType(roomInfo.mapID))
               {
                  case MapType.COOP:
                  case MapType.COOP_SPEC:
                     matchType = QuickMatchType.COOP;
                     break;
                  case MapType.MP:
                     matchType = QuickMatchType.MP;
                     break;
                  case MapType.ZM:
                     matchType = QuickMatchType.CYBORG;
               }
            }
            this.setCurrentMatchType(matchType);
         }
      }
      
      private function setCurrentMatchType(cType:uint) : void
      {
         var lastMatchType:uint = 0;
         var isStyleChanged:Boolean = false;
         Logger.print("MatchController:::setCurrentMatchType cType: " + cType);
         if(this._currentMatchType != cType)
         {
            lastMatchType = this._currentMatchType;
            this._currentMatchType = cType;
            isStyleChanged = lastMatchType == QuickMatchType.CYBORG != (this._currentMatchType == QuickMatchType.CYBORG);
            Logger.print("MatchController:::setCurrentMatchType isStyleChanged: " + isStyleChanged);
            if(isStyleChanged)
            {
               this.onPlayMainMenuSound();
               this.broadcast(BCEvent.CHANGE_UI_STYLE);
               this.broadcast(BCEvent.LOADOUT_TRANSITION);
            }
         }
      }
      
      private function getCurrentMatchType() : uint
      {
         return this._currentMatchType;
      }
      
      private function getMatchTypeByMap(mapID:String) : uint
      {
         var matchType:uint = QuickMatchType.NONE;
         if(mapID.indexOf("zm_") == 0)
         {
            matchType = QuickMatchType.CYBORG;
         }
         else if(mapID.indexOf("coop_") == 0)
         {
            matchType = QuickMatchType.COOP;
         }
         else if(mapID.indexOf("mp_") == 0)
         {
            matchType = QuickMatchType.MP;
         }
         return matchType;
      }
      
      private function isCyborgMode() : Boolean
      {
         return this._currentMatchType == QuickMatchType.CYBORG;
      }
      
      private function getCurrentRankedMatchStatus() : String
      {
         return this._currentRankedMatchStatus;
      }
      
      private function isInRankedMatch() : Boolean
      {
         return this._currentRankedMatchStatus != RankedMatchStatus.IDLE;
      }
      
      private function isRankedMatchInProgress() : Boolean
      {
         return this._currentRankedMatchStatus == RankedMatchStatus.IN_PROGRESS || this._currentRankedMatchStatus == RankedMatchStatus.IN_PROGRESS_IN_ROOM;
      }
      
      private function isRankedMatchReady() : Boolean
      {
         return this._currentRankedMatchStatus == RankedMatchStatus.READY || this._currentRankedMatchStatus == RankedMatchStatus.READY_IN_ROOM;
      }
      
      private function getCurrentLeague() : int
      {
         return MatchService.getInstance().getCurrentLeague();
      }
      
      private function getCurrentLeagueName() : String
      {
         return LocalizationRoom.LEAGUES[this.getCurrentLeague()];
      }
      
      private function isActComplete(act:int) : Boolean
      {
         return MatchService.getInstance().isActComplete(act);
      }
      
      private function isOnBoardComplete() : Boolean
      {
         return MatchService.getInstance().isOnBoardComplete();
      }
      
      private function isBootCamp() : Boolean
      {
         return this.getCurrentLeague() == 0;
      }
      
      private function isCyborgEnabled() : Boolean
      {
         return ScaleformUtil.isScaleform ? Boolean(CFunctionContainerCommon.Dvar_GetBool("ui_enableCyborg")) : true;
      }
      
      private function isSurvivalEnabled() : Boolean
      {
         return ScaleformUtil.isScaleform ? Boolean(CFunctionContainerCommon.Dvar_GetBool("ui_enableSurvival")) : true;
      }
      
      private function isMarketEnabled() : Boolean
      {
         return ScaleformUtil.isScaleform ? Boolean(CFunctionContainerCommon.Dvar_GetBool("ui_enableMarket")) : true;
      }
      
      private function isMatchButtonPosEnabled() : Boolean
      {
         return ScaleformUtil.isScaleform ? Boolean(CFunctionContainerCommon.Dvar_GetBool("ui_enableMatchButtonPos")) : true;
      }
      
      private function isGroupCountEnabled() : Boolean
      {
         return ScaleformUtil.isScaleform ? Boolean(CFunctionContainerCommon.Dvar_GetBool("dw_enableGroupCount")) : true;
      }
      
      private function isCyborgTutorialComplete() : Boolean
      {
         return MatchService.getInstance().isCyborgOnBoardComplete();
      }
      
      private function isRankedMatch() : Boolean
      {
         return ScaleformUtil.isScaleform ? Boolean(CFunctionContainer.IsRankedMatch()) : this.isInRankedMatch();
      }
      
      private function isRoomFull() : Boolean
      {
         return Boolean(CFunctionContainer.IsRoomFull());
      }
      
      private function isPlayerReady() : Boolean
      {
         return MatchService.getInstance().isPlayerReady;
      }
      
      private function isInRoom() : Boolean
      {
         return MatchService.getInstance().isInRoom;
      }
      
      private function listSameTypeMap(mapID:String) : Array
      {
         var matchType:String = null;
         var i:int = 0;
         var coopType:uint = CoopMapType.ANY;
         if(mapID.indexOf("mp_") == 0)
         {
            matchType = MatchType.PVP;
         }
         else if(mapID.indexOf("zm_") == 0)
         {
            matchType = MatchType.ZM;
         }
         else
         {
            matchType = MatchType.COOP;
         }
         if(matchType == MatchType.COOP)
         {
            coopType = CoopMapType.GetCoopSubType(mapID);
         }
         var maps:Array = this.listMap(matchType,false,coopType);
         if(matchType == MatchType.COOP)
         {
            i = maps.length - 1;
            while(i >= 0)
            {
               if(!maps[i].unlocked)
               {
                  maps.splice(i,1);
               }
               i--;
            }
         }
         return maps;
      }
      
      private function listMap(matchType:String, insertAny:Boolean = false, coop:uint = 0, coopList:Boolean = false) : Array
      {
         var i:int = 0;
         var maps:Array = MatchService.getInstance().listMap(matchType);
         if(matchType == MatchType.COOP)
         {
            i = maps.length - 1;
            while(i >= 0)
            {
               if(!maps[i].unlocked)
               {
                  maps.splice(i,1);
               }
               i--;
            }
            if(coop == CoopMapType.STORY)
            {
               i = maps.length - 1;
               while(i >= 0)
               {
                  if(!(maps[i].isCoop && (coopList && maps[i].chapter == 0 || !coopList)))
                  {
                     maps.splice(i,1);
                  }
                  i--;
               }
            }
            else if(coop == CoopMapType.SPEC_OPS)
            {
               i = maps.length - 1;
               while(i >= 0)
               {
                  if(!maps[i].isCoopSpec)
                  {
                     maps.splice(i,1);
                  }
                  i--;
               }
            }
            else if(coop == CoopMapType.SURVIVAL)
            {
               i = maps.length - 1;
               while(i >= 0)
               {
                  if(!maps[i].isCoopSurvival)
                  {
                     maps.splice(i,1);
                  }
                  i--;
               }
            }
         }
         if(insertAny)
         {
            maps.unshift(new MapObj({"name":LocalizationRoom.ANY}));
         }
         return maps;
      }
      
      private function listMode(matchType:String, isCombat:Boolean = false, insertAny:Boolean = false) : Array
      {
         var modes:Array = MatchService.getInstance().listMatchMode(matchType);
         var newModes:Array = [];
         var i:int = 0;
         while(i < modes.length)
         {
            if(isCombat)
            {
               if(modes[i].id == "combat")
               {
                  newModes.push(modes[i]);
               }
            }
            else if(modes[i].id != "combat")
            {
               newModes.push(modes[i]);
            }
            i++;
         }
         if(insertAny)
         {
            newModes.unshift(new ModeObj({"name":LocalizationRoom.ANY}));
         }
         return newModes;
      }
      
      private function listLeague(isRoom:Boolean = false, insertAny:Boolean = false) : Array
      {
         var i:int = 0;
         var leagues:Array = MatchService.getInstance().listLeague();
         if(isRoom)
         {
            i = leagues.length - 1;
            while(i >= 0)
            {
               if(leagues[i].id == 4)
               {
                  leagues.splice(i,1);
               }
               i--;
            }
         }
         if(insertAny)
         {
            leagues.unshift({
               "name":LocalizationRoom.ANY,
               "id":-1
            });
         }
         return leagues;
      }
      
      private function listCTDifficulty() : Array
      {
         var difficulties:Array = MatchService.getInstance().listDifficulty();
         var newDiffs:Array = [];
         var i:int = 0;
         while(i < difficulties.length)
         {
            newDiffs.push({
               "name":difficulties[i].name,
               "id":difficulties[i].id + 1
            });
            i++;
         }
         return newDiffs;
      }
      
      private function listDifficulty(cMap:Object = null, insertAny:Boolean = false, coopFilter:Boolean = false) : Array
      {
         var difficulties:Array = MatchService.getInstance().listDifficulty(cMap);
         if(coopFilter)
         {
            difficulties.splice(0,1);
         }
         if(insertAny)
         {
            difficulties.unshift({
               "name":LocalizationRoom.ANY,
               "id":-1
            });
         }
         return difficulties;
      }
      
      private function listPlayListNew() : Array
      {
         var choices:Array = this.listPlayList(0,PlayListType.COMBAT,true).concat(this.listPlayList(0,PlayListType.RANKED,true));
         var visiblePlaylists:Array = [];
         for each(var choice in choices)
         {
            if(choice != null && (choice.type == PlayListType.COMBAT && (choice.id == 0 || choice.id == 2) && choice.combatTrainingType == CombatType.CT || choice.name == "Quick Match" && choice.type == PlayListType.RANKED && choice.minPartySize == 1 && (choice.maxPartySize == 1 || choice.maxPartySize == 6) && choice.maxRoomSize == 12))
            {
               visiblePlaylists.push(choice);
            }
         }
         return visiblePlaylists;
      }
      
      private function listPlayList(leagueId:int, playlistType:uint = 1, ignoreLeague:Boolean = false) : Array
      {
         return MatchService.getInstance().listPlayList(leagueId,playlistType,ignoreLeague);
      }
      
      private function listQuickMPPlayList() : Array
      {
         return this.listPlayList(this._currentSelectedLeague);
      }
      
      private function listCoopType(insertAny:Boolean = false) : Array
      {
         var types:Array = [{
            "id":CoopMapType.STORY,
            "name":LocalizationRoom.MAP_TYPE_STORY
         },{
            "id":CoopMapType.SPEC_OPS,
            "name":LocalizationRoom.MAP_TYPE_SPEC
         }];
         if(insertAny)
         {
            types.unshift({
               "id":CoopMapType.ANY,
               "name":LocalizationRoom.ANY
            });
         }
         return types;
      }
      
      private function listMaxPlayer(insertAny:Boolean = false) : Array
      {
         var maxPlayers:Array = [{
            "name":6,
            "id":6
         },{
            "name":8,
            "id":8
         },{
            "name":10,
            "id":10
         },{
            "name":12,
            "id":12
         },{
            "name":14,
            "id":14
         },{
            "name":16,
            "id":16
         },{
            "name":18,
            "id":18
         }];
         if(insertAny)
         {
            maxPlayers.unshift({
               "name":LocalizationRoom.ANY,
               "id":0
            });
         }
         return maxPlayers;
      }
      
      private function listCTMaxPlayer() : Array
      {
         return [{
            "name":4,
            "id":4
         },{
            "name":6,
            "id":6
         },{
            "name":8,
            "id":8
         }];
      }
      
      private function listLoadout(insertAny:Boolean = false) : Array
      {
         var loadouts:Array = [{
            "name":"No Custom",
            "id":1
         },{
            "name":"Class Restricted",
            "id":2
         }];
         if(insertAny)
         {
            loadouts.unshift({
               "name":LocalizationRoom.ANY,
               "id":0
            });
         }
         return loadouts;
      }
      
      private function listRoomListFilterStatus() : Array
      {
         return MatchService.getInstance().listRoomListFilterStatus();
      }
      
      private function isCoopMap(map:Object) : Boolean
      {
         return map.id.indexOf("coop_") == 0;
      }
      
      private function isMpMap(map:Object) : Boolean
      {
         return map.id.indexOf("mp_") == 0;
      }
      
      private function isZmMap(map:Object) : Boolean
      {
         return map.id.indexOf("zm_") == 0;
      }
      
      private function getMapType(mapID:String) : uint
      {
         var mapType:uint = MapType.NONE;
         if(mapID.indexOf("coop_spec") == 0)
         {
            mapType = MapType.COOP_SPEC;
         }
         else if(mapID.indexOf("coop_") == 0)
         {
            mapType = MapType.COOP;
         }
         else if(mapID.indexOf("mp_") == 0)
         {
            mapType = MapType.MP;
         }
         else if(mapID.indexOf("zm_") == 0)
         {
            mapType = MapType.ZM;
         }
         return mapType;
      }
      
      private function getMapCoopType(map:Object) : uint
      {
         return MatchService.getInstance().getMapCoopType(map);
      }
      
      private function getGameRulesByModeID(modeID:String) : Array
      {
         return MatchService.getInstance().getGameRulesByModeID(modeID);
      }
      
      private function getSpectatingByModeID(modeID:String) : Object
      {
         return MatchService.getInstance().getSpectatingByModeID(modeID);
      }
      
      private function getGamePlayOptions() : Object
      {
         return MatchService.getInstance().getGamePlayOptions();
      }
      
      private function getMapObj(mapID:String) : MapObj
      {
         return MatchService.getInstance().getMapObj(mapID);
      }
      
      private function getMatchModeObj(modeID:String) : ModeObj
      {
         return MatchService.getInstance().getModeObj(modeID);
      }
      
      private function getPlayListObj(playListID:int) : PlayListObj
      {
         return MatchService.getInstance().getPlayListObj(playListID);
      }
      
      private function getCTBonus(leagueID:uint) : int
      {
         return MatchService.getInstance().getCTBonus(leagueID);
      }
      
      private function getMPBonus(leagueID:uint) : int
      {
         return MatchService.getInstance().getMPBonus(leagueID);
      }
      
      private function getXPLeagueFactor(ctType:int, league:int, playListObj:PlayListObj = null) : Number
      {
         var factor:Number = NaN;
         if(playListObj != null && playListObj.type == PlayListType.RANKED)
         {
            factor = 1;
         }
         else
         {
            factor = MatchService.getInstance().getXPLeagueFactor(ctType,league);
         }
         return factor;
      }
      
      private function getXPLeagueFactors(ctType:int) : Array
      {
         return MatchService.getInstance().getXPLeagueFactors(ctType);
      }
      
      private function getGPLeagueFactor(ctType:int, league:int, playListObj:PlayListObj = null) : Number
      {
         var factor:Number = NaN;
         if(playListObj != null && playListObj.type == PlayListType.RANKED)
         {
            factor = 1;
         }
         else
         {
            factor = MatchService.getInstance().getGPLeagueFactor(ctType,league);
         }
         return factor;
      }
      
      private function getGPLeagueFactors(ctType:int) : Array
      {
         return MatchService.getInstance().getGPLeagueFactors(ctType);
      }
      
      private function getLeagueName(leagueID:uint) : String
      {
         var leagues:Array = this.listLeague();
         var i:int = 0;
         while(i < leagues.length)
         {
            if(leagueID == leagues[i].id)
            {
               return leagues[i].name;
            }
            i++;
         }
         return "";
      }
      
      private function getDelayToShowButton() : Number
      {
         return !!this.getSyncDP(SyncDataID.IS_SHIP_BUILD) ? MatchService.getInstance().delayToShowButton : 5000;
      }
      
      private function onQuickMatchDebug(cRes:Object) : void
      {
         this.changeRankedMatchStatus(RankedMatchStatus.IN_PROGRESS);
      }
      
      public function onRoomJoined(cRes:Object) : void
      {
         CFunctionContainerCommon.setdvar("codol_quick_lineage","");
         CFunctionContainerCommon.setdvar("codol_quick_return","");
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickVoteOpen"] = false;
            this._currentQuickMatchFilter["_localQuickVoteFinal"] = false;
            this._currentQuickMatchFilter["_localQuickVoteSent"] = false;
            this._currentQuickMatchFilter["_localQuickVoteRequireHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickVoteHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickManagedSummary"] = false;
            this._currentQuickMatchFilter["_localQuickRecoveryKey"] = "";
         }
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickExternal20"] = false;
            this._currentQuickMatchFilter["_localQuickExternal"] = false;
            this._currentQuickMatchFilter["_localQuickExternalRoomSeen"] = false;
            this._currentQuickMatchFilter["_localQuickExternalReturning"] = false;
            this._currentQuickMatchFilter["_localQuickExternalSummary"] = false;
            this._currentQuickMatchFilter["_localQuickExternalResumeRequested"] = false;
         }
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickSharedInvalidated"] = true;
            this._currentQuickMatchFilter["_localQuickSharedLineage"] = false;
            this._currentQuickMatchFilter["_localQuickSharedJoined"] = false;
            this._currentQuickMatchFilter["_localQuickSharedSelecting"] = false;
            this._currentQuickMatchFilter["_localQuickSharedRoomSeen"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturning"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturnPending"] = false;
            this._currentQuickMatchFilter["_localQuickSharedBookingStarted"] = false;
            this._currentQuickMatchFilter["_localQuickSharedMap"] = null;
            this._currentQuickMatchFilter["_localQuickSharedMode"] = null;
         }
         var quickOldRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(quickOldRoom != null)
         {
            quickOldRoom["_localQuickSharedLineage"] = false;
         }
         this.goRoom(cRes.result,cRes.resultMsg);
      }
      
      public function onRoomCreated(cRes:Object) : void
      {
         CFunctionContainerCommon.setdvar("codol_quick_lineage","");
         CFunctionContainerCommon.setdvar("codol_quick_return","");
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickVoteOpen"] = false;
            this._currentQuickMatchFilter["_localQuickVoteFinal"] = false;
            this._currentQuickMatchFilter["_localQuickVoteSent"] = false;
            this._currentQuickMatchFilter["_localQuickVoteRequireHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickVoteHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickManagedSummary"] = false;
            this._currentQuickMatchFilter["_localQuickRecoveryKey"] = "";
         }
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickExternal20"] = false;
            this._currentQuickMatchFilter["_localQuickExternal"] = false;
            this._currentQuickMatchFilter["_localQuickExternalRoomSeen"] = false;
            this._currentQuickMatchFilter["_localQuickExternalReturning"] = false;
            this._currentQuickMatchFilter["_localQuickExternalSummary"] = false;
            this._currentQuickMatchFilter["_localQuickExternalResumeRequested"] = false;
         }
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickSharedInvalidated"] = true;
            this._currentQuickMatchFilter["_localQuickSharedLineage"] = false;
            this._currentQuickMatchFilter["_localQuickSharedJoined"] = false;
            this._currentQuickMatchFilter["_localQuickSharedSelecting"] = false;
            this._currentQuickMatchFilter["_localQuickSharedRoomSeen"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturning"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturnPending"] = false;
            this._currentQuickMatchFilter["_localQuickSharedBookingStarted"] = false;
            this._currentQuickMatchFilter["_localQuickSharedMap"] = null;
            this._currentQuickMatchFilter["_localQuickSharedMode"] = null;
         }
         var quickOldRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(quickOldRoom != null)
         {
            quickOldRoom["_localQuickSharedLineage"] = false;
         }
         if(cRes.result == ResultObj.RESULT_SUCCESS)
         {
            this.enableRoomListEvent(false);
         }
         this.goRoom(cRes.result,cRes.resultMsg);
         if(cRes.result == ResultObj.RESULT_SUCCESS && this._roomToCreate != null && this._roomToCreate.mode == MatchType.ZM)
         {
            CFunctionContainer.ChangeRoomInfo({"difficulty":int(this._roomToCreate.difficulty)},"difficulty");
         }
      }
      
      private function bind_rankedMatchIntermission(cRes:Object) : void
      {
         if(this._currentRankedMatchStatus == RankedMatchStatus.IN_PROGRESS_IN_ROOM)
         {
            this.broadcast(BCEvent.EVT_RANKED_MATCH_INTERMISSION);
         }
      }
      
      private function bind_rankedMatchInProgressInRoom(cRes:Object) : void
      {
         var queuedHumanIDs:Array = [];
         var queuedRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var queuedEntry:Object = this._currentQuickMatchFilter != null ? this.getPlayListObj(this._currentQuickMatchFilter.playListID) : null;
         var queuedRequired:int = queuedEntry != null && queuedEntry.name == "Quick Match" && queuedEntry.type == 1 && queuedEntry.minPartySize == 1 && queuedEntry.maxPartySize == 6 && queuedEntry.maxRoomSize == 12 && queuedEntry.minRoomSize == 1 ? 1 : 2;
         if(queuedRoom != null && queuedRoom.forces != null && queuedRoom.forces.force0 != null && queuedRoom.forces.force1 != null)
         {
            var queuedMembers:Array = queuedRoom.forces.force0.concat(queuedRoom.forces.force1);
            for each(var queuedHuman in queuedMembers)
            {
               if(queuedHuman != null && queuedHuman["_localQuickNativeRoleKnown"] == true && queuedHuman["_localQuickNativeMemberID"] == String(queuedHuman.id) && String(queuedHuman.id).length > 0 && queuedHumanIDs.indexOf(String(queuedHuman.id)) < 0)
               {
                  queuedHumanIDs.push(String(queuedHuman.id));
               }
            }
         }
         if(this._currentQuickMatchFilter != null && (this._currentQuickMatchFilter["_localQuickExternal"] == true || this._currentQuickMatchFilter["_localQuickExternal20"] == true))
         {
            if(this._currentQuickMatchFilter != null)
            {
               this._currentQuickMatchFilter["_localQuickExternal20"] = false;
               this._currentQuickMatchFilter["_localQuickExternal"] = false;
               this._currentQuickMatchFilter["_localQuickExternalRoomSeen"] = false;
               this._currentQuickMatchFilter["_localQuickExternalReturning"] = false;
               this._currentQuickMatchFilter["_localQuickExternalSummary"] = false;
               this._currentQuickMatchFilter["_localQuickExternalResumeRequested"] = false;
            }
            this._currentQuickMatchFilter["_localQuickSharedInvalidated"] = true;
            return;
         }
         if(cRes.result == ResultObj.RESULT_SUCCESS && this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickSharedJoined"] = true;
         }
         var durableNative:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
         var durableOwner:String = CFunctionContainerCommon.GetPlayerID();
         var durableKey:String = durableNative.length == 3 ? String(durableNative[2]) : "";
         var durableSerial:Number = durableNative.length == 3 ? Number(durableNative[1]) : 0;
         var durableValid:Boolean = durableNative.length == 3 && durableNative[0] == "v1" && durableSerial > 0 && durableSerial <= Number("4294967295") && durableSerial == Math.floor(durableSerial) && durableKey.length == 16 && durableKey != "0000000000000000" && durableOwner.length > 0;
         var durableDigit:int = 0;
         while(durableValid && durableDigit < durableKey.length)
         {
            if("0123456789abcdef".indexOf(durableKey.charAt(durableDigit)) < 0)
            {
               durableValid = false;
            }
            durableDigit++;
         }
         var durableSaved:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
         var durableReturn:String = CFunctionContainerCommon.Dvar_GetString("codol_quick_return");
         var durablePlaylist:PlayListObj = durableSaved.length == 5 ? this.getPlayListObj(int(durableSaved[3])) : null;
         var durableOwn:Boolean = false;
         var durableRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(durableRoom != null && durableRoom.forces != null)
         {
            var durableMembers:Array = durableRoom.forces.force0.concat(durableRoom.forces.force1);
            for each(var durableMember in durableMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[durableMember])))
               {
                  durableOwn = true;
               }
            }
         }
         var durableFilter:Object = this._currentQuickMatchFilter;
         var durableEligible:Boolean = ScaleformUtil.isScaleform && durableValid && this.isInRoom() && CFunctionContainer.IsRankedMatch() && durableOwn && durableSaved.length == 5 && durableSaved[0] == "v1" && durableSaved[1] == durableOwner && durableSaved[2] == durableKey && durablePlaylist != null && durablePlaylist.name == "Quick Match" && durablePlaylist.type == 1 && durablePlaylist.minPartySize == 1 && durablePlaylist.maxPartySize == 6 && durablePlaylist.maxRoomSize == 12 && (durableFilter == null || durableFilter["_localQuickSharedInvalidated"] != true && durableFilter["_localQuickExternal"] != true && durableFilter["_localQuickExternal20"] != true && (durableFilter.playListID == int(durableSaved[3]) && durableFilter.playListName == "Quick Match" || durableFilter.playListID == -1 && durableFilter.type == RoomListType.NONE && durableFilter.playListName.length == 0));
         if(durableEligible && (durableFilter == null || durableFilter["_localQuickSharedLineage"] != true))
         {
            if(durableFilter == null)
            {
               this._currentQuickMatchFilter = new QuickMatchFilterObj();
               durableFilter = this._currentQuickMatchFilter;
            }
            if(durableFilter["_localQuickRecoveryKey"] != durableKey)
            {
               durableFilter["_localQuickRecoveryKey"] = durableKey;
               durableFilter["_localQuickRecoverySerial"] = durableSerial;
               this.startRankedMatchPassedTimeTimer();
            }
            else if(false && durableSerial >= Number(durableFilter["_localQuickRecoverySerial"]) + 2)
            {
               durableFilter.type = RoomListType.RANKED;
               durableFilter.playListID = int(durableSaved[3]);
               durableFilter.playListName = durablePlaylist.name;
               durableFilter.leagueName = String(durableSaved[4]);
               durableFilter["_localQuickSharedJoined"] = true;
               durableFilter["_localQuickSharedLineage"] = true;
               durableFilter["_localQuickSharedRoomSeen"] = true;
               durableFilter["_localQuickSharedBookingStarted"] = true;
               durableFilter["_localQuickSharedMap"] = durableRoom.currentMap.id;
               durableFilter["_localQuickSharedMode"] = durableRoom.currentMatchMode.id;
               if(durableReturn == "v1|" + durableOwner + "|" + durableKey)
               {
                  durableFilter["_localQuickSharedReturnPending"] = true;
                  durableFilter["_localQuickSharedReturning"] = true;
               }
               durableRoom["_localQuickSharedLineage"] = true;
               this._isRankedMatchInRoomDone = true;
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.BACK_FROM_GAME);
               this.enableLeaveRoom(true);
            }
         }
         var quickRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var quickFilter:Object = this._currentQuickMatchFilter;
         var quickEntry:PlayListObj = quickFilter != null ? this.getPlayListObj(quickFilter.playListID) : null;
         var quickOwnMember:Boolean = false;
         if(quickRoom != null && quickRoom.forces != null)
         {
            var quickMembers:Array = quickRoom.forces.force0.concat(quickRoom.forces.force1);
            for each(var quickMember in quickMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[quickMember])))
               {
                  quickOwnMember = true;
               }
            }
         }
         var quickSelected:Boolean = quickFilter != null && quickFilter["_localQuickSharedInvalidated"] != true && quickFilter.type == RoomListType.RANKED && quickFilter.playListName == "Quick Match" && (quickEntry != null && quickEntry.name == "Quick Match" && quickEntry.type == 1 && quickEntry.minPartySize == 1 && quickEntry.maxPartySize == 6 && quickEntry.maxRoomSize == 12);
         if(quickSelected && quickFilter["_localQuickExternal20"] == true && quickFilter["_localQuickSharedJoined"] != true && quickFilter["_localQuickSharedLineage"] != true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch())
         {
            quickFilter["_localQuickExternal"] = true;
            quickFilter["_localQuickExternal20"] = false;
         }
         var externalQuick:Boolean = quickSelected && quickFilter["_localQuickExternal"] == true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch();
         if(ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true && (quickFilter["_localQuickSharedJoined"] == true || quickRoom.isPlayList && quickRoom.playListID == quickFilter.playListID))
         {
            quickFilter["_localQuickSharedLineage"] = true;
         }
         var sharedQuick:Boolean = ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickSharedLineage"] == true && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true;
         if(quickRoom != null)
         {
            quickRoom["_localQuickSharedLineage"] = sharedQuick;
         }
         if(sharedQuick && durableValid && CFunctionContainer.IsRankedMatch())
         {
            CFunctionContainerCommon.setdvar("codol_quick_lineage","v1|" + durableOwner + "|" + durableKey + "|" + String(quickFilter.playListID) + "|" + String(quickFilter.leagueName));
         }
         if(cRes.result == ResultObj.RESULT_SUCCESS)
         {
            if(sharedQuick && queuedHumanIDs.length < queuedRequired)
            {
               if(quickFilter["_localQuickHumanWaiting"] != true)
               {
                  quickFilter["_localQuickHumanWaiting"] = true;
                  quickFilter["_localQuickVoteOpen"] = false;
                  quickFilter["_localQuickVoteFinal"] = false;
                  quickFilter["_localQuickVoteSent"] = false;
                  quickFilter["_localQuickSharedRoomSeen"] = false;
                  quickFilter["_localQuickSharedReturning"] = false;
                  quickFilter["_localQuickSharedReturnPending"] = false;
                  quickFilter["_localQuickSharedBookingStarted"] = false;
                  quickRoom["_localQuickVoteFinal"] = -1;
                  CFunctionContainer.SetReadyState(false);
                  this.stopRankedMatchGoRoomTimer();
                  this.stopRankedMatchGoGameTimer();
                  this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.IN_PROGRESS);
               this.startRankedMatchPassedTimeTimer();
               return;
            }
            if(sharedQuick && quickFilter["_localQuickHumanWaiting"] == true)
            {
               quickFilter["_localQuickHumanWaiting"] = false;
               quickFilter["_localQuickSharedRoomSeen"] = false;
               quickFilter["_localQuickSharedReturning"] = false;
               quickFilter["_localQuickSharedReturnPending"] = false;
               quickFilter["_localQuickVoteOpen"] = false;
               this.startRankedMatchPassedTimeTimer();
            }
         }
         if(sharedQuick)
         {
            if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
            {
               this.goRoom(true);
            }
            this.changeRankedMatchStatus(RankedMatchStatus.IN_PROGRESS_IN_ROOM);
            this.enableLeaveRoom(true);
            return;
         }
         if(this.isRankedMatch())
         {
            if(this.getCurrentMatchType() != QuickMatchType.MP)
            {
               this.setCurrentMatchType(QuickMatchType.MP);
            }
            if(CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj != CodoPageViewDB.PAGE_POPUP_WEAPON_ZOOMIN)
            {
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
            }
            this.changeRankedMatchStatus(RankedMatchStatus.IN_PROGRESS_IN_ROOM);
         }
      }
      
      private function bind_startMatchCountingDown(cRes:Object) : void
      {
         var summaryNative:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
         var summarySaved:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
         var summaryOwner:String = CFunctionContainerCommon.GetPlayerID();
         var summaryKey:String = summaryNative.length == 3 ? String(summaryNative[2]) : "";
         var summaryEntry:PlayListObj = summarySaved.length == 5 ? this.getPlayListObj(int(summarySaved[3])) : null;
         var summaryValid:Boolean = summaryNative.length == 3 && summaryNative[0] == "v1" && Number(summaryNative[1]) > 0 && Number(summaryNative[1]) <= Number("4294967295") && Number(summaryNative[1]) == Math.floor(Number(summaryNative[1])) && summaryKey.length == 16 && summaryKey != "0000000000000000";
         var summaryDigit:int = 0;
         while(summaryValid && summaryDigit < summaryKey.length)
         {
            summaryValid = "0123456789abcdef".indexOf(summaryKey.charAt(summaryDigit)) >= 0;
            summaryDigit++;
         }
         if(ScaleformUtil.isScaleform && summaryValid && this.isInRoom() && CFunctionContainer.IsRankedMatch() && summaryOwner.length > 0 && summarySaved.length == 5 && summarySaved[0] == "v1" && summarySaved[1] == summaryOwner && summarySaved[2] == summaryKey && summaryEntry != null && summaryEntry.name == "Quick Match" && summaryEntry.type == 1 && summaryEntry.minPartySize == 1 && summaryEntry.maxPartySize == 6 && summaryEntry.maxRoomSize == 12 && CFunctionContainerCommon.Dvar_GetString("codol_quick_return") == "v1|" + summaryOwner + "|" + summaryKey)
         {
            this.stopRankedMatchGoGameTimer();
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         if(this.isRankedMatch())
         {
            this.startRankedMatchGoGameTimer(ScaleformUtil.isScaleform ? int(cRes.data) : 5);
         }
         else if(cRes.data >= 0)
         {
            this.startUnrankedMatchGoGameTimer(ScaleformUtil.isScaleform ? int(Math.ceil(cRes.data)) : 5);
         }
         else
         {
            this.stopUnrankedMatchGoGameTimer();
         }
      }
      
      private function bind_startMatchComplete(cRes:Object) : void
      {
         if(cRes.result == ResultObj.RESULT_FAIL)
         {
            if(this._currentMatchType == QuickMatchType.MP)
            {
               if(this.isInRankedMatch())
               {
                  this.onRankedMatchCanceled();
                  this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
               }
            }
            this.stopUnrankedMatchGoGameTimer();
            this.showMsg(cRes.resultMsg,MsgBtnCount.ONE,SystemMsgPriority.WARNING);
         }
      }
      
      private function bind_roomListDP(cRes:ResultObj) : void
      {
         var room:Object = null;
         var isFull:Boolean = false;
         var isInLobby:Boolean = false;
         if(!this._isListingRoom)
         {
            return;
         }
         var i:int = 0;
         while(i < cRes.data.length)
         {
            room = cRes.data[i];
            if(room == null || room.currentMap == null || room.currentMap.id == null || String(room.currentMap.id).length == 0)
            {
               i++;
            }
            else
            {
               var zombiesRoom:Boolean = room.matchType == MatchType.ZM || room.currentMap != null && String(room.currentMap.id).indexOf("zm_") == 0;
               if(!(this._currentRoomListType == RoomListType.MP && zombiesRoom || this._currentRoomListType == RoomListType.CYBORG && !zombiesRoom))
               {
                  isFull = room.currentPlayers >= room.currentMaxPlayers || room.matchType == MatchType.COOP && room.status == 1;
                  isInLobby = room.status == 0;
                  room.status = isInLobby ? (isFull ? RoomStatus.BUSY_LOBBY : RoomStatus.FREE_LOBBY) : (isFull ? RoomStatus.BUSY_GAMING : RoomStatus.FREE_GAMING);
                  room.mapType = this.getMapType(room.currentMap.id);
                  if(room.mapType == MapType.COOP || room.mapType == MapType.COOP_SPEC)
                  {
                     if(room.status == RoomStatus.FREE_GAMING)
                     {
                        room.status = RoomStatus.BUSY_GAMING;
                     }
                  }
                  this._roomListDP.push(room);
               }
               i++;
            }
         }
         this._roomListSortingType = RoomListSortType.NONE;
         this.dispatchRooms(this._roomListSortingType == RoomListType.NONE ? RoomListSortType.NUMBER : this._roomListSortingType,true,true);
      }
      
      private function bind_quickMatchReady(cRes:Object) : void
      {
         var queuedHumanIDs:Array = [];
         var queuedRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var queuedEntry:Object = this._currentQuickMatchFilter != null ? this.getPlayListObj(this._currentQuickMatchFilter.playListID) : null;
         var queuedRequired:int = queuedEntry != null && queuedEntry.name == "Quick Match" && queuedEntry.type == 1 && queuedEntry.minPartySize == 1 && queuedEntry.maxPartySize == 6 && queuedEntry.maxRoomSize == 12 && queuedEntry.minRoomSize == 1 ? 1 : 2;
         if(queuedRoom != null && queuedRoom.forces != null && queuedRoom.forces.force0 != null && queuedRoom.forces.force1 != null)
         {
            var queuedMembers:Array = queuedRoom.forces.force0.concat(queuedRoom.forces.force1);
            for each(var queuedHuman in queuedMembers)
            {
               if(queuedHuman != null && queuedHuman["_localQuickNativeRoleKnown"] == true && queuedHuman["_localQuickNativeMemberID"] == String(queuedHuman.id) && String(queuedHuman.id).length > 0 && queuedHumanIDs.indexOf(String(queuedHuman.id)) < 0)
               {
                  queuedHumanIDs.push(String(queuedHuman.id));
               }
            }
         }
         var durableNative:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
         var durableOwner:String = CFunctionContainerCommon.GetPlayerID();
         var durableKey:String = durableNative.length == 3 ? String(durableNative[2]) : "";
         var durableSerial:Number = durableNative.length == 3 ? Number(durableNative[1]) : 0;
         var durableValid:Boolean = durableNative.length == 3 && durableNative[0] == "v1" && durableSerial > 0 && durableSerial <= Number("4294967295") && durableSerial == Math.floor(durableSerial) && durableKey.length == 16 && durableKey != "0000000000000000" && durableOwner.length > 0;
         var durableDigit:int = 0;
         while(durableValid && durableDigit < durableKey.length)
         {
            if("0123456789abcdef".indexOf(durableKey.charAt(durableDigit)) < 0)
            {
               durableValid = false;
            }
            durableDigit++;
         }
         var durableSaved:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
         var durableReturn:String = CFunctionContainerCommon.Dvar_GetString("codol_quick_return");
         var durablePlaylist:PlayListObj = durableSaved.length == 5 ? this.getPlayListObj(int(durableSaved[3])) : null;
         var durableOwn:Boolean = false;
         var durableRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(durableRoom != null && durableRoom.forces != null)
         {
            var durableMembers:Array = durableRoom.forces.force0.concat(durableRoom.forces.force1);
            for each(var durableMember in durableMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[durableMember])))
               {
                  durableOwn = true;
               }
            }
         }
         var durableFilter:Object = this._currentQuickMatchFilter;
         var durableEligible:Boolean = ScaleformUtil.isScaleform && durableValid && this.isInRoom() && CFunctionContainer.IsRankedMatch() && durableOwn && durableSaved.length == 5 && durableSaved[0] == "v1" && durableSaved[1] == durableOwner && durableSaved[2] == durableKey && durablePlaylist != null && durablePlaylist.name == "Quick Match" && durablePlaylist.type == 1 && durablePlaylist.minPartySize == 1 && durablePlaylist.maxPartySize == 6 && durablePlaylist.maxRoomSize == 12 && (durableFilter == null || durableFilter["_localQuickSharedInvalidated"] != true && durableFilter["_localQuickExternal"] != true && durableFilter["_localQuickExternal20"] != true && (durableFilter.playListID == int(durableSaved[3]) && durableFilter.playListName == "Quick Match" || durableFilter.playListID == -1 && durableFilter.type == RoomListType.NONE && durableFilter.playListName.length == 0));
         if(durableEligible && (durableFilter == null || durableFilter["_localQuickSharedLineage"] != true))
         {
            if(durableFilter == null)
            {
               this._currentQuickMatchFilter = new QuickMatchFilterObj();
               durableFilter = this._currentQuickMatchFilter;
            }
            if(durableFilter["_localQuickRecoveryKey"] != durableKey)
            {
               durableFilter["_localQuickRecoveryKey"] = durableKey;
               durableFilter["_localQuickRecoverySerial"] = durableSerial;
               this.startRankedMatchPassedTimeTimer();
            }
            else if(false && durableSerial >= Number(durableFilter["_localQuickRecoverySerial"]) + 2)
            {
               durableFilter.type = RoomListType.RANKED;
               durableFilter.playListID = int(durableSaved[3]);
               durableFilter.playListName = durablePlaylist.name;
               durableFilter.leagueName = String(durableSaved[4]);
               durableFilter["_localQuickSharedJoined"] = true;
               durableFilter["_localQuickSharedLineage"] = true;
               durableFilter["_localQuickSharedRoomSeen"] = true;
               durableFilter["_localQuickSharedBookingStarted"] = true;
               durableFilter["_localQuickSharedMap"] = durableRoom.currentMap.id;
               durableFilter["_localQuickSharedMode"] = durableRoom.currentMatchMode.id;
               if(durableReturn == "v1|" + durableOwner + "|" + durableKey)
               {
                  durableFilter["_localQuickSharedReturnPending"] = true;
                  durableFilter["_localQuickSharedReturning"] = true;
               }
               durableRoom["_localQuickSharedLineage"] = true;
               this._isRankedMatchInRoomDone = true;
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.BACK_FROM_GAME);
               this.enableLeaveRoom(true);
            }
         }
         if(durableEligible && durableFilter != null && durableFilter["_localQuickSharedLineage"] != true)
         {
            this.stopRankedMatchGoRoomTimer();
            this.stopRankedMatchGoGameTimer();
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         var quickRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var quickFilter:Object = this._currentQuickMatchFilter;
         var quickEntry:PlayListObj = quickFilter != null ? this.getPlayListObj(quickFilter.playListID) : null;
         var quickOwnMember:Boolean = false;
         if(quickRoom != null && quickRoom.forces != null)
         {
            var quickMembers:Array = quickRoom.forces.force0.concat(quickRoom.forces.force1);
            for each(var quickMember in quickMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[quickMember])))
               {
                  quickOwnMember = true;
               }
            }
         }
         var quickSelected:Boolean = quickFilter != null && quickFilter["_localQuickSharedInvalidated"] != true && quickFilter.type == RoomListType.RANKED && quickFilter.playListName == "Quick Match" && (quickEntry != null && quickEntry.name == "Quick Match" && quickEntry.type == 1 && quickEntry.minPartySize == 1 && quickEntry.maxPartySize == 6 && quickEntry.maxRoomSize == 12);
         if(cRes.result == ResultObj.RESULT_SUCCESS && quickSelected && quickFilter["_localQuickSharedJoined"] != true && quickFilter["_localQuickSharedLineage"] != true && quickFilter["_localQuickExternal"] != true)
         {
            quickFilter["_localQuickExternal20"] = true;
         }
         if(quickSelected && quickFilter["_localQuickExternal20"] == true && quickFilter["_localQuickSharedJoined"] != true && quickFilter["_localQuickSharedLineage"] != true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch())
         {
            quickFilter["_localQuickExternal"] = true;
            quickFilter["_localQuickExternal20"] = false;
         }
         var externalQuick:Boolean = quickSelected && quickFilter["_localQuickExternal"] == true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch();
         if(ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true && (quickFilter["_localQuickSharedJoined"] == true || quickRoom.isPlayList && quickRoom.playListID == quickFilter.playListID))
         {
            quickFilter["_localQuickSharedLineage"] = true;
         }
         var sharedQuick:Boolean = ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickSharedLineage"] == true && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true;
         if(quickRoom != null)
         {
            quickRoom["_localQuickSharedLineage"] = sharedQuick;
         }
         if(sharedQuick && durableValid && CFunctionContainer.IsRankedMatch())
         {
            CFunctionContainerCommon.setdvar("codol_quick_lineage","v1|" + durableOwner + "|" + durableKey + "|" + String(quickFilter.playListID) + "|" + String(quickFilter.leagueName));
         }
         if(cRes.result == ResultObj.RESULT_SUCCESS)
         {
            if(sharedQuick && queuedHumanIDs.length < queuedRequired)
            {
               if(quickFilter["_localQuickHumanWaiting"] != true)
               {
                  quickFilter["_localQuickHumanWaiting"] = true;
                  quickFilter["_localQuickVoteOpen"] = false;
                  quickFilter["_localQuickVoteFinal"] = false;
                  quickFilter["_localQuickVoteSent"] = false;
                  quickFilter["_localQuickSharedRoomSeen"] = false;
                  quickFilter["_localQuickSharedReturning"] = false;
                  quickFilter["_localQuickSharedReturnPending"] = false;
                  quickFilter["_localQuickSharedBookingStarted"] = false;
                  quickRoom["_localQuickVoteFinal"] = -1;
                  CFunctionContainer.SetReadyState(false);
                  this.stopRankedMatchGoRoomTimer();
                  this.stopRankedMatchGoGameTimer();
                  this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.IN_PROGRESS);
               this.startRankedMatchPassedTimeTimer();
               return;
            }
            if(sharedQuick && quickFilter["_localQuickHumanWaiting"] == true)
            {
               quickFilter["_localQuickHumanWaiting"] = false;
               quickFilter["_localQuickSharedRoomSeen"] = false;
               quickFilter["_localQuickSharedReturning"] = false;
               quickFilter["_localQuickSharedReturnPending"] = false;
               quickFilter["_localQuickVoteOpen"] = false;
               this.startRankedMatchPassedTimeTimer();
            }
         }
         if(sharedQuick && !durableValid)
         {
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         if(externalQuick)
         {
            if(quickFilter["_localQuickExternalRoomSeen"] != true)
            {
               quickFilter["_localQuickExternalRoomSeen"] = true;
               this.stopRankedMatchGoRoomTimer();
               this.stopRankedMatchGoGameTimer();
               this.stopRankedMatchPassedTimeTimer();
               this._isRankedMatchInRoomDone = true;
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.IDLE);
               this.enableLeaveRoom(true);
            }
            return;
         }
         if(quickFilter != null && quickFilter["_localQuickExternal20"] == true)
         {
            return;
         }
         if(sharedQuick && cRes.result == ResultObj.RESULT_SUCCESS)
         {
            if(quickFilter["_localQuickManagedSummary"] == true || quickFilter["_localQuickSharedReturnPending"] == true)
            {
               return;
            }
            var acceptedHost:Object = null;
            var acceptedHostCount:int = 0;
            if(quickRoom != null && quickRoom.forces != null)
            {
               var acceptedMembers:Array = quickRoom.forces.force0.concat(quickRoom.forces.force1);
               for each(var acceptedMember in acceptedMembers)
               {
                  if(acceptedMember["_localQuickNativeRoleKnown"] == true && acceptedMember["_localQuickNativeMemberID"] == String(acceptedMember.id) && quickRoom.name != null && String(quickRoom.name).length > 0 && typeof acceptedMember.realName == "string" && acceptedMember.realName.length > 0 && acceptedMember.realName == String(quickRoom.name))
                  {
                     acceptedHost = acceptedMember;
                     acceptedHostCount++;
                  }
               }
            }
            if(acceptedHostCount == 1 && Boolean(this.getSyncDP(SyncDataID.IS_ME,[acceptedHost])) != Boolean(this.getSyncDP(SyncDataID.IS_ROOM_HOST)))
            {
               acceptedHostCount = 0;
               acceptedHost = null;
            }
            if(acceptedHostCount == 1 && acceptedHost["_localQuickNativeReady"] == false)
            {
               quickFilter["_localQuickVoteHostFalse"] = true;
            }
            if(quickRoom != null && quickRoom["_localQuickHostFalseSeen"] == true)
            {
               quickFilter["_localQuickVoteHostFalse"] = true;
            }
            var acceptedLocalHost:Boolean = Boolean(this.getSyncDP(SyncDataID.IS_ROOM_HOST));
            if(!acceptedLocalHost && acceptedHostCount == 1 && acceptedHost["_localQuickNativeReady"] == false && quickFilter["_localQuickVoteFinal"] == true && quickFilter["_localQuickSharedBookingStarted"] != true)
            {
               quickFilter["_localQuickVoteFinal"] = false;
               quickFilter["_localQuickVoteOpen"] = true;
            }
            var acceptedNext:int = quickRoom != null ? int(quickRoom.playListNextEntry) : -1;
            if(acceptedHostCount == 1 && acceptedHost["_localQuickNativeReady"] == true && quickFilter["_localQuickVoteOpen"] == true && quickEntry != null && quickEntry.entries != null && acceptedNext >= 0 && acceptedNext < quickEntry.entries.length && (acceptedLocalHost ? quickFilter["_localQuickVoteSent"] == true && quickFilter["_localQuickVoteHostFalse"] == true : quickFilter["_localQuickVoteRequireHostFalse"] != true || quickFilter["_localQuickVoteHostFalse"] == true))
            {
               quickFilter["_localQuickVoteFinal"] = true;
               quickFilter["_localQuickVoteOpen"] = false;
               if(quickRoom.forces != null && quickRoom.forces.votes != null)
               {
                  quickRoom.forces.votes.enable = false;
                  this.broadcast(BCEvent.REFRESH_MAP_VOTING,quickRoom);
               }
               if(!acceptedLocalHost)
               {
                  CFunctionContainer.SetReadyState(true);
               }
            }
            if(quickFilter["_localQuickVoteFinal"] != true)
            {
               return;
            }
            // BEGIN shared Quick all-current-member gate
            var quickMembersReady:Boolean = false;
            quickReadyCheck: do
            {
               if(quickRoom == null || quickRoom["_localQuickSharedLineage"] != true || quickRoom.forces == null || quickRoom.forces.force0 == null || quickRoom.forces.force1 == null)
               {
                  break quickReadyCheck;
               }
               var gateCycle:uint = uint(quickRoom["_localQuickReadyCycle"]);
               var gateMembers:Array = quickRoom.forces.force0.concat(quickRoom.forces.force1);
               if(gateCycle == 0 || gateMembers.length < queuedRequired || gateMembers.length > 12)
               {
                  break quickReadyCheck;
               }
               var gateSeen:Array = [];
               var gateHosts:int = 0;
               for each(var gateMember in gateMembers)
               {
                  if(gateMember == null || gateMember["_localQuickNativeRoleKnown"] != true || String(gateMember.id).length == 0 || gateMember["_localQuickNativeMemberID"] != String(gateMember.id) || gateSeen.indexOf(String(gateMember.id)) >= 0 || gateMember["_localQuickNativeReady"] != true || uint(gateMember["_localQuickReadyCycle"]) != gateCycle)
                  {
                     break quickReadyCheck;
                  }
                  gateSeen.push(String(gateMember.id));
                  if(typeof gateMember.realName == "string" && gateMember.realName.length > 0 && gateMember.realName == String(quickRoom.name))
                  {
                     if(quickRoom["_localQuickReadyCycleHost"] != String(gateMember.id))
                     {
                        break quickReadyCheck;
                     }
                     gateHosts++;
                  }
               }
               quickMembersReady = gateHosts == 1;
            }
            while(false);
            // END shared Quick all-current-member gate
            if(!quickMembersReady)
            {
               this.startRankedMatchPassedTimeTimer();
               return;
            }
            this.stopRankedMatchPassedTimeTimer();
            this.stopRankedMatchGoRoomTimer();
            this.stopRankedMatchGoGameTimer();
            this._isRankedMatchInRoomDone = true;
            if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
            {
               this.goRoom(true);
            }
            this.changeRankedMatchStatus(RankedMatchStatus.READY_IN_ROOM);
            this.enableLeaveRoom(true);
            if(this.getSyncDP(SyncDataID.IS_ROOM_HOST) && quickFilter["_localQuickSharedBookingStarted"] != true)
            {
               quickFilter["_localQuickSharedBookingStarted"] = true;
               this.startMatchCountDown();
            }
            return;
         }
         var pendingRecoverySerial:Number = durableFilter != null ? Number(durableFilter["_localQuickRecoverySerial"]) : 0;
         if(cRes.result == ResultObj.RESULT_SUCCESS && durableEligible && durableFilter != null && durableFilter["_localQuickSharedLineage"] != true && durableFilter["_localQuickRecoveryKey"] == durableKey && pendingRecoverySerial > 0 && pendingRecoverySerial <= Number("4294967295") && pendingRecoverySerial == Math.floor(pendingRecoverySerial) && durableSerial >= pendingRecoverySerial && durableReturn == "v1|" + durableOwner + "|" + durableKey)
         {
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         if(this._currentMatchType == QuickMatchType.MP && this.isInRankedMatch())
         {
            if(this.isRankedMatchInProgress())
            {
               this.startRankedMatchGoRoomTimer(ScaleformUtil.isScaleform ? int(this.goRoomCountingDown) : 10);
               this.changeRankedMatchStatus(RankedMatchStatus.READY);
               SoundUtil.playSound(SE_ID.MATCH_SUCCESS);
            }
            else if(this._currentRankedMatchStatus == RankedMatchStatus.BACK_FROM_GAME)
            {
               this.startRankedMatchGoGameTimer(ScaleformUtil.isScaleform ? int(this.goRoomCountingDown + this.goGameCountingDown) : 15,false);
               this.changeRankedMatchStatus(RankedMatchStatus.READY_IN_ROOM);
            }
            this.enableLeaveRoom(false);
         }
         else
         {
            this.goRoom(cRes.result,cRes.resultMsg);
         }
      }
      
      private function bind_groupCounts(cRes:Object) : void
      {
         if(cRes.data == null)
         {
            return;
         }
         this._groupCounts = cRes.data;
         MatchService.getInstance().setPlayListPlayerNum(this._groupCounts.playlist);
         this.setBindData(AsyncTaskID.GROUP_COUNTS,this._groupCounts);
      }
      
      private function restartStartMatchCountDown() : void
      {
         this.hideMsg();
         if(this.getSyncDP(SyncDataID.IS_ROOM_HOST))
         {
            this.startMatchCountDown();
         }
      }
      
      private function startRankedMatchPassedTimeTimer() : void
      {
         if(this._rankedMatchPassedTimeTimer == null)
         {
            this._rankedMatchPassedTimeTimer = new Timer(1000,0);
            this._rankedMatchPassedTimeTimer.addEventListener(TimerEvent.TIMER,this.onRankedMatchPassedTimeTimerHandle);
         }
         if(!this._rankedMatchPassedTimeTimer.running)
         {
            this._rankedMatchPassedTimeTimer.reset();
            this._rankedMatchPassedTimeTimer.start();
         }
      }
      
      private function stopRankedMatchPassedTimeTimer(cancel:Boolean = false) : void
      {
         if(this._rankedMatchPassedTimeTimer != null && this._rankedMatchPassedTimeTimer.running)
         {
            UILogUtil.UILog_rankedMatchWaitingTime(this._rankedMatchPassedTimeTimer.currentCount,cancel);
            this._rankedMatchPassedTimeTimer.stop();
         }
      }
      
      private function startRankedMatchGoRoomTimer(repeatCount:int) : void
      {
         if(this._rankedMatchGoGameTimer != null && this._rankedMatchGoGameTimer.running)
         {
            return;
         }
         this._rankedMatchGoRoomTime = repeatCount;
         if(this._rankedMatchGoRoomTimer == null)
         {
            this._rankedMatchGoRoomTimer = new Timer(1000,this._rankedMatchGoRoomTime + 1);
            this._rankedMatchGoRoomTimer.addEventListener(TimerEvent.TIMER,this.onRankedMatchGoRoomTimerHandle);
            this._rankedMatchGoRoomTimer.addEventListener(TimerEvent.TIMER_COMPLETE,this.onRankedMatchGoRoomTimerCompleteHandle);
         }
         this._rankedMatchGoRoomTimer.reset();
         this._rankedMatchGoRoomTimer.start();
      }
      
      private function stopRankedMatchGoRoomTimer() : void
      {
         if(this._rankedMatchGoRoomTimer != null && this._rankedMatchGoRoomTimer.running)
         {
            this._rankedMatchGoRoomTime = 0;
            this._rankedMatchGoRoomTimer.stop();
         }
      }
      
      private function startRankedMatchGoGameTimer(repeatCount:int, lockNow:Boolean = true) : void
      {
         var summaryNative:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
         var summarySaved:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
         var summaryOwner:String = CFunctionContainerCommon.GetPlayerID();
         var summaryKey:String = summaryNative.length == 3 ? String(summaryNative[2]) : "";
         var summaryEntry:PlayListObj = summarySaved.length == 5 ? this.getPlayListObj(int(summarySaved[3])) : null;
         var summaryValid:Boolean = summaryNative.length == 3 && summaryNative[0] == "v1" && Number(summaryNative[1]) > 0 && Number(summaryNative[1]) <= Number("4294967295") && Number(summaryNative[1]) == Math.floor(Number(summaryNative[1])) && summaryKey.length == 16 && summaryKey != "0000000000000000";
         var summaryDigit:int = 0;
         while(summaryValid && summaryDigit < summaryKey.length)
         {
            summaryValid = "0123456789abcdef".indexOf(summaryKey.charAt(summaryDigit)) >= 0;
            summaryDigit++;
         }
         if(ScaleformUtil.isScaleform && summaryValid && this.isInRoom() && CFunctionContainer.IsRankedMatch() && summaryOwner.length > 0 && summarySaved.length == 5 && summarySaved[0] == "v1" && summarySaved[1] == summaryOwner && summarySaved[2] == summaryKey && summaryEntry != null && summaryEntry.name == "Quick Match" && summaryEntry.type == 1 && summaryEntry.minPartySize == 1 && summaryEntry.maxPartySize == 6 && summaryEntry.maxRoomSize == 12 && CFunctionContainerCommon.Dvar_GetString("codol_quick_return") == "v1|" + summaryOwner + "|" + summaryKey)
         {
            this.stopRankedMatchGoGameTimer();
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         if(Boolean(this._rankedMatchGoGameTimer) && this._rankedMatchGoGameTimer.running)
         {
            return;
         }
         this._rankedMatchGoGameTime = repeatCount;
         if(this._rankedMatchGoGameTime > 0)
         {
            if(this._rankedMatchGoGameTimer == null)
            {
               this._rankedMatchGoGameTimer = new Timer(1000,this._rankedMatchGoGameTime + 1);
               this._rankedMatchGoGameTimer.addEventListener(TimerEvent.TIMER,this.onRankedMatchGoGameTimerHandle);
               this._rankedMatchGoGameTimer.addEventListener(TimerEvent.TIMER_COMPLETE,this.onRankedMatchGoGameTimerCompleteHandle);
            }
            this._rankedMatchGoGameTimer.reset();
            this._rankedMatchGoGameTimer.start();
         }
         else
         {
            this.onRankedMatchGoGameTimerCompleteHandle();
         }
         if(lockNow)
         {
            this.lockMatch();
         }
      }
      
      private function stopRankedMatchGoGameTimer() : void
      {
         if(this._rankedMatchGoGameTimer != null && this._rankedMatchGoGameTimer.running)
         {
            this._rankedMatchGoGameTime = 0;
            this._rankedMatchGoGameTimer.stop();
            this.broadcast(BCEvent.EVT_RANKED_MATCH_GO_GAME_TIMER,0);
         }
      }
      
      private function startUnrankedMatchGoGameTimer(repeatCount:int) : void
      {
         this._unrankedMatchGoGameTime = repeatCount;
         if(this._unrankedMatchGoGameTime > 0)
         {
            if(this._unrankedMatchGoGameTimer == null)
            {
               this._unrankedMatchGoGameTimer = new Timer(1000,this._unrankedMatchGoGameTime + 1);
               this._unrankedMatchGoGameTimer.addEventListener(TimerEvent.TIMER,this.onUnrankedMatchGoGameTimerHandle);
               this._unrankedMatchGoGameTimer.addEventListener(TimerEvent.TIMER_COMPLETE,this.onUnrankedMatchGoGameTimerCompleteHandle);
            }
            this._unrankedMatchGoGameTimer.reset();
            this._unrankedMatchGoGameTimer.start();
         }
         else
         {
            this.onUnrankedMatchGoGameTimerCompleteHandle();
         }
      }
      
      private function stopUnrankedMatchGoGameTimer() : void
      {
         if(this._unrankedMatchGoGameTimer != null && this._unrankedMatchGoGameTimer.running)
         {
            this._unrankedMatchGoGameTime = 0;
            this._unrankedMatchGoGameTimer.stop();
            this._unrankedMatchGoGameTimer = null;
            this.broadcast(BCEvent.EVT_UNRANKED_MATCH_GO_GAME_TIMER,0);
         }
      }
      
      private function lockMatch() : void
      {
         if(!CFunctionContainerCommon.Dvar_GetBool("dw_autoStartGame"))
         {
            return;
         }
         if(this._isRankedMatchBeingLocked)
         {
            return;
         }
         this._isRankedMatchBeingLocked = true;
         this.showMsg(LocalizationRoom.NOTE_START_GAME,MsgBtnCount.ONE,SystemMsgPriority.NORMAL,this.leaveRoom,LocalizationRoom.LEAVE_ROOM,null,"",null,"",this.getDelayToShowButton());
      }
      
      private function leaveRoom(auto:Boolean = false, cancelRanked:Boolean = false, withParty:Boolean = false) : void
      {
         CFunctionContainerCommon.setdvar("codol_quick_lineage","");
         CFunctionContainerCommon.setdvar("codol_quick_return","");
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickVoteOpen"] = false;
            this._currentQuickMatchFilter["_localQuickVoteFinal"] = false;
            this._currentQuickMatchFilter["_localQuickVoteSent"] = false;
            this._currentQuickMatchFilter["_localQuickVoteRequireHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickVoteHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickManagedSummary"] = false;
            this._currentQuickMatchFilter["_localQuickRecoveryKey"] = "";
         }
         if(!auto || this._currentQuickMatchFilter == null || this._currentQuickMatchFilter["_localQuickExternalResumeRequested"] != true)
         {
            if(this._currentQuickMatchFilter != null)
            {
               this._currentQuickMatchFilter["_localQuickExternal20"] = false;
               this._currentQuickMatchFilter["_localQuickExternal"] = false;
               this._currentQuickMatchFilter["_localQuickExternalRoomSeen"] = false;
               this._currentQuickMatchFilter["_localQuickExternalReturning"] = false;
               this._currentQuickMatchFilter["_localQuickExternalSummary"] = false;
               this._currentQuickMatchFilter["_localQuickExternalResumeRequested"] = false;
            }
         }
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickSharedInvalidated"] = true;
            this._currentQuickMatchFilter["_localQuickSharedLineage"] = false;
            this._currentQuickMatchFilter["_localQuickSharedJoined"] = false;
            this._currentQuickMatchFilter["_localQuickSharedSelecting"] = false;
            this._currentQuickMatchFilter["_localQuickSharedRoomSeen"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturning"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturnPending"] = false;
            this._currentQuickMatchFilter["_localQuickSharedBookingStarted"] = false;
            this._currentQuickMatchFilter["_localQuickSharedMap"] = null;
            this._currentQuickMatchFilter["_localQuickSharedMode"] = null;
         }
         var quickOldRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(quickOldRoom != null)
         {
            quickOldRoom["_localQuickSharedLineage"] = false;
         }
         this.request(AsyncTaskID.LEAVE_ROOM,{
            "auto":auto,
            "cancelRanked":cancelRanked,
            "party":withParty
         });
      }
      
      private function leaveRoomWithParty() : void
      {
         if(this.getSyncDP(SyncDataID.IS_SYSTEM_MSG_SHOWING))
         {
            this.hideMsg();
         }
         this.leaveRoom(false,false,true);
      }
      
      private function leaveRoomAlone() : void
      {
         if(this.getSyncDP(SyncDataID.IS_SYSTEM_MSG_SHOWING))
         {
            this.hideMsg();
         }
         this.leaveRoom();
      }
      
      private function startMatchCountDown(cEvt:TimerEvent = null) : void
      {
         if(this._currentQuickMatchFilter != null && (this._currentQuickMatchFilter["_localQuickExternal"] == true || this._currentQuickMatchFilter["_localQuickExternal20"] == true))
         {
            return;
         }
         if(this.getSyncDP(SyncDataID.IS_ROOM_HOST) && this.goGameCountingDown >= 0)
         {
            CFunctionContainer.StartMatchCountDown();
            if(!ScaleformUtil.isScaleform)
            {
               this.bind_startMatchCountingDown(new ResultObj());
            }
         }
         this.enableLeaveRoom(false);
      }
      
      private function get goRoomCountingDown() : Number
      {
         return CFunctionContainerCommon.Dvar_GetFloat("dw_enterRoomCountingDown");
      }
      
      private function get goGameCountingDown() : Number
      {
         CFunctionContainerCommon.Dvar_GetFloat("dw_startGameCountingDown");
         return 0;
      }
      
      private function onRankedMatchPassedTimeTimerHandle(cEvt:TimerEvent) : void
      {
         this.broadcast(BCEvent.EVT_RANKED_MATCH_PASSED_TIME_TIMER,cEvt.target.currentCount);
         var queuedHumanIDs:Array = [];
         var queuedRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var queuedEntry:Object = this._currentQuickMatchFilter != null ? this.getPlayListObj(this._currentQuickMatchFilter.playListID) : null;
         var queuedRequired:int = queuedEntry != null && queuedEntry.name == "Quick Match" && queuedEntry.type == 1 && queuedEntry.minPartySize == 1 && queuedEntry.maxPartySize == 6 && queuedEntry.maxRoomSize == 12 && queuedEntry.minRoomSize == 1 ? 1 : 2;
         if(queuedRoom != null && queuedRoom.forces != null && queuedRoom.forces.force0 != null && queuedRoom.forces.force1 != null)
         {
            var queuedMembers:Array = queuedRoom.forces.force0.concat(queuedRoom.forces.force1);
            for each(var queuedHuman in queuedMembers)
            {
               if(queuedHuman != null && queuedHuman["_localQuickNativeRoleKnown"] == true && queuedHuman["_localQuickNativeMemberID"] == String(queuedHuman.id) && String(queuedHuman.id).length > 0 && queuedHumanIDs.indexOf(String(queuedHuman.id)) < 0)
               {
                  queuedHumanIDs.push(String(queuedHuman.id));
               }
            }
         }
         var durableNative:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
         var durableOwner:String = CFunctionContainerCommon.GetPlayerID();
         var durableKey:String = durableNative.length == 3 ? String(durableNative[2]) : "";
         var durableSerial:Number = durableNative.length == 3 ? Number(durableNative[1]) : 0;
         var durableValid:Boolean = durableNative.length == 3 && durableNative[0] == "v1" && durableSerial > 0 && durableSerial <= Number("4294967295") && durableSerial == Math.floor(durableSerial) && durableKey.length == 16 && durableKey != "0000000000000000" && durableOwner.length > 0;
         var durableDigit:int = 0;
         while(durableValid && durableDigit < durableKey.length)
         {
            if("0123456789abcdef".indexOf(durableKey.charAt(durableDigit)) < 0)
            {
               durableValid = false;
            }
            durableDigit++;
         }
         var durableSaved:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
         var durableReturn:String = CFunctionContainerCommon.Dvar_GetString("codol_quick_return");
         var durablePlaylist:PlayListObj = durableSaved.length == 5 ? this.getPlayListObj(int(durableSaved[3])) : null;
         var durableOwn:Boolean = false;
         var durableRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(durableRoom != null && durableRoom.forces != null)
         {
            var durableMembers:Array = durableRoom.forces.force0.concat(durableRoom.forces.force1);
            for each(var durableMember in durableMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[durableMember])))
               {
                  durableOwn = true;
               }
            }
         }
         var durableFilter:Object = this._currentQuickMatchFilter;
         var durableEligible:Boolean = ScaleformUtil.isScaleform && durableValid && this.isInRoom() && CFunctionContainer.IsRankedMatch() && durableOwn && durableSaved.length == 5 && durableSaved[0] == "v1" && durableSaved[1] == durableOwner && durableSaved[2] == durableKey && durablePlaylist != null && durablePlaylist.name == "Quick Match" && durablePlaylist.type == 1 && durablePlaylist.minPartySize == 1 && durablePlaylist.maxPartySize == 6 && durablePlaylist.maxRoomSize == 12 && (durableFilter == null || durableFilter["_localQuickSharedInvalidated"] != true && durableFilter["_localQuickExternal"] != true && durableFilter["_localQuickExternal20"] != true && (durableFilter.playListID == int(durableSaved[3]) && durableFilter.playListName == "Quick Match" || durableFilter.playListID == -1 && durableFilter.type == RoomListType.NONE && durableFilter.playListName.length == 0));
         if(durableEligible && (durableFilter == null || durableFilter["_localQuickSharedLineage"] != true))
         {
            if(durableFilter == null)
            {
               this._currentQuickMatchFilter = new QuickMatchFilterObj();
               durableFilter = this._currentQuickMatchFilter;
            }
            if(durableFilter["_localQuickRecoveryKey"] != durableKey)
            {
               durableFilter["_localQuickRecoveryKey"] = durableKey;
               durableFilter["_localQuickRecoverySerial"] = durableSerial;
               this.startRankedMatchPassedTimeTimer();
            }
            else if(durableSerial >= Number(durableFilter["_localQuickRecoverySerial"]) + 2)
            {
               durableFilter.type = RoomListType.RANKED;
               durableFilter.playListID = int(durableSaved[3]);
               durableFilter.playListName = durablePlaylist.name;
               durableFilter.leagueName = String(durableSaved[4]);
               durableFilter["_localQuickSharedJoined"] = true;
               durableFilter["_localQuickSharedLineage"] = true;
               durableFilter["_localQuickSharedRoomSeen"] = true;
               durableFilter["_localQuickSharedBookingStarted"] = true;
               durableFilter["_localQuickSharedMap"] = durableRoom.currentMap.id;
               durableFilter["_localQuickSharedMode"] = durableRoom.currentMatchMode.id;
               if(durableReturn == "v1|" + durableOwner + "|" + durableKey)
               {
                  durableFilter["_localQuickSharedReturnPending"] = true;
                  durableFilter["_localQuickSharedReturning"] = true;
               }
               durableRoom["_localQuickSharedLineage"] = true;
               this._isRankedMatchInRoomDone = true;
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               if(durableRoom.forces != null && durableRoom.forces.votes != null)
               {
                  this.broadcast(BCEvent.REFRESH_MAP_VOTING,durableRoom);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.BACK_FROM_GAME);
               this.enableLeaveRoom(true);
            }
         }
         var quickRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var quickFilter:Object = this._currentQuickMatchFilter;
         var quickEntry:PlayListObj = quickFilter != null ? this.getPlayListObj(quickFilter.playListID) : null;
         var quickOwnMember:Boolean = false;
         if(quickRoom != null && quickRoom.forces != null)
         {
            var quickMembers:Array = quickRoom.forces.force0.concat(quickRoom.forces.force1);
            for each(var quickMember in quickMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[quickMember])))
               {
                  quickOwnMember = true;
               }
            }
         }
         var quickSelected:Boolean = quickFilter != null && quickFilter["_localQuickSharedInvalidated"] != true && quickFilter.type == RoomListType.RANKED && quickFilter.playListName == "Quick Match" && (quickEntry != null && quickEntry.name == "Quick Match" && quickEntry.type == 1 && quickEntry.minPartySize == 1 && quickEntry.maxPartySize == 6 && quickEntry.maxRoomSize == 12);
         if(quickSelected && quickFilter["_localQuickExternal20"] == true && quickFilter["_localQuickSharedJoined"] != true && quickFilter["_localQuickSharedLineage"] != true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch())
         {
            quickFilter["_localQuickExternal"] = true;
            quickFilter["_localQuickExternal20"] = false;
         }
         var externalQuick:Boolean = quickSelected && quickFilter["_localQuickExternal"] == true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch();
         if(ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true && (quickFilter["_localQuickSharedJoined"] == true || quickRoom.isPlayList && quickRoom.playListID == quickFilter.playListID))
         {
            quickFilter["_localQuickSharedLineage"] = true;
         }
         var sharedQuick:Boolean = ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickSharedLineage"] == true && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true;
         if(quickRoom != null)
         {
            quickRoom["_localQuickSharedLineage"] = sharedQuick;
         }
         if(sharedQuick && durableValid && CFunctionContainer.IsRankedMatch())
         {
            CFunctionContainerCommon.setdvar("codol_quick_lineage","v1|" + durableOwner + "|" + durableKey + "|" + String(quickFilter.playListID) + "|" + String(quickFilter.leagueName));
         }
         if(sharedQuick && queuedHumanIDs.length < queuedRequired)
         {
            if(quickFilter["_localQuickHumanWaiting"] != true)
            {
               quickFilter["_localQuickHumanWaiting"] = true;
               quickFilter["_localQuickVoteOpen"] = false;
               quickFilter["_localQuickVoteFinal"] = false;
               quickFilter["_localQuickVoteSent"] = false;
               quickFilter["_localQuickSharedRoomSeen"] = false;
               quickFilter["_localQuickSharedReturning"] = false;
               quickFilter["_localQuickSharedReturnPending"] = false;
               quickFilter["_localQuickSharedBookingStarted"] = false;
               quickRoom["_localQuickVoteFinal"] = -1;
               CFunctionContainer.SetReadyState(false);
               this.stopRankedMatchGoRoomTimer();
               this.stopRankedMatchGoGameTimer();
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
            }
            this.changeRankedMatchStatus(RankedMatchStatus.IN_PROGRESS);
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         if(sharedQuick && quickFilter["_localQuickHumanWaiting"] == true)
         {
            quickFilter["_localQuickHumanWaiting"] = false;
            quickFilter["_localQuickSharedRoomSeen"] = false;
            quickFilter["_localQuickSharedReturning"] = false;
            quickFilter["_localQuickSharedReturnPending"] = false;
            quickFilter["_localQuickVoteOpen"] = false;
            this.startRankedMatchPassedTimeTimer();
         }
         if(sharedQuick && !durableValid)
         {
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         if(externalQuick)
         {
            if(quickFilter["_localQuickExternalRoomSeen"] != true)
            {
               quickFilter["_localQuickExternalRoomSeen"] = true;
               this.stopRankedMatchGoRoomTimer();
               this.stopRankedMatchGoGameTimer();
               this.stopRankedMatchPassedTimeTimer();
               this._isRankedMatchInRoomDone = true;
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.IDLE);
               this.enableLeaveRoom(true);
            }
            return;
         }
         if(quickFilter != null && quickFilter["_localQuickExternal20"] == true)
         {
            return;
         }
         if(sharedQuick)
         {
            if(quickFilter["_localQuickSharedRoomSeen"] != true && quickFilter["_localQuickSharedReturning"] != true && CFunctionContainerCommon.Dvar_GetInt("ui_eog_show") != 1)
            {
               if(quickFilter["_localQuickVoteOpen"] != true)
               {
                  quickFilter["_localQuickVoteOpen"] = true;
                  quickFilter["_localQuickVoteTicks"] = 0;
                  quickFilter["_localQuickVoteStartMS"] = getTimer();
                  quickFilter["_localQuickVoteSent"] = false;
                  quickFilter["_localQuickVoteHostFalse"] = false;
                  quickFilter["_localQuickVoteLastSendMS"] = -1000;
                  quickRoom["_localQuickHostFalseSeen"] = false;
                  quickFilter["_localQuickVoteFinal"] = false;
                  quickRoom["_localQuickVoteFinal"] = -1;
                  quickFilter["_localQuickSharedRoomSeen"] = true;
                  quickFilter["_localQuickSharedBookingStarted"] = false;
                  quickFilter["_localQuickSharedReturning"] = false;
                  quickFilter["_localQuickSharedReturnPending"] = false;
                  this._isRankedMatchInRoomDone = true;
                  this.stopRankedMatchGoRoomTimer();
                  this.stopRankedMatchGoGameTimer();
                  if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
                  {
                     this.goRoom(true);
                  }
                  this.changeRankedMatchStatus(RankedMatchStatus.BACK_FROM_GAME);
                  this.enableLeaveRoom(true);
                  CFunctionContainer.SetReadyState(false);
                  if(this.getSyncDP(SyncDataID.IS_ROOM_HOST))
                  {
                     CFunctionContainer.ChangeRoomInfo({"mode":"playlist"},"mode");
                  }
                  this.startRankedMatchPassedTimeTimer();
               }
            }
            var acceptedHost:Object = null;
            var acceptedHostCount:int = 0;
            if(quickRoom != null && quickRoom.forces != null)
            {
               var acceptedMembers:Array = quickRoom.forces.force0.concat(quickRoom.forces.force1);
               for each(var acceptedMember in acceptedMembers)
               {
                  if(acceptedMember["_localQuickNativeRoleKnown"] == true && acceptedMember["_localQuickNativeMemberID"] == String(acceptedMember.id) && quickRoom.name != null && String(quickRoom.name).length > 0 && typeof acceptedMember.realName == "string" && acceptedMember.realName.length > 0 && acceptedMember.realName == String(quickRoom.name))
                  {
                     acceptedHost = acceptedMember;
                     acceptedHostCount++;
                  }
               }
            }
            if(acceptedHostCount == 1 && Boolean(this.getSyncDP(SyncDataID.IS_ME,[acceptedHost])) != Boolean(this.getSyncDP(SyncDataID.IS_ROOM_HOST)))
            {
               acceptedHostCount = 0;
               acceptedHost = null;
            }
            if(acceptedHostCount == 1 && acceptedHost["_localQuickNativeReady"] == false)
            {
               quickFilter["_localQuickVoteHostFalse"] = true;
            }
            if(quickRoom != null && quickRoom["_localQuickHostFalseSeen"] == true)
            {
               quickFilter["_localQuickVoteHostFalse"] = true;
            }
            var acceptedLocalHost:Boolean = Boolean(this.getSyncDP(SyncDataID.IS_ROOM_HOST));
            if(!acceptedLocalHost && acceptedHostCount == 1 && acceptedHost["_localQuickNativeReady"] == false && quickFilter["_localQuickVoteFinal"] == true && quickFilter["_localQuickSharedBookingStarted"] != true)
            {
               quickFilter["_localQuickVoteFinal"] = false;
               quickFilter["_localQuickVoteOpen"] = true;
            }
            var acceptedNext:int = quickRoom != null ? int(quickRoom.playListNextEntry) : -1;
            if(acceptedHostCount == 1 && acceptedHost["_localQuickNativeReady"] == true && quickFilter["_localQuickVoteOpen"] == true && quickEntry != null && quickEntry.entries != null && acceptedNext >= 0 && acceptedNext < quickEntry.entries.length && (acceptedLocalHost ? quickFilter["_localQuickVoteSent"] == true && quickFilter["_localQuickVoteHostFalse"] == true : quickFilter["_localQuickVoteRequireHostFalse"] != true || quickFilter["_localQuickVoteHostFalse"] == true))
            {
               quickFilter["_localQuickVoteFinal"] = true;
               quickFilter["_localQuickVoteOpen"] = false;
               if(quickRoom.forces != null && quickRoom.forces.votes != null)
               {
                  quickRoom.forces.votes.enable = false;
                  this.broadcast(BCEvent.REFRESH_MAP_VOTING,quickRoom);
               }
               if(!acceptedLocalHost)
               {
                  CFunctionContainer.SetReadyState(true);
               }
            }
            var quickHumanIDs:Array = [];
            if(quickRoom != null && quickRoom.forces != null)
            {
               for each(var quickHuman in acceptedMembers)
               {
                  if(quickHuman["_localQuickNativeRoleKnown"] == true && quickHuman["_localQuickNativeMemberID"] == String(quickHuman.id) && String(quickHuman.id).length > 0 && quickHumanIDs.indexOf(String(quickHuman.id)) < 0)
                  {
                     quickHumanIDs.push(String(quickHuman.id));
                  }
               }
            }
            // BEGIN shared Quick all-current-member gate
            var quickMembersReady:Boolean = false;
            quickReadyCheck: do
            {
               if(quickRoom == null || quickRoom["_localQuickSharedLineage"] != true || quickRoom.forces == null || quickRoom.forces.force0 == null || quickRoom.forces.force1 == null)
               {
                  break quickReadyCheck;
               }
               var gateCycle:uint = uint(quickRoom["_localQuickReadyCycle"]);
               var gateMembers:Array = quickRoom.forces.force0.concat(quickRoom.forces.force1);
               if(gateCycle == 0 || gateMembers.length < queuedRequired || gateMembers.length > 12)
               {
                  break quickReadyCheck;
               }
               var gateSeen:Array = [];
               var gateHosts:int = 0;
               for each(var gateMember in gateMembers)
               {
                  if(gateMember == null || gateMember["_localQuickNativeRoleKnown"] != true || String(gateMember.id).length == 0 || gateMember["_localQuickNativeMemberID"] != String(gateMember.id) || gateSeen.indexOf(String(gateMember.id)) >= 0 || gateMember["_localQuickNativeReady"] != true || uint(gateMember["_localQuickReadyCycle"]) != gateCycle)
                  {
                     break quickReadyCheck;
                  }
                  gateSeen.push(String(gateMember.id));
                  if(typeof gateMember.realName == "string" && gateMember.realName.length > 0 && gateMember.realName == String(quickRoom.name))
                  {
                     if(quickRoom["_localQuickReadyCycleHost"] != String(gateMember.id))
                     {
                        break quickReadyCheck;
                     }
                     gateHosts++;
                  }
               }
               quickMembersReady = gateHosts == 1;
            }
            while(false);
            // END shared Quick all-current-member gate
            if(quickFilter["_localQuickVoteFinal"] == true && quickFilter["_localQuickSharedBookingStarted"] != true && this.getSyncDP(SyncDataID.IS_ROOM_HOST) && quickMembersReady)
            {
               this.stopRankedMatchPassedTimeTimer();
               this.stopRankedMatchGoRoomTimer();
               this.stopRankedMatchGoGameTimer();
               this.changeRankedMatchStatus(RankedMatchStatus.READY_IN_ROOM);
               quickFilter["_localQuickSharedBookingStarted"] = true;
               this.startMatchCountDown();
               return;
            }
            if(quickFilter["_localQuickVoteOpen"] == true && quickFilter["_localQuickVoteFinal"] != true)
            {
               quickFilter["_localQuickVoteTicks"] = int(quickFilter["_localQuickVoteTicks"]) + 1;
               var voteElapsedMS:Number = getTimer() - Number(quickFilter["_localQuickVoteStartMS"]);
               this.broadcast(BCEvent.EVT_RANKED_MATCH_GO_GAME_TIMER,Math.max(0,Math.ceil((30000 - voteElapsedMS) / 1000)));
               if(voteElapsedMS >= 30000 && this.getSyncDP(SyncDataID.IS_ROOM_HOST) && acceptedHostCount == 1 && quickHumanIDs.length >= queuedRequired && quickHumanIDs.length <= 12)
               {
                  if(this.getSyncDP(SyncDataID.IS_ROOM_HOST))
                  {
                     var votePool:Array = [{
                        "map":"mp_afghan",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_afghan",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_afghan",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_afghan",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_afghan",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_afghan",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_afghan",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_bloc_sh",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_bloc_sh",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_bloc_sh",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_bloc_sh",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_bloc_sh",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_bloc_sh",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_bloc_sh",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_bog_sh",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_bog_sh",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_bog_sh",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_bog_sh",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_bog_sh",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_bog_sh",
                        "mode":"sab",
                        "alias":"sab"
                     },{
                        "map":"mp_bog_sh",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_cargoship_sh",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_cargoship_sh",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_cargoship_sh",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_cargoship_sh",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_cargoship_sh",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_crash_tropical",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_crash_tropical",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_crash_tropical",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_crash_tropical",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_crash_tropical",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_crash_tropical",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_crash_tropical",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_cross_fire",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_cross_fire",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_cross_fire",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_cross_fire",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_cross_fire",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_cross_fire",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_cross_fire",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_estate_tropical",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_estate_tropical",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_estate_tropical",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_estate_tropical",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_estate_tropical",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_estate_tropical",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_estate_tropical",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_fav_tropical",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_fav_tropical",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_fav_tropical",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_fav_tropical",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_fav_tropical",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_fav_tropical",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_fav_tropical",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_killhouse_sh",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_killhouse_sh",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_killhouse_sh",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_killhouse_sh",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_killhouse_sh",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_killhouse_sh",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_killhouse_sh",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_nuked",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_nuked",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_nuked",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_nuked",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_nuked",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_nuked",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_overgrown",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_overgrown",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_overgrown",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_overgrown",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_overgrown",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_overgrown",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_overgrown",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_rust",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_rust",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_rust",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_rust",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_rust",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_rust",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_rust",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_storm_spring",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_storm_spring",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_storm_spring",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_storm_spring",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_storm_spring",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_storm_spring",
                        "mode":"koth",
                        "alias":"hq"
                     },{
                        "map":"mp_storm_spring",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_storm_spring",
                        "mode":"war",
                        "alias":"tdm"
                     },{
                        "map":"mp_underpass",
                        "mode":"conf",
                        "alias":"kc"
                     },{
                        "map":"mp_underpass",
                        "mode":"ctf",
                        "alias":"ctf"
                     },{
                        "map":"mp_underpass",
                        "mode":"dd",
                        "alias":"dd"
                     },{
                        "map":"mp_underpass",
                        "mode":"dm",
                        "alias":"ffa"
                     },{
                        "map":"mp_underpass",
                        "mode":"dom",
                        "alias":"dom"
                     },{
                        "map":"mp_underpass",
                        "mode":"sd",
                        "alias":"sd"
                     },{
                        "map":"mp_underpass",
                        "mode":"war",
                        "alias":"tdm"
                     }];
                     if(quickEntry.entries == null || quickEntry.entries.length != votePool.length || quickRoom.forces == null || quickRoom.forces.votes == null)
                     {
                        return;
                     }
                     var voteCheck:int = 0;
                     while(voteCheck < votePool.length)
                     {
                        if(String(quickEntry.entries[voteCheck].map) != votePool[voteCheck].map || String(quickEntry.entries[voteCheck].gametype) != votePool[voteCheck].alias)
                        {
                           return;
                        }
                        voteCheck++;
                     }
                     var voteIndex:int = quickFilter["_localQuickVoteSent"] == true ? int(quickFilter["_localQuickVoteResolved"]) : int(quickRoom.forces.votes.entry);
                     if(voteIndex < 0 || voteIndex >= votePool.length)
                     {
                        var voteChoices:Array = [];
                        var voteI:int = 0;
                        while(voteI < votePool.length)
                        {
                           if(voteI != quickRoom.playListLastEntry)
                           {
                              voteChoices.push(voteI);
                           }
                           voteI++;
                        }
                        if(voteChoices.length == 0)
                        {
                           return;
                        }
                        voteIndex = int(voteChoices[int(Math.random() * voteChoices.length)]);
                     }
                     var votePair:Object = votePool[voteIndex];
                     var voteMap:MapObj = this.getMapObj(votePair.map);
                     var voteMode:ModeObj = this.getMatchModeObj(votePair.mode);
                     if(String(quickEntry.entries[voteIndex].map) != votePair.map || voteMap == null || !voteMap.unlocked || voteMode == null || voteMode.id != votePair.mode && voteMode.alias != votePair.mode)
                     {
                        return;
                     }
                     if(quickFilter["_localQuickVoteSent"] == true && getTimer() - Number(quickFilter["_localQuickVoteLastSendMS"]) < 1000)
                     {
                        return;
                     }
                     quickFilter["_localQuickVoteResolved"] = voteIndex;
                     quickFilter["_localQuickVoteLastSendMS"] = getTimer();
                     quickRoom["_localQuickVoteFinal"] = voteIndex;
                     CFunctionContainer.SetFinalVoting(voteIndex);
                     CFunctionContainer.ChangeRoomInfo({
                        "map":votePair.map,
                        "minPlayer":queuedRequired,
                        "maxPlayer":12,
                        "difficulty":int(quickRoom.currentLevel)
                     },"map");
                  }
                  quickFilter["_localQuickVoteSent"] = true;
                  CFunctionContainer.SetReadyState(true);
               }
            }
            return;
         }
      }
      
      private function onRankedMatchGoRoomTimerHandle(cEvt:TimerEvent) : void
      {
         this.broadcast(BCEvent.EVT_RANKED_MATCH_GO_ROOM_TIMER,this._rankedMatchGoRoomTime--);
         if(this._rankedMatchGoRoomTime >= 0 && this._rankedMatchGoRoomTime < 5)
         {
            SoundUtil.playSound(SE_ID.MATCH_START_COUNT_DOWN);
         }
         if(this._rankedMatchGoRoomTime == 4)
         {
            this.lockMatch();
         }
      }
      
      private function onRankedMatchGoRoomTimerCompleteHandle(cEvt:TimerEvent) : void
      {
         var queueFilter:Object = this._currentQuickMatchFilter;
         var queuePlaylist:Object = queueFilter != null ? this.getPlayListObj(queueFilter.playListID) : null;
         if(ScaleformUtil.isScaleform && this.isInRoom() && CFunctionContainer.IsRankedMatch() && queueFilter != null && queueFilter["_localQuickSharedLineage"] == true && queueFilter["_localQuickSharedInvalidated"] != true && queueFilter["_localQuickExternal"] != true && queueFilter["_localQuickExternal20"] != true && queuePlaylist != null && queuePlaylist.name == "Quick Match" && queuePlaylist.type == 1 && queuePlaylist.minPartySize == 1 && queuePlaylist.maxPartySize == 6 && queuePlaylist.maxRoomSize == 12)
         {
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         if(!this._isRankedMatchInRoomDone)
         {
            this._isRankedMatchInRoomDone = true;
            this.goRoom(true);
         }
         this.changeRankedMatchStatus(RankedMatchStatus.READY_IN_ROOM);
         this.startMatchCountDown();
         if(!this.getSyncDP(SyncDataID.IS_ROOM_HOST) && this._currentRankedMatchStatus == RankedMatchStatus.READY_IN_ROOM)
         {
            this.startRankedMatchGoGameTimer(ScaleformUtil.isScaleform ? int(this.goGameCountingDown) : 5);
         }
      }
      
      private function onRankedMatchGoGameTimerHandle(cEvt:TimerEvent) : void
      {
         var summaryNative:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
         var summarySaved:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
         var summaryOwner:String = CFunctionContainerCommon.GetPlayerID();
         var summaryKey:String = summaryNative.length == 3 ? String(summaryNative[2]) : "";
         var summaryEntry:PlayListObj = summarySaved.length == 5 ? this.getPlayListObj(int(summarySaved[3])) : null;
         var summaryValid:Boolean = summaryNative.length == 3 && summaryNative[0] == "v1" && Number(summaryNative[1]) > 0 && Number(summaryNative[1]) <= Number("4294967295") && Number(summaryNative[1]) == Math.floor(Number(summaryNative[1])) && summaryKey.length == 16 && summaryKey != "0000000000000000";
         var summaryDigit:int = 0;
         while(summaryValid && summaryDigit < summaryKey.length)
         {
            summaryValid = "0123456789abcdef".indexOf(summaryKey.charAt(summaryDigit)) >= 0;
            summaryDigit++;
         }
         if(ScaleformUtil.isScaleform && summaryValid && this.isInRoom() && CFunctionContainer.IsRankedMatch() && summaryOwner.length > 0 && summarySaved.length == 5 && summarySaved[0] == "v1" && summarySaved[1] == summaryOwner && summarySaved[2] == summaryKey && summaryEntry != null && summaryEntry.name == "Quick Match" && summaryEntry.type == 1 && summaryEntry.minPartySize == 1 && summaryEntry.maxPartySize == 6 && summaryEntry.maxRoomSize == 12 && CFunctionContainerCommon.Dvar_GetString("codol_quick_return") == "v1|" + summaryOwner + "|" + summaryKey)
         {
            this.stopRankedMatchGoGameTimer();
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         this.broadcast(BCEvent.EVT_RANKED_MATCH_GO_GAME_TIMER,this._rankedMatchGoGameTime--);
         if(this._rankedMatchGoGameTime == 4)
         {
            SoundUtil.playSound(SE_ID.MATCH_START);
            this.lockMatch();
         }
         if(this._rankedMatchGoGameTime <= 9 && this._rankedMatchGoGameTime > 4)
         {
            SoundUtil.playSound(SE_ID.MATCH_START_COUNT_DOWN);
         }
      }
      
      private function onRankedMatchGoGameTimerCompleteHandle(cEvt:TimerEvent = null) : void
      {
         var canStart:Boolean = false;
         if(this.getSyncDP(SyncDataID.IS_ROOM_HOST))
         {
            canStart = this.getSyncDP(SyncDataID.IS_SHIP_BUILD);
            if(!canStart)
            {
               canStart = this.goGameCountingDown >= 0;
            }
            if(canStart)
            {
               this.hideMsg();
            }
            this.changeRankedMatchStatus(RankedMatchStatus.STARTING_GAME);
         }
      }
      
      private function onUnrankedMatchGoGameTimerHandle(cEvt:TimerEvent) : void
      {
         this.broadcast(BCEvent.EVT_UNRANKED_MATCH_GO_GAME_TIMER,this._unrankedMatchGoGameTime--);
         if(this._unrankedMatchGoGameTime == 4)
         {
            SoundUtil.playSound(SE_ID.MATCH_START);
            this.lockMatch();
         }
      }
      
      private function onUnrankedMatchGoGameTimerCompleteHandle(cEvt:TimerEvent = null) : void
      {
      }
      
      private function onLeaveRoomHandle(cEvt:ParaEvent) : void
      {
         if(this._currentQuickMatchFilter != null && this._currentQuickMatchFilter["_localQuickPartyReady"] == "pending")
         {
            this.onRankedMatchCanceled();
            return;
         }
         var msg:String = null;
         var label0:String = null;
         var label1:String = null;
         this.cancelListRoom();
         var cancelRanked:Boolean = this.isRankedMatchInProgress() ? true : false;
         var withParty:Boolean = cancelRanked && this.getSyncDP(SyncDataID.IS_PARTY_HOST);
         if(this.isRankedMatchInProgress())
         {
            this.cancelRankedMatch();
            if(!this.isInRoom())
            {
               this.onRankedMatchCanceled();
            }
         }
         else if(this.getSyncDP(SyncDataID.IS_PARTY_HOST))
         {
            msg = LocalizationRoom.NOTE_LEAVE_ROOM_LONE_OR_WITH_PARTY;
            label0 = LocalizationRoom.LEAVE_ROOM_WITH_PARTY;
            label1 = LocalizationRoom.LEAVE_ROOM_ALONE;
            this.showMsg(msg,MsgBtnCount.THREE,0,this.leaveRoomWithParty,label0,this.leaveRoomAlone,label1,null,LocalizationSystem.CANCEL);
            return;
         }
         this.leaveRoom(false,cancelRanked,withParty);
      }
      
      private function onRankedMatchCanceled() : void
      {
         this.cancelRankedMatch();
         CFunctionContainerCommon.setdvar("codol_quick_lineage","");
         CFunctionContainerCommon.setdvar("codol_quick_return","");
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickVoteOpen"] = false;
            this._currentQuickMatchFilter["_localQuickVoteFinal"] = false;
            this._currentQuickMatchFilter["_localQuickVoteSent"] = false;
            this._currentQuickMatchFilter["_localQuickVoteRequireHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickVoteHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickManagedSummary"] = false;
            this._currentQuickMatchFilter["_localQuickRecoveryKey"] = "";
         }
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickExternal20"] = false;
            this._currentQuickMatchFilter["_localQuickExternal"] = false;
            this._currentQuickMatchFilter["_localQuickExternalRoomSeen"] = false;
            this._currentQuickMatchFilter["_localQuickExternalReturning"] = false;
            this._currentQuickMatchFilter["_localQuickExternalSummary"] = false;
            this._currentQuickMatchFilter["_localQuickExternalResumeRequested"] = false;
         }
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickSharedInvalidated"] = true;
            this._currentQuickMatchFilter["_localQuickSharedLineage"] = false;
            this._currentQuickMatchFilter["_localQuickSharedJoined"] = false;
            this._currentQuickMatchFilter["_localQuickSharedSelecting"] = false;
            this._currentQuickMatchFilter["_localQuickSharedRoomSeen"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturning"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturnPending"] = false;
            this._currentQuickMatchFilter["_localQuickSharedBookingStarted"] = false;
            this._currentQuickMatchFilter["_localQuickSharedMap"] = null;
            this._currentQuickMatchFilter["_localQuickSharedMode"] = null;
         }
         var quickOldRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(quickOldRoom != null)
         {
            quickOldRoom["_localQuickSharedLineage"] = false;
         }
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickSharedRoomSeen"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturning"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturnPending"] = false;
            this._currentQuickMatchFilter["_localQuickSharedBookingStarted"] = false;
         }
         if(this._currentQuickMatchFilter != null && this._currentQuickMatchFilter["_localQuickPartyReady"] != null)
         {
            var wasPending:Boolean = this._currentQuickMatchFilter["_localQuickPartyReady"] == "pending";
            this._currentQuickMatchFilter["_localQuickPartyReady"] = "cancelled";
            this._currentQuickMatchFilter["_localQuickPartyCancel"] = null;
            if(wasPending)
            {
               this.broadcast(BCEvent.PARTY_SET_READY_STATE,{
                  "ready":false,
                  "filter":this._currentQuickMatchFilter
               });
               this.hideMsg();
            }
         }
         this._isRankedMatchInRoomDone = false;
         this._isRankedMatchBeingLocked = false;
         this.enableLeaveRoom(true);
         this.stopRankedMatchGoRoomTimer();
         this.stopRankedMatchGoGameTimer();
         this.stopRankedMatchPassedTimeTimer(true);
         this.changeRankedMatchStatus(RankedMatchStatus.IDLE);
      }
      
      private function onRoomLeftHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("codol_quick_lineage","");
         CFunctionContainerCommon.setdvar("codol_quick_return","");
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickVoteOpen"] = false;
            this._currentQuickMatchFilter["_localQuickVoteFinal"] = false;
            this._currentQuickMatchFilter["_localQuickVoteSent"] = false;
            this._currentQuickMatchFilter["_localQuickVoteRequireHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickVoteHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickManagedSummary"] = false;
            this._currentQuickMatchFilter["_localQuickRecoveryKey"] = "";
         }
         var externalResume:Object = this._currentQuickMatchFilter != null && this._currentQuickMatchFilter["_localQuickExternal"] == true && this._currentQuickMatchFilter["_localQuickExternalResumeRequested"] == true ? this._currentQuickMatchFilter : null;
         this.enableRoomListEvent(true);
         this._currentQuickMatchFilter = null;
         this._isRankedMatchBeingLocked = false;
         var play:Boolean = true;
         if(this.isInRankedMatch())
         {
            if(this._currentRankedMatchStatus == RankedMatchStatus.IN_PROGRESS_IN_ROOM)
            {
               play = false;
            }
            else if(this.isRankedMatchReady())
            {
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
            }
            this.onRankedMatchCanceled();
         }
         else
         {
            this.stopUnrankedMatchGoGameTimer();
         }
         if(play)
         {
            this.onPlayMainMenuSound(null);
         }
         if(externalResume != null && externalResume["_localQuickExternalResumeRequested"] == true && ScaleformUtil.isScaleform && !this.isInRoom())
         {
            externalResume["_localQuickExternalResumeRequested"] = false;
            externalResume["_localQuickExternal"] = false;
            if(!this.getSyncDP(SyncDataID.IS_IN_PARTY) || this.getSyncDP(SyncDataID.IS_PARTY_HOST))
            {
               this.broadcast(BCEvent.EVT_QUICK_MATCH,{"filter":externalResume});
            }
         }
      }
      
      private function onPlayMainMenuSound(cEvt:ParaEvent = null) : void
      {
         if(this.isCyborgMode())
         {
            SoundUtil.playMusic(MusicID.MAIN_MENU_ZM,MusicID.MAIN_MENU_AMB_ZM);
         }
         else
         {
            SoundUtil.playMusic(MusicID.MAIN_MENU,MusicID.MAIN_MENU_AMB);
         }
      }
      
      private function onCreateRoomHandle(cEvt:ParaEvent) : void
      {
         if(this._currentMatchType == QuickMatchType.CYBORG)
         {
            if(this.checkInPartyAsNotHost())
            {
               return;
            }
            if(this.checkInPartySize(cEvt.myPara.maxPlayer))
            {
               return;
            }
         }
         else if(this.checkInParty())
         {
            return;
         }
         this._roomToCreate = cEvt.myPara;
         this.broadcast(BCEventCommon.EVT_SANITIZE_STRING,{
            "type":SanitizedType.ROOM_NAME,
            "content":StringUtil.trim(this._roomToCreate.name)
         });
      }
      
      private function onPartyEveryoneIsReadyHandle(cEvt:ParaEvent) : void
      {
         if(this._currentQuickMatchFilter == null)
         {
            return;
         }
         if(this._currentQuickMatchFilter["_localQuickPartyReady"] != null)
         {
            if(this._currentQuickMatchFilter["_localQuickPartyReady"] != "pending")
            {
               return;
            }
            var playlistObj:PlayListObj = this.getPlayListObj(this._currentQuickMatchFilter.playListID);
            if(!(playlistObj != null && playlistObj.name == "Quick Match" && playlistObj.type == 1 && playlistObj.minPartySize == 1 && playlistObj.maxPartySize == 6 && playlistObj.maxRoomSize == 12) || !this.getSyncDP(SyncDataID.IS_IN_PARTY) || !this.getSyncDP(SyncDataID.IS_PARTY_HOST) || this.isInRoom() || this.isInRankedMatch())
            {
               this.onRankedMatchCanceled();
               return;
            }
            this._currentQuickMatchFilter["_localQuickPartyReady"] = "started";
            this._currentQuickMatchFilter["_localQuickPartyCancel"] = null;
         }
         this.startQuickMatch();
      }
      
      private function onRetryTutorialHandle(cEvt:ParaEvent) : void
      {
         this.startTutorial();
      }
      
      private function onSkipTutorialHandle(cEvt:ParaEvent) : void
      {
         this.showMsg(LocalizationRoom.TUTORIAL_SKIP_GAME_WARNING,MsgBtnCount.TWO,SystemMsgPriority.NORMAL,this.skipTutorial,LocalizationRoom.TUTORIAL_SKIP);
      }
      
      private function onStartTutorialHandle(cEvt:ParaEvent) : void
      {
         if(this._currentTutorialObj == null)
         {
            this._currentTutorialObj = cEvt.myPara;
            this.startTutorial();
         }
      }
      
      private function onQuickMatchHandle(cEvt:ParaEvent) : void
      {
         if(this._currentQuickMatchFilter != null && this._currentQuickMatchFilter["_localQuickPartyReady"] == "pending")
         {
            if(this.getSyncDP(SyncDataID.IS_IN_PARTY) && this.getSyncDP(SyncDataID.IS_PARTY_HOST))
            {
               return;
            }
            this._currentQuickMatchFilter["_localQuickPartyReady"] = "cancelled";
            this._currentQuickMatchFilter["_localQuickPartyCancel"] = null;
         }
         var playlistObj:PlayListObj = null;
         if(this.isInRoom())
         {
            return;
         }
         if(this.isInRankedMatch())
         {
            return;
         }
         if(this.checkInPartyAsNotHost())
         {
            return;
         }
         var filter:QuickMatchFilterObj = cEvt.myPara.filter;
         var maxPlayer:int = Math.max(1,filter.mode == "combat" && filter.map.length > 0 ? filter.maxPlayer / 2 : filter.maxPlayer);
         if(this.checkInPartySize(maxPlayer))
         {
            return;
         }
         this._currentQuickMatchFilter = cEvt.myPara.filter;
         CFunctionContainerCommon.setdvar("codol_quick_lineage","");
         CFunctionContainerCommon.setdvar("codol_quick_return","");
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickVoteOpen"] = false;
            this._currentQuickMatchFilter["_localQuickVoteFinal"] = false;
            this._currentQuickMatchFilter["_localQuickVoteSent"] = false;
            this._currentQuickMatchFilter["_localQuickVoteRequireHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickVoteHostFalse"] = false;
            this._currentQuickMatchFilter["_localQuickManagedSummary"] = false;
            this._currentQuickMatchFilter["_localQuickRecoveryKey"] = "";
         }
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickExternal20"] = false;
            this._currentQuickMatchFilter["_localQuickExternal"] = false;
            this._currentQuickMatchFilter["_localQuickExternalRoomSeen"] = false;
            this._currentQuickMatchFilter["_localQuickExternalReturning"] = false;
            this._currentQuickMatchFilter["_localQuickExternalSummary"] = false;
            this._currentQuickMatchFilter["_localQuickExternalResumeRequested"] = false;
         }
         if(this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter["_localQuickSharedInvalidated"] = false;
            this._currentQuickMatchFilter["_localQuickSharedLineage"] = false;
            this._currentQuickMatchFilter["_localQuickSharedJoined"] = false;
            this._currentQuickMatchFilter["_localQuickSharedSelecting"] = false;
            this._currentQuickMatchFilter["_localQuickSharedRoomSeen"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturning"] = false;
            this._currentQuickMatchFilter["_localQuickSharedReturnPending"] = false;
            this._currentQuickMatchFilter["_localQuickSharedBookingStarted"] = false;
            this._currentQuickMatchFilter["_localQuickSharedMap"] = null;
            this._currentQuickMatchFilter["_localQuickSharedMode"] = null;
         }
         var quickOldRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(quickOldRoom != null)
         {
            quickOldRoom["_localQuickSharedLineage"] = false;
         }
         if(this._currentQuickMatchFilter.playListID >= 0)
         {
            playlistObj = this.getPlayListObj(this._currentQuickMatchFilter.playListID);
            if(!this.checkPartyCanPlayPlaylist(playlistObj))
            {
               return;
            }
            if(!this.checkPlaylistPartyOnly(playlistObj))
            {
               return;
            }
            if(playlistObj.type == PlayListType.COMBAT && playlistObj.combatTrainingType == CombatType.CT)
            {
               this.joinChannel(ServerChannel.CHANNEL_8);
            }
         }
         if(playlistObj != null && playlistObj.name == "Quick Match" && playlistObj.type == 1 && playlistObj.minPartySize == 1 && playlistObj.maxPartySize == 6 && playlistObj.maxRoomSize == 12 && this._currentQuickMatchFilter.type == RoomListType.RANKED && this.getSyncDP(SyncDataID.IS_IN_PARTY) && this.getSyncDP(SyncDataID.IS_PARTY_HOST))
         {
            this._currentQuickMatchFilter["_localQuickPartyReady"] = "pending";
            this._currentQuickMatchFilter["_localQuickPartyCancel"] = this.onRankedMatchCanceled;
            this.broadcast(BCEvent.PARTY_SET_READY_STATE,{
               "ready":true,
               "filter":this._currentQuickMatchFilter
            });
            return;
         }
         this.startQuickMatch();
      }
      
      private function onQuickMatchMpHandle(cEvt:ParaEvent) : void
      {
         if(!this.checkPartyCanPlayPlaylist(cEvt.myPara.data))
         {
            return;
         }
         var filter:QuickMatchFilterObj = new QuickMatchFilterObj();
         filter.type = RoomListType.RANKED;
         filter.league = cEvt.myPara.data.leagueID;
         filter.leagueName = this.getLeagueName(filter.league);
         filter.playListID = cEvt.myPara.data.id;
         filter.playListName = cEvt.myPara.data.name;
         filter.minPlayer = cEvt.myPara.data.minPartySize;
         filter.maxPlayer = cEvt.myPara.data.maxPartySize;
         this.joinChannel(cEvt.myPara.data.leagueID);
         this.broadcast(BCEvent.EVT_QUICK_MATCH,{"filter":filter});
      }
      
      private function onMatchRoomListFilterChangedHandle(cEvt:ParaEvent) : void
      {
         if(cEvt.myPara == null || cEvt.myPara.value == null)
         {
            return;
         }
         switch(cEvt.myPara.type)
         {
            case RoomListFilterType.NUMBER:
               this._roomListFilter.roomNumber = cEvt.myPara.value;
               break;
            case RoomListFilterType.NAME:
               this._roomListFilter.roomName = cEvt.myPara.value;
               break;
            case RoomListFilterType.LEAGUE:
               this._roomListFilter.leagueID = cEvt.myPara.value.id;
               break;
            case RoomListFilterType.MAP:
               this._roomListFilter.mapID = cEvt.myPara.value.id;
               break;
            case RoomListFilterType.MODE:
               this._roomListFilter.modeID = cEvt.myPara.value.id;
               break;
            case RoomListFilterType.PLAYER:
               this._roomListFilter.maxPlayer = cEvt.myPara.value.id;
               break;
            case RoomListFilterType.STATUS:
               this._roomListFilter.status = cEvt.myPara.value.id;
               break;
            case RoomListFilterType.RESET:
               this._roomListFilter.roomNumber = "";
               this._roomListFilter.roomName = "";
               this._roomListFilter.leagueID = -1;
               this._roomListFilter.mapID = "";
               this._roomListFilter.modeID = "";
               this._roomListFilter.maxPlayer = 0;
               this._roomListFilter.status = 0;
         }
         this.dispatchRooms(this._roomListSortingType == RoomListType.NONE ? RoomListSortType.NUMBER : this._roomListSortingType,true,false);
      }
      
      private function onMatchRoomListSortingTypeChangedHandle(cEvt:ParaEvent) : void
      {
         this.dispatchRooms(cEvt.myPara,false,true);
      }
      
      private function onMatchQuickMpLeagueChangedHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.MATCH_QUICK_MP_CHANGE_PLAYLIST,this.listPlayList(cEvt.myPara.id));
      }
      
      private function onBackToRoomHandle(cEvt:ParaEvent) : void
      {
         var queuedHumanIDs:Array = [];
         var queuedRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var queuedEntry:Object = this._currentQuickMatchFilter != null ? this.getPlayListObj(this._currentQuickMatchFilter.playListID) : null;
         var queuedRequired:int = queuedEntry != null && queuedEntry.name == "Quick Match" && queuedEntry.type == 1 && queuedEntry.minPartySize == 1 && queuedEntry.maxPartySize == 6 && queuedEntry.maxRoomSize == 12 && queuedEntry.minRoomSize == 1 ? 1 : 2;
         if(queuedRoom != null && queuedRoom.forces != null && queuedRoom.forces.force0 != null && queuedRoom.forces.force1 != null)
         {
            var queuedMembers:Array = queuedRoom.forces.force0.concat(queuedRoom.forces.force1);
            for each(var queuedHuman in queuedMembers)
            {
               if(queuedHuman != null && queuedHuman["_localQuickNativeRoleKnown"] == true && queuedHuman["_localQuickNativeMemberID"] == String(queuedHuman.id) && String(queuedHuman.id).length > 0 && queuedHumanIDs.indexOf(String(queuedHuman.id)) < 0)
               {
                  queuedHumanIDs.push(String(queuedHuman.id));
               }
            }
         }
         if(cEvt.type == BCEvent.LOBBY_CLOSE_SUMMARY)
         {
            var ackNative:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
            var ackSaved:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
            var ackOwner:String = CFunctionContainerCommon.GetPlayerID();
            var ackKey:String = ackNative.length == 3 ? String(ackNative[2]) : "";
            var ackSerial:Number = ackNative.length == 3 ? Number(ackNative[1]) : 0;
            var ackValid:Boolean = ackNative.length == 3 && ackNative[0] == "v1" && ackSerial > 0 && ackSerial <= Number("4294967295") && ackSerial == Math.floor(ackSerial) && ackKey.length == 16 && ackKey != "0000000000000000" && ackOwner.length > 0;
            var ackDigit:int = 0;
            while(ackValid && ackDigit < ackKey.length)
            {
               if("0123456789abcdef".indexOf(ackKey.charAt(ackDigit)) < 0)
               {
                  ackValid = false;
               }
               ackDigit++;
            }
            var ackPlaylist:PlayListObj = ackSaved.length == 5 ? this.getPlayListObj(int(ackSaved[3])) : null;
            var ackRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
            var ackFilter:Object = this._currentQuickMatchFilter;
            var ackOwn:Boolean = false;
            if(ackRoom != null && ackRoom.forces != null && ackRoom.forces.force0 != null && ackRoom.forces.force1 != null)
            {
               var ackMembers:Array = ackRoom.forces.force0.concat(ackRoom.forces.force1);
               for each(var ackMember in ackMembers)
               {
                  if(ackMember != null && ackMember["_localQuickNativeRoleKnown"] == true && ackMember["_localQuickNativeMemberID"] == String(ackMember.id) && Boolean(this.getSyncDP(SyncDataID.IS_ME,[ackMember])))
                  {
                     ackOwn = true;
                  }
               }
            }
            if(ScaleformUtil.isScaleform && ackValid && this.isInRoom() && CFunctionContainer.IsRankedMatch() && ackOwn && ackSaved.length == 5 && ackSaved[0] == "v1" && ackSaved[1] == ackOwner && ackSaved[2] == ackKey && CFunctionContainerCommon.Dvar_GetString("codol_quick_return") == "v1|" + ackOwner + "|" + ackKey && ackPlaylist != null && ackPlaylist.name == "Quick Match" && ackPlaylist.type == 1 && ackPlaylist.minPartySize == 1 && ackPlaylist.maxPartySize == 6 && ackPlaylist.maxRoomSize == 12 && (ackFilter == null || ackFilter["_localQuickSharedInvalidated"] != true && ackFilter["_localQuickExternal"] != true && ackFilter["_localQuickExternal20"] != true && (ackFilter.playListID == int(ackSaved[3]) && ackFilter.playListName == "Quick Match" || ackFilter.playListID == -1 && ackFilter.type == RoomListType.NONE && ackFilter.playListName.length == 0)))
            {
               if(ackFilter == null)
               {
                  this._currentQuickMatchFilter = new QuickMatchFilterObj();
                  ackFilter = this._currentQuickMatchFilter;
               }
               ackFilter.type = RoomListType.RANKED;
               ackFilter.playListID = int(ackSaved[3]);
               ackFilter.playListName = ackPlaylist.name;
               ackFilter.leagueName = String(ackSaved[4]);
               ackFilter["_localQuickSharedJoined"] = true;
               ackFilter["_localQuickSharedLineage"] = true;
               ackFilter["_localQuickManagedSummary"] = true;
               ackRoom["_localQuickSharedLineage"] = true;
            }
         }
         if(cEvt.type == BCEvent.LOBBY_CLOSE_SUMMARY && this._currentQuickMatchFilter != null && this._currentQuickMatchFilter["_localQuickManagedSummary"] == true)
         {
            var quickFilter:Object = this._currentQuickMatchFilter;
            var quickRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
            var durableNative:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
            var durableOwner:String = CFunctionContainerCommon.GetPlayerID();
            var durableKey:String = durableNative.length == 3 ? String(durableNative[2]) : "";
            var durableSerial:Number = durableNative.length == 3 ? Number(durableNative[1]) : 0;
            var durableValid:Boolean = durableNative.length == 3 && durableNative[0] == "v1" && durableSerial > 0 && durableSerial <= Number("4294967295") && durableSerial == Math.floor(durableSerial) && durableKey.length == 16 && durableKey != "0000000000000000" && durableOwner.length > 0;
            var durableDigit:int = 0;
            while(durableValid && durableDigit < durableKey.length)
            {
               if("0123456789abcdef".indexOf(durableKey.charAt(durableDigit)) < 0)
               {
                  durableValid = false;
               }
               durableDigit++;
            }
            var durableSaved:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
            if(!durableValid || durableSaved.length != 5 || durableSaved[1] != durableOwner || durableSaved[2] != durableKey)
            {
               return;
            }
            if(quickFilter["_localQuickSharedInvalidated"] == true || quickFilter["_localQuickSharedLineage"] != true || !this.isInRoom() || !CFunctionContainer.IsRankedMatch() || quickRoom == null || quickRoom["_localQuickSharedLineage"] != true)
            {
               return;
            }
            if(queuedHumanIDs.length < queuedRequired)
            {
               quickFilter["_localQuickHumanWaiting"] = true;
               quickFilter["_localQuickManagedSummary"] = false;
               quickFilter["_localQuickVoteOpen"] = false;
               quickFilter["_localQuickSharedRoomSeen"] = false;
               quickFilter["_localQuickSharedReturning"] = false;
               quickFilter["_localQuickSharedReturnPending"] = false;
               CFunctionContainerCommon.setdvar("codol_quick_return","");
               CFunctionContainer.SetReadyState(false);
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
               this.changeRankedMatchStatus(RankedMatchStatus.IN_PROGRESS);
               this.startRankedMatchPassedTimeTimer();
               return;
            }
            quickFilter["_localQuickManagedSummary"] = false;
            quickFilter["_localQuickVoteRequireHostFalse"] = true;
            var resultHostFalse:Boolean = CFunctionContainerCommon.Dvar_GetString("codol_quick_return_host_false") == "v1|" + durableOwner + "|" + durableKey;
            CFunctionContainerCommon.setdvar("codol_quick_return","");
            CFunctionContainerCommon.setdvar("codol_quick_return_host_false","");
            if(quickFilter["_localQuickVoteOpen"] != true)
            {
               quickFilter["_localQuickVoteOpen"] = true;
               quickFilter["_localQuickVoteTicks"] = 0;
               quickFilter["_localQuickVoteStartMS"] = getTimer();
               quickFilter["_localQuickVoteSent"] = false;
               quickFilter["_localQuickVoteHostFalse"] = resultHostFalse;
               quickFilter["_localQuickVoteLastSendMS"] = -1000;
               quickRoom["_localQuickHostFalseSeen"] = false;
               quickFilter["_localQuickVoteFinal"] = false;
               quickRoom["_localQuickVoteFinal"] = -1;
               quickFilter["_localQuickSharedRoomSeen"] = true;
               quickFilter["_localQuickSharedBookingStarted"] = false;
               quickFilter["_localQuickSharedReturning"] = false;
               quickFilter["_localQuickSharedReturnPending"] = false;
               this._isRankedMatchInRoomDone = true;
               this.stopRankedMatchGoRoomTimer();
               this.stopRankedMatchGoGameTimer();
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.BACK_FROM_GAME);
               this.enableLeaveRoom(true);
               CFunctionContainer.SetReadyState(false);
               if(this.getSyncDP(SyncDataID.IS_ROOM_HOST))
               {
                  CFunctionContainer.ChangeRoomInfo({"mode":"playlist"},"mode");
               }
               this.startRankedMatchPassedTimeTimer();
            }
            return;
         }
         if(cEvt.type == BCEvent.LOBBY_CLOSE_SUMMARY)
         {
            var externalFilter:Object = this._currentQuickMatchFilter;
            if(externalFilter == null || externalFilter["_localQuickExternal"] != true || externalFilter["_localQuickExternalReturning"] != true || externalFilter["_localQuickExternalSummary"] != true || externalFilter["_localQuickSharedInvalidated"] == true)
            {
               return;
            }
            externalFilter["_localQuickExternalSummary"] = false;
            if(this.getSyncDP(SyncDataID.IS_IN_PARTY) && !this.getSyncDP(SyncDataID.IS_PARTY_HOST))
            {
               return;
            }
            if(!this.isInRoom())
            {
               return;
            }
            var externalRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
            var externalOwn:Boolean = false;
            if(externalRoom != null && !externalRoom.isPlayList && externalRoom.forces != null)
            {
               var externalMembers:Array = externalRoom.forces.force0.concat(externalRoom.forces.force1);
               for each(var externalMember in externalMembers)
               {
                  if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[externalMember])))
                  {
                     externalOwn = true;
                  }
               }
            }
            if(!externalOwn)
            {
               return;
            }
            externalFilter["_localQuickExternalResumeRequested"] = true;
            this.leaveRoom(true,false,Boolean(this.getSyncDP(SyncDataID.IS_IN_PARTY)) && Boolean(this.getSyncDP(SyncDataID.IS_PARTY_HOST)));
            return;
         }
         durableNative = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
         durableOwner = CFunctionContainerCommon.GetPlayerID();
         durableKey = durableNative.length == 3 ? String(durableNative[2]) : "";
         durableSerial = durableNative.length == 3 ? Number(durableNative[1]) : 0;
         durableValid = durableNative.length == 3 && durableNative[0] == "v1" && durableSerial > 0 && durableSerial <= Number("4294967295") && durableSerial == Math.floor(durableSerial) && durableKey.length == 16 && durableKey != "0000000000000000" && durableOwner.length > 0;
         durableDigit = 0;
         while(durableValid && durableDigit < durableKey.length)
         {
            if("0123456789abcdef".indexOf(durableKey.charAt(durableDigit)) < 0)
            {
               durableValid = false;
            }
            durableDigit++;
         }
         durableSaved = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
         var durableReturn:String = CFunctionContainerCommon.Dvar_GetString("codol_quick_return");
         var durablePlaylist:PlayListObj = durableSaved.length == 5 ? this.getPlayListObj(int(durableSaved[3])) : null;
         var durableOwn:Boolean = false;
         var durableRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(durableRoom != null && durableRoom.forces != null)
         {
            var durableMembers:Array = durableRoom.forces.force0.concat(durableRoom.forces.force1);
            for each(var durableMember in durableMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[durableMember])))
               {
                  durableOwn = true;
               }
            }
         }
         var durableFilter:Object = this._currentQuickMatchFilter;
         var durableEligible:Boolean = ScaleformUtil.isScaleform && durableValid && this.isInRoom() && CFunctionContainer.IsRankedMatch() && durableOwn && durableSaved.length == 5 && durableSaved[0] == "v1" && durableSaved[1] == durableOwner && durableSaved[2] == durableKey && durablePlaylist != null && durablePlaylist.name == "Quick Match" && durablePlaylist.type == 1 && durablePlaylist.minPartySize == 1 && durablePlaylist.maxPartySize == 6 && durablePlaylist.maxRoomSize == 12 && (durableFilter == null || durableFilter["_localQuickSharedInvalidated"] != true && durableFilter["_localQuickExternal"] != true && durableFilter["_localQuickExternal20"] != true && (durableFilter.playListID == int(durableSaved[3]) && durableFilter.playListName == "Quick Match" || durableFilter.playListID == -1 && durableFilter.type == RoomListType.NONE && durableFilter.playListName.length == 0));
         if(durableEligible && (durableFilter == null || durableFilter["_localQuickSharedLineage"] != true))
         {
            if(durableFilter == null)
            {
               this._currentQuickMatchFilter = new QuickMatchFilterObj();
               durableFilter = this._currentQuickMatchFilter;
            }
            if(durableFilter["_localQuickRecoveryKey"] != durableKey)
            {
               durableFilter["_localQuickRecoveryKey"] = durableKey;
               durableFilter["_localQuickRecoverySerial"] = durableSerial;
               this.startRankedMatchPassedTimeTimer();
            }
            else if(false && durableSerial >= Number(durableFilter["_localQuickRecoverySerial"]) + 2)
            {
               durableFilter.type = RoomListType.RANKED;
               durableFilter.playListID = int(durableSaved[3]);
               durableFilter.playListName = durablePlaylist.name;
               durableFilter.leagueName = String(durableSaved[4]);
               durableFilter["_localQuickSharedJoined"] = true;
               durableFilter["_localQuickSharedLineage"] = true;
               durableFilter["_localQuickSharedRoomSeen"] = true;
               durableFilter["_localQuickSharedBookingStarted"] = true;
               durableFilter["_localQuickSharedMap"] = durableRoom.currentMap.id;
               durableFilter["_localQuickSharedMode"] = durableRoom.currentMatchMode.id;
               if(durableReturn == "v1|" + durableOwner + "|" + durableKey)
               {
                  durableFilter["_localQuickSharedReturnPending"] = true;
                  durableFilter["_localQuickSharedReturning"] = true;
               }
               durableRoom["_localQuickSharedLineage"] = true;
               this._isRankedMatchInRoomDone = true;
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.BACK_FROM_GAME);
               this.enableLeaveRoom(true);
            }
         }
         quickRoom = this.getSyncDP(SyncDataID.ROOM_INFO);
         quickFilter = this._currentQuickMatchFilter;
         var quickEntry:PlayListObj = quickFilter != null ? this.getPlayListObj(quickFilter.playListID) : null;
         var quickOwnMember:Boolean = false;
         if(quickRoom != null && quickRoom.forces != null)
         {
            var quickMembers:Array = quickRoom.forces.force0.concat(quickRoom.forces.force1);
            for each(var quickMember in quickMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[quickMember])))
               {
                  quickOwnMember = true;
               }
            }
         }
         var quickSelected:Boolean = quickFilter != null && quickFilter["_localQuickSharedInvalidated"] != true && quickFilter.type == RoomListType.RANKED && quickFilter.playListName == "Quick Match" && (quickEntry != null && quickEntry.name == "Quick Match" && quickEntry.type == 1 && quickEntry.minPartySize == 1 && quickEntry.maxPartySize == 6 && quickEntry.maxRoomSize == 12);
         if(quickSelected && quickFilter["_localQuickExternal20"] == true && quickFilter["_localQuickSharedJoined"] != true && quickFilter["_localQuickSharedLineage"] != true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch())
         {
            quickFilter["_localQuickExternal"] = true;
            quickFilter["_localQuickExternal20"] = false;
         }
         var externalQuick:Boolean = quickSelected && quickFilter["_localQuickExternal"] == true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch();
         if(ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true && (quickFilter["_localQuickSharedJoined"] == true || quickRoom.isPlayList && quickRoom.playListID == quickFilter.playListID))
         {
            quickFilter["_localQuickSharedLineage"] = true;
         }
         var sharedQuick:Boolean = ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickSharedLineage"] == true && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true;
         if(quickRoom != null)
         {
            quickRoom["_localQuickSharedLineage"] = sharedQuick;
         }
         if(sharedQuick && durableValid && CFunctionContainer.IsRankedMatch())
         {
            CFunctionContainerCommon.setdvar("codol_quick_lineage","v1|" + durableOwner + "|" + durableKey + "|" + String(quickFilter.playListID) + "|" + String(quickFilter.leagueName));
         }
         var isPlayingOnBoard:Boolean = false;
         var mapID:String = null;
         var coopType:int = 0;
         var isCyborg:Boolean = false;
         var matchType:uint = 0;
         var status:String = null;
         var canLeaveRoom:Boolean = false;
         var state:int = 0;
         var maps:Array = null;
         var mapIndex:int = 0;
         var idx:int = 0;
         var tempDat:Array = null;
         Logger.print("MatchController:::onBackToRoomHandle");
         switch(cEvt.myPara)
         {
            case BackToRoomType.FIRST_PAGE:
               var durableActualRoom:Object = CFunctionContainer.GetRoomInfo();
               var durableSavedKey:String = durableSaved.length == 5 ? String(durableSaved[2]) : "";
               var durableSavedValid:Boolean = durableSavedKey.length == 16 && durableSavedKey != "0000000000000000";
               var durableSavedDigit:int = 0;
               while(durableSavedValid && durableSavedDigit < durableSavedKey.length)
               {
                  if("0123456789abcdef".indexOf(durableSavedKey.charAt(durableSavedDigit)) < 0)
                  {
                     durableSavedValid = false;
                  }
                  durableSavedDigit++;
               }
               if(durableSavedValid && durableSaved.length == 5 && durableSaved[0] == "v1" && durableSaved[1] == durableOwner && durablePlaylist != null && durablePlaylist.name == "Quick Match" && durablePlaylist.type == 1 && durablePlaylist.minPartySize == 1 && durablePlaylist.maxPartySize == 6 && durablePlaylist.maxRoomSize == 12 && (!durableValid || durableSaved[2] == durableKey) && this.isInRoom() && CFunctionContainer.IsRankedMatch() && CFunctionContainerCommon.Dvar_GetInt("ui_eog_show") == 1 && durableActualRoom != null && durableActualRoom.mapID == CFunctionContainerCommon.Dvar_GetString("ui_played_mapname") && String(durableActualRoom.mapID).length > 0)
               {
                  if(CFunctionContainerCommon.Dvar_GetString("codol_quick_return") != "v1|" + durableOwner + "|" + String(durableSaved[2]))
                  {
                     CFunctionContainerCommon.setdvar("codol_quick_return_host_false","");
                  }
                  CFunctionContainerCommon.setdvar("codol_quick_return","v1|" + durableOwner + "|" + String(durableSaved[2]));
                  this.startRankedMatchPassedTimeTimer();
               }
               if(quickFilter != null && quickSelected && quickFilter["_localQuickSharedJoined"] == true && quickFilter["_localQuickSharedLineage"] == true && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true)
               {
                  quickFilter["_localQuickSharedReturnPending"] = true;
               }
               if(externalQuick)
               {
                  quickFilter["_localQuickExternalReturning"] = true;
               }
               isPlayingOnBoard = CFunctionContainerCommon.Dvar_GetBool("ui_onboard_playing") as Boolean;
               mapID = isPlayingOnBoard ? "" : CFunctionContainerCommon.Dvar_GetString("ui_played_mapname");
               Logger.print("MatchController:::onBackToRoomHandle isPlayingOnBoard: " + isPlayingOnBoard);
               Logger.print("MatchController:::onBackToRoomHandle mapID: " + mapID);
               coopType = mapID.length > 0 ? int(this.getMapCoopType({"id":mapID})) : 0;
               isCyborg = MatchService.getInstance().isCyborgMap(mapID);
               CFunctionContainerCommon.setdvar("ui_onboard_playing","0");
               matchType = QuickMatchType.NONE;
               if(isCyborg)
               {
                  matchType = QuickMatchType.CYBORG;
               }
               else if(coopType > CoopMapType.ANY)
               {
                  matchType = QuickMatchType.COOP;
               }
               else
               {
                  matchType = QuickMatchType.MP;
               }
               this.setCurrentMatchType(matchType);
               if(this.isInRoom())
               {
                  CFunctionContainer.RefreshInRoom();
                  quickRoom = this.getSyncDP(SyncDataID.ROOM_INFO);
                  quickFilter = this._currentQuickMatchFilter;
                  quickEntry = quickFilter != null ? this.getPlayListObj(quickFilter.playListID) : null;
                  quickOwnMember = false;
                  if(quickRoom != null && quickRoom.forces != null)
                  {
                     quickMembers = quickRoom.forces.force0.concat(quickRoom.forces.force1);
                     for each(quickMember in quickMembers)
                     {
                        if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[quickMember])))
                        {
                           quickOwnMember = true;
                        }
                     }
                  }
                  quickSelected = quickFilter != null && quickFilter["_localQuickSharedInvalidated"] != true && quickFilter.type == RoomListType.RANKED && quickFilter.playListName == "Quick Match" && (quickEntry != null && quickEntry.name == "Quick Match" && quickEntry.type == 1 && quickEntry.minPartySize == 1 && quickEntry.maxPartySize == 6 && quickEntry.maxRoomSize == 12);
                  if(quickSelected && quickFilter["_localQuickExternal20"] == true && quickFilter["_localQuickSharedJoined"] != true && quickFilter["_localQuickSharedLineage"] != true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch())
                  {
                     quickFilter["_localQuickExternal"] = true;
                     quickFilter["_localQuickExternal20"] = false;
                  }
                  externalQuick = quickSelected && quickFilter["_localQuickExternal"] == true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch();
                  if(ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true && (quickFilter["_localQuickSharedJoined"] == true || quickRoom.isPlayList && quickRoom.playListID == quickFilter.playListID))
                  {
                     quickFilter["_localQuickSharedLineage"] = true;
                  }
                  sharedQuick = ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickSharedLineage"] == true && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true;
                  if(quickRoom != null)
                  {
                     quickRoom["_localQuickSharedLineage"] = sharedQuick;
                  }
                  if(sharedQuick && quickFilter["_localQuickSharedReturnPending"] == true && CFunctionContainer.IsRankedMatch())
                  {
                     quickFilter["_localQuickSharedReturning"] = true;
                  }
                  if(this.isRankedMatch() && !externalQuick)
                  {
                     status = RankedMatchStatus.IDLE;
                     canLeaveRoom = true;
                     state = CFunctionContainer.GetRankedMatchState();
                     switch(state)
                     {
                        case 0:
                        case 1:
                           status = RankedMatchStatus.IN_PROGRESS_IN_ROOM;
                           canLeaveRoom = false;
                           break;
                        case 2:
                        case 3:
                           status = RankedMatchStatus.BACK_FROM_GAME;
                     }
                     if(sharedQuick)
                     {
                        status = RankedMatchStatus.BACK_FROM_GAME;
                        canLeaveRoom = true;
                        if(this._currentQuickMatchFilter != null)
                        {
                           this._currentQuickMatchFilter["_localQuickSharedReturning"] = true;
                        }
                     }
                     if(status == RankedMatchStatus.IN_PROGRESS_IN_ROOM)
                     {
                        this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
                     }
                     else
                     {
                        this.goRoom(true,"",true,true);
                     }
                     this.changeRankedMatchStatus(status);
                     this.enableLeaveRoom(canLeaveRoom);
                  }
                  else
                  {
                     this.goRoom(true,"",true,true);
                  }
               }
               else
               {
                  if(coopType == CoopMapType.STORY || coopType == CoopMapType.SPEC_OPS)
                  {
                     if(coopType == CoopMapType.SPEC_OPS)
                     {
                        if(!this.getSyncDP(SyncDataID.IS_NOTIFIED,[UINotifications.SPEC_OPS]))
                        {
                           coopType = int(CoopMapType.STORY);
                        }
                     }
                     this.broadcast(BCEvent.EVT_GOTO_QUICKMATCH_COOP);
                     maps = this.listSameTypeMap(mapID);
                     mapIndex = 0;
                     idx = 0;
                     while(idx < maps.length)
                     {
                        if(maps[idx].id == mapID)
                        {
                           mapIndex = idx;
                           break;
                        }
                        idx++;
                     }
                     this.broadcast(BCEvent.SET_COOP_MAP_FILTER,{
                        "filter":coopType,
                        "selectedMapIndex":mapIndex
                     });
                  }
                  else if(isCyborg)
                  {
                     if(!this.isCyborgTutorialComplete())
                     {
                        this.setBindData(AsyncTaskID.TUTORIAL_TYPE,Common.TUTORIAL_ZM);
                        this.gotoPage(CodoPageViewDB.PAGE_TUTORIAL_CHOOSER);
                     }
                     else
                     {
                        if(!this.getSyncDP(SyncDataID.IS_NOTIFIED,[UINotifications.ZM_ONBOARD]))
                        {
                           this.broadcast(BCEvent.ADD_NEW_UNLOCK,UINotifications.ZM_ONBOARD);
                        }
                        this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
                     }
                  }
                  else if(mapID.length > 0)
                  {
                     if(!this.getSyncDP(SyncDataID.IS_MP_TUTORIAL_COMPLETE))
                     {
                        this.setBindData(AsyncTaskID.TUTORIAL_TYPE,Common.TUTORIAL_MP);
                        this.gotoPage(CodoPageViewDB.PAGE_TUTORIAL_CHOOSER);
                     }
                     else if(!this.getSyncDP(SyncDataID.IS_NOTIFIED,[UINotifications.MP_ONBOARD]))
                     {
                        this.broadcast(BCEvent.ADD_NEW_UNLOCK,UINotifications.MP_ONBOARD);
                        this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
                     }
                     else
                     {
                        this.gotoPage(CodoPageViewDB.PAGE_MAIN_MATCH_MPCT_PLAYLIST);
                     }
                  }
                  else
                  {
                     this.gotoPage(CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW);
                  }
                  this.onPlayMainMenuSound(null);
               }
               if((this.getSyncDP(SyncDataID.EOG_IS_DATA_UPDATED) || isCyborg) && mapID != "zm_zombie_training" && CFunctionContainerCommon.Dvar_GetInt("ui_eog_show") == 1)
               {
                  Logger.print("MatchController::show eog:" + CFunctionContainerCommon.Dvar_GetInt("ui_eog_show"));
                  this.broadcast(BCEvent.EVT_ENABLE_PREV_MATCH_SUMMARY,{"isCyborg":isCyborg});
                  this._showLobbySummaryTimer = new Timer(this._isPlayerStatsReady ? 200 : 1200,1);
                  this._showLobbySummaryTimer.addEventListener(TimerEvent.TIMER_COMPLETE,this.onShowLobbySummaryTimerComplete);
                  this._showLobbySummaryTimer.start();
                  CFunctionContainerCommon.setdvar("ui_eog_show","0");
               }
               else
               {
                  this.clearEOGDvars();
               }
               if(matchType == QuickMatchType.COOP && coopType == CoopMapType.STORY)
               {
                  tempDat = this.getSyncDP(SyncDataID.CAREER_UNLOCK_OBJ_LEVEL_UP);
                  if(tempDat.length > 0)
                  {
                     this.broadcast(BCEvent.EVT_ON_UNLOCKED_OBJ_LISTED,tempDat);
                  }
               }
               break;
            case BackToRoomType.RANKED_MATCH:
               this.goRoom(true,"",true,false);
               if(this._currentMatchType == QuickMatchType.MP)
               {
                  if(Boolean(this._rankedMatchGoRoomTimer) && this._rankedMatchGoRoomTimer.running)
                  {
                     this._isRankedMatchInRoomDone = true;
                     this.enableLeaveRoom(false);
                     this.changeRankedMatchStatus(RankedMatchStatus.READY_IN_ROOM);
                  }
               }
               break;
            case BackToRoomType.UNRANKED_MATCH:
               this.goRoom(true,"",false,false);
         }
      }
      
      private function onShowLobbySummaryTimerComplete(evt:TimerEvent) : void
      {
         var queuedHumanIDs:Array = [];
         var queuedRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var queuedEntry:Object = this._currentQuickMatchFilter != null ? this.getPlayListObj(this._currentQuickMatchFilter.playListID) : null;
         var queuedRequired:int = queuedEntry != null && queuedEntry.name == "Quick Match" && queuedEntry.type == 1 && queuedEntry.minPartySize == 1 && queuedEntry.maxPartySize == 6 && queuedEntry.maxRoomSize == 12 && queuedEntry.minRoomSize == 1 ? 1 : 2;
         if(queuedRoom != null && queuedRoom.forces != null && queuedRoom.forces.force0 != null && queuedRoom.forces.force1 != null)
         {
            var queuedMembers:Array = queuedRoom.forces.force0.concat(queuedRoom.forces.force1);
            for each(var queuedHuman in queuedMembers)
            {
               if(queuedHuman != null && queuedHuman["_localQuickNativeRoleKnown"] == true && queuedHuman["_localQuickNativeMemberID"] == String(queuedHuman.id) && String(queuedHuman.id).length > 0 && queuedHumanIDs.indexOf(String(queuedHuman.id)) < 0)
               {
                  queuedHumanIDs.push(String(queuedHuman.id));
               }
            }
         }
         var durableNative:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
         var durableOwner:String = CFunctionContainerCommon.GetPlayerID();
         var durableKey:String = durableNative.length == 3 ? String(durableNative[2]) : "";
         var durableSerial:Number = durableNative.length == 3 ? Number(durableNative[1]) : 0;
         var durableValid:Boolean = durableNative.length == 3 && durableNative[0] == "v1" && durableSerial > 0 && durableSerial <= Number("4294967295") && durableSerial == Math.floor(durableSerial) && durableKey.length == 16 && durableKey != "0000000000000000" && durableOwner.length > 0;
         var durableDigit:int = 0;
         while(durableValid && durableDigit < durableKey.length)
         {
            if("0123456789abcdef".indexOf(durableKey.charAt(durableDigit)) < 0)
            {
               durableValid = false;
            }
            durableDigit++;
         }
         var durableSaved:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
         var durableReturn:String = CFunctionContainerCommon.Dvar_GetString("codol_quick_return");
         var durablePlaylist:PlayListObj = durableSaved.length == 5 ? this.getPlayListObj(int(durableSaved[3])) : null;
         var durableOwn:Boolean = false;
         var durableRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(durableRoom != null && durableRoom.forces != null)
         {
            var durableMembers:Array = durableRoom.forces.force0.concat(durableRoom.forces.force1);
            for each(var durableMember in durableMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[durableMember])))
               {
                  durableOwn = true;
               }
            }
         }
         var durableFilter:Object = this._currentQuickMatchFilter;
         var durableEligible:Boolean = ScaleformUtil.isScaleform && durableValid && this.isInRoom() && CFunctionContainer.IsRankedMatch() && durableOwn && durableSaved.length == 5 && durableSaved[0] == "v1" && durableSaved[1] == durableOwner && durableSaved[2] == durableKey && durablePlaylist != null && durablePlaylist.name == "Quick Match" && durablePlaylist.type == 1 && durablePlaylist.minPartySize == 1 && durablePlaylist.maxPartySize == 6 && durablePlaylist.maxRoomSize == 12 && (durableFilter == null || durableFilter["_localQuickSharedInvalidated"] != true && durableFilter["_localQuickExternal"] != true && durableFilter["_localQuickExternal20"] != true && (durableFilter.playListID == int(durableSaved[3]) && durableFilter.playListName == "Quick Match" || durableFilter.playListID == -1 && durableFilter.type == RoomListType.NONE && durableFilter.playListName.length == 0));
         if(durableEligible && (durableFilter == null || durableFilter["_localQuickSharedLineage"] != true))
         {
            if(durableFilter == null)
            {
               this._currentQuickMatchFilter = new QuickMatchFilterObj();
               durableFilter = this._currentQuickMatchFilter;
            }
            if(durableFilter["_localQuickRecoveryKey"] != durableKey)
            {
               durableFilter["_localQuickRecoveryKey"] = durableKey;
               durableFilter["_localQuickRecoverySerial"] = durableSerial;
               this.startRankedMatchPassedTimeTimer();
            }
            else if(false && durableSerial >= Number(durableFilter["_localQuickRecoverySerial"]) + 2)
            {
               durableFilter.type = RoomListType.RANKED;
               durableFilter.playListID = int(durableSaved[3]);
               durableFilter.playListName = durablePlaylist.name;
               durableFilter.leagueName = String(durableSaved[4]);
               durableFilter["_localQuickSharedJoined"] = true;
               durableFilter["_localQuickSharedLineage"] = true;
               durableFilter["_localQuickSharedRoomSeen"] = true;
               durableFilter["_localQuickSharedBookingStarted"] = true;
               durableFilter["_localQuickSharedMap"] = durableRoom.currentMap.id;
               durableFilter["_localQuickSharedMode"] = durableRoom.currentMatchMode.id;
               if(durableReturn == "v1|" + durableOwner + "|" + durableKey)
               {
                  durableFilter["_localQuickSharedReturnPending"] = true;
                  durableFilter["_localQuickSharedReturning"] = true;
               }
               durableRoom["_localQuickSharedLineage"] = true;
               this._isRankedMatchInRoomDone = true;
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.BACK_FROM_GAME);
               this.enableLeaveRoom(true);
            }
         }
         var quickRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var quickFilter:Object = this._currentQuickMatchFilter;
         var quickEntry:PlayListObj = quickFilter != null ? this.getPlayListObj(quickFilter.playListID) : null;
         var quickOwnMember:Boolean = false;
         if(quickRoom != null && quickRoom.forces != null)
         {
            var quickMembers:Array = quickRoom.forces.force0.concat(quickRoom.forces.force1);
            for each(var quickMember in quickMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[quickMember])))
               {
                  quickOwnMember = true;
               }
            }
         }
         var quickSelected:Boolean = quickFilter != null && quickFilter["_localQuickSharedInvalidated"] != true && quickFilter.type == RoomListType.RANKED && quickFilter.playListName == "Quick Match" && (quickEntry != null && quickEntry.name == "Quick Match" && quickEntry.type == 1 && quickEntry.minPartySize == 1 && quickEntry.maxPartySize == 6 && quickEntry.maxRoomSize == 12);
         if(quickSelected && quickFilter["_localQuickExternal20"] == true && quickFilter["_localQuickSharedJoined"] != true && quickFilter["_localQuickSharedLineage"] != true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch())
         {
            quickFilter["_localQuickExternal"] = true;
            quickFilter["_localQuickExternal20"] = false;
         }
         var externalQuick:Boolean = quickSelected && quickFilter["_localQuickExternal"] == true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch();
         if(ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true && (quickFilter["_localQuickSharedJoined"] == true || quickRoom.isPlayList && quickRoom.playListID == quickFilter.playListID))
         {
            quickFilter["_localQuickSharedLineage"] = true;
         }
         var sharedQuick:Boolean = ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickSharedLineage"] == true && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true;
         if(quickRoom != null)
         {
            quickRoom["_localQuickSharedLineage"] = sharedQuick;
         }
         if(sharedQuick && durableValid && CFunctionContainer.IsRankedMatch())
         {
            CFunctionContainerCommon.setdvar("codol_quick_lineage","v1|" + durableOwner + "|" + durableKey + "|" + String(quickFilter.playListID) + "|" + String(quickFilter.leagueName));
         }
         if(durableEligible && durableReturn == "v1|" + durableOwner + "|" + durableKey && !sharedQuick)
         {
            this._showLobbySummaryTimer = new Timer(1000,1);
            this._showLobbySummaryTimer.addEventListener(TimerEvent.TIMER_COMPLETE,this.onShowLobbySummaryTimerComplete);
            this._showLobbySummaryTimer.start();
            return;
         }
         var managedReturnPending:Boolean = quickFilter != null && quickSelected && quickFilter["_localQuickSharedJoined"] == true && quickFilter["_localQuickSharedLineage"] == true && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true && quickFilter["_localQuickSharedReturnPending"] == true;
         if(managedReturnPending && (!sharedQuick || !CFunctionContainer.IsRankedMatch() || !CFunctionContainer.IsPlayerStatsReady()))
         {
            this._showLobbySummaryTimer = new Timer(1000,1);
            this._showLobbySummaryTimer.addEventListener(TimerEvent.TIMER_COMPLETE,this.onShowLobbySummaryTimerComplete);
            this._showLobbySummaryTimer.start();
            return;
         }
         if(managedReturnPending && sharedQuick)
         {
            quickFilter["_localQuickSharedReturning"] = true;
         }
         if((sharedQuick || externalQuick && quickFilter["_localQuickExternalReturning"] == true) && !CFunctionContainer.IsPlayerStatsReady())
         {
            this._showLobbySummaryTimer = new Timer(1000,1);
            this._showLobbySummaryTimer.addEventListener(TimerEvent.TIMER_COMPLETE,this.onShowLobbySummaryTimerComplete);
            this._showLobbySummaryTimer.start();
            return;
         }
         this._showLobbySummaryTimer = null;
         Logger.print("MatchController::show eog: goto page");
         if(externalQuick && quickFilter["_localQuickExternalReturning"] == true)
         {
            quickFilter["_localQuickExternalSummary"] = true;
         }
         this.gotoPage(CodoPageViewDB.PAGE_ADV_POPUP_LOBBY_SUMMARY);
         if(sharedQuick && quickFilter["_localQuickSharedReturning"] == true)
         {
            quickFilter["_localQuickManagedSummary"] = true;
         }
      }
      
      private function clearEOGDvars() : void
      {
         var MAX_CHALLENGES:int = 6;
         var i:int = 0;
         while(i < MAX_CHALLENGES)
         {
            CFunctionContainerCommon.setdvar("ui_eog_ch" + String(i),"");
            i++;
         }
      }
      
      private function onOnlyRoomInfoHandle(cEvt:ParaEvent) : void
      {
         var queuedHumanIDs:Array = [];
         var queuedRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var queuedEntry:Object = this._currentQuickMatchFilter != null ? this.getPlayListObj(this._currentQuickMatchFilter.playListID) : null;
         var queuedRequired:int = queuedEntry != null && queuedEntry.name == "Quick Match" && queuedEntry.type == 1 && queuedEntry.minPartySize == 1 && queuedEntry.maxPartySize == 6 && queuedEntry.maxRoomSize == 12 && queuedEntry.minRoomSize == 1 ? 1 : 2;
         if(queuedRoom != null && queuedRoom.forces != null && queuedRoom.forces.force0 != null && queuedRoom.forces.force1 != null)
         {
            var queuedMembers:Array = queuedRoom.forces.force0.concat(queuedRoom.forces.force1);
            for each(var queuedHuman in queuedMembers)
            {
               if(queuedHuman != null && queuedHuman["_localQuickNativeRoleKnown"] == true && queuedHuman["_localQuickNativeMemberID"] == String(queuedHuman.id) && String(queuedHuman.id).length > 0 && queuedHumanIDs.indexOf(String(queuedHuman.id)) < 0)
               {
                  queuedHumanIDs.push(String(queuedHuman.id));
               }
            }
         }
         var durableNative:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_native_room_context").split("|");
         var durableOwner:String = CFunctionContainerCommon.GetPlayerID();
         var durableKey:String = durableNative.length == 3 ? String(durableNative[2]) : "";
         var durableSerial:Number = durableNative.length == 3 ? Number(durableNative[1]) : 0;
         var durableValid:Boolean = durableNative.length == 3 && durableNative[0] == "v1" && durableSerial > 0 && durableSerial <= Number("4294967295") && durableSerial == Math.floor(durableSerial) && durableKey.length == 16 && durableKey != "0000000000000000" && durableOwner.length > 0;
         var durableDigit:int = 0;
         while(durableValid && durableDigit < durableKey.length)
         {
            if("0123456789abcdef".indexOf(durableKey.charAt(durableDigit)) < 0)
            {
               durableValid = false;
            }
            durableDigit++;
         }
         var durableSaved:Array = CFunctionContainerCommon.Dvar_GetString("codol_quick_lineage").split("|");
         var durableReturn:String = CFunctionContainerCommon.Dvar_GetString("codol_quick_return");
         var durablePlaylist:PlayListObj = durableSaved.length == 5 ? this.getPlayListObj(int(durableSaved[3])) : null;
         var durableOwn:Boolean = false;
         var durableRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         if(durableRoom != null && durableRoom.forces != null)
         {
            var durableMembers:Array = durableRoom.forces.force0.concat(durableRoom.forces.force1);
            for each(var durableMember in durableMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[durableMember])))
               {
                  durableOwn = true;
               }
            }
         }
         var durableFilter:Object = this._currentQuickMatchFilter;
         var durableEligible:Boolean = ScaleformUtil.isScaleform && durableValid && this.isInRoom() && CFunctionContainer.IsRankedMatch() && durableOwn && durableSaved.length == 5 && durableSaved[0] == "v1" && durableSaved[1] == durableOwner && durableSaved[2] == durableKey && durablePlaylist != null && durablePlaylist.name == "Quick Match" && durablePlaylist.type == 1 && durablePlaylist.minPartySize == 1 && durablePlaylist.maxPartySize == 6 && durablePlaylist.maxRoomSize == 12 && (durableFilter == null || durableFilter["_localQuickSharedInvalidated"] != true && durableFilter["_localQuickExternal"] != true && durableFilter["_localQuickExternal20"] != true && (durableFilter.playListID == int(durableSaved[3]) && durableFilter.playListName == "Quick Match" || durableFilter.playListID == -1 && durableFilter.type == RoomListType.NONE && durableFilter.playListName.length == 0));
         if(durableEligible && (durableFilter == null || durableFilter["_localQuickSharedLineage"] != true))
         {
            if(durableFilter == null)
            {
               this._currentQuickMatchFilter = new QuickMatchFilterObj();
               durableFilter = this._currentQuickMatchFilter;
            }
            if(durableFilter["_localQuickRecoveryKey"] != durableKey)
            {
               durableFilter["_localQuickRecoveryKey"] = durableKey;
               durableFilter["_localQuickRecoverySerial"] = durableSerial;
               this.startRankedMatchPassedTimeTimer();
            }
            else if(false && durableSerial >= Number(durableFilter["_localQuickRecoverySerial"]) + 2)
            {
               durableFilter.type = RoomListType.RANKED;
               durableFilter.playListID = int(durableSaved[3]);
               durableFilter.playListName = durablePlaylist.name;
               durableFilter.leagueName = String(durableSaved[4]);
               durableFilter["_localQuickSharedJoined"] = true;
               durableFilter["_localQuickSharedLineage"] = true;
               durableFilter["_localQuickSharedRoomSeen"] = true;
               durableFilter["_localQuickSharedBookingStarted"] = true;
               durableFilter["_localQuickSharedMap"] = durableRoom.currentMap.id;
               durableFilter["_localQuickSharedMode"] = durableRoom.currentMatchMode.id;
               if(durableReturn == "v1|" + durableOwner + "|" + durableKey)
               {
                  durableFilter["_localQuickSharedReturnPending"] = true;
                  durableFilter["_localQuickSharedReturning"] = true;
               }
               durableRoom["_localQuickSharedLineage"] = true;
               this._isRankedMatchInRoomDone = true;
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.BACK_FROM_GAME);
               this.enableLeaveRoom(true);
            }
         }
         var quickRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
         var quickFilter:Object = this._currentQuickMatchFilter;
         var quickEntry:PlayListObj = quickFilter != null ? this.getPlayListObj(quickFilter.playListID) : null;
         var quickOwnMember:Boolean = false;
         if(quickRoom != null && quickRoom.forces != null)
         {
            var quickMembers:Array = quickRoom.forces.force0.concat(quickRoom.forces.force1);
            for each(var quickMember in quickMembers)
            {
               if(Boolean(this.getSyncDP(SyncDataID.IS_ME,[quickMember])))
               {
                  quickOwnMember = true;
               }
            }
         }
         var quickSelected:Boolean = quickFilter != null && quickFilter["_localQuickSharedInvalidated"] != true && quickFilter.type == RoomListType.RANKED && quickFilter.playListName == "Quick Match" && (quickEntry != null && quickEntry.name == "Quick Match" && quickEntry.type == 1 && quickEntry.minPartySize == 1 && quickEntry.maxPartySize == 6 && quickEntry.maxRoomSize == 12);
         if(quickSelected && quickFilter["_localQuickExternal20"] == true && quickFilter["_localQuickSharedJoined"] != true && quickFilter["_localQuickSharedLineage"] != true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch())
         {
            quickFilter["_localQuickExternal"] = true;
            quickFilter["_localQuickExternal20"] = false;
         }
         var externalQuick:Boolean = quickSelected && quickFilter["_localQuickExternal"] == true && ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickRoom != null && !quickRoom.isPlayList && !CFunctionContainer.IsRankedMatch();
         if(ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true && (quickFilter["_localQuickSharedJoined"] == true || quickRoom.isPlayList && quickRoom.playListID == quickFilter.playListID))
         {
            quickFilter["_localQuickSharedLineage"] = true;
         }
         var sharedQuick:Boolean = ScaleformUtil.isScaleform && this.isInRoom() && quickOwnMember && quickSelected && quickFilter["_localQuickSharedLineage"] == true && quickFilter["_localQuickExternal"] != true && quickFilter["_localQuickExternal20"] != true;
         if(quickRoom != null)
         {
            quickRoom["_localQuickSharedLineage"] = sharedQuick;
         }
         if(sharedQuick && durableValid && CFunctionContainer.IsRankedMatch())
         {
            CFunctionContainerCommon.setdvar("codol_quick_lineage","v1|" + durableOwner + "|" + durableKey + "|" + String(quickFilter.playListID) + "|" + String(quickFilter.leagueName));
         }
         if(sharedQuick && queuedHumanIDs.length < queuedRequired)
         {
            if(quickFilter["_localQuickHumanWaiting"] != true)
            {
               quickFilter["_localQuickHumanWaiting"] = true;
               quickFilter["_localQuickVoteOpen"] = false;
               quickFilter["_localQuickVoteFinal"] = false;
               quickFilter["_localQuickVoteSent"] = false;
               quickFilter["_localQuickSharedRoomSeen"] = false;
               quickFilter["_localQuickSharedReturning"] = false;
               quickFilter["_localQuickSharedReturnPending"] = false;
               quickFilter["_localQuickSharedBookingStarted"] = false;
               quickRoom["_localQuickVoteFinal"] = -1;
               CFunctionContainer.SetReadyState(false);
               this.stopRankedMatchGoRoomTimer();
               this.stopRankedMatchGoGameTimer();
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
            }
            this.changeRankedMatchStatus(RankedMatchStatus.IN_PROGRESS);
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         if(sharedQuick && quickFilter["_localQuickHumanWaiting"] == true)
         {
            quickFilter["_localQuickHumanWaiting"] = false;
            quickFilter["_localQuickSharedRoomSeen"] = false;
            quickFilter["_localQuickSharedReturning"] = false;
            quickFilter["_localQuickSharedReturnPending"] = false;
            quickFilter["_localQuickVoteOpen"] = false;
            this.startRankedMatchPassedTimeTimer();
         }
         if(sharedQuick && !durableValid)
         {
            this.startRankedMatchPassedTimeTimer();
            return;
         }
         if(externalQuick)
         {
            if(quickFilter["_localQuickExternalRoomSeen"] != true)
            {
               quickFilter["_localQuickExternalRoomSeen"] = true;
               this.stopRankedMatchGoRoomTimer();
               this.stopRankedMatchGoGameTimer();
               this.stopRankedMatchPassedTimeTimer();
               this._isRankedMatchInRoomDone = true;
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.IDLE);
               this.enableLeaveRoom(true);
            }
            return;
         }
         if(quickFilter != null && quickFilter["_localQuickExternal20"] == true)
         {
            return;
         }
         if(sharedQuick && quickFilter["_localQuickSharedReturnPending"] == true && CFunctionContainer.IsRankedMatch())
         {
            quickFilter["_localQuickSharedReturning"] = true;
         }
         if(sharedQuick)
         {
            if(quickFilter["_localQuickSharedRoomSeen"] != true && quickFilter["_localQuickSharedReturning"] != true && CFunctionContainerCommon.Dvar_GetInt("ui_eog_show") != 1)
            {
               if(quickFilter["_localQuickVoteOpen"] != true)
               {
                  quickFilter["_localQuickVoteOpen"] = true;
                  quickFilter["_localQuickVoteTicks"] = 0;
                  quickFilter["_localQuickVoteStartMS"] = getTimer();
                  quickFilter["_localQuickVoteSent"] = false;
                  quickFilter["_localQuickVoteHostFalse"] = false;
                  quickFilter["_localQuickVoteLastSendMS"] = -1000;
                  quickRoom["_localQuickHostFalseSeen"] = false;
                  quickFilter["_localQuickVoteFinal"] = false;
                  quickRoom["_localQuickVoteFinal"] = -1;
                  quickFilter["_localQuickSharedRoomSeen"] = true;
                  quickFilter["_localQuickSharedBookingStarted"] = false;
                  quickFilter["_localQuickSharedReturning"] = false;
                  quickFilter["_localQuickSharedReturnPending"] = false;
                  this._isRankedMatchInRoomDone = true;
                  this.stopRankedMatchGoRoomTimer();
                  this.stopRankedMatchGoGameTimer();
                  if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
                  {
                     this.goRoom(true);
                  }
                  this.changeRankedMatchStatus(RankedMatchStatus.BACK_FROM_GAME);
                  this.enableLeaveRoom(true);
                  CFunctionContainer.SetReadyState(false);
                  if(this.getSyncDP(SyncDataID.IS_ROOM_HOST))
                  {
                     CFunctionContainer.ChangeRoomInfo({"mode":"playlist"},"mode");
                  }
                  this.startRankedMatchPassedTimeTimer();
               }
            }
            return;
         }
         var roomObj:RoomObj = cEvt.myPara;
         if(roomObj == null)
         {
            return;
         }
         if(this.isRankedMatch() && this._currentQuickMatchFilter == null)
         {
            this._currentQuickMatchFilter = new QuickMatchFilterObj();
            this._currentQuickMatchFilter.league = roomObj.leagueID;
            this._currentQuickMatchFilter.leagueName = LocalizationRoom.LEAGUES[roomObj.leagueID];
            this._currentQuickMatchFilter.playListID = roomObj.playListID;
            this._currentQuickMatchFilter.playListName = roomObj.playListName;
            this.dispatchRankedMatchStatus();
         }
         if(sharedQuick && this._currentQuickMatchFilter != null)
         {
            this._currentQuickMatchFilter.type = RoomListType.RANKED;
            if(this._currentQuickMatchFilter != null && this._currentQuickMatchFilter["_localQuickSharedSelecting"] != true && this._currentQuickMatchFilter["_localQuickSharedRoomSeen"] != true && this._currentQuickMatchFilter["_localQuickSharedReturning"] != true && CFunctionContainerCommon.Dvar_GetInt("ui_eog_show") != 1)
            {
               if(this.getSyncDP(SyncDataID.IS_ROOM_HOST))
               {
                  if(quickFilter["_localQuickSharedSelecting"] == true)
                  {
                     return;
                  }
                  quickPool = [{
                     "map":"mp_afghan",
                     "mode":"conf"
                  },{
                     "map":"mp_afghan",
                     "mode":"ctf"
                  },{
                     "map":"mp_afghan",
                     "mode":"dd"
                  },{
                     "map":"mp_afghan",
                     "mode":"dm"
                  },{
                     "map":"mp_afghan",
                     "mode":"dom"
                  },{
                     "map":"mp_afghan",
                     "mode":"sd"
                  },{
                     "map":"mp_afghan",
                     "mode":"war"
                  },{
                     "map":"mp_bloc_sh",
                     "mode":"conf"
                  },{
                     "map":"mp_bloc_sh",
                     "mode":"ctf"
                  },{
                     "map":"mp_bloc_sh",
                     "mode":"dd"
                  },{
                     "map":"mp_bloc_sh",
                     "mode":"dm"
                  },{
                     "map":"mp_bloc_sh",
                     "mode":"dom"
                  },{
                     "map":"mp_bloc_sh",
                     "mode":"sd"
                  },{
                     "map":"mp_bloc_sh",
                     "mode":"war"
                  },{
                     "map":"mp_bog_sh",
                     "mode":"conf"
                  },{
                     "map":"mp_bog_sh",
                     "mode":"ctf"
                  },{
                     "map":"mp_bog_sh",
                     "mode":"dd"
                  },{
                     "map":"mp_bog_sh",
                     "mode":"dm"
                  },{
                     "map":"mp_bog_sh",
                     "mode":"dom"
                  },{
                     "map":"mp_bog_sh",
                     "mode":"sab"
                  },{
                     "map":"mp_bog_sh",
                     "mode":"war"
                  },{
                     "map":"mp_cargoship_sh",
                     "mode":"conf"
                  },{
                     "map":"mp_cargoship_sh",
                     "mode":"ctf"
                  },{
                     "map":"mp_cargoship_sh",
                     "mode":"dd"
                  },{
                     "map":"mp_cargoship_sh",
                     "mode":"dm"
                  },{
                     "map":"mp_cargoship_sh",
                     "mode":"war"
                  },{
                     "map":"mp_crash_tropical",
                     "mode":"conf"
                  },{
                     "map":"mp_crash_tropical",
                     "mode":"ctf"
                  },{
                     "map":"mp_crash_tropical",
                     "mode":"dd"
                  },{
                     "map":"mp_crash_tropical",
                     "mode":"dm"
                  },{
                     "map":"mp_crash_tropical",
                     "mode":"dom"
                  },{
                     "map":"mp_crash_tropical",
                     "mode":"sd"
                  },{
                     "map":"mp_crash_tropical",
                     "mode":"war"
                  },{
                     "map":"mp_cross_fire",
                     "mode":"conf"
                  },{
                     "map":"mp_cross_fire",
                     "mode":"ctf"
                  },{
                     "map":"mp_cross_fire",
                     "mode":"dd"
                  },{
                     "map":"mp_cross_fire",
                     "mode":"dm"
                  },{
                     "map":"mp_cross_fire",
                     "mode":"dom"
                  },{
                     "map":"mp_cross_fire",
                     "mode":"sd"
                  },{
                     "map":"mp_cross_fire",
                     "mode":"war"
                  },{
                     "map":"mp_estate_tropical",
                     "mode":"conf"
                  },{
                     "map":"mp_estate_tropical",
                     "mode":"ctf"
                  },{
                     "map":"mp_estate_tropical",
                     "mode":"dd"
                  },{
                     "map":"mp_estate_tropical",
                     "mode":"dm"
                  },{
                     "map":"mp_estate_tropical",
                     "mode":"dom"
                  },{
                     "map":"mp_estate_tropical",
                     "mode":"sd"
                  },{
                     "map":"mp_estate_tropical",
                     "mode":"war"
                  },{
                     "map":"mp_fav_tropical",
                     "mode":"conf"
                  },{
                     "map":"mp_fav_tropical",
                     "mode":"ctf"
                  },{
                     "map":"mp_fav_tropical",
                     "mode":"dd"
                  },{
                     "map":"mp_fav_tropical",
                     "mode":"dm"
                  },{
                     "map":"mp_fav_tropical",
                     "mode":"dom"
                  },{
                     "map":"mp_fav_tropical",
                     "mode":"sd"
                  },{
                     "map":"mp_fav_tropical",
                     "mode":"war"
                  },{
                     "map":"mp_killhouse_sh",
                     "mode":"conf"
                  },{
                     "map":"mp_killhouse_sh",
                     "mode":"ctf"
                  },{
                     "map":"mp_killhouse_sh",
                     "mode":"dd"
                  },{
                     "map":"mp_killhouse_sh",
                     "mode":"dm"
                  },{
                     "map":"mp_killhouse_sh",
                     "mode":"dom"
                  },{
                     "map":"mp_killhouse_sh",
                     "mode":"sd"
                  },{
                     "map":"mp_killhouse_sh",
                     "mode":"war"
                  },{
                     "map":"mp_nuked",
                     "mode":"conf"
                  },{
                     "map":"mp_nuked",
                     "mode":"ctf"
                  },{
                     "map":"mp_nuked",
                     "mode":"dd"
                  },{
                     "map":"mp_nuked",
                     "mode":"dm"
                  },{
                     "map":"mp_nuked",
                     "mode":"dom"
                  },{
                     "map":"mp_nuked",
                     "mode":"war"
                  },{
                     "map":"mp_overgrown",
                     "mode":"conf"
                  },{
                     "map":"mp_overgrown",
                     "mode":"ctf"
                  },{
                     "map":"mp_overgrown",
                     "mode":"dd"
                  },{
                     "map":"mp_overgrown",
                     "mode":"dm"
                  },{
                     "map":"mp_overgrown",
                     "mode":"dom"
                  },{
                     "map":"mp_overgrown",
                     "mode":"sd"
                  },{
                     "map":"mp_overgrown",
                     "mode":"war"
                  },{
                     "map":"mp_rust",
                     "mode":"conf"
                  },{
                     "map":"mp_rust",
                     "mode":"ctf"
                  },{
                     "map":"mp_rust",
                     "mode":"dd"
                  },{
                     "map":"mp_rust",
                     "mode":"dm"
                  },{
                     "map":"mp_rust",
                     "mode":"dom"
                  },{
                     "map":"mp_rust",
                     "mode":"sd"
                  },{
                     "map":"mp_rust",
                     "mode":"war"
                  },{
                     "map":"mp_storm_spring",
                     "mode":"conf"
                  },{
                     "map":"mp_storm_spring",
                     "mode":"ctf"
                  },{
                     "map":"mp_storm_spring",
                     "mode":"dd"
                  },{
                     "map":"mp_storm_spring",
                     "mode":"dm"
                  },{
                     "map":"mp_storm_spring",
                     "mode":"dom"
                  },{
                     "map":"mp_storm_spring",
                     "mode":"koth"
                  },{
                     "map":"mp_storm_spring",
                     "mode":"sd"
                  },{
                     "map":"mp_storm_spring",
                     "mode":"war"
                  },{
                     "map":"mp_underpass",
                     "mode":"conf"
                  },{
                     "map":"mp_underpass",
                     "mode":"ctf"
                  },{
                     "map":"mp_underpass",
                     "mode":"dd"
                  },{
                     "map":"mp_underpass",
                     "mode":"dm"
                  },{
                     "map":"mp_underpass",
                     "mode":"dom"
                  },{
                     "map":"mp_underpass",
                     "mode":"sd"
                  },{
                     "map":"mp_underpass",
                     "mode":"war"
                  }];
                  quickChoices = [];
                  quickModes = [];
                  for each(quickPair in quickPool)
                  {
                     quickMap = this.getMapObj(quickPair.map);
                     quickMode = this.getMatchModeObj(quickPair.mode);
                     if(quickMap != null && quickMap.unlocked && quickMode != null && (quickMode.id == quickPair.mode || quickMode.alias == quickPair.mode) && quickPair.map != quickFilter["_localQuickSharedMap"] && quickPair.mode != quickFilter["_localQuickSharedMode"])
                     {
                        quickChoices.push(quickPair);
                        if(quickModes.indexOf(quickPair.mode) < 0)
                        {
                           quickModes.push(quickPair.mode);
                        }
                     }
                  }
                  if(quickModes.length == 0)
                  {
                     return;
                  }
                  quickChosenMode = quickModes[int(Math.random() * quickModes.length)];
                  quickModeChoices = [];
                  for each(quickChoice in quickChoices)
                  {
                     if(quickChoice.mode == quickChosenMode)
                     {
                        quickModeChoices.push(quickChoice);
                     }
                  }
                  if(quickModeChoices.length == 0)
                  {
                     return;
                  }
                  quickChosen = quickModeChoices[int(Math.random() * quickModeChoices.length)];
                  quickFilter["_localQuickSharedSelecting"] = true;
                  quickFilter["_localQuickSharedMap"] = quickChosen.map;
                  quickFilter["_localQuickSharedMode"] = quickChosen.mode;
                  CFunctionContainer.ChangeRoomInfo({
                     "map":quickChosen.map,
                     "minPlayer":int(quickRoom.currentMinPlayers),
                     "maxPlayer":12,
                     "difficulty":int(quickRoom.currentLevel)
                  },"map");
                  CFunctionContainer.ChangeRoomInfo({"mode":quickChosen.mode},"mode");
                  quickFilter["_localQuickSharedSelecting"] = false;
               }
               this._currentQuickMatchFilter["_localQuickSharedRoomSeen"] = true;
               this._isRankedMatchInRoomDone = true;
               if(!(durableEligible || sharedQuick) || queuedHumanIDs.length >= queuedRequired)
               {
                  this.goRoom(true);
               }
               this.changeRankedMatchStatus(RankedMatchStatus.READY_IN_ROOM);
               this.enableLeaveRoom(true);
               CFunctionContainer.SetReadyState(false);
               CFunctionContainer.SetReadyState(true);
            }
            if(this._currentQuickMatchFilter["_localQuickSharedSelecting"] != true && this._currentQuickMatchFilter["_localQuickSharedRoomSeen"] != true && this._currentRankedMatchStatus == RankedMatchStatus.IDLE)
            {
               this.changeRankedMatchStatus(RankedMatchStatus.IN_PROGRESS);
            }
         }
      }
      
      private function onProposeRoomMembershipHandle(cEvt:ParaEvent) : void
      {
         return;
      }
      
      private function onRejectRoomMembershipHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainer.RejectRoomMembership(cEvt.myPara.senderID);
      }
      
      private function tryJoinRoomHandle(cEvt:ParaEvent) : void
      {
         this._tryJoinRoomPara = cEvt.myPara;
         if(this._tryJoinRoomPara.room.password != "")
         {
            this.gotoPage(CodoPageViewDB.PAGE_POPUP_JOIN_ROOM_PASSWORD);
         }
         else
         {
            this.broadcast(BCEvent.EVT_JOIN_ROOM,this._tryJoinRoomPara);
         }
      }
      
      private function onConfirmEnterJoinRoomPasswordHandle(cEvt:ParaEvent) : void
      {
         this.prevPage();
         this._tryJoinRoomPara.room.password = cEvt.myPara;
         this.broadcast(BCEvent.EVT_JOIN_ROOM,this._tryJoinRoomPara);
      }
      
      private function onCancelEnterJoinRoomPasswordHandle(cEvt:ParaEvent) : void
      {
         this.prevPage();
         this.broadcast(BCEvent.EVT_ENABLE_LIST_ROOM,{
            "enable":true,
            "immediate":false
         });
      }
      
      private function joinRoomHandle(cEvt:ParaEvent) : void
      {
         if(MatchService.getInstance().isJoiningRoom())
         {
            return;
         }
         if(this.isInRoom())
         {
            this.showMsg(this.isRankedMatch() ? LocalizationRoom.NOTE_ALREADY_IN_RANKED_MATCH : LocalizationRoom.NOTE_ALREADY_IN_ROOM,MsgBtnCount.ONE);
            return;
         }
         if(this._currentMatchType == QuickMatchType.CYBORG)
         {
            if(this.checkInPartyAsNotHost())
            {
               return;
            }
         }
         else if(this.checkInParty())
         {
            return;
         }
         if(cEvt.myPara.room != null && !this.isMapUnlocked(cEvt.myPara.room.currentMap.id))
         {
            this.showMsg(LocalizationRoom.NOTE_MAP_LOCKED,MsgBtnCount.ONE);
            return;
         }
         this.request(AsyncTaskID.JOIN_ROOM,cEvt.myPara,this.onRoomJoined);
      }
      
      private function onEnableAutoListRoomHandle(cEvt:ParaEvent) : void
      {
         Logger.print("MatchController::onEnableAutoListRoomHandle cEvt.myPara.enable: " + cEvt.myPara.enable);
         this._autoListRoomTimeEnabled = cEvt.myPara.enable;
         this._autoListRoomTime = Boolean(cEvt.myPara.enable) && Boolean(cEvt.myPara.immediate) ? AUTO_LIST_ROOM_TIMER : 0;
         if(!this._autoListRoomTimeEnabled)
         {
            this.cancelListRoom();
         }
      }
      
      private function onListRoomHandle(cEvt:ParaEvent) : void
      {
         this.listRoom(cEvt.myPara.auto,cEvt.myPara.mode);
      }
      
      private function joinChannel(channel:uint) : void
      {
         if(this._currentChannel != channel && channel != ServerChannel.NONE)
         {
            this._currentChannel = channel;
            CFunctionContainer.JoinChannel(channel);
         }
         switch(channel)
         {
            case ServerChannel.CHANNEL_0:
               this._currentRoomListType = RoomListType.MP;
               break;
            case ServerChannel.CHANNEL_5:
               this._currentRoomListType = RoomListType.COOP;
               break;
            case ServerChannel.CHANNEL_6:
               this._currentRoomListType = RoomListType.CYBORG;
               break;
            case ServerChannel.CHANNEL_7:
               this._currentRoomListType = RoomListType.TRAINING;
         }
      }
      
      private function skipTutorial() : void
      {
         if(!this.getSyncDP(SyncDataID.HAS_MP_CHARACTER))
         {
            this.gotoPage(CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW);
            CFunctionContainerCommon.setdvar("ui_onboard_continue","0");
         }
      }
      
      private function startTutorial() : void
      {
         if(this._currentTutorialObj != null)
         {
            this.request(AsyncTaskID.JUMP_GAME,this._currentTutorialObj,this.onCoopTutorialStarted);
         }
      }
      
      private function onCoopTutorialStarted(cRes:Object) : void
      {
         if(cRes.result == ResultObj.RESULT_FAIL)
         {
            this.showMsg(cRes.resultMsg,MsgBtnCount.TWO,0,this.startTutorial,LocalizationSystem.RETRY);
         }
      }
      
      private function startQuickMatch() : void
      {
         if(this._currentQuickMatchFilter.type == RoomListType.RANKED)
         {
            this.changeRankedMatchStatus(RankedMatchStatus.IN_PROGRESS);
            this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
         }
         this.request(AsyncTaskID.QUICK_MATCH,this._currentQuickMatchFilter);
      }
      
      private function enableLeaveRoom(enable:Boolean) : void
      {
         this.broadcast(BCEvent.EVT_RANKED_MATCH_ENABLE_LEAVE_ROOM,enable);
      }
      
      private function getSortingFlag(sortingType:int) : String
      {
         switch(sortingType)
         {
            case RoomListSortType.NUMBER:
               return "index";
            case RoomListSortType.NAME:
               return "name";
            case RoomListSortType.MAP:
               return "currentMap.id";
            case RoomListSortType.MODE:
               return this._currentMatchType == QuickMatchType.MP ? "currentMatchMode.id" : "currentMap.id";
            case RoomListSortType.LEAGUE:
               return this._currentMatchType == QuickMatchType.MP ? "leagueID" : "currentLevel";
            case RoomListSortType.PLAYER:
               return "currentMaxPlayers";
            case RoomListSortType.STATUS:
               return "status";
            case RoomListSortType.PASSWORD:
               return "password";
            default:
               return "";
         }
      }
      
      private function dispatchRooms(sortingType:uint = 0, isFromFilter:Boolean = false, isFromSorting:Boolean = false) : void
      {
         var rooms:Array = this._roomListDP;
         this.sortRooms(rooms,sortingType,isFromFilter,isFromSorting);
         this.broadcast(BCEvent.EVT_ON_ROOM_LISTED,rooms);
      }
      
      private function customizeSort(roomsToSort:Array, flag:String, typeChanged:Boolean = false, changeOption:Boolean = false) : void
      {
         var i:int = 0;
         var names:Array = null;
         if(roomsToSort == null || roomsToSort.length <= 0)
         {
            return;
         }
         if(flag.indexOf(".") > 0)
         {
            if(typeChanged)
            {
               i = 0;
               while(i < roomsToSort.length)
               {
                  names = flag.split(".");
                  if(names.length == 2)
                  {
                     roomsToSort[i].sortName = roomsToSort[i][names[0]][names[1]];
                  }
                  i++;
               }
            }
            flag = "sortName";
         }
         if(!typeChanged)
         {
            if(changeOption)
            {
               if(Boolean(this._roomListSortingOptions & Array.DESCENDING))
               {
                  this._roomListSortingOptions &= ~Array.DESCENDING;
               }
               else
               {
                  this._roomListSortingOptions |= Array.DESCENDING;
               }
            }
         }
         else
         {
            this._roomListSortingOptions = flag == "index" || flag == "status" ? Array.NUMERIC : Array.CASEINSENSITIVE;
         }
         roomsToSort.sortOn(flag,this._roomListSortingOptions);
      }
      
      private function sortRooms(roomsToSort:Array, sortingType:int, isFromFilter:Boolean = false, isFromSorting:Boolean = false) : void
      {
         if(roomsToSort.length <= 0)
         {
            return;
         }
         this.customizeSort(roomsToSort,this.getSortingFlag(sortingType),this._roomListSortingType != sortingType,!isFromFilter || isFromSorting);
         this._roomListSortingType = sortingType;
      }
      
      private function listRoomNow(auto:Boolean, mode:uint = 0) : void
      {
         if(this._isListingRoom)
         {
            if(this._previousRoomListType == this._currentRoomListType)
            {
               if(MatchService.getInstance().isListingRoom())
               {
                  return;
               }
            }
            else
            {
               this.cancelListRoom();
            }
         }
         this._roomListDP.splice(0);
         this._isListingRoom = true;
         this._previousRoomListType = this._currentRoomListType;
         this.request(AsyncTaskID.LIST_ROOM,{
            "auto":true,
            "type":this._currentRoomListType,
            "mode":mode
         },this.onRoomListed);
      }
      
      private function listRoom(auto:Boolean, mode:uint = 0) : void
      {
         this.listRoomNow(auto,mode);
      }
      
      public function onRoomListed(cRes:Object) : void
      {
         if(!ScaleformUtil.isScaleform)
         {
            this.bind_roomListDP(cRes as ResultObj);
         }
         this._isListingRoom = false;
      }
      
      private function enableRoomListEvent(cEnable:Boolean) : void
      {
         this.request(AsyncTaskID.ENABLE_ROOM_LIST_EVENT,cEnable);
      }
      
      private function onChangeRoomInfoHandle(cEvt:ParaEvent) : void
      {
         var map:String = null;
         var terrain:String = null;
         if(cEvt.myPara.type == "map")
         {
            map = cEvt.myPara.data.id;
            terrain = CFunctionContainer.AS_GetTerrainForMap(map);
            if(terrain != "" && this._currentTerrain != terrain)
            {
               this._currentTerrain = terrain;
               SoundUtil.playMusic(MusicID.IN_ROOM,"amb_lobby_" + this._currentTerrain);
            }
         }
      }
      
      private function goRoom(cRes:Boolean, cMsg:String = "", cShouldDispatch:Boolean = true, cPlay:Boolean = true) : void
      {
         if(!cRes)
         {
            CFunctionContainerCommon.setdvar("codol_quick_lineage","");
            CFunctionContainerCommon.setdvar("codol_quick_return","");
            if(this._currentQuickMatchFilter != null)
            {
               this._currentQuickMatchFilter["_localQuickVoteOpen"] = false;
               this._currentQuickMatchFilter["_localQuickVoteFinal"] = false;
               this._currentQuickMatchFilter["_localQuickVoteSent"] = false;
               this._currentQuickMatchFilter["_localQuickVoteRequireHostFalse"] = false;
               this._currentQuickMatchFilter["_localQuickVoteHostFalse"] = false;
               this._currentQuickMatchFilter["_localQuickManagedSummary"] = false;
               this._currentQuickMatchFilter["_localQuickRecoveryKey"] = "";
            }
         }
         if(!cRes)
         {
            if(this._currentQuickMatchFilter != null)
            {
               this._currentQuickMatchFilter["_localQuickExternal20"] = false;
               this._currentQuickMatchFilter["_localQuickExternal"] = false;
               this._currentQuickMatchFilter["_localQuickExternalRoomSeen"] = false;
               this._currentQuickMatchFilter["_localQuickExternalReturning"] = false;
               this._currentQuickMatchFilter["_localQuickExternalSummary"] = false;
               this._currentQuickMatchFilter["_localQuickExternalResumeRequested"] = false;
            }
         }
         if(!cRes)
         {
            if(this._currentQuickMatchFilter != null)
            {
               this._currentQuickMatchFilter["_localQuickSharedInvalidated"] = true;
               this._currentQuickMatchFilter["_localQuickSharedLineage"] = false;
               this._currentQuickMatchFilter["_localQuickSharedJoined"] = false;
               this._currentQuickMatchFilter["_localQuickSharedSelecting"] = false;
               this._currentQuickMatchFilter["_localQuickSharedRoomSeen"] = false;
               this._currentQuickMatchFilter["_localQuickSharedReturning"] = false;
               this._currentQuickMatchFilter["_localQuickSharedReturnPending"] = false;
               this._currentQuickMatchFilter["_localQuickSharedBookingStarted"] = false;
               this._currentQuickMatchFilter["_localQuickSharedMap"] = null;
               this._currentQuickMatchFilter["_localQuickSharedMode"] = null;
            }
            var quickOldRoom:Object = this.getSyncDP(SyncDataID.ROOM_INFO);
            if(quickOldRoom != null)
            {
               quickOldRoom["_localQuickSharedLineage"] = false;
            }
         }
         if(cShouldDispatch)
         {
            MatchService.getInstance().isInRoom = cRes;
         }
         if(cRes)
         {
            this.setMatchTypeByRoom();
            this.gotoPage(CodoPageViewDB.PAGE_MAIN_IN_ROOM);
            if(cPlay)
            {
               this._currentTerrain = CFunctionContainer.UIViewer_GetTerrain();
               SoundUtil.playMusic(this._currentTerrain == "zombie" ? MusicID.IN_ROOM_ZM : MusicID.IN_ROOM,"amb_lobby_" + this._currentTerrain);
            }
         }
         if(cShouldDispatch)
         {
            this.broadcast(BCEvent.EVT_ON_ROOM_JOINED,{
               "success":cRes,
               "message":cMsg
            });
         }
      }
      
      private function changeRankedMatchStatus(cStatus:String) : void
      {
         Logger.print("MatchController:::changeRankedMatchStatus cStatus: " + cStatus);
         if(this._currentRankedMatchStatus == cStatus)
         {
            return;
         }
         this._currentRankedMatchStatus = cStatus;
         switch(this._currentRankedMatchStatus)
         {
            case RankedMatchStatus.IDLE:
               break;
            case RankedMatchStatus.IN_PROGRESS:
            case RankedMatchStatus.IN_PROGRESS_IN_ROOM:
               this.startRankedMatchPassedTimeTimer();
               break;
            case RankedMatchStatus.READY:
            case RankedMatchStatus.READY_IN_ROOM:
               this.stopRankedMatchPassedTimeTimer();
               break;
            case RankedMatchStatus.BACK_FROM_GAME:
               this.startRankedMatchPassedTimeTimer();
         }
         this.dispatchRankedMatchStatus();
      }
      
      private function dispatchRankedMatchStatus() : void
      {
         var league:String = null;
         var playList:String = null;
         if(this._currentRankedMatchStatus == RankedMatchStatus.IDLE || this._currentQuickMatchFilter != null)
         {
            if(this._currentQuickMatchFilter != null && this._currentQuickMatchFilter.leagueName.length <= 0)
            {
               this._currentQuickMatchFilter.leagueName = LocalizationRoom.LEAGUES[this._currentQuickMatchFilter.league];
            }
            league = this._currentQuickMatchFilter != null ? this._currentQuickMatchFilter.leagueName : "";
            playList = this._currentQuickMatchFilter != null ? this._currentQuickMatchFilter.playListName : "";
            this.broadcast(BCEvent.EVT_RANKED_MATCH_CHANGE_STATUS,{
               "status":this._currentRankedMatchStatus,
               "leagueName":league,
               "playListName":playList
            });
         }
      }
      
      private function isMapUnlocked(id:String) : Boolean
      {
         return MatchService.getInstance().isMapUnlocked(id);
      }
      
      private function checkPartyCanPlayPlaylist(playlistObj:PlayListObj) : Boolean
      {
         var members:Array = null;
         if(this.getSyncDP(SyncDataID.IS_PARTY_HOST))
         {
            if(playlistObj.minPartySize == 0 && playlistObj.maxPartySize == 0)
            {
               this.showMsg(LocalizeUtil.localize(LocalizationRoom.LIST_LOCKPARTY),MsgBtnCount.ONE);
               return false;
            }
            members = this.getSyncDP(SyncDataID.PARTY_MEMBER);
            if(members.length < playlistObj.minPartySize)
            {
               this.showMsg(LocalizeUtil.param(LocalizationParty.PARTY_MEM_LESS_THAN_PLAYLIST,[members.length,playlistObj.minPartySize]),MsgBtnCount.ONE);
               return false;
            }
            if(members.length > playlistObj.maxPartySize)
            {
               this.showMsg(LocalizeUtil.param(LocalizationParty.PARTY_MEM_MORE_THAN_PLAYLIST,[members.length,playlistObj.maxPartySize]),MsgBtnCount.ONE);
               return false;
            }
         }
         return true;
      }
      
      private function checkPlaylistPartyOnly(playlistObj:PlayListObj) : Boolean
      {
         Logger.print("MatchController:::checkPlaylistPartyOnly playlistObj.partyOnly: " + playlistObj.partyOnly);
         if(!this.getSyncDP(SyncDataID.IS_IN_PARTY) && playlistObj.partyOnly)
         {
            this.showMsg(LocalizationParty.PARTY_ONLY_PLAYLIST,MsgBtnCount.ONE);
            return false;
         }
         return true;
      }
      
      private function checkInParty(isCyborg:Boolean = false) : Boolean
      {
         if(this.getSyncDP(SyncDataID.IS_IN_PARTY))
         {
            this.showMsg(isCyborg ? LocalizationParty.NOTE_NO_CYBORG_TUTORIAL_WITH_PARTY : LocalizationParty.NOTE_IN_A_PARTY,MsgBtnCount.ONE);
            return true;
         }
         return false;
      }
      
      private function checkInPartyAsNotHost() : Boolean
      {
         if(this.getSyncDP(SyncDataID.IS_IN_PARTY) && !this.getSyncDP(SyncDataID.IS_PARTY_HOST))
         {
            this.showMsg(LocalizationParty.NOTE_IN_A_PARTY_AS_NOT_HOST,MsgBtnCount.ONE);
            return true;
         }
         return false;
      }
      
      private function checkInPartySize(maxPlayer:int) : Boolean
      {
         var members:Array = null;
         if(this.getSyncDP(SyncDataID.IS_IN_PARTY))
         {
            members = this.getSyncDP(SyncDataID.PARTY_MEMBER);
            if(members.length > maxPlayer)
            {
               this.showMsg(LocalizeUtil.param(LocalizationParty.NOTE_PARTY_NUM_TOO_MUCH,[members.length,maxPlayer]),MsgBtnCount.ONE);
               return true;
            }
         }
         return false;
      }
      
      private function onGotoMPCTChildPageHandle(cEvt:ParaEvent) : void
      {
         switch(cEvt.type)
         {
            case BCEvent.EVT_GOTO_MATCH_MPCT_PLAY_LIST:
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_MATCH_MPCT_PLAYLIST);
               break;
            case BCEvent.EVT_GOTO_MATCH_MPCT_ROOM_LIST:
               this.joinChannel(ServerChannel.CHANNEL_0);
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_MATCH_MPCT_ROOMLIST);
               break;
            case BCEvent.EVT_GOTO_MATCH_MPCT_ROOM_CREATOR:
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_MATCH_MPCT_ROOMCTEATOR);
         }
      }
      
      private function onFetchGroupCountTimerHandle(event:TimerEvent) : void
      {
         this.cppRefreshGroupCount();
         if(!ScaleformUtil.isScaleform)
         {
            this.cppRefreshPlaylistPlayerCount("0@600");
         }
      }
      
      public function cppRefreshGroupCount(refresh:Boolean = true) : void
      {
         Logger.print("MatchController:::cppRefreshGroupCount refresh: " + refresh);
         if(refresh)
         {
            this.request(AsyncTaskID.GROUP_COUNTS);
         }
      }
      
      public function cppRefreshPlaylistPlayerCount(params:String) : void
      {
         Logger.print("MatchController:::cppRefreshPlaylistPlayerCount params: " + params);
         if(params == null || params.length <= 0 || params.indexOf("@") == -1)
         {
            return;
         }
         var paramsArr:Array = params.split("@");
         Logger.print("MatchController:::cppRefreshPlaylistPlayerCount paramsArr: " + paramsArr);
         MatchService.getInstance().setPlayListPlayerNum([{
            "groupID":int(paramsArr[0]),
            "groupCount":int(paramsArr[1])
         }]);
         this.setBindData(AsyncTaskID.GROUP_COUNTS,this._groupCounts);
      }
      
      public function cppEnableCyborg(enable:Boolean) : void
      {
         Logger.print("MatchController:::cppEnableCyborg enable: " + enable);
         this.broadcast(BCEvent.UI_ENABLE_CYBORG,enable);
      }
      
      public function cppEnableSurvival(enable:Boolean) : void
      {
         Logger.print("MatchController:::cppEnableSurvival enable: " + enable);
         this.broadcast(BCEvent.UI_ENABLE_SURVIVAL,enable);
      }
      
      public function cppEnableMarket(enable:Boolean) : void
      {
         Logger.print("MatchController:::cppEnableMarket enable: " + enable);
         this.broadcast(BCEvent.UI_ENABLE_MARKET,enable);
      }
      
      public function cppMatchButtonPos(enable:Boolean) : void
      {
         Logger.print("MatchController:::cppMatchButtonPos enable: " + enable);
         this.broadcast(BCEvent.UI_ENABLE_MATCH_BUTTON_POS,enable);
      }
      
      public function cppShutdownFlash() : void
      {
         Logger.print("MatchController:::cppShutdownFlash");
         this.broadcast(BCEvent.SHUTDOWN_FLASH);
      }
   }
}

