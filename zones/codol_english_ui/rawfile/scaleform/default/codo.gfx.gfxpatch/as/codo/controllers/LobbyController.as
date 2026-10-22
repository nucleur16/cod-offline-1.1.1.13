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
      
      // Decompilation skipped
      
      private var _roomObj:RoomObj;
      
      private var _roomMembers:Array;
      
      private var _dedicatedServers:Array;
      
      private var _ac130Types:Array;
      
      private var _currentSelectedWeaponSlot:Object = null;
      
      private var _currentSelectedCharacter:RoomMemberObj = null;
      
      private var _isCareerStatsSet:Boolean = false;
      
      private var _isRenewingAvatar:uint = 0;
      
      private var _parentGlowFilter:GlowFilter;
      
      private var _childGlowFilter:GlowFilter;
      
      private var _glowFilterTimers:Array;
      
      private var _stageMouseX:Number = 0;
      
      private var _stageMouseY:Number = 0;
      
      public function LobbyController()
      {
         // Decompilation skipped
      }
      
      override protected function onInitialized() : void
      {
         // Decompilation skipped
      }
      
      public function cppCanSelectCharacter(character:String, xPos:int, yPos:int) : Boolean
      {
         // Decompilation skipped
      }
      
      public function cppSelectCharacterStart() : void
      {
         // Decompilation skipped
      }
      
      public function cppSelectCharacter(character:String, forceHide:Boolean, selected:Boolean, xPos:int, yPos:int) : Boolean
      {
         // Decompilation skipped
      }
      
      public function cppSetCharacterAvatarSlots(playerID:String, avatarSlots:Object, weaponSlots:Object) : void
      {
         // Decompilation skipped
      }
      
      public function cppRefreshRoomMemberStats(stats:Object) : void
      {
         // Decompilation skipped
      }
      
      public function cppSetRoomRTTParams(params:Object) : void
      {
         // Decompilation skipped
      }
      
      public function cppSetMemberVote(member:String, vote:int) : void
      {
         // Decompilation skipped
      }
      
      public function cppSetMemberReadyState(member:String, ready:Boolean) : void
      {
         // Decompilation skipped
      }
      
      private function onListDedicatedServerHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function isInMyRoom(cUser:Object) : Boolean
      {
         // Decompilation skipped
      }
      
      private function isLeavingRoom() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isRoomHost() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isGameStartCountingDown() : Boolean
      {
         // Decompilation skipped
      }
      
      private function onChangeReadyStateHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function isReady() : Boolean
      {
         // Decompilation skipped
      }
      
      private function listAC130Type() : Array
      {
         // Decompilation skipped
      }
      
      private function isAC130(map:Object) : Boolean
      {
         // Decompilation skipped
      }
      
      private function getRoomInfo() : RoomObj
      {
         // Decompilation skipped
      }
      
      private function getRoomMemberObj(name:String) : RoomMemberObj
      {
         // Decompilation skipped
      }
      
      private function getRoomMemberObjByID(id:String) : RoomMemberObj
      {
         // Decompilation skipped
      }
      
      private function isBootCampPlayList() : Boolean
      {
         // Decompilation skipped
      }
      
      private function bind_roomInfo(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_roomMemberInfo(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_onRoomLeft(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_dedicatedServerDP(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onSetPilotHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onStartMatchHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onChangeTeamHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onVoteMapHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onChangeRoomInfoHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRankedMatchStatusChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRefreshAvatarHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRefreshWeaponHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRenewAvatarHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRenewWeaponHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onControllerCheckAvatarRefreshInTeam(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onInventoryChangeHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function broadcastAvatarRefreshInTeam() : void
      {
         // Decompilation skipped
      }
      
      private function onWeaponSlotClickedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onWeaponLoadoutChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function setFinalVoting() : void
      {
         // Decompilation skipped
      }
      
      private function isRoomInfoReady() : Boolean
      {
         // Decompilation skipped
      }
      
      private function initRoom(res:Object, isRoom:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      private function initRoomInfo(obj:Object) : void
      {
         // Decompilation skipped
      }
      
      private function initRoomMembers(members:Array) : void
      {
         // Decompilation skipped
      }
      
      private function refreshRoomMaps() : void
      {
         // Decompilation skipped
      }
      
      private function refreshRoomStatus() : void
      {
         // Decompilation skipped
      }
      
      private function refreshRoomVoting(fromVote:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      private function getPlayListEntryMap(playListID:int, idxEntry:int) : MapObj
      {
         // Decompilation skipped
      }
      
      private function getCurrentPlayList() : PlayListObj
      {
         // Decompilation skipped
      }
      
      private function resetRoom() : void
      {
         // Decompilation skipped
      }
      
      private function resetRoomVoting() : void
      {
         // Decompilation skipped
      }
      
      private function get me() : RoomMemberObj
      {
         // Decompilation skipped
      }
      
      private function refreshRoomMember(dest:RoomMemberObj, src:Object) : void
      {
         // Decompilation skipped
      }
      
      private function filterOnTeam(dp:Array, faction:int) : void
      {
         // Decompilation skipped
      }
      
      private function refreshMyAvatar(roomMemberObj:RoomMemberObj) : void
      {
         // Decompilation skipped
      }
      
      private function refreshMyWeaponLoadout(roomMemberObj:RoomMemberObj) : void
      {
         // Decompilation skipped
      }
      
      private function refreshMyWeaponItems(weaponSlot:Object) : void
      {
         // Decompilation skipped
      }
      
      private function refreshCareerStats(roomMemberObj:RoomMemberObj, select:Boolean = false) : void
      {
         // Decompilation skipped
      }
      
      private function refreshLobbyCharacter(roomMemberObj:RoomMemberObj) : void
      {
         // Decompilation skipped
      }
      
      private function broadcastCareerStats(roomMemberObj:RoomMemberObj) : void
      {
         // Decompilation skipped
      }
      
      private function dispatchRoom() : void
      {
         // Decompilation skipped
      }
      
      private function onGotoAARHandle() : void
      {
         // Decompilation skipped
      }
      
      private function setMenuSelections(roomMemberObj:RoomMemberObj) : void
      {
         // Decompilation skipped
      }
      
      private function isBaseView(container:DisplayObjectContainer) : Boolean
      {
         // Decompilation skipped
      }
      
      private function hasBaseView(container:DisplayObjectContainer) : Boolean
      {
         // Decompilation skipped
      }
      
      private function hasUIComponent(container:DisplayObjectContainer) : Boolean
      {
         // Decompilation skipped
      }
      
      private function hasClass(container:DisplayObjectContainer, class1:Class, class2:Class = null) : Boolean
      {
         // Decompilation skipped
      }
      
      private function hasDisplayObjectContainer(container:DisplayObjectContainer) : Boolean
      {
         // Decompilation skipped
      }
      
      private function hitOnAUIComponent(container:DisplayObjectContainer) : Boolean
      {
         // Decompilation skipped
      }
      
      private function addGlowFilterTimer(container:*, child:* = null) : void
      {
         // Decompilation skipped
      }
      
      private function onGlowFilterTimerComplete(event:TimerEvent) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

