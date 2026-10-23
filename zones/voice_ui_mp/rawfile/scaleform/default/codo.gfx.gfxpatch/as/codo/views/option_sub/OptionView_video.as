package codo.views.option_sub
{
   // C1's codo.views.option_sub.OptionView_video (Options > Graphics) as decompiled from patch_ui_mp's
   // codo.gfx, with a frame-rate cap row for Kernel33 (kernel33.c PatchFrameCap). build_voice_ui.py
   // compiles this with JPEXS FFDec and splices the class's DoABC tag back into codo.gfx. Changes from
   // C1's code are marked "k33".
   //   Max FPS   C1's Aspect Ratio row, which its onInit hides (the box is not used: the aspect ratio
   //             follows the resolution), shown again below Anti-Aliasing (Vsync moves down a row):
   //             Unlimited / 30 / 60 / 75 / 90 / 120 / 144 / 165 / 240 / 360 / 500 -> "1848"
   //             (com_maxfps, 0 = no cap, archived in config_mp.cfg), set by Apply like the rest of
   //             the screen; a value set from the console is listed as its own entry
   import codo.common.BaseSubElement;
   import codo.common.CFunctionContainerCommon;
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
      
      // k33: frame-rate choices for "1848" (com_maxfps); 0 = no cap
      private static const K33_FPS_DVAR:String = "1848";
      
      private static const K33_FPS_VALUES:Array = [0,30,60,75,90,120,144,165,240,360,500];
      
      private var _k33FpsApplied:int = -1;
      
      public function OptionView_video()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.myTFBase.text = LocalizationDict.OPTION_BASE;
         this.myTFGraphic.text = LocalizationDict.OPTION_GRAPHIC;
         this.mySettingWarnTF.text = LocalizationSetting.SETTING_CHANGE_WARNING;
         this.myApplyBtn.label = LocalizationSetting.APPLY_SETTING;
         this.myDefaultBtn.label = LocalizationSetting.RECOMMENDED_SETTING;
         this.myCancelBtn.label = LocalizationSetting.CANCEL_SETTING;
         this.myTFDisplayMode.text = LocalizationSetting.DISPLAY_MODE;
         this.myTFResolution.text = LocalizationSetting.RESOLUTION;
         this.myTFBrightness.text = LocalizationSetting.BRIGHTNESS;
         this.myTFAspectRatio.text = LocalizationSetting.ASPECT_RATIO;
         this.myTFAntiAliasing.text = LocalizationSetting.ANTI_ALIASING;
         this.myTFScreenRefreshRate.text = LocalizationSetting.SCREEN_REFRESH_RATE;
         this.myCBSyncEveryFrame.label = LocalizationSetting.SYNC_EVERY_FRAME;
         this.myCBShadows.label = LocalizationSetting.SHADOWS;
         this.myCBSpecularMap.label = LocalizationSetting.SPECULAR_MAP;
         this.myCBDepthOfField.label = LocalizationSetting.DEPTH_OF_FIELD;
         this.myCBSoftenSmokeEdges.label = LocalizationSetting.SOFTEN_SMOKE_EDGES;
         this.myCBRagdoll.label = LocalizationSetting.RAGDOLL;
         this.myCBBulletImpacts.label = LocalizationSetting.BULLET_IMPACTS;
         this.myCBDistortion.label = LocalizationSetting.DISTORTION;
         this.myCBGlowAllowed.label = LocalizationSetting.GLOW_ALLOWED;
         this.myTFTextureQuality.text = LocalizationSetting.TEXTURE_QUALITY;
         this.myTFTextureRes.text = LocalizationSetting.TEXTURE_RESOLUTION;
         this.myTFNormalMapRes.text = LocalizationSetting.NORMAL_MAP_RESOLUTION;
         this.myTFSpecularMapRes.text = LocalizationSetting.SPECULAR_MAP_RESOLUTION;
         this.myTFSSAO.text = LocalizationSetting.SSAO;
         this.myTFDynamicLight.text = LocalizationSetting.DYNAMIC_LIGHT_LIMIT;
         this.myTFFOV.text = LocalizationSetting.VIDEO_FOV;
         this.myPBFOV.step = 1;
         this.myTFAspectRatio.text = "Max FPS";
         this.myTFAntiAliasing.y = this.myTFScreenRefreshRate.y;
         this.myCBAntiAliasing.y = this.myCBScreenRefreshRate.y;
         this.myTFScreenRefreshRate.y = this.myTFAspectRatio.y;
         this.myCBScreenRefreshRate.y = this.myCBAspectRatio.y;
         // k33: the Aspect Ratio row becomes Max FPS, one row below Anti-Aliasing; Vsync moves down a row
         var tempRow:Number = this.myCBAntiAliasing.y - this.myCBScreenRefreshRate.y;
         this.myTFAspectRatio.x = this.myTFAntiAliasing.x;
         this.myCBAspectRatio.x = this.myCBAntiAliasing.x;
         this.myTFAspectRatio.y = this.myTFAntiAliasing.y + tempRow;
         this.myCBAspectRatio.y = this.myCBAntiAliasing.y + tempRow;
         this.myCBSyncEveryFrame.y += tempRow;
         this.myTFAspectRatio.visible = true;
         this.myCBAspectRatio.visible = true;
         this.myCBSyncEveryFrame.selected = false;
         this.myCBShadows.selected = false;
         this.myCBSpecularMap.selected = false;
         this.myCBDepthOfField.selected = false;
         this.myCBSoftenSmokeEdges.selected = false;
         this.myCBRagdoll.selected = false;
         this.myCBBulletImpacts.selected = false;
         this.myCBDistortion.selected = false;
         this.myCBGlowAllowed.selected = false;
         this.myCancelBtn.visible = true;
      }
      
      override protected function onInitListener() : void
      {
         this.myApplyBtn.addEventListener(BaseButton.EVT_CLICK,this.onApplyBtnClickHandle);
         this.myDefaultBtn.addEventListener(BaseButton.EVT_CLICK,this.onDefaultBtnClickHandle);
         this.myCancelBtn.addEventListener(BaseButton.EVT_CLICK,this.onCancelBtnClickHandle);
         this.myCBDisplayMode.addEventListener(CodoComboBox.EVT_CHANGE,this.onDisplayModeChanged);
         this.myCBResolution.addEventListener(CodoComboBox.EVT_CHANGE,this.onResolutionChanged);
         this.myCBAspectRatio.addEventListener(CodoComboBox.EVT_CHANGE,this.onAspectRatioChanged);
         this.myCBAntiAliasing.addEventListener(CodoComboBox.EVT_CHANGE,this.onAntiAliasingChanged);
         this.myCBScreenRefreshRate.addEventListener(CodoComboBox.EVT_CHANGE,this.onScreenRefreshRateChanged);
         this.myCBSyncEveryFrame.addEventListener(BaseButton.EVT_CLICK,this.onCBClickHandle);
         this.myCBShadows.addEventListener(BaseButton.EVT_CLICK,this.onCBClickHandle);
         this.myCBSpecularMap.addEventListener(BaseButton.EVT_CLICK,this.onCBClickHandle);
         this.myCBDepthOfField.addEventListener(BaseButton.EVT_CLICK,this.onCBClickHandle);
         this.myCBSoftenSmokeEdges.addEventListener(BaseButton.EVT_CLICK,this.onCBClickHandle);
         this.myCBRagdoll.addEventListener(BaseButton.EVT_CLICK,this.onCBClickHandle);
         this.myCBBulletImpacts.addEventListener(BaseButton.EVT_CLICK,this.onCBClickHandle);
         this.myCBDistortion.addEventListener(BaseButton.EVT_CLICK,this.onCBClickHandle);
         this.myCBGlowAllowed.addEventListener(BaseButton.EVT_CLICK,this.onCBClickHandle);
         this.myCBTextureQuality.addEventListener(CodoComboBox.EVT_CHANGE,this.onTextureQualityChanged);
         this.myCBTextureRes.addEventListener(CodoComboBox.EVT_CHANGE,this.onTextureResChanged);
         this.myCBNormalMapRes.addEventListener(CodoComboBox.EVT_CHANGE,this.onNormalMapResChanged);
         this.myCBSpecularMapRes.addEventListener(CodoComboBox.EVT_CHANGE,this.onSpecularMapResChanged);
         this.myCBSSAO.addEventListener(CodoComboBox.EVT_CHANGE,this.onSSAOChanged);
         this.myCBDynamicLight.addEventListener(CodoComboBox.EVT_CHANGE,this.onDynamicLightChanged);
         this.listen(BCEvent.EVT_VIDEOCHANGE_CONFIRMED,this.onVideoChangeConfirmed);
         // k33: a combo box opening comes to the front, so its list covers the rows below it
         var tempCBs:Array = [this.myCBDisplayMode,this.myCBResolution,this.myCBAspectRatio,this.myCBAntiAliasing,this.myCBScreenRefreshRate,this.myCBTextureQuality,this.myCBTextureRes,this.myCBNormalMapRes,this.myCBSpecularMapRes,this.myCBSSAO,this.myCBDynamicLight];
         for each(var tempCB:CodoComboBox in tempCBs)
         {
            if(tempCB != null)
            {
               tempCB.addEventListener(CodoComboBox.EVT_PRE_OPEN,this.onK33ComboPreOpen);
            }
         }
      }
      
      override public function enable() : void
      {
         super.enable();
         this.videoSettings = this.getSyncDP(SyncDataID.VIDEO_SETTING);
      }
      
      override public function disable() : void
      {
         super.disable();
         this._preVideoObj = null;
         this._curVideoObj = null;
      }
      
      public function set videoSettings(settings:Object) : void
      {
         this._preVideoObj = new VideoObj(settings);
         this.myCBDisplayMode.dataProvider = this._preVideoObj.screenModes;
         this.myCBDisplayMode.selectedDataIndex = this._preVideoObj.idxDisplayMode;
         this.myPBBrightness.visible = this._preVideoObj.idxDisplayMode != 0;
         this.myTFBrightness.visible = this._preVideoObj.idxDisplayMode != 0;
         this.myCBResolution.dataProvider = this._preVideoObj.resolutions;
         this.myCBResolution.selectedDataIndex = this._preVideoObj.idxResolution;
         var gammaRange:Object = this.getSyncDP(SyncDataID.GAMMA_RANGE);
         this.myPBBrightness.min = gammaRange.min;
         this.myPBBrightness.max = gammaRange.max;
         this.myPBBrightness.value = this._preVideoObj.ui_r_brightness;
         var fovRange:Object = this.getSyncDP(SyncDataID.FOV_RANGE);
         this.myPBFOV.min = fovRange.min;
         this.myPBFOV.max = fovRange.max;
         this.myPBFOV.value = this._preVideoObj.ui_r_fov;
         this.k33FillFps();
         this.myCBAntiAliasing.dataProvider = this._preVideoObj.antiAliasings;
         this.myCBAntiAliasing.selectedDataIndex = this._preVideoObj.idxAntiAliasing;
         this.myCBScreenRefreshRate.dataProvider = this._preVideoObj.displayRefreshes;
         this.myCBScreenRefreshRate.selectedDataIndex = this._preVideoObj.idxDisplayRefresh;
         this.myCBSyncEveryFrame.selected = this._preVideoObj.ui_r_vsync;
         this.myCBShadows.selected = this._preVideoObj.ui_sm_enable;
         this.myCBSpecularMap.selected = this._preVideoObj.ui_r_specular;
         this.myCBDepthOfField.selected = this._preVideoObj.ui_r_dof_enable;
         this.myCBSoftenSmokeEdges.selected = this._preVideoObj.ui_r_zFeather;
         this.myCBRagdoll.selected = this._preVideoObj.ui_ragdoll_enable;
         this.myCBBulletImpacts.selected = this._preVideoObj.ui_fx_marks;
         this.myCBGlowAllowed.selected = this._preVideoObj.ui_r_glow_allowed;
         this.myCBDistortion.selected = this._preVideoObj.ui_r_distortion;
         this.myCBTextureQuality.dataProvider = this._preVideoObj.texQualities;
         this.myCBTextureQuality.selectedDataIndex = this._preVideoObj.idxTextureQuality;
         this.myCBTextureRes.dataProvider = this._preVideoObj.texResolutions;
         this.myCBTextureRes.selectedDataIndex = this.getIndexByID(this._preVideoObj.texResolutions,this._preVideoObj.ui_r_picmip);
         this.myCBNormalMapRes.dataProvider = this._preVideoObj.normalMapResolutions;
         this.myCBNormalMapRes.selectedDataIndex = this.getIndexByID(this._preVideoObj.normalMapResolutions,this._preVideoObj.ui_r_picmip_bump);
         this.myCBSpecularMapRes.dataProvider = this._preVideoObj.specularMapResolutions;
         this.myCBSpecularMapRes.selectedDataIndex = this.getIndexByID(this._preVideoObj.normalMapResolutions,this._preVideoObj.ui_r_picmip_spec);
         this.myCBSSAO.dataProvider = this._preVideoObj.ssaos;
         this.myCBSSAO.selectedDataIndex = this.getIndexByID(this._preVideoObj.ssaos,this._preVideoObj.ui_r_ssao);
         this.myCBDynamicLight.dataProvider = this._preVideoObj.dynamicLights;
         this.myCBDynamicLight.selectedDataIndex = this.getIndexByID(this._preVideoObj.dynamicLights,this._preVideoObj.ui_r_dlightlimit);
         this.myCBAntiAliasing.enabled = this.myCBSyncEveryFrame.enable = this.myCBShadows.enable = this.myCBSpecularMap.enable = this.myCBDepthOfField.enable = this.myCBSoftenSmokeEdges.enable = this.myCBRagdoll.enable = this.myCBBulletImpacts.enable = this.myCBGlowAllowed.enable = this.myCBDistortion.enable = this.myCBTextureQuality.enabled = this.myCBTextureRes.enabled = this.myCBNormalMapRes.enabled = this.myCBSpecularMapRes.enabled = this.myCBSSAO.enabled = this.myCBDynamicLight.enabled = !this._preVideoObj.disable_adv_option;
         this.showTextureResolutions(this.myCBTextureQuality.selectedDataIndex == 1);
         this._curVideoObj = new VideoObj(settings);
      }
      
      private function getIndexByID(dp:Array, id:int) : int
      {
         var i:int = 0;
         while(i < dp.length)
         {
            if(dp[i].id == id)
            {
               return i;
            }
            i++;
         }
         return 0;
      }
      
      private function showTextureResolutions(visible:Boolean) : void
      {
         this.myTFTextureRes.visible = visible;
         this.myCBTextureRes.visible = visible;
         this.myTFNormalMapRes.visible = visible;
         this.myCBNormalMapRes.visible = visible;
         this.myTFSpecularMapRes.visible = visible;
         this.myCBSpecularMapRes.visible = visible;
      }
      
      private function onDisplayModeChanged(cEvt:Event) : void
      {
         if(this._curVideoObj != null)
         {
            this._curVideoObj.ui_r_fullscreen = this.myCBDisplayMode.selectedValue.id;
         }
      }
      
      private function onResolutionChanged(cEvt:Event) : void
      {
         if(this._curVideoObj != null)
         {
            this._curVideoObj.ui_r_customMode = this.myCBResolution.selectedValue.id;
         }
      }
      
      private function onCBClickHandle(cEvt:Event) : void
      {
         var cb:CheckBox = cEvt.target as CheckBox;
         cb.selected = !cb.selected;
         if(this._curVideoObj == null)
         {
            return;
         }
         switch(cb)
         {
            case this.myCBSyncEveryFrame:
               this._curVideoObj.ui_r_vsync = cb.selected;
               break;
            case this.myCBShadows:
               this._curVideoObj.ui_sm_enable = cb.selected;
               break;
            case this.myCBSpecularMap:
               this._curVideoObj.ui_r_specular = cb.selected;
               break;
            case this.myCBDepthOfField:
               this._curVideoObj.ui_r_dof_enable = cb.selected;
               break;
            case this.myCBSoftenSmokeEdges:
               this._curVideoObj.ui_r_zFeather = cb.selected;
               break;
            case this.myCBRagdoll:
               this._curVideoObj.ui_ragdoll_enable = cb.selected;
               break;
            case this.myCBBulletImpacts:
               this._curVideoObj.ui_fx_marks = cb.selected;
               break;
            case this.myCBDistortion:
               this._curVideoObj.ui_r_distortion = cb.selected;
               break;
            case this.myCBGlowAllowed:
               this._curVideoObj.ui_r_glow_allowed = cb.selected;
         }
      }
      
      // k33: the Aspect Ratio box is Max FPS; it no longer touches ui_r_aspectRatio
      private function onAspectRatioChanged(event:Event) : void
      {
      }
      
      private function onK33ComboPreOpen(event:Event) : void
      {
         var tempCB:CodoComboBox = event.target as CodoComboBox;
         if(tempCB != null && tempCB.parent == this)
         {
            this.setChildIndex(tempCB,this.numChildren - 1);
         }
      }
      
      private function k33FpsLabel(fps:int) : String
      {
         return fps <= 0 ? "Unlimited" : String(fps);
      }
      
      private function k33FillFps() : void
      {
         var tempCur:int = CFunctionContainerCommon.Dvar_GetInt(K33_FPS_DVAR);
         if(tempCur < 0)
         {
            tempCur = 0;
         }
         var tempDP:Array = [];
         var tempSel:int = -1;
         var tempAdded:Boolean = false;
         for each(var tempFps:int in K33_FPS_VALUES)
         {
            if(!tempAdded && tempSel < 0 && tempCur > 0 && tempFps > tempCur)
            {
               tempSel = tempDP.length;
               tempDP.push({"label":String(tempCur),"id":tempCur});
               tempAdded = true;
            }
            if(tempFps == tempCur)
            {
               tempSel = tempDP.length;
            }
            tempDP.push({"label":this.k33FpsLabel(tempFps),"id":tempFps});
         }
         if(tempSel < 0)
         {
            tempSel = tempDP.length;
            tempDP.push({"label":String(tempCur),"id":tempCur});
         }
         this.myCBAspectRatio.dataProvider = tempDP;
         this.myCBAspectRatio.selectedDataIndex = tempSel;
         this._k33FpsApplied = tempCur;
      }
      
      private function onAntiAliasingChanged(event:Event) : void
      {
         if(this._curVideoObj != null)
         {
            this._curVideoObj.ui_r_aaSamples = this.myCBAntiAliasing.selectedValue.id;
         }
      }
      
      private function onScreenRefreshRateChanged(event:Event) : void
      {
         if(this._curVideoObj != null)
         {
            this._curVideoObj.ui_r_displayRefresh = this.myCBScreenRefreshRate.selectedValue.id;
         }
      }
      
      private function onTextureQualityChanged(event:Event) : void
      {
         this.showTextureResolutions(this.myCBTextureQuality.selectedDataIndex == 1);
         if(this._curVideoObj != null)
         {
            this._curVideoObj.ui_r_picmip_manual = this.myCBTextureQuality.selectedValue.id;
         }
      }
      
      private function onTextureResChanged(event:Event) : void
      {
         if(this._curVideoObj != null)
         {
            this._curVideoObj.ui_r_picmip = this._curVideoObj.texResolutions[this.myCBTextureRes.selectedDataIndex].id;
         }
      }
      
      private function onNormalMapResChanged(event:Event) : void
      {
         if(this._curVideoObj != null)
         {
            this._curVideoObj.ui_r_picmip_bump = this._curVideoObj.normalMapResolutions[this.myCBNormalMapRes.selectedDataIndex].id;
         }
      }
      
      private function onSpecularMapResChanged(event:Event) : void
      {
         if(this._curVideoObj != null)
         {
            this._curVideoObj.ui_r_picmip_spec = this._curVideoObj.specularMapResolutions[this.myCBSpecularMapRes.selectedDataIndex].id;
         }
      }
      
      private function onSSAOChanged(evt:Event) : void
      {
         if(this._curVideoObj != null)
         {
            this._curVideoObj.ui_r_ssao = this.myCBSSAO.selectedValue.id;
         }
      }
      
      private function onDynamicLightChanged(evt:Event) : void
      {
         if(this._curVideoObj != null)
         {
            this._curVideoObj.ui_r_dlightlimit = this.myCBDynamicLight.selectedValue.id;
         }
      }
      
      private function isVideoChanged() : Boolean
      {
         return this._preVideoObj.ui_r_fullscreen != this._curVideoObj.ui_r_fullscreen || this._preVideoObj.ui_r_customMode != this._curVideoObj.ui_r_customMode || this._preVideoObj.ui_r_aspectRatio != this._curVideoObj.ui_r_aspectRatio || this._preVideoObj.ui_r_aaSamples != this._curVideoObj.ui_r_aaSamples || this._preVideoObj.ui_r_displayRefresh != this._curVideoObj.ui_r_displayRefresh || this._preVideoObj.ui_r_vsync != this._curVideoObj.ui_r_vsync || this._preVideoObj.ui_sm_enable != this._curVideoObj.ui_sm_enable || this._preVideoObj.ui_r_specular != this._curVideoObj.ui_r_specular || this._preVideoObj.ui_r_dof_enable != this._curVideoObj.ui_r_dof_enable || this._preVideoObj.ui_r_zFeather != this._curVideoObj.ui_r_zFeather || this._preVideoObj.ui_ragdoll_enable != this._curVideoObj.ui_ragdoll_enable || this._preVideoObj.ui_fx_marks != this._curVideoObj.ui_fx_marks || this._preVideoObj.ui_r_picmip_manual != this._curVideoObj.ui_r_picmip_manual || this._preVideoObj.ui_r_picmip != this._curVideoObj
         .ui_r_picmip || this._preVideoObj.ui_r_picmip_bump != this._curVideoObj.ui_r_picmip_bump || this._preVideoObj.ui_r_picmip_spec != this._curVideoObj.ui_r_picmip_spec || this._preVideoObj.ui_r_ssao != this._curVideoObj.ui_r_ssao || this._preVideoObj.ui_r_dlightlimit != this._curVideoObj.ui_r_dlightlimit || this._preVideoObj.ui_r_glow_allowed != this._curVideoObj.ui_r_glow_allowed || this._preVideoObj.ui_r_distortion != this._curVideoObj.ui_r_distortion;
      }
      
      private function onApplyBtnClickHandle(cEvt:Event) : void
      {
         // k33: the frame cap needs no restart, so it is set here directly
         var tempFps:Object = this.myCBAspectRatio.selectedValue;
         if(tempFps != null && int(tempFps.id) != this._k33FpsApplied)
         {
            CFunctionContainerCommon.setdvar(K33_FPS_DVAR,String(int(tempFps.id)));
            this._k33FpsApplied = int(tempFps.id);
         }
         this.broadcast(BCEvent.EVT_SET_BRIGHTNESS,this.myPBBrightness.value);
         this.broadcast(BCEvent.EVT_SET_FOV,this.myPBFOV.value);
         this.broadcast(BCEvent.EVT_OPTION_VIDEO_APPLY,{
            "changed":this.isVideoChanged(),
            "cur":this._curVideoObj,
            "pre":this._preVideoObj
         });
      }
      
      private function onDefaultBtnClickHandle(cEvt:Event) : void
      {
         this.broadcast(BCEvent.DEFAULT_VIDEO);
      }
      
      private function onCancelBtnClickHandle(cEvt:ParaEvent) : void
      {
         this.closeOption();
      }
      
      private function closeOption() : void
      {
         this.broadcast(BCEvent.EVT_CLOSE_OPTION);
      }
      
      private function onVideoChangeConfirmed(cEvt:ParaEvent) : void
      {
         this._preVideoObj.ui_r_fullscreen = this._curVideoObj.ui_r_fullscreen;
         this._preVideoObj.ui_r_customMode = this._curVideoObj.ui_r_customMode;
         this._preVideoObj.ui_r_aspectRatio = this._curVideoObj.ui_r_aspectRatio;
         this._preVideoObj.ui_r_aaSamples = this._curVideoObj.ui_r_aaSamples;
         this._preVideoObj.ui_r_displayRefresh = this._curVideoObj.ui_r_displayRefresh;
         this._preVideoObj.ui_r_vsync = this._curVideoObj.ui_r_vsync;
         this._preVideoObj.ui_sm_enable = this._curVideoObj.ui_sm_enable;
         this._preVideoObj.ui_r_specular = this._curVideoObj.ui_r_specular;
         this._preVideoObj.ui_r_dof_enable = this._curVideoObj.ui_r_dof_enable;
         this._preVideoObj.ui_r_zFeather = this._curVideoObj.ui_r_zFeather;
         this._preVideoObj.ui_ragdoll_enable = this._curVideoObj.ui_ragdoll_enable;
         this._preVideoObj.ui_fx_marks = this._curVideoObj.ui_fx_marks;
         this._preVideoObj.ui_r_picmip_manual = this._curVideoObj.ui_r_picmip_manual;
         this._preVideoObj.ui_r_picmip = this._curVideoObj.ui_r_picmip;
         this._preVideoObj.ui_r_picmip_bump = this._curVideoObj.ui_r_picmip_bump;
         this._preVideoObj.ui_r_picmip_spec = this._curVideoObj.ui_r_picmip_spec;
         this._preVideoObj.ui_r_ssao = this._curVideoObj.ui_r_ssao;
         this._preVideoObj.ui_r_dlightlimit = this._curVideoObj.ui_r_dlightlimit;
         this._preVideoObj.ui_r_glow_allowed = this._curVideoObj.ui_r_glow_allowed;
         this._preVideoObj.ui_r_distortion = this._curVideoObj.ui_r_distortion;
      }
   }
}

