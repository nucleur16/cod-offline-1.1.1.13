package codo.views.matchmpctroomcreator_sub
{
   import codo.common.BaseSubElement;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.BCEventCommon;
   import codo.constants.LocalizationRoom;
   import codo.constants.MatchType;
   import codo.constants.MsgBtnCount;
   import codo.constants.QuickMatchType;
   import codo.constants.SanitizedType;
   import codo.constants.SyncDataID;
   import codo.utils.StringUtil;
   import components.Album;
   import components.CodoComboBox;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;
   
   public class MatchMPCTRoomCreatorView_mpcreator extends BaseSubElement
   {
      
      private static const LIMIT_PASSWORD:uint = 16;
      
      private static const STRING_LEN_ROOM_NAME_MAX:uint = 24;
      
      // Decompilation skipped
      
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
      
      private var _optionMap:CodoComboBox;
      
      private var _currentMatchType:int = -1;
      
      private var _mapAlbum:Album;
      
      public function MatchMPCTRoomCreatorView_mpcreator()
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
      
      override public function enable() : void
      {
         // Decompilation skipped
      }
      
      override public function disable() : void
      {
         // Decompilation skipped
      }
      
      public function set active(cSwi:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function set mapAlbum(cAlbum:Album) : void
      {
         // Decompilation skipped
      }
      
      private function onInputChangedHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onMapChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomNameChangedHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onRoomNameSanitizedHandle(cEvt:ParaEvent) : void
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
      
      private function onMapAlbumChangeHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function getRoomCreatorParams() : Object
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

