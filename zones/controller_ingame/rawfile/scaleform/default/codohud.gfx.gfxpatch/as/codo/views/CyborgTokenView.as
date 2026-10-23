package codo.views
{
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.Logger;
   import codo.constants.LocalizationInGame;
   import components.cyborg.cyborgInGame_part.CyborgTokenItem;
   import flash.events.KeyboardEvent;
   import flash.text.TextField;
   
   public class CyborgTokenView extends BaseView
   {
      
      private static const POS_ORIGIN_X:Number = 165;
      
      private static const POS_ORIGIN_Y:Number = 560;
      
      // Decompilation skipped
      
      public var myTokenTitleTF:TextField;
      
      public var myTokenSubTitleTF:TextField;
      
      public var myToken2:CyborgTokenItem;
      
      public var myToken1:CyborgTokenItem;
      
      public var myToken0:CyborgTokenItem;
      
      private var _currentFocusToken:CyborgTokenItem;
      
      private var _tokens:Vector.<CyborgTokenItem>;
      
      public function CyborgTokenView()
      {
         // Decompilation skipped
      }
      
      override protected function onInit() : void
      {
         // Decompilation skipped
      }
      
      override protected function enableData() : void
      {
         // Decompilation skipped
      }
      
      private function onKeyDownHandle(cEvt:KeyboardEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onKeyUpHandle(cEvt:KeyboardEvent) : void
      {
         // Decompilation skipped
      }
      
      override protected function disableData() : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

