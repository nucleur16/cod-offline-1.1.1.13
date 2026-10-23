package codo.services
{
   import codo.common.CFunctionContainer;
   import codo.common.CFunctionContainerCommon;
   import codo.common.Logger;
   import codo.common.ServiceBase;
   import codo.constants.AsyncTaskGroup;
   import codo.constants.AsyncTaskID;
   import codo.constants.Common;
   import codo.constants.LocalizationDict;
   import codo.constants.LocalizationMarket;
   import codo.constants.MarketMenuData;
   import codo.constants.MarketSlots;
   import codo.constants.MsgBtnCount;
   import codo.constants.ServiceID;
   import codo.models.DataProvider;
   import codo.objects.AccountObj;
   import codo.objects.MailAttachmentObj;
   import codo.objects.ProductObj;
   import codo.objects.ResultObj;
   import codo.utils.ScaleformUtil;
   import flash.net.SharedObject;
   
   public class MarketService extends ServiceBase
   {
      
      private static var instance:MarketService;
      
      public static const SORT_TYPE_NORMAL:uint = 0;
      
      public static const SORT_TYPE_ATTACHMENT:uint = 1;
      
      public static const SORT_TYPE_PERK:uint = 2;
      
      // Decompilation skipped
      
      private var _shoppingCartDP:Array;
      
      private var _currentCategory:String = "";
      
      private var _currentType:String;
      
      private var _currentListDP:Array;
      
      public var _marketDP:Array;
      
      public var _inventoryDP:Array;
      
      private var _itemSortPropDat:Array;
      
      private var _itemSortOptionDat:Array;
      
      private var _itemSortPropDat_Attachment:Array;
      
      private var _itemSortOptionDat_Attachment:Array;
      
      private var _itemSortPropDat_Perk:Array;
      
      private var _itemSortOptionDat_Perk:Array;
      
      private var _fakeMailAttachments:Array;
      
      private var _fakeMarketFavorites:Array;
      
      private var _shoppingCart:Array;
      
      public function MarketService()
      {
         // Decompilation skipped
      }
      
      public static function getInstance() : MarketService
      {
         // Decompilation skipped
      }
      
      override public function init() : void
      {
         // Decompilation skipped
      }
      
      public function getShoppingCartDP() : Array
      {
         // Decompilation skipped
      }
      
      public function getItemListByCategory(cCategoryNameID:String, cTypeFilter:String = "all", bApplyContextFilter:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      public function listMarketCategoryItems(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      public function listInventoryCategoryItems(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      public function invalidateCurrentCategoryItemList() : void
      {
         // Decompilation skipped
      }
      
      public function sortData(dp:Array, sortType:uint = 0) : Array
      {
         // Decompilation skipped
      }
      
      private function getSpecificSortType(category:String) : uint
      {
         // Decompilation skipped
      }
      
      public function setCategoryItemsDataToInventory(dp:Array, category:String) : void
      {
         // Decompilation skipped
      }
      
      public function setCategoryItemsDataToMarket(dp:Array, category:String) : void
      {
         // Decompilation skipped
      }
      
      public function addToShoppingCart(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      public function deleteFromShoppingCart(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      public function tryInMarket(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      public function shoppingCartSwapSku(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      public function requestShoppingCart() : void
      {
         // Decompilation skipped
      }
      
      private function requestMarketRecommendedItems() : void
      {
         // Decompilation skipped
      }
      
      private function requestMarketFavorites() : void
      {
         // Decompilation skipped
      }
      
      public function addMarketFavorite(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      public function removeMarketFavorite(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      private function fillFakeListDataForCategory(tempDP:Array, categoryNameID:String) : void
      {
         // Decompilation skipped
      }
      
      public function fillFakeListData(tempDP:Array) : void
      {
         // Decompilation skipped
      }
      
      public function getItemInfoObj(cID:int) : Object
      {
         // Decompilation skipped
      }
      
      public function requestMyAccount() : void
      {
         // Decompilation skipped
      }
      
      private function setMarketFilter(cSelectedFilter:uint) : void
      {
         // Decompilation skipped
      }
      
      private function setItemSort(cSortDat:Array) : void
      {
         // Decompilation skipped
      }
      
      public function buyMarketItem(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      public function updateInventroy() : void
      {
         // Decompilation skipped
      }
      
      private function purchaseGift(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      private function claimCoupon(couponID:String) : void
      {
         // Decompilation skipped
      }
      
      private function transferCoupon(cParams:Object) : void
      {
         // Decompilation skipped
      }
      
      private function redeemCoupon(couponID:String) : void
      {
         // Decompilation skipped
      }
      
      private function updateCoupon() : void
      {
         // Decompilation skipped
      }
      
      private function fetchMailAttachments() : void
      {
         // Decompilation skipped
      }
      
      private function get fakeMailAttachments() : Array
      {
         // Decompilation skipped
      }
      
      public function autoFillProductObj(productObject:Object) : Object
      {
         // Decompilation skipped
      }
      
      public function getMarketDPs(marketSlotNameIDs:Array, cFilter:String, bApplyContextFilter:Boolean) : Array
      {
         // Decompilation skipped
      }
      
      public function GetProductList(categoryid:String, filter:String, bApplyContextFilter:Boolean = false) : Array
      {
         // Decompilation skipped
      }
      
      public function getCouponDP(cFilter:String) : Array
      {
         // Decompilation skipped
      }
      
      public function getMarketDP(marketSlotNameID:String, cFilter:String, bApplyContextFilter:Boolean) : Array
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

