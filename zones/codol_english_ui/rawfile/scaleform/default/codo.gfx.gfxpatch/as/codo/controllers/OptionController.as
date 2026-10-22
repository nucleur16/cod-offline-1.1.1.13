package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.CFunctionContainer;
   import codo.common.CFunctionContainerCommon;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.CODOVirtualKey;
   import codo.constants.CodoPageViewDB;
   import codo.constants.Common;
   import codo.constants.LocalizationSetting;
   import codo.constants.LocalizationSystem;
   import codo.constants.MsgBtnCount;
   import codo.constants.SyncDataID;
   import codo.constants.Table.UINotifications;
   import codo.services.SettingsService;
   import codo.utils.LocalizeUtil;
   import codo.utils.ScaleformUtil;
   import flash.events.TimerEvent;
   import flash.utils.Timer;
   
   public class OptionController extends BaseController
   {
      
      private static var vital_commands_movement:Array;
      
      private static var vital_commands_toggle:Array;
      
      private static var vital_commands_longpress:Array;
      
      private static var vital_commands_action:Array;
      
      private static var vital_commands_chat:Array;
      
      private static var CONTROL_CMD_LOOK:Array;
      
      private static var CONTROL_CMD_ACTION:Array;
      
      private static var CONTROL_CMD_ACTION2:Array;
      
      private static var CONTROL_CMD_MOVEMENT:Array;
      
      private static var CONTROL_CMD_MOVEMENT2:Array;
      
      private static var CONTROL_CMD_CHAT:Array;
      
      private static var CONTROL_CMD_CHAT2:Array;
      
      private static var CONTROL_CMD_ALL:Array;
      
      // Decompilation skipped
      
      private var _outputConfigurations:Array = null;
      
      private var _isMenuDvarsSetup:Boolean = false;
      
      private var _optionVideoApplyParams:Object;
      
      private var keyNum:uint;
      
      private var cmd:String;
      
      private var unboundCommands:Array;
      
      public function OptionController()
      {
         // Decompilation skipped
      }
      
      override protected function onInitialized() : void
      {
         // Decompilation skipped
      }
      
      private function getCurrentRes() : Array
      {
         // Decompilation skipped
      }
      
      private function getToggleSubtitle() : Boolean
      {
         // Decompilation skipped
      }
      
      private function getMasterVolume() : Number
      {
         // Decompilation skipped
      }
      
      private function getCinematicVolume() : Number
      {
         // Decompilation skipped
      }
      
      private function getVoiceChatVolume() : Number
      {
         // Decompilation skipped
      }
      
      private function getVoiceMicVolume() : Number
      {
         // Decompilation skipped
      }
      
      private function getCurrentOutputConfigurationID() : int
      {
         // Decompilation skipped
      }
      
      private function getOutputConfigurations() : Array
      {
         // Decompilation skipped
      }
      
      private function isVoiceMicMute() : Boolean
      {
         // Decompilation skipped
      }
      
      private function getGammaRange() : Object
      {
         // Decompilation skipped
      }
      
      private function getFOVRange() : Object
      {
         // Decompilation skipped
      }
      
      private function getSensitivityRange() : Object
      {
         // Decompilation skipped
      }
      
      private function getMouseSensitivity() : Number
      {
         // Decompilation skipped
      }
      
      private function getMouseInvert() : Number
      {
         // Decompilation skipped
      }
      
      private function getMouseSmooth() : Boolean
      {
         // Decompilation skipped
      }
      
      private function getFreeLook() : Boolean
      {
         // Decompilation skipped
      }
      
      private function getKeyBindingCountFromCommand(command:String) : uint
      {
         // Decompilation skipped
      }
      
      private function getKeyBindingNamesFromCommand(cmd:String) : String
      {
         // Decompilation skipped
      }
      
      private function getMappingKey(keyNum:uint) : uint
      {
         // Decompilation skipped
      }
      
      private function onGotoOptionHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onToggleSubtitleHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSetVolumeHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onChangeOutputConfigHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSetMouseSensitivityHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSetMouseInvertHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSetMouseSmoothHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSetFreeLookHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onBindKeyToHoldCommandHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onBindKeyToCommandHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function confirmBind() : void
      {
         // Decompilation skipped
      }
      
      private function onCloseOptionHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onResetAudioHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onPlayerStatsReadyHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onVideoEnableChaged(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onOptionVideoApplyHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onOptionVideoApplyConfirm() : void
      {
         // Decompilation skipped
      }
      
      private function useDefaultAudio() : void
      {
         // Decompilation skipped
      }
      
      private function useDefaultVideo() : void
      {
         // Decompilation skipped
      }
      
      private function useDefaultControl() : void
      {
         // Decompilation skipped
      }
      
      private function onRefreshDefaultAudios(cEvt:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRefreshDefaultControls(cEvt:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      private function isVitalCommandsBounded() : Boolean
      {
         // Decompilation skipped
      }
      
      private function retrieveVideoSettings() : Object
      {
         // Decompilation skipped
      }
      
      private function onSetDefaultVideo(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSetDefaultControl(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSetBrightnessHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSetFovHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onChooseKeyboard(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function hasKeyboard() : Boolean
      {
         // Decompilation skipped
      }
      
      private function getKeyboard() : int
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

