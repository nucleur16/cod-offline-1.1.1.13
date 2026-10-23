package codo.views.option_sub
{
   import codo.common.BaseSubElement;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.LocalizationDict;
   import codo.constants.LocalizationSetting;
   import codo.constants.SyncDataID;
   import codo.objects.VideoObj;
   import components.BaseButton;
   import components.CheckBox;
   import components.CodoComboBox;
   import components.CodoGrowButtonNoTint;
   import components.ProgressBar;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;
   
   public class OptionView_video extends BaseSubElement
   {
      
      // Decompilation skipped
      
      public var myTFBase:TextField;
      
      public var myTFGraphic:TextField;
      
      public var mySettingWarnTF:TextField;
      
      public var myBg:Sprite;
      
      public var myApplyBtn:CodoGrowButtonNoTint;
      
      public var myDefaultBtn:CodoGrowButtonNoTint;
      
      public var myCancelBtn:CodoGrowButtonNoTint;
      
      public var myTFDisplayMode:TextField;
      
      public var myCBDisplayMode:CodoComboBox;
      
      public var myTFResolution:TextField;
      
      public var myCBResolution:CodoComboBox;
      
      public var myTFAspectRatio:TextField;
      
      public var myCBAspectRatio:CodoComboBox;
      
      public var myTFScreenRefreshRate:TextField;
      
      public var myCBScreenRefreshRate:CodoComboBox;
      
      public var myTFAntiAliasing:TextField;
      
      public var myCBAntiAliasing:CodoComboBox;
      
      public var myTFFOV:TextField;
      
      public var myPBFOV:ProgressBar;
      
      public var myTFBrightness:TextField;
      
      public var myPBBrightness:ProgressBar;
      
      public var myCBSyncEveryFrame:CheckBox;
      
      public var myCBShadows:CheckBox;
      
      public var myCBSpecularMap:CheckBox;
      
      public var myCBDepthOfField:CheckBox;
      
      public var myCBSoftenSmokeEdges:CheckBox;
      
      public var myCBDistortion:CheckBox;
      
      public var myCBRagdoll:CheckBox;
      
      public var myCBBulletImpacts:CheckBox;
      
      public var myCBGlowAllowed:CheckBox;
      
      public var myTFTextureQuality:TextField;
      
      public var myCBTextureQuality:CodoComboBox;
      
      public var myTFTextureRes:TextField;
      
      public var myCBTextureRes:CodoComboBox;
      
      public var myTFNormalMapRes:TextField;
      
      public var myCBNormalMapRes:CodoComboBox;
      
      public var myTFSpecularMapRes:TextField;
      
      public var myCBSpecularMapRes:CodoComboBox;
      
      public var myTFSSAO:TextField;
      
      public var myCBSSAO:CodoComboBox;
      
      public var myTFDynamicLight:TextField;
      
      public var myCBDynamicLight:CodoComboBox;
      
      private var _preVideoObj:VideoObj = null;
      
      private var _curVideoObj:VideoObj = null;
      
      public function OptionView_video()
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
      
      public function set videoSettings(settings:Object) : void
      {
         // Decompilation skipped
      }
      
      private function getIndexByID(dp:Array, id:int) : int
      {
         // Decompilation skipped
      }
      
      private function showTextureResolutions(visible:Boolean) : void
      {
         // Decompilation skipped
      }
      
      private function onDisplayModeChanged(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onResolutionChanged(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onCBClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onAspectRatioChanged(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onAntiAliasingChanged(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onScreenRefreshRateChanged(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onTextureQualityChanged(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onTextureResChanged(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onNormalMapResChanged(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onSpecularMapResChanged(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onSSAOChanged(evt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onDynamicLightChanged(evt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function isVideoChanged() : Boolean
      {
         // Decompilation skipped
      }
      
      private function onApplyBtnClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onDefaultBtnClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onCancelBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function closeOption() : void
      {
         // Decompilation skipped
      }
      
      private function onVideoChangeConfirmed(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

