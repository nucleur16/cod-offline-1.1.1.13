package codo.views.option_sub
{
   // C1's codo.views.option_sub.OptionView_audio (Options > Audio) as decompiled from patch_ui_mp's
   // codo.gfx, with its hidden Voice section turned on for Kernel33's voice chat (voice.c).
   // build_voice_ui.py compiles this with JPEXS FFDec and splices the class's DoABC tag back into
   // codo.gfx. Changes from C1's code are marked "k33".
   //   Voice            slider -> k33_voiceVolume (playback %), checkbox mutes it (like the others)
   //   Microphone       checkbox -> k33_voiceMicMute (0 = mic on); slider -> k33_voiceThreshold, the
   //                    voice-activation level (0..100 = -60..0 dBFS)
   //   Voice Mode       C1's unused "Sound Presets" combo box, moved into the Voice section:
   //                    Push to talk / Voice activation -> k33_voiceMode 0/1
   //   Input Level      C1's unused mic indicator text: live mic level from k33_voiceLevel; while
   //                    this tab is shown it keeps k33_voiceMeter set so voice.c opens the mic
   //                    (metering only, nothing is sent)
   import codo.common.BaseSubElement;
   import codo.common.CFunctionContainerCommon;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.CodoVersion;
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

      private var _volumeComponentBind:Dictionary = new Dictionary();

      private var _k33Frames:int = 0;
      
      private var _k33Refreshing:Boolean = false;

      public function OptionView_audio()
      {
         super();
      }

      override protected function onInit() : void
      {
         this.myTFVolume.text = LocalizationSetting.SETTING_VOLUME;
         this.myTFVoice.text = LocalizationSetting.SETTING_VOICE;
         this.myCBVolumeBgm.label = LocalizationSetting.SETTING_VOLUME_BGM;
         this.myCBVolumeSE.label = LocalizationSetting.SETTING_VOLUME_SE;
         this.myCBVolumeVoice.label = LocalizationSetting.SETTING_VOLUME_VOICE;
         this.myCBVolumeMaster.label = LocalizationSetting.SETTING_VOLUME_MASTER;
         this.myCBSubtitle.label = LocalizationSetting.SETTING_SUBTITLE;
         this.myTFDevice.text = LocalizationSetting.SETTING_SOUND_DEVICE;
         this.myTFPresets.text = "Voice Mode";
         this.myDefaultBtn.label = LocalizationSetting.DEFAULT_SETTING;
         this.myCancelBtn.label = LocalizationSetting.CLOSE_SETTING;
         this.myCBVolumeVoiceChat.label = LocalizationSetting.SETTING_VOICE_CHAT_VOLUMN;
         this.myCBVolumeVoiceMic.label = LocalizationSetting.SETTING_VOICE_CHAT_RECORD;
         this.myTFMicLevel.text = LocalizationSetting.SETTING_MIC_INDICATOR;
         this._volumeComponentBind[this.myPBBgm] = {
            "dvar":"",
            "cb":this.myCBVolumeBgm,
            "volume":80
         };
         this._volumeComponentBind[this.myPBSE] = {
            "dvar":"",
            "cb":this.myCBVolumeSE,
            "volume":80
         };
         this._volumeComponentBind[this.myPBVoice] = {
            "dvar":"snd_cinematicVolumeScale",
            "cb":this.myCBVolumeVoice,
            "volume":80
         };
         this._volumeComponentBind[this.myPBMaster] = {
            "dvar":"snd_volume",
            "cb":this.myCBVolumeMaster,
            "volume":80
         };
         // k33: voice chat playback volume is Kernel33's
         this._volumeComponentBind[this.myPBVolumeVoiceChat] = {
            "dvar":"k33_voiceVolume",
            "cb":this.myCBVolumeVoiceChat,
            "volume":100
         };
         this.myPBBgm.step = 1;
         this.myPBSE.step = 1;
         this.myPBVoice.step = 1;
         this.myPBMaster.step = 1;
         this.myPBVolumeVoiceChat.step = 1;
         this.myPBVolumeVoiceMic.step = 1;
         this.myCBVolumeBgm.visible = false;
         this.myPBBgm.visible = false;
         this.myCBVolumeSE.visible = false;
         this.myPBSE.visible = false;
         this.myCBVolumeVoice.y = this.myCBVolumeBgm.y;
         this.myPBVoice.y = this.myPBBgm.y;
         this.myCBVolumeMaster.y = this.myCBVolumeSE.y;
         this.myPBMaster.y = this.myPBSE.y;
         // k33: the Voice section, which C1 hid, one control per row: Voice Mode (C1's unused presets
         // box), Voice (chat volume), Microphone (switch + activation level), Input Level (meter).
         // Subtitles moves up beside Output in the Volume section to make room.
         var tempRow:Number = this.myCBVolumeVoiceMic.y - this.myCBVolumeVoiceChat.y;
         this.myCBSubtitle.y = this.myCBDevice.y;
         this.myTFPresets.x = this.myTFMicLevel.x;
         this.myTFPresets.y = this.myCBVolumeVoiceChat.y + 4;
         this.myCBPresets.x = this.myPBVolumeVoiceChat.x;
         this.myCBPresets.y = this.myCBVolumeVoiceChat.y;
         this.myCBVolumeVoiceChat.y += tempRow;
         this.myPBVolumeVoiceChat.y += tempRow;
         this.myCBVolumeVoiceMic.y += tempRow;
         this.myPBVolumeVoiceMic.y += tempRow;
         this.myTFMicLevel.y = this.myCBVolumeVoiceMic.y + tempRow + 4;
         this.myMicLevel.x = this.myPBVolumeVoiceMic.x;
         this.myMicLevel.y = this.myTFMicLevel.y;
         this.myMicLevel.text = "";
         this.myTFVoice.visible = true;
         this.myCBVolumeVoiceChat.visible = true;
         this.myPBVolumeVoiceChat.visible = true;
         this.myCBVolumeVoiceMic.visible = true;
         this.myPBVolumeVoiceMic.visible = true;
         this.myTFPresets.visible = true;
         this.myCBPresets.visible = true;
         this.myTFMicLevel.visible = true;
         this.myMicLevel.visible = true;
         if(this.myDecoVoice != null)
         {
            this.myDecoVoice.visible = true;
         }
      }

      override protected function onInitListener() : void
      {
         this.myCBVolumeBgm.addEventListener(BaseButton.EVT_CLICK,this.onVolumeMuteHandle);
         this.myCBVolumeSE.addEventListener(BaseButton.EVT_CLICK,this.onVolumeMuteHandle);
         this.myCBVolumeVoice.addEventListener(BaseButton.EVT_CLICK,this.onVolumeMuteHandle);
         this.myCBVolumeMaster.addEventListener(BaseButton.EVT_CLICK,this.onVolumeMuteHandle);
         this.myCBVolumeVoiceChat.addEventListener(BaseButton.EVT_CLICK,this.onVolumeMuteHandle);
         this.myCBVolumeVoiceMic.addEventListener(BaseButton.EVT_CLICK,this.onMicMuteHandle);
         this.myCBSubtitle.addEventListener(BaseButton.EVT_CLICK,this.onToggleSubtitleHandle);
         this.myPBBgm.addEventListener(ProgressBar.EVT_VALUE_CHANGE,this.onVolumeChangeHandle);
         this.myPBSE.addEventListener(ProgressBar.EVT_VALUE_CHANGE,this.onVolumeChangeHandle);
         this.myPBVoice.addEventListener(ProgressBar.EVT_VALUE_CHANGE,this.onVolumeChangeHandle);
         this.myPBMaster.addEventListener(ProgressBar.EVT_VALUE_CHANGE,this.onVolumeChangeHandle);
         this.myPBVolumeVoiceChat.addEventListener(ProgressBar.EVT_VALUE_CHANGE,this.onVolumeChangeHandle);
         this.myPBVolumeVoiceMic.addEventListener(ProgressBar.EVT_VALUE_CHANGE,this.onThresholdChangeHandle);
         this.myCBDevice.addEventListener(CodoComboBox.EVT_CHANGE,this.onDeviceChangedHandle);
         this.myCBPresets.addEventListener(CodoComboBox.EVT_CHANGE,this.onPresetsChangedHandle);
         this.myDefaultBtn.addEventListener(BaseButton.EVT_CLICK,this.onDefaultBtnClickHandle);
         this.myCancelBtn.addEventListener(BaseButton.EVT_CLICK,this.onCancelBtnClickHandle);
         this.listen(BCEvent.EVT_REFRESH_AUDIO_VIEW,this.onRefreshAudioViewHandle);
      }

      override public function enable() : void
      {
         super.enable();
         this.myPBMaster.value = this.getSyncDP(SyncDataID.GET_MASTER_VOLUME);
         this._volumeComponentBind[this.myPBMaster].volume = this.myPBMaster.value;
         this.myCBVolumeMaster.selected = this.myPBMaster.value > 0;
         this.myPBVoice.value = this.getSyncDP(SyncDataID.GET_CINEMATIC_VOLUME);
         this._volumeComponentBind[this.myPBVoice].volume = this.myPBVoice.value;
         this.myCBVolumeVoice.selected = this.myPBVoice.value > 0;
         var tempDeviceID:int = this.getSyncDP(SyncDataID.GET_CURRENT_OUTPUT_CONFIG);
         this.myCBDevice.dataProvider = this.getSyncDP(SyncDataID.GET_OUTPUT_CONFIGURATIONS);
         var i:uint = 0;
         while(i < this.myCBDevice.dataProvider.length)
         {
            if(this.myCBDevice.dataProvider[i].id == tempDeviceID)
            {
               this.myCBDevice.selectedDataIndex = i;
               break;
            }
            i++;
         }
         // k33
         var tempVolume:int = CFunctionContainerCommon.Dvar_GetInt("k33_voiceVolume");
         this.myPBVolumeVoiceChat.value = tempVolume > 100 ? 100 : tempVolume;
         if(this.myPBVolumeVoiceChat.value > 0)
         {
            this._volumeComponentBind[this.myPBVolumeVoiceChat].volume = this.myPBVolumeVoiceChat.value;
         }
         this.myCBVolumeVoiceChat.selected = this.myPBVolumeVoiceChat.value > 0;
         this.myPBVolumeVoiceMic.value = CFunctionContainerCommon.Dvar_GetInt("k33_voiceThreshold");
         this.myCBVolumeVoiceMic.selected = CFunctionContainerCommon.Dvar_GetInt("k33_voiceMicMute") == 0;
         this._k33Refreshing = true;
         this.myCBPresets.dataProvider = [{
            "label":"Push to talk",
            "id":0
         },{
            "label":"Voice activation",
            "id":1
         }];
         this.myCBPresets.selectedDataIndex = CFunctionContainerCommon.Dvar_GetInt("k33_voiceMode") == 1 ? 1 : 0;
         this._k33Refreshing = false;
         this.myCBSubtitle.selected = this.getSyncDP(SyncDataID.GET_TOGGLE_SUBTITLE);
         this._k33Frames = 0;
         this.addEventListener(Event.ENTER_FRAME,this.onMicLevelFrame);
      }

      override public function disable() : void
      {
         super.disable();
         this.removeEventListener(Event.ENTER_FRAME,this.onMicLevelFrame);
      }

      private function onToggleSubtitleHandle(cEvt:ParaEvent) : void
      {
         var tempCB:CheckBox = cEvt.target as CheckBox;
         tempCB.selected = !tempCB.selected;
         this.broadcast(BCEvent.EVT_TOGGLE_SUBTITLE,tempCB.selected);
      }

      private function onVolumeMuteHandle(cEvt:ParaEvent) : void
      {
         var tempCB:CheckBox = cEvt.target as CheckBox;
         tempCB.selected = !tempCB.selected;
         var tempPB:ProgressBar = this.getPBByCB(tempCB) as ProgressBar;
         tempPB.value = tempCB.selected ? Number(this._volumeComponentBind[tempPB].volume) : 0;
         this.changeVolume(this._volumeComponentBind[tempPB].dvar,tempPB.value,!tempCB.selected,true);
      }

      private function onVolumeChangeHandle(cEvt:Event) : void
      {
         var tempPB:ProgressBar = cEvt.target as ProgressBar;
         this._volumeComponentBind[tempPB].volume = tempPB.value;
         Logger.print("OptionAudioView::onVolumeChangeHandle this._volumeComponentBind[tempPB].volume: " + this._volumeComponentBind[tempPB].volume);
         this._volumeComponentBind[tempPB].cb.selected = tempPB.value > 0;
         this.changeVolume(this._volumeComponentBind[tempPB].dvar,tempPB.value,tempPB.value <= 0);
      }

      // k33: the Microphone checkbox is the mic switch, the slider the voice-activation level
      private function onMicMuteHandle(cEvt:ParaEvent) : void
      {
         var tempCB:CheckBox = cEvt.target as CheckBox;
         tempCB.selected = !tempCB.selected;
         CFunctionContainerCommon.setdvar("k33_voiceMicMute",tempCB.selected ? "0" : "1");
      }

      private function onThresholdChangeHandle(cEvt:Event) : void
      {
         var tempPB:ProgressBar = cEvt.target as ProgressBar;
         CFunctionContainerCommon.setdvar("k33_voiceThreshold",String(Math.round(tempPB.value)));
      }

      private function onPresetsChangedHandle(cEvt:ParaEvent) : void
      {
         var tempValue:Object = this.myCBPresets.selectedValue;
         if(tempValue != null && !this._k33Refreshing)
         {
            CFunctionContainerCommon.setdvar("k33_voiceMode",String(tempValue.id));
         }
      }

      private function onMicLevelFrame(cEvt:Event) : void
      {
         if(this._k33Frames % 10 == 0)
         {
            CFunctionContainerCommon.setdvar("k33_voiceMeter","1");
         }
         this._k33Frames++;
         var tempLevel:int = CFunctionContainerCommon.Dvar_GetInt("k33_voiceLevel");
         var tempText:String = "";
         if(tempLevel < 0)
         {
            tempText = "no microphone";
         }
         else
         {
            var tempBars:int = int(tempLevel / 5);
            var tempMark:int = int(this.myPBVolumeVoiceMic.value / 5);
            var j:int = 0;
            while(j < 20)
            {
               tempText += j == tempMark ? "|" : (j < tempBars ? "#" : ".");
               j++;
            }
            tempText += " " + tempLevel;
         }
         if(this.myMicLevel.text != tempText)
         {
            this.myMicLevel.text = tempText;
         }
      }

      private function onDeviceChangedHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.EVT_CHANGE_OUTPUT_CONFIG,this.myCBDevice.selectedValue.id);
      }

      private function onApplyBtnClickHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.EVT_CLOSE_OPTION);
      }

      private function onDefaultBtnClickHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.EVT_RESET_AUDIO);
      }

      private function onCancelBtnClickHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.EVT_CLOSE_OPTION);
      }

      private function onRefreshAudioViewHandle(cEvt:ParaEvent) : void
      {
         if(this.visible == true)
         {
            this.enable();
         }
      }

      private function cancelModification() : void
      {
      }

      private function changeVolume(dvar:String, volume:Number, mute:Boolean, fromMute:Boolean = false) : void
      {
         this.broadcast(BCEvent.EVT_SET_VOLUME,{
            "dvar":dvar,
            "volume":volume,
            "mute":mute,
            "fromMute":fromMute
         });
      }

      private function getPBByCB(cCB:CheckBox) : ProgressBar
      {
         var targetPB:ProgressBar = null;
         var i:* = undefined;
         for(i in this._volumeComponentBind)
         {
            if(this._volumeComponentBind[i].cb == cCB)
            {
               targetPB = i;
               break;
            }
         }
         return targetPB;
      }
   }
}
