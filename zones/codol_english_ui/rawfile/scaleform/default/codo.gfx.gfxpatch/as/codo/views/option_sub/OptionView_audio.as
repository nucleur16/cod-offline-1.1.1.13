package codo.views.option_sub
{
   import codo.common.BaseSubElement;
   import codo.common.CFunctionContainerCommon;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.LocalizationSetting;
   import codo.constants.SyncDataID;
   import components.BaseButton;
   import components.CheckBox;
   import components.CodoComboBox;
   import components.CodoGrowButtonNoTint;
   import components.ProgressBar;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;
   import flash.utils.Dictionary;
   
   public class OptionView_audio extends BaseSubElement
   {
      
      // Decompilation skipped
      
      public var myTFVoice:TextField;
      
      public var myTFVolume:TextField;
      
      public var myCBVolumeBgm:CheckBox;
      
      public var myPBBgm:ProgressBar;
      
      public var myCBVolumeSE:CheckBox;
      
      public var myPBSE:ProgressBar;
      
      public var myCBVolumeVoice:CheckBox;
      
      public var myPBVoice:ProgressBar;
      
      public var myCBVolumeMaster:CheckBox;
      
      public var myPBMaster:ProgressBar;
      
      public var myTFDevice:TextField;
      
      public var myCBDevice:CodoComboBox;
      
      public var myTFPresets:TextField;
      
      public var myCBPresets:CodoComboBox;
      
      public var myDecoVoice:Sprite;
      
      public var myCBVolumeVoiceChat:CheckBox;
      
      public var myPBVolumeVoiceChat:ProgressBar;
      
      public var myCBVolumeVoiceMic:CheckBox;
      
      public var myPBVolumeVoiceMic:ProgressBar;
      
      public var myCBSubtitle:CheckBox;
      
      public var myTFMicLevel:TextField;
      
      public var myMicLevel:TextField;
      
      public var myDefaultBtn:CodoGrowButtonNoTint;
      
      public var myCancelBtn:CodoGrowButtonNoTint;
      
      private var _volumeComponentBind:Dictionary;
      
      private var _k33Frames:int = 0;
      
      private var _k33Bar:Sprite = null;
      
      private var _k33BarW:Number = 200;
      
      private var _k33BarH:Number = 6;
      
      private var _k33Shown:Number = 0;
      
      private var _k33Peak:Number = 0;
      
      private var _k33Drawn:String = "";
      
      private var _k33Refreshing:Boolean = false;
      
      public function OptionView_audio()
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
      
      private function onToggleSubtitleHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onVolumeMuteHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onVolumeChangeHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onMicMuteHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onThresholdChangeHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onPresetsChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onMicLevelFrame(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function drawLevelBar(cLevel:Number, cPeak:Number, cThr:Number, cMic:Boolean) : void
      {
         // Decompilation skipped
      }
      
      private function onDeviceChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onApplyBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onDefaultBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onCancelBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onRefreshAudioViewHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function cancelModification() : void
      {
         // Decompilation skipped
      }
      
      private function changeVolume(dvar:String, volume:Number, mute:Boolean, fromMute:Boolean = false) : void
      {
         // Decompilation skipped
      }
      
      private function getPBByCB(cCB:CheckBox) : ProgressBar
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

