package codo.views
{
   import codo.common.BaseView;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.BCEventCommon;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationDict;
   import codo.constants.LocalizationRoom;
   import codo.constants.MatchType;
   import codo.constants.MsgBtnCount;
   import codo.constants.QuickMatchType;
   import codo.constants.SanitizedType;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.utils.ImageLoader;
   import codo.utils.StringUtil;
   import com.greensock.TweenLite;
   import components.BaseButton;
   import components.CodoComboBox;
   import components.CodoFlipButton;
   import components.CodoGrowButton;
   import components.CodoGrowButtonNoTint;
   import flash.display.Bitmap;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;
   
   public class MatchMiniRoomCreatorView extends BaseView
   {
      
      private static const LIMIT_PASSWORD:uint = 16;
      
      private static var ALPHA_ORIGIN:int = 0;
      
      private static var ALPHA_OPEN:int = 1;
      
      private static const STRING_LEN_ROOM_NAME_MAX:uint = 24;
      
      private static const POS_X_ORIGIN:int = 870;
      
      private static const POS_X_OPEN:int = 50;
      
      private static const POS_Y_ORIGIN:int = 139;
      
      // Decompilation skipped
      
      public var myFlipBtn:CodoFlipButton;
      
      public var myHeader:CodoGrowButtonNoTint;
      
      public var myCreateBtn:CodoGrowButtonNoTint;
      
      public var myMapPretBtn:CodoGrowButton;
      
      public var myMapNextBtn:CodoGrowButton;
      
      public var myRoomNameTF:TextField;
      
      public var myRoomNameLabelTF:TextField;
      
      public var myPasswordLabelTF:TextField;
      
      public var myPasswordTF:TextField;
      
      public var myOptionLabelTF_0:TextField;
      
      public var myOptionCB_0:CodoComboBox;
      
      public var myOptionBg_0:Sprite;
      
      public var myOptionLabelTF_1:TextField;
      
      public var myOptionCB_1:CodoComboBox;
      
      public var myOptionBg_1:Sprite;
      
      public var myOptionLabelTF_2:TextField;
      
      public var myOptionCB_2:CodoComboBox;
      
      public var myOptionBg_2:Sprite;
      
      public var myOptionLabelTF_3:TextField;
      
      public var myOptionCB_3:CodoComboBox;
      
      public var myOptionBg_3:Sprite;
      
      public var myMapImg:Sprite;
      
      protected var _optionMap:CodoComboBox;
      
      protected var _currentMatchType:int = -1;
      
      public function MatchMiniRoomCreatorView()
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
      
      override protected function disableData() : void
      {
         // Decompilation skipped
      }
      
      private function onInputChangedHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onCoopTypeChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      protected function onMapChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomNameChangedHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onBackBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      protected function onCreateRoomBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomNameSanitizedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onPreMapBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onNextMapBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      protected function showMapInfo(cMap:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onMapImageLoadComplete(cBMP:Bitmap) : void
      {
         // Decompilation skipped
      }
      
      private function refreshCoopData() : void
      {
         // Decompilation skipped
      }
      
      private function refreshMPData() : void
      {
         // Decompilation skipped
      }
      
      private function refreshCyborgsData() : void
      {
         // Decompilation skipped
      }
      
      private function set mapDP(mapDP:Array) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

