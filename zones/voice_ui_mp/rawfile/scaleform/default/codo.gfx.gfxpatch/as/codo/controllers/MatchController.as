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
   
   public class MatchController extends BaseController
   {
      
      private static const AUTO_LIST_ROOM_TIMER:Number = 90;
      
      // Decompilation skipped
      
      private var _autoListRoomTime:Number = 0;
      
      private var _autoListRoomTimeEnabled:Boolean = false;
      
      private var _isListingRoom:Boolean = false;
      
      private var _currentRoomListType:uint;
      
      private var _previousRoomListType:uint;
      
      private var _roomListDP:Array;
      
      private var _roomListFilter:RoomListFilterObj;
      
      private var _roomListSortingType:uint;
      
      private var _roomListSortingOptions:uint = 0;
      
      private var _currentMatchType:uint;
      
      private var _currentQuickMatchFilter:QuickMatchFilterObj = null;
      
      private var _currentChannel:uint;
      
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
      
      private var _groupCounts:Object;
      
      private var _currentTerrain:String = "";
      
      private var _fakeElapsedTime:Number = 0;
      
      public function MatchController()
      {
         // Decompilation skipped
      }
      
      override protected function onInit() : void
      {
         // Decompilation skipped
      }
      
      override protected function onInitialized() : void
      {
         // Decompilation skipped
      }
      
      override public function update(deltaTime:Number) : void
      {
         // Decompilation skipped
      }
      
      private function autoListRoom(deltaTime:Number) : void
      {
         // Decompilation skipped
      }
      
      private function cancelListRoom() : void
      {
         // Decompilation skipped
      }
      
      private function cancelRankedMatch() : void
      {
         // Decompilation skipped
      }
      
      private function onCloseCoopRoomListHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onCloseRoomCreatorHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSetQuickMatchTypeHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function gotoCyborgQuickMatchHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function gotoSurvivalQuickMatchHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomNameSanitizedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function getDefaultRoomName() : String
      {
         // Decompilation skipped
      }
      
      private function setMatchTypeByRoom() : void
      {
         // Decompilation skipped
      }
      
      private function setCurrentMatchType(cType:uint) : void
      {
         // Decompilation skipped
      }
      
      private function getCurrentMatchType() : uint
      {
         // Decompilation skipped
      }
      
      private function getMatchTypeByMap(mapID:String) : uint
      {
         // Decompilation skipped
      }
      
      private function isCyborgMode() : Boolean
      {
         // Decompilation skipped
      }
      
      private function getCurrentRankedMatchStatus() : String
      {
         // Decompilation skipped
      }
      
      private function isInRankedMatch() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isRankedMatchInProgress() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isRankedMatchReady() : Boolean
      {
         // Decompilation skipped
      }
      
      private function getCurrentLeague() : int
      {
         // Decompilation skipped
      }
      
      private function getCurrentLeagueName() : String
      {
         // Decompilation skipped
      }
      
      private function isActComplete(act:int) : Boolean
      {
         // Decompilation skipped
      }
      
      private function isOnBoardComplete() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isBootCamp() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isCyborgEnabled() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isSurvivalEnabled() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isMarketEnabled() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isMatchButtonPosEnabled() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isGroupCountEnabled() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isCyborgTutorialComplete() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isRankedMatch() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isRoomFull() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isPlayerReady() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isInRoom() : Boolean
      {
         // Decompilation skipped
      }
      
      private function listSameTypeMap(mapID:String) : Array
      {
         // Decompilation skipped
      }
      
      private function listMap(matchType:String, insertAny:Boolean = false, coop:uint = 0, coopList:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      private function listMode(matchType:String, isCombat:Boolean = false, insertAny:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      private function listLeague(isRoom:Boolean = false, insertAny:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      private function listCTDifficulty() : Array
      {
         // Decompilation skipped
      }
      
      private function listDifficulty(cMap:Object = null, insertAny:Boolean = false, coopFilter:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      private function listPlayListNew() : Array
      {
         // Decompilation skipped
      }
      
      private function listPlayList(leagueId:int, playlistType:uint = 1, ignoreLeague:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      private function listQuickMPPlayList() : Array
      {
         // Decompilation skipped
      }
      
      private function listCoopType(insertAny:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      private function listMaxPlayer(insertAny:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      private function listCTMaxPlayer() : Array
      {
         // Decompilation skipped
      }
      
      private function listLoadout(insertAny:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      private function listRoomListFilterStatus() : Array
      {
         // Decompilation skipped
      }
      
      private function isCoopMap(map:Object) : Boolean
      {
         // Decompilation skipped
      }
      
      private function isMpMap(map:Object) : Boolean
      {
         // Decompilation skipped
      }
      
      private function isZmMap(map:Object) : Boolean
      {
         // Decompilation skipped
      }
      
      private function getMapType(mapID:String) : uint
      {
         // Decompilation skipped
      }
      
      private function getMapCoopType(map:Object) : uint
      {
         // Decompilation skipped
      }
      
      private function getGameRulesByModeID(modeID:String) : Array
      {
         // Decompilation skipped
      }
      
      private function getSpectatingByModeID(modeID:String) : Object
      {
         // Decompilation skipped
      }
      
      private function getGamePlayOptions() : Object
      {
         // Decompilation skipped
      }
      
      private function getMapObj(mapID:String) : MapObj
      {
         // Decompilation skipped
      }
      
      private function getMatchModeObj(modeID:String) : ModeObj
      {
         // Decompilation skipped
      }
      
      private function getPlayListObj(playListID:int) : PlayListObj
      {
         // Decompilation skipped
      }
      
      private function getCTBonus(leagueID:uint) : int
      {
         // Decompilation skipped
      }
      
      private function getMPBonus(leagueID:uint) : int
      {
         // Decompilation skipped
      }
      
      private function getXPLeagueFactor(ctType:int, league:int, playListObj:PlayListObj = null) : Number
      {
         // Decompilation skipped
      }
      
      private function getXPLeagueFactors(ctType:int) : Array
      {
         // Decompilation skipped
      }
      
      private function getGPLeagueFactor(ctType:int, league:int, playListObj:PlayListObj = null) : Number
      {
         // Decompilation skipped
      }
      
      private function getGPLeagueFactors(ctType:int) : Array
      {
         // Decompilation skipped
      }
      
      private function getLeagueName(leagueID:uint) : String
      {
         // Decompilation skipped
      }
      
      private function getDelayToShowButton() : Number
      {
         // Decompilation skipped
      }
      
      private function onQuickMatchDebug(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      public function onRoomJoined(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      public function onRoomCreated(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_rankedMatchIntermission(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_rankedMatchInProgressInRoom(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_startMatchCountingDown(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_startMatchComplete(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_roomListDP(cRes:ResultObj) : void
      {
         // Decompilation skipped
      }
      
      private function bind_quickMatchReady(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_groupCounts(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function restartStartMatchCountDown() : void
      {
         // Decompilation skipped
      }
      
      private function startRankedMatchPassedTimeTimer() : void
      {
         // Decompilation skipped
      }
      
      private function stopRankedMatchPassedTimeTimer(cancel:Boolean = false) : void
      {
         // Decompilation skipped
      }
      
      private function startRankedMatchGoRoomTimer(repeatCount:int) : void
      {
         // Decompilation skipped
      }
      
      private function stopRankedMatchGoRoomTimer() : void
      {
         // Decompilation skipped
      }
      
      private function startRankedMatchGoGameTimer(repeatCount:int, lockNow:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      private function stopRankedMatchGoGameTimer() : void
      {
         // Decompilation skipped
      }
      
      private function startUnrankedMatchGoGameTimer(repeatCount:int) : void
      {
         // Decompilation skipped
      }
      
      private function stopUnrankedMatchGoGameTimer() : void
      {
         // Decompilation skipped
      }
      
      private function lockMatch() : void
      {
         // Decompilation skipped
      }
      
      private function leaveRoom(auto:Boolean = false, cancelRanked:Boolean = false, withParty:Boolean = false) : void
      {
         // Decompilation skipped
      }
      
      private function leaveRoomWithParty() : void
      {
         // Decompilation skipped
      }
      
      private function leaveRoomAlone() : void
      {
         // Decompilation skipped
      }
      
      private function startMatchCountDown(cEvt:TimerEvent = null) : void
      {
         // Decompilation skipped
      }
      
      private function get goRoomCountingDown() : Number
      {
         // Decompilation skipped
      }
      
      private function get goGameCountingDown() : Number
      {
         // Decompilation skipped
      }
      
      private function onRankedMatchPassedTimeTimerHandle(cEvt:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRankedMatchGoRoomTimerHandle(cEvt:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRankedMatchGoRoomTimerCompleteHandle(cEvt:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRankedMatchGoGameTimerHandle(cEvt:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRankedMatchGoGameTimerCompleteHandle(cEvt:TimerEvent = null) : void
      {
         // Decompilation skipped
      }
      
      private function onUnrankedMatchGoGameTimerHandle(cEvt:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onUnrankedMatchGoGameTimerCompleteHandle(cEvt:TimerEvent = null) : void
      {
         // Decompilation skipped
      }
      
      private function onLeaveRoomHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRankedMatchCanceled() : void
      {
         // Decompilation skipped
      }
      
      private function onRoomLeftHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onPlayMainMenuSound(cEvt:ParaEvent = null) : void
      {
         // Decompilation skipped
      }
      
      private function onCreateRoomHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onPartyEveryoneIsReadyHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRetryTutorialHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSkipTutorialHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onStartTutorialHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onQuickMatchHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onQuickMatchMpHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onMatchRoomListFilterChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onMatchRoomListSortingTypeChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onMatchQuickMpLeagueChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onBackToRoomHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onShowLobbySummaryTimerComplete(evt:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      private function clearEOGDvars() : void
      {
         // Decompilation skipped
      }
      
      private function onOnlyRoomInfoHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onProposeRoomMembershipHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRejectRoomMembershipHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function tryJoinRoomHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onConfirmEnterJoinRoomPasswordHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onCancelEnterJoinRoomPasswordHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function joinRoomHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onEnableAutoListRoomHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onListRoomHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function joinChannel(channel:uint) : void
      {
         // Decompilation skipped
      }
      
      private function skipTutorial() : void
      {
         // Decompilation skipped
      }
      
      private function startTutorial() : void
      {
         // Decompilation skipped
      }
      
      private function onCoopTutorialStarted(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function startQuickMatch() : void
      {
         // Decompilation skipped
      }
      
      private function enableLeaveRoom(enable:Boolean) : void
      {
         // Decompilation skipped
      }
      
      private function getSortingFlag(sortingType:int) : String
      {
         // Decompilation skipped
      }
      
      private function dispatchRooms(sortingType:uint = 0, isFromFilter:Boolean = false, isFromSorting:Boolean = false) : void
      {
         // Decompilation skipped
      }
      
      private function customizeSort(roomsToSort:Array, flag:String, typeChanged:Boolean = false, changeOption:Boolean = false) : void
      {
         // Decompilation skipped
      }
      
      private function sortRooms(roomsToSort:Array, sortingType:int, isFromFilter:Boolean = false, isFromSorting:Boolean = false) : void
      {
         // Decompilation skipped
      }
      
      private function listRoomNow(auto:Boolean, mode:uint = 0) : void
      {
         // Decompilation skipped
      }
      
      private function listRoom(auto:Boolean, mode:uint = 0) : void
      {
         // Decompilation skipped
      }
      
      public function onRoomListed(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function enableRoomListEvent(cEnable:Boolean) : void
      {
         // Decompilation skipped
      }
      
      private function onChangeRoomInfoHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function goRoom(cRes:Boolean, cMsg:String = "", cShouldDispatch:Boolean = true, cPlay:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      private function changeRankedMatchStatus(cStatus:String) : void
      {
         // Decompilation skipped
      }
      
      private function dispatchRankedMatchStatus() : void
      {
         // Decompilation skipped
      }
      
      private function isMapUnlocked(id:String) : Boolean
      {
         // Decompilation skipped
      }
      
      private function checkPartyCanPlayPlaylist(playlistObj:PlayListObj) : Boolean
      {
         // Decompilation skipped
      }
      
      private function checkPlaylistPartyOnly(playlistObj:PlayListObj) : Boolean
      {
         // Decompilation skipped
      }
      
      private function checkInParty(isCyborg:Boolean = false) : Boolean
      {
         // Decompilation skipped
      }
      
      private function checkInPartyAsNotHost() : Boolean
      {
         // Decompilation skipped
      }
      
      private function checkInPartySize(maxPlayer:int) : Boolean
      {
         // Decompilation skipped
      }
      
      private function onGotoMPCTChildPageHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onFetchGroupCountTimerHandle(event:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      public function cppRefreshGroupCount(refresh:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      public function cppRefreshPlaylistPlayerCount(params:String) : void
      {
         // Decompilation skipped
      }
      
      public function cppEnableCyborg(enable:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function cppEnableSurvival(enable:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function cppEnableMarket(enable:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function cppMatchButtonPos(enable:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function cppShutdownFlash() : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

