package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.CFunctionContainer;
   import codo.common.CFunctionContainerCommon;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.SyncDataID;
   import codo.constants.SyncDataIDCommon;
   import codo.objects.PlayerLevelObj;
   import codo.objects.PlayerObj;
   import codo.utils.SharedUtil;
   import codo.utils.TableUtil;
   
   public class CareerBaseController extends BaseController
   {
      
      // Decompilation skipped
      
      protected var _playerObj:PlayerObj = null;
      
      protected var _currentRank:int = -1;
      
      protected var _currentLevelInfo:PlayerLevelObj = null;
      
      public function CareerBaseController()
      {
         // Decompilation skipped
      }
      
      override protected function onInitialized() : void
      {
         // Decompilation skipped
      }
      
      protected function getCareerInfo() : PlayerObj
      {
         // Decompilation skipped
      }
      
      protected function getCurrentRank() : int
      {
         // Decompilation skipped
      }
      
      protected function getXPForLevel(level:int) : int
      {
         // Decompilation skipped
      }
      
      protected function getCurrentLevel() : int
      {
         // Decompilation skipped
      }
      
      protected function getMaxLevel() : int
      {
         // Decompilation skipped
      }
      
      protected function getFloatProp(prop:String, bSecureProfile:Boolean = false) : int
      {
         // Decompilation skipped
      }
      
      protected function getIntProp(prop:String, bSecureProfile:Boolean = false) : int
      {
         // Decompilation skipped
      }
      
      protected function getRankName(rank:uint) : String
      {
         // Decompilation skipped
      }
      
      protected function getLevelInfo() : PlayerLevelObj
      {
         // Decompilation skipped
      }
      
      private function getPlayerName() : String
      {
         // Decompilation skipped
      }
      
      protected function refreshBadgeObj() : void
      {
         // Decompilation skipped
      }
      
      private function updatePlayerClanInfo() : void
      {
         // Decompilation skipped
      }
      
      private function onClanJoined(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onClanLeft(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

