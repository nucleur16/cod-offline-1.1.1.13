package codo.views.option_sub
{
   import codo.common.BaseSubElement;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.CODOVirtualKey;
   import codo.constants.Common;
   import codo.constants.LocalizationSetting;
   import codo.constants.SyncDataID;
   import components.BaseButton;
   import components.CheckBox;
   import components.CodoButton;
   import components.CodoGrowButton;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import flash.events.MouseEvent;
   import flash.text.TextField;
   import scaleform.gfx.MouseEventEx;
   
   public class OptionView_control_chat extends BaseSubElement
   {
      
      private static const SRC_NAME_CONTROL_BTN:String = "myBtn_";
      
      private static var CONTROL_BTN_NUM:int = 2;
      
      private static var CONTROL_CMD:Array;
      
      private static var CONTROL_CMD_2:Array;
      
      // Decompilation skipped
      
      public var myTFChat:TextField;
      
      public var myTFTalk:TextField;
      
      public var myCBTalk:CheckBox;
      
      public var myBtn_0:CodoGrowButton;
      
      public var myBtn_1:CodoGrowButton;
      
      public var myTFChangeBindHint:TextField;
      
      private var _currentBtnIndex:uint = 0;
      
      private var _isChangingBind:Boolean = false;
      
      private var _isMouseDownDuringBind:Boolean = false;
      
      public function OptionView_control_chat()
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
      
      private function onResetControlsHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function addBtnListener(cBtn:CodoGrowButton) : void
      {
         // Decompilation skipped
      }
      
      private function refreshCBState() : void
      {
         // Decompilation skipped
      }
      
      private function setCBBind(bBind:Boolean, command:String, associatedCommand:String) : void
      {
         // Decompilation skipped
      }
      
      private function refreshAllCommands() : void
      {
         // Decompilation skipped
      }
      
      private function onCBClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onBtnClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      public function bindKeyToCmd(keyNum:uint, bMouse:Boolean = false) : void
      {
         // Decompilation skipped
      }
      
      private function finishKeyBinding() : void
      {
         // Decompilation skipped
      }
      
      private function onMouseDownHandle(cEvt:MouseEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onMouseWheelHandle(cEvt:MouseEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onStageKeyDownHandle(cEvt:KeyboardEvent) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

