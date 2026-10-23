package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.BaseView;
   import codo.common.CFunctionContainer;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.CodoPageViewDB;
   import codo.constants.NotificationState;
   import codo.constants.PageID;
   import codo.constants.QuickMatchType;
   import codo.constants.SyncDataID;
   import codo.constants.Table.UINotifications;
   import codo.utils.ScaleformUtil;
   import codo.utils.TableUtil;
   import components.BaseButton;
   import flash.utils.Dictionary;
   
   public class UIUnlockController extends BaseController
   {
      
      private static var SF_UI:String = "@SF_UI_";
      
      private static var uiComponentUnlockDictionary:Dictionary;
      
      private static var uiComponentUnlockTip:Dictionary;
      
      // Decompilation skipped
      
      private var _notifications:Array;
      
      private var _currentNotification:String = null;
      
      private var _isNotificationInitialized:Boolean = false;
      
      public function UIUnlockController()
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
      
      private function onUINotification(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function getNotificationState(notificationRef:String) : uint
      {
         // Decompilation skipped
      }
      
      private function isNotified(notificationRef:String) : Boolean
      {
         // Decompilation skipped
      }
      
      private function isNotificationUnlocked(notificationRef:String) : Boolean
      {
         // Decompilation skipped
      }
      
      private function setNotified(notificationRef:String) : void
      {
         // Decompilation skipped
      }
      
      private function onAddNewUnlock(e:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function checkUnlockNotification() : void
      {
         // Decompilation skipped
      }
      
      private function findOneValidNotification() : String
      {
         // Decompilation skipped
      }
      
      private function unlockUICloseComplete(e:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function getComponentUnlocked(componentID:String) : Boolean
      {
         // Decompilation skipped
      }
      
      private function getComponentTip(componentID:String) : String
      {
         // Decompilation skipped
      }
      
      private function onMainPageChangeHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function isMatchType(type:String) : Boolean
      {
         // Decompilation skipped
      }
      
      private function unlockUIStartClose(e:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onPlayerStatsReadyHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function initNotifications() : void
      {
         // Decompilation skipped
      }
      
      private function getNotificationInfo() : Object
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

