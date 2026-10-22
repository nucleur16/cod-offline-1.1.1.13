package codo.views
{
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ListEvent;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationRoom;
   import codo.constants.QuickMatchType;
   import codo.constants.RoomListSortType;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import components.BaseButton;
   import components.CodoGrowButtonNoTint;
   import components.CodoList;
   import flash.display.Sprite;
   import flash.utils.getDefinitionByName;
   import flash.utils.getQualifiedClassName;
   
   public class MatchRoomListView extends BaseView
   {
      
      protected static const POS_X_ORIGIN:int;
      
      protected static const POS_Y_ORIGIN:int;
      
      protected static const SORTING_BTN:String = "myRoomLabelBtn_";
      
      // Decompilation skipped
      
      public var myHeader:CodoGrowButtonNoTint;
      
      public var myRefreshIcon:Sprite;
      
      public var myRefreshBtn:CodoGrowButtonNoTint;
      
      public var myList:CodoList;
      
      private var _currentRoomObj:Object;
      
      private var _enableRefresh:Boolean = true;
      
      public var theatreOnly:Boolean = false;
      
      private var theatrePrevious:CodoGrowButtonNoTint;
      
      private var theatreNext:CodoGrowButtonNoTint;
      
      private var theatreModes:CodoGrowButtonNoTint;
      
      public function MatchRoomListView()
      {
         // Decompilation skipped
      }
      
      override protected function onInit() : void
      {
         // Decompilation skipped
      }
      
      protected function initHeader() : void
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
      
      protected function get sortBtnCount() : int
      {
         // Decompilation skipped
      }
      
      override protected function onInitListener() : void
      {
         // Decompilation skipped
      }
      
      override protected function enableData() : void
      {
         // Decompilation skipped
      }
      
      override protected function disableData() : void
      {
         // Decompilation skipped
      }
      
      private function onRefreshBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function listRoom() : void
      {
         // Decompilation skipped
      }
      
      private function joinRoom(cRoom:Object = null) : void
      {
         // Decompilation skipped
      }
      
      private function sendJoinRoomRequest() : void
      {
         // Decompilation skipped
      }
      
      private function enableListRoom(bEnable:Boolean, bImmediate:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomJoinedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomListedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomDoubleClickHandle(cEvt:ListEvent) : void
      {
         // Decompilation skipped
      }
      
      protected function onSortingBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomListSelectedChangedHandle(cEvt:ListEvent = null) : void
      {
         // Decompilation skipped
      }
      
      private function refreshJoinRoomButton() : void
      {
         // Decompilation skipped
      }
      
      private function getCurrentSelectedRoom() : Object
      {
         // Decompilation skipped
      }
      
      private function theatreInitPager() : void
      {
         // Decompilation skipped
      }
      
      private function theatreClose(event:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function theatrePreviousPage(event:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function theatreNextPage(event:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function theatreWatch() : void
      {
         // Decompilation skipped
      }
      
      public function theatrePopulate(rows:Array, page:int, pages:int) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

