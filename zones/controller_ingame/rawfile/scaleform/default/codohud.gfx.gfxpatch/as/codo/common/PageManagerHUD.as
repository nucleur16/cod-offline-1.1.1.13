package codo.common
{
   import codo.constants.CodoPageViewDBHUD;
   import codo.constants.ControllerNameHUD;
   import codo.constants.PageID;
   import codo.constants.ViewState;
   import codo.controllers.AASController;
   import codo.controllers.HudChattingController;
   import codo.controllers.HudCoopEOGController;
   import codo.controllers.HudPlayerNameController;
   import codo.controllers.NotificationInGameController;
   import codo.controllers.SanitizeController;
   import codo.controllers.ScoreboardController;
   import codo.objects.CodoPageObj;
   import codo.services.CommonService;
   import codo.services.HudService;
   
   public class PageManagerHUD extends PageManagerBase
   {
      
      private static var instance:PageManagerHUD;
      
      // Decompilation skipped
      
      public function PageManagerHUD()
      {
         // Decompilation skipped
      }
      
      public static function getInstance() : PageManagerHUD
      {
         // Decompilation skipped
      }
      
      override protected function initAllServices() : void
      {
         // Decompilation skipped
      }
      
      override protected function initAllViews() : void
      {
         // Decompilation skipped
      }
      
      override protected function onStartUI(cParam:* = null) : void
      {
         // Decompilation skipped
      }
      
      override public function showPage(cPageObj:CodoPageObj, cMask:Boolean = false, cStatus:int = -1) : void
      {
         // Decompilation skipped
      }
      
      public function showScoreboard(show:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function showRevivePrompt(show:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function isCoopEOGShowing() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get aasController() : AASController
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

