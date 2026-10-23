package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.PageManagerMain;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.BackToRoomType;
   import codo.constants.CodoPageViewDB;
   import codo.constants.CombatType;
   import codo.constants.Common;
   import codo.constants.GlobalBackBtnStatus;
   import codo.constants.GlobalBottomBtnStatus;
   import codo.constants.LocalizationParty;
   import codo.constants.MsgBtnCount;
   import codo.constants.PageID;
   import codo.constants.PlayListType;
   import codo.constants.QuickMatchType;
   import codo.constants.RankedMatchStatus;
   import codo.constants.RoomListType;
   import codo.constants.SyncDataID;
   import codo.constants.Table.UINotifications;
   import codo.objects.QuickMatchFilterObj;
   import codo.services.MatchService;
   import codo.utils.ScaleformUtil;
   
   public class MainMenuController extends BaseController
   {
      
      // Decompilation skipped
      
      private var _currentPageID:String;
      
      private var _currentRankedMatchStatus:String;
      
      private var _currentBackBtnStatus:uint;
      
      private var _isInRoomReady:Boolean = false;
      
      private var _currentBottomBtnStatus:uint;
      
      private var _prePageIsMP:Boolean = true;
      
      public function MainMenuController()
      {
         // Decompilation skipped
      }
      
      override protected function onInitialized() : void
      {
         // Decompilation skipped
      }
      
      private function onPageIDChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomForcesHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomLeftHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRankedMatchStatusChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function refreshGlobalBackStatus() : void
      {
         // Decompilation skipped
      }
      
      private function refreshBottomButtonStatus() : void
      {
         // Decompilation skipped
      }
      
      private function onQuickMatchBackHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoPlayModeSelectionHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoHomeHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onLogoBtnClickedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function getLogoIsClickable() : Boolean
      {
         // Decompilation skipped
      }
      
      private function setQuickMatchType(type:uint) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoQuickMatchMPHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoRoomListMPHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoQuickMatchCoopHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoRoomListCoopHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoQuickMatchZombieHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoQuickMatchCombatTrainingNewHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoWeaponLoadoutHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoCharacterLoadoutHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoItemHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoLobbySummaryHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoQuitHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoMarketHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoQuickMatchLobbyHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSwitchMyUserInfoHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function getIsCallsignExpand() : Boolean
      {
         // Decompilation skipped
      }
      
      private function onSwitchLeaderboardHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onShowClaimRewards(event:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoNewMainHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onChangeStartBtnStatusHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onBottomStartBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onBottomLeftBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGlobalBackHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function backToRoom() : void
      {
         // Decompilation skipped
      }
      
      private function onTryOpenMailHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onTryOpenFriendlistHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onTryOpenMessageBoxHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSocialExpandChangeHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

