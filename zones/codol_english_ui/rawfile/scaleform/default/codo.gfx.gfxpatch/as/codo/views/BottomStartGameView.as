package codo.views
{
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.GlobalBottomBtnStatus;
   import codo.constants.LocalizationDict;
   import codo.constants.LocalizationRoom;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import components.BaseButton;
   import components.BlinkButton;
   import components.CodoGrowButton;
   import flash.display.Sprite;
   
   public class BottomStartGameView extends BaseView
   {
      
      private static const POS_ORIGIN_X:int = 1000;
      
      private static const POS_ORIGIN_Y:int = 967;
      
      private static const POS_SHOW_Y:int = 878;
      
      private static const POS_ORIGIN_X_NEW:int = 1000;
      
      private static const POS_ORIGIN_Y_NEW:int = -129;
      
      private static const POS_SHOW_Y_NEW:int = 16;
      
      // Decompilation skipped
      
      public var myBottomMPTwoUnreadyBtn:CodoGrowButton;
      
      public var myBottomMPTwoStartGameBtn:CodoGrowButton;
      
      public var myStartBtn:BlinkButton;
      
      public var myBottomCYTwoStartGameBtn:CodoGrowButton;
      
      public var myStartBtn_cyborg:CodoGrowButton;
      
      public var myBottomMPTwoBg:Sprite;
      
      private var _currentLabelTxt:String;
      
      private var _currentTwoStartTxt:String;
      
      private var _matchMpctEnableStartButton:Boolean = true;
      
      private var _currentBottomBtnStatusEnabled:Boolean = true;
      
      public var theatreOnly:Boolean = false;
      
      public function BottomStartGameView()
      {
         // Decompilation skipped
      }
      
      override protected function onInit() : void
      {
         // Decompilation skipped
      }
      
      override protected function showView() : void
      {
         // Decompilation skipped
      }
      
      private function onShowViewCompleteHandle() : void
      {
         // Decompilation skipped
      }
      
      override protected function hideView() : void
      {
         // Decompilation skipped
      }
      
      override protected function enableData() : void
      {
         // Decompilation skipped
      }
      
      private function bind_matchMpctEnableStartButton(cResult:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_currentBottomBtnStatus(cResult:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onStartBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onTwoBtnLeftClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

