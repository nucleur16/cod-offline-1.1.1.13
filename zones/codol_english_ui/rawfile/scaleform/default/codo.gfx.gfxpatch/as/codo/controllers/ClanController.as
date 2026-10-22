package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.CFunctionContainer;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.BCEventCommon;
   import codo.constants.ChattingChannelType;
   import codo.constants.ClanMemberPosition;
   import codo.constants.LocalizationAAS;
   import codo.constants.LocalizationClan;
   import codo.constants.LocalizationInvitation;
   import codo.constants.LocalizationSystem;
   import codo.constants.MsgBtnCount;
   import codo.constants.SanitizedType;
   import codo.constants.SyncDataID;
   import codo.objects.ChatMessageObj;
   import codo.objects.ClanMemberObj;
   import codo.objects.ClanObj;
   import codo.objects.InvitationObj;
   import codo.objects.ResultObj;
   import codo.services.ClanService;
   import codo.utils.DataUtil;
   import codo.utils.LocalizeUtil;
   import codo.utils.ScaleformUtil;
   import codo.utils.SharedUtil;
   import codo.utils.StringUtil;
   import flash.events.TimerEvent;
   import flash.utils.Timer;
   
   public class ClanController extends BaseController
   {
      
      // Decompilation skipped
      
      private var _clanObj:ClanObj;
      
      private var _clanIncomingProposals:Array;
      
      private var _clanOutgoingProposals:Array;
      
      private var _fetchClanInfoTimer:Timer;
      
      private var _clanNameTF:Object = null;
      
      private var _clanTagTF:Object = null;
      
      private var _memberToOperate:Object = null;
      
      private var _isClanJoinDispatched:Boolean = false;
      
      private var _showClan:Boolean = true;
      
      public function ClanController()
      {
         // Decompilation skipped
      }
      
      override protected function onInitialized() : void
      {
         // Decompilation skipped
      }
      
      public function cppRefreshClan(info:Object) : void
      {
         // Decompilation skipped
      }
      
      public function cppRefreshClanMembers(members:Array) : void
      {
         // Decompilation skipped
      }
      
      public function cppRefreshClanProposals(proposals:Array, incoming:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function cppRefreshClanMemberStatus(userID:String, status:int) : void
      {
         // Decompilation skipped
      }
      
      public function cppOnClanMemberLeft(userID:String, kicked:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function cppOnClanDissolved() : void
      {
         // Decompilation skipped
      }
      
      private function bind_clanLeft(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_clanProfileUpdated(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_clanProfileReceived(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onClanNameSanitizedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onClanTagSanitizedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onCreateClanHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onLeaveClanHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onPromoteClanMemberHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onDemoteClanMemberHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onKickClanMemberHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onInviteUsersToClanHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function sendSystemNotification(msg:String, param:Array = null) : void
      {
         // Decompilation skipped
      }
      
      private function onShowClanHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onClanCreated(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function isInClan() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isClanAdmin() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isInMyClan(user:Object) : Boolean
      {
         // Decompilation skipped
      }
      
      private function isInClanOutgoingProposals(user:Object) : Boolean
      {
         // Decompilation skipped
      }
      
      private function canProposeClanMembership(user:Object) : Boolean
      {
         // Decompilation skipped
      }
      
      private function fillClanObj(data:*, isInfo:Boolean) : void
      {
         // Decompilation skipped
      }
      
      private function refreshClan() : void
      {
         // Decompilation skipped
      }
      
      private function isReady() : Boolean
      {
         // Decompilation skipped
      }
      
      private function resetClanObj() : void
      {
         // Decompilation skipped
      }
      
      private function showClan(cShow:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      private function onFetchClanInfoTimerComplete(event:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      private function reset() : void
      {
         // Decompilation skipped
      }
      
      private function fetchClanInfo() : void
      {
         // Decompilation skipped
      }
      
      private function promoteClanMember() : void
      {
         // Decompilation skipped
      }
      
      private function demoteClanMember() : void
      {
         // Decompilation skipped
      }
      
      private function kickClanMember() : void
      {
         // Decompilation skipped
      }
      
      private function transferClan() : void
      {
         // Decompilation skipped
      }
      
      private function leaveClan() : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

