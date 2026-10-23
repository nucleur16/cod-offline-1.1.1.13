package
{
   import codo.common.AsyncTaskManager;
   import codo.common.BaseMain;
   import codo.common.CFunctionContainerCommon;
   import codo.common.CommManagerPause;
   import codo.common.EventManager;
   import codo.common.FPSMemCounter;
   import codo.common.PageManagerPause;
   import codo.constants.AsyncTaskNamePause;
   import codo.constants.BCEventPause;
   import codo.constants.CodoPageViewDBPause;
   import codo.constants.LocalizationInGame;
   import codo.constants.MenuType;
   import codo.utils.ScaleformUtil;
   import components.BaseButton;
   import components.ingamemenu_part.InGameMenuBtn;
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.display.Shape;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   
   [SWF(frameRate="30",backgroundColor="#999999",width="1280",height="960")]
   public class MainPause extends BaseMain
   {
      
      private static var originalCyborgPauseClass:Class;
      
      // Decompilation skipped
      
      private var _fpsMemCounter:FPSMemCounter;
      
      private var theatreWasOpen:Boolean = false;
      
      public function MainPause()
      {
         // Decompilation skipped
      }
      
      public static function prepareTheatrePauseViews() : void
      {
         // Decompilation skipped
      }
      
      override protected function onBeforeAddToStage() : void
      {
         // Decompilation skipped
      }
      
      override protected function onInit() : void
      {
         // Decompilation skipped
      }
      
      private function onKeyDownHandle(e:KeyboardEvent) : void
      {
         // Decompilation skipped
      }
      
      private function addMemCounter() : void
      {
         // Decompilation skipped
      }
      
      private function initTheatrePause() : void
      {
         // Decompilation skipped
      }
      
      private function isTheatrePause() : Boolean
      {
         // Decompilation skipped
      }
      
      private function blockTheatreLoadout(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function resumeTheatre(event:Event = null) : void
      {
         // Decompilation skipped
      }
      
      private function leaveTheatre(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function theatrePauseKey(event:KeyboardEvent) : void
      {
         // Decompilation skipped
      }
      
      private function theatrePauseFrame(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function layoutTheatrePause(container:DisplayObjectContainer) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

