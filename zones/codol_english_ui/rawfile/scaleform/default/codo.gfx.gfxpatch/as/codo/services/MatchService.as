package codo.services
{
   import codo.common.*;
   import codo.constants.*;
   import codo.objects.*;
   import codo.utils.*;
   import flash.utils.Dictionary;
   
   public class MatchService extends ServiceBase
   {
      
      private static var _instance:MatchService;
      
      // Decompilation skipped
      
      private var _leagues:Array;
      
      private var _levels:Array;
      
      private var _playListWithFilter:Dictionary;
      
      private var _mapsWithFilter:Dictionary;
      
      private var _modesWithFilter:Dictionary;
      
      private var _modeRules:Dictionary;
      
      private var _roomListFilterStatus:Array;
      
      private var _isPlayListInit:Boolean = false;
      
      private var _isInRoom:Boolean = false;
      
      private var _fakeRoomList:Array;
      
      private var _fakeDedicatedServers:Array;
      
      private var _fakeGroupCounts:GroupCountsObj = null;
      
      public function MatchService()
      {
         // Decompilation skipped
      }
      
      public static function getInstance() : MatchService
      {
         // Decompilation skipped
      }
      
      override public function init() : void
      {
         // Decompilation skipped
      }
      
      private function get fakeRoomList() : Array
      {
         // Decompilation skipped
      }
      
      private function get fakeGroupCounts() : GroupCountsObj
      {
         // Decompilation skipped
      }
      
      private function get fakeDedicatedServers() : Array
      {
         // Decompilation skipped
      }
      
      public function get delayToShowButton() : Number
      {
         // Decompilation skipped
      }
      
      public function listRoom(cInfo:Object) : void
      {
         // Decompilation skipped
      }
      
      public function joinRoom(cObj:Object) : void
      {
         // Decompilation skipped
      }
      
      public function createRoom(cRoomObj:Object) : void
      {
         // Decompilation skipped
      }
      
      public function tencentGetAASRecord() : void
      {
         // Decompilation skipped
      }
      
      public function cancelListRoom() : void
      {
         // Decompilation skipped
      }
      
      public function cancelRankedMatch() : void
      {
         // Decompilation skipped
      }
      
      public function quickMatch(matchFilter:Object) : void
      {
         // Decompilation skipped
      }
      
      public function getGroupCounts() : void
      {
         // Decompilation skipped
      }
      
      private function jumpGame(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      public function cancelListDedicatedServer() : void
      {
         // Decompilation skipped
      }
      
      public function listDedicatedServer(bAuto:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function leaveRoom(cPara:Object) : void
      {
         // Decompilation skipped
      }
      
      public function startMatch(dsObj:Object) : void
      {
         // Decompilation skipped
      }
      
      private function isRowValid(csvFile:String, row:int) : Boolean
      {
         // Decompilation skipped
      }
      
      public function getPlayListName(playListID:uint) : String
      {
         // Decompilation skipped
      }
      
      public function getMapName(mapID:String) : String
      {
         // Decompilation skipped
      }
      
      public function isMapUnlocked(mapID:String) : Boolean
      {
         // Decompilation skipped
      }
      
      private function initMapModePlayList() : void
      {
         // Decompilation skipped
      }
      
      public function getMapCoopType(map:Object) : uint
      {
         // Decompilation skipped
      }
      
      public function isMPMap(map:String) : Boolean
      {
         // Decompilation skipped
      }
      
      public function isCyborgMap(map:String) : Boolean
      {
         // Decompilation skipped
      }
      
      public function getMapObj(mapID:String) : MapObj
      {
         // Decompilation skipped
      }
      
      public function getModeObj(modeID:String) : ModeObj
      {
         // Decompilation skipped
      }
      
      public function getPlayListObj(playListID:int) : PlayListObj
      {
         // Decompilation skipped
      }
      
      public function getFirstMapOfAct(act:int) : MapObj
      {
         // Decompilation skipped
      }
      
      public function getLastMapOfAct(act:int) : MapObj
      {
         // Decompilation skipped
      }
      
      public function isActComplete(act:int) : Boolean
      {
         // Decompilation skipped
      }
      
      public function isOnBoardComplete() : Boolean
      {
         // Decompilation skipped
      }
      
      public function isCyborgOnBoardComplete() : Boolean
      {
         // Decompilation skipped
      }
      
      public function listMap(filter:String) : Array
      {
         // Decompilation skipped
      }
      
      public function listMatchMode(matchType:String) : Array
      {
         // Decompilation skipped
      }
      
      public function listLeague() : Array
      {
         // Decompilation skipped
      }
      
      public function listDifficulty(cMap:Object = null) : Array
      {
         // Decompilation skipped
      }
      
      public function listPlayList(leagueID:uint, playlistType:uint = 1, ignoreLeague:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      public function listRoomListFilterStatus() : Array
      {
         // Decompilation skipped
      }
      
      public function setPlayListPlayerNum(playListGroup:Array) : void
      {
         // Decompilation skipped
      }
      
      private function getModeRule(column:uint, label:String = "", localizedUnit:String = "", localizedNone:String = "") : Object
      {
         // Decompilation skipped
      }
      
      private function getTimeLimit(column:uint, unit0:String = "@SF_UI_LIMIT_TIME_M", unit1:String = "@SF_UI_UNLIMITED") : Object
      {
         // Decompilation skipped
      }
      
      private function getScoreLimit(column:uint) : Object
      {
         // Decompilation skipped
      }
      
      private function getRoundSwitch(column:uint) : Object
      {
         // Decompilation skipped
      }
      
      private function getBombTimer(column:uint) : Object
      {
         // Decompilation skipped
      }
      
      private function getPlantDiffuseTime(column:uint) : Object
      {
         // Decompilation skipped
      }
      
      private function getExtraTime(column:uint) : Object
      {
         // Decompilation skipped
      }
      
      private function getRoundLength(column:uint) : Object
      {
         // Decompilation skipped
      }
      
      public function getGameRulesByModeID(modeID:String) : Array
      {
         // Decompilation skipped
      }
      
      public function getSpectatingByModeID(modeID:String) : Object
      {
         // Decompilation skipped
      }
      
      public function getGamePlayOptions() : Object
      {
         // Decompilation skipped
      }
      
      public function get respawnDelay() : Object
      {
         // Decompilation skipped
      }
      
      public function get waveRespawnDelay() : Object
      {
         // Decompilation skipped
      }
      
      private function get healthRegen() : Object
      {
         // Decompilation skipped
      }
      
      private function getSpectating(modeID:String = "") : Object
      {
         // Decompilation skipped
      }
      
      private function get friendlyFire() : Object
      {
         // Decompilation skipped
      }
      
      private function get switches() : Object
      {
         // Decompilation skipped
      }
      
      public function getCurrentLeague() : uint
      {
         // Decompilation skipped
      }
      
      public function getCTBonus(leagueID:uint) : int
      {
         // Decompilation skipped
      }
      
      public function getMPBonus(leagueID:uint) : int
      {
         // Decompilation skipped
      }
      
      public function getXPLeagueFactors(ctType:int) : Array
      {
         // Decompilation skipped
      }
      
      public function getXPLeagueFactor(ctType:int, league:int) : Number
      {
         // Decompilation skipped
      }
      
      public function getGPLeagueFactors(ctType:int) : Array
      {
         // Decompilation skipped
      }
      
      public function getGPLeagueFactor(ctType:int, league:int) : Number
      {
         // Decompilation skipped
      }
      
      private function isMapUnlockedByFlag(unlockFlag:String) : Boolean
      {
         // Decompilation skipped
      }
      
      private function getObjByID(dic:Dictionary, id:String) : Object
      {
         // Decompilation skipped
      }
      
      private function enableRoomListEvent(bEnable:Boolean) : void
      {
         // Decompilation skipped
      }
      
      private function enableInRoomEvent(bEnable:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function get isInRoom() : Boolean
      {
         // Decompilation skipped
      }
      
      public function set isInRoom(cIn:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function isJoiningRoom() : Boolean
      {
         // Decompilation skipped
      }
      
      public function isListingRoom() : Boolean
      {
         // Decompilation skipped
      }
      
      public function isLeavingRoom() : Boolean
      {
         // Decompilation skipped
      }
      
      public function get isPlayerReady() : Boolean
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

