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
   import codo.constants.ItemType;
   import codo.constants.MarketNavData;
   import codo.constants.PageID;
   import codo.constants.SyncDataID;
   import codo.objects.CodoPageObj;
   import codo.objects.RoomMemberObj;
   import codo.services.CharacterLoadoutService;
   import codo.utils.ScaleformUtil;
   
   public class CharacterLoadOutController extends BaseController
   {
      
      private static const TERRAIN_NAME:Array;
      
      // Decompilation skipped
      
      private var _currentCharacterItemID:int = 0;
      
      private var _relatedToWeaponLoadoutList:Boolean = true;
      
      private var _currentWeaponLoadoutFilter:uint = 0;
      
      private var _relatedToCharacterItemList:Boolean = false;
      
      private var _currentMainPageID:String;
      
      private var _pageRecordForResetCharacterOrder:CodoPageObj;
      
      private var _currentCategory:String = "";
      
      private var bEnabledAutoRefresh:Boolean = false;
      
      public function CharacterLoadOutController()
      {
         // Decompilation skipped
      }
      
      override protected function onInitialized() : void
      {
         // Decompilation skipped
      }
      
      private function onCharacterLoadoutChangeHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      public function onChangeTerrainHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function prePageChange(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function prePageChange_RelatedToWeaponLoadoutList(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function prePageChange_RelatedToCharacterItemList(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function currentWeaponLoadoutFilter() : uint
      {
         // Decompilation skipped
      }
      
      private function onClickCharacterLoadoutSlot(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoLoadoutCharacterByHeadHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function set currentCategory(cCategory:String) : void
      {
         // Decompilation skipped
      }
      
      private function getCurrentCategory() : String
      {
         // Decompilation skipped
      }
      
      private function onUnequipSlotItem(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onUnequipItemByItem(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onInventoryChangeHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function safeRefreshCharacterLoadoutAndList(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onEnableAutoRefresh(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      public function onChangeCategory(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      private function tryInventoryItem(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function setDefaultCharacter(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      public function unequipSlot(cSlotObj:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onMainPageChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function refreshCharacterLoadoutAndList(refreshMarketList:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      private function refreshCharacterPageList(refreshMarketList:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      private function onChangeCharacter3DRotationHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function initCharactersData(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function listOwnedCharacters(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function setCharacterLoadout(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function selectCharacter(characterObj:Object) : void
      {
         // Decompilation skipped
      }
      
      private function UpdateSlotsVisibilityOnCharacter(itemID:int) : void
      {
         // Decompilation skipped
      }
      
      public function showSelectedCharacter() : void
      {
         // Decompilation skipped
      }
      
      public function onCheckAvatarRefreshInTeam(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoLoadoutHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoLoadoutCharacterHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onGotoLoadoutWeaponHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onCloseCharacterHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onCloseWeaponHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onCyborgTokenUpdated(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function UpdateCyborgTokenCount() : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

