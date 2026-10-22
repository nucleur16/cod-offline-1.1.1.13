package codo.views
{
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationRoom;
   import codo.constants.TweenParam;
   import components.BaseButton;
   import components.CodoGrowButton;
   import components.CodoGrowButtonNoTint;
   import flash.display.Bitmap;
   import flash.display.BitmapData;
   import flash.display.DisplayObjectContainer;
   import flash.events.Event;
   import flash.geom.Matrix;
   import flash.text.TextField;
   import flash.utils.getDefinitionByName;
   
   public class MatchMPCTMenuView extends BaseView
   {
      
      private static const POS_X_ORIGIN:int;
      
      private static const POS_Y_ORIGIN:int;
      
      // Decompilation skipped
      
      public var myHeader:CodoGrowButtonNoTint;
      
      public var myPlayListBtn:CodoGrowButton;
      
      public var myRoomListBtn:CodoGrowButton;
      
      public var myRoomCreatorBtn:CodoGrowButton;
      
      private var _currentMenuBtn:CodoGrowButton = null;
      
      private var _originMatrix:Matrix;
      
      public var theatreOnly:Boolean = false;
      
      private var theatreHelp:Array;
      
      public function MatchMPCTMenuView()
      {
         // Decompilation skipped
      }
      
      override protected function onInit() : void
      {
         // Decompilation skipped
      }
      
      override protected function onInitListener() : void
      {
         // Decompilation skipped
      }
      
      override protected function showView() : void
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
      
      private function bind_groupCounts(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onMenuBtnClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function changeCurrentMenu(cTargetBtn:CodoGrowButton, cBC:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      private function theatreZombieArt() : void
      {
         // Decompilation skipped
      }
      
      private function theatreFindHelp(container:DisplayObjectContainer) : void
      {
         // Decompilation skipped
      }
      
      public function theatreInfo(status:String, page:int, pages:int) : void
      {
         // Decompilation skipped
      }
      
      private function theatreFilter(event:Event) : void
      {
         // Decompilation skipped
      }
      
      public function theatreSelect(mode:String) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

