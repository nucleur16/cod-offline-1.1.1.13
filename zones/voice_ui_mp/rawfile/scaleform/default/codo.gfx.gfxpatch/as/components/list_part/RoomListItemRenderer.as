package components.list_part
{
   import codo.common.ParaEvent;
   import codo.constants.LocalizationRoom;
   import codo.constants.MatchType;
   import codo.constants.RoomStatus;
   import components.BaseButton;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;
   import flash.text.TextFormat;
   
   public class RoomListItemRenderer extends ListItemRendererBase
   {
      
      protected static const ITEM_HEIGHT:uint = 31;
      
      protected static const TF_ALPHA_NORMAL:Number = 0.6;
      
      protected static const TF_ALPHA_FOCUS:Number = 1;
      
      public static const EVT_JOIN_ROOM:String = "evt_join_room";
      
      // Decompilation skipped
      
      public var myIconLock:Sprite;
      
      public var myIDTF:TextField;
      
      public var myNameTF:TextField;
      
      public var myMapTF:TextField;
      
      public var myLeagueTF:TextField;
      
      public var myPlayersTF:TextField;
      
      public var myStatusTF:TextField;
      
      public var myModeTF:TextField;
      
      public var myBgNormal:Sprite;
      
      public var myBgFocus:Sprite;
      
      public function RoomListItemRenderer()
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
      
      protected function get isCyborg() : Boolean
      {
         // Decompilation skipped
      }
      
      override public function get height() : Number
      {
         // Decompilation skipped
      }
      
      override protected function onGetData() : void
      {
         // Decompilation skipped
      }
      
      override protected function setToNormal(cTween:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      override protected function setToRollOver(cTween:Boolean = true) : void
      {
         // Decompilation skipped
      }
      
      private function onJoinRoomHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function theatreFitText(field:TextField) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

