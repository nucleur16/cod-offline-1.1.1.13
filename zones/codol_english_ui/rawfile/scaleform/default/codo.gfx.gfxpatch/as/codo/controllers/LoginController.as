package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.CFunctionContainer;
   import codo.common.CFunctionContainerCommon;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.CodoPageViewDB;
   import codo.constants.Common;
   import codo.constants.LocalizationParty;
   import codo.constants.LocalizationRoom;
   import codo.constants.LocalizationSummary;
   import codo.constants.LocalizationSystem;
   import codo.constants.MatchType;
   import codo.constants.MsgBtnCount;
   import codo.constants.SyncDataID;
   import codo.constants.Table.UINotifications;
   import codo.constants.TutorialMap;
   import codo.objects.ResultObj;
   import codo.services.LoginService;
   import codo.utils.CommonUtil;
   
   public class LoginController extends BaseController
   {
      
      // Decompilation skipped
      
      private var _isShowingConnection:Boolean = false;
      
      public function LoginController()
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
      
      private function getPlayerName() : String
      {
         // Decompilation skipped
      }
      
      private function isMPTutorialComplete() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isZMTutorialComplete() : Boolean
      {
         // Decompilation skipped
      }
      
      private function onCreateCharacterNameHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onCreateCharacterNameComplete(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onQuit(cEvt:ParaEvent = null) : void
      {
         // Decompilation skipped
      }
      
      private function onChooseOnBoard(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function startOnBoard() : void
      {
         // Decompilation skipped
      }
      
      private function skipOnBoard() : void
      {
         // Decompilation skipped
      }
      
      private function startMPTutorial() : void
      {
         // Decompilation skipped
      }
      
      private function skipMPTutorial() : void
      {
         // Decompilation skipped
      }
      
      private function startZMTutorial() : void
      {
         // Decompilation skipped
      }
      
      private function skipZMTutorial() : void
      {
         // Decompilation skipped
      }
      
      private function onOnBoardStarted(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onMPTutorialStarted(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onZMTutorialStarted(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function checkIsInParty() : Boolean
      {
         // Decompilation skipped
      }
      
      private function set onBoardPlaying(playing:Boolean) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

