package codo.views.option_sub
{
   // codol-classic's English movie's display-mode fix is in (Brightness follows fullscreen; a display-mode change counts
   // as a change). Every string shown is a localize key (K33UI_*): English in codol_english_ui, Chinese in voice_ui_mp.
   // C1's codo.views.option_sub.OptionView_video (Options > Graphics) as decompiled from patch_ui_mp's
   // codo.gfx, with a frame-rate cap row for Kernel33 (kernel33.c PatchFrameCap). build_voice_ui.py
   // compiles this with JPEXS FFDec and splices the class's DoABC tag back into codo.gfx. Changes from
   // C1's code are marked "k33".
   //   Max FPS   C1's Aspect Ratio row, which its onInit hides (the box is not used: the aspect ratio
   //             follows the resolution), shown again below Anti-Aliasing (Vsync moves down a row):
   //             Unlimited / 30 / 60 / 75 / 90 / 120 / 144 / 165 / 240 / 360 / 500 -> "1848"
   //             (com_maxfps, 0 = no cap, archived in config_mp.cfg), set by Apply like the rest of
   //             the screen; a value set from the console is listed as its own entry
   //   Language / Button icons / HUD style (docs/UI_STYLE_OPTIONS_20261112.md): three rows built in code under the
   //             Graphics check boxes, a label and a "<  name  >" stepper each (a click on its left half steps back,
   //             on its right half forward, wrapping; codo.k33.PadSnap stops the pad cursor on either half):
   //               Language      k33_language, one of k33_languageList ("en;zh"); names K33UI_LANG_NAME_<ID>
   //               Button icons  gpad_glyphs 0..6 (shown when the controller module is there: k33_padinfo is set)
   //               HUD style     k33_hudStyle, one of the numbers of k33_hudStyleList ("0:stock;2:v26")
   //             A row whose list is empty (an older Kernel33.dll) is not made; a name without a localize entry
   //             shows the raw id; a value the list lacks shows the first choice. Unlike the rest of the tab these
   //             apply at once (setdvar) and take no part in Apply / Cancel / Default: the stepper shows what the
   //             dvar holds, read back a few frames after a change. k33_languageState 1 / 2 shows a line under the
   //             rows (switching / restart to apply); when k33_languageSerial changes (a live switch finished) the
   //             tab's texts are set again (k33StyleRelocalize). The texts onInit sets are in k33Texts for that.
   //             The k33Style* functions between "k33 style shared" are the same text as in codoingame.gfx's
   //             MainPause (tests/ui_style_rows_test.js runs both).
   import codo.common.BaseSubElement;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.LocalizationDict;
   import codo.constants.LocalizationSetting;
   import codo.constants.SyncDataID;
   import codo.k33.PadSnap;
   import codo.objects.VideoObj;
   import codo.utils.LocalizeUtil;
   import components.BaseButton;
   import components.CheckBox;
   import components.CodoComboBox;
   import components.CodoGrowButtonNoTint;
   import components.ProgressBar;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.events.MouseEvent;
   import flash.geom.ColorTransform;
   import flash.text.TextField;
   import flash.text.TextFieldAutoSize;
   import flash.text.TextFormat;
   
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
         this.k33Texts(); // k33: the texts, set again after a language switch
         this.myPBFOV.step = 1;
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
      
      // k33: every text onInit gave this tab (C1's translator resolves "@KEY" as a text is set)
      private function k33Texts() : void
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
         this.myTFAspectRatio.text = "@K33UI_MAX_FPS"; // k33: the Aspect Ratio row is Max FPS
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
         // k33: the style rows are made from their dvars each time the tab opens, and follow them while it is shown;
         // a language switch since the tab was last shown translates its texts again first
         if(this._k33StyleSerial >= 0 && CFunctionContainerCommon.Dvar_GetInt("k33_languageSerial") != this._k33StyleSerial)
         {
            this.k33StyleRelocalize();
         }
         else
         {
            this.k33StyleBuild();
         }
         this.k33StyleWatch(true);
      }
      
      override public function disable() : void
      {
         super.disable();
         this.k33StyleWatch(false); // k33
         this._preVideoObj = null;
         this._curVideoObj = null;
      }
      
      public function set videoSettings(settings:Object) : void
      {
         this._preVideoObj = new VideoObj(settings);
         this.myCBDisplayMode.dataProvider = this._preVideoObj.screenModes;
         this.myCBDisplayMode.selectedDataIndex = this._preVideoObj.idxDisplayMode;
         this.myPBBrightness.visible = this._preVideoObj.ui_r_fullscreen;
         this.myTFBrightness.visible = this._preVideoObj.ui_r_fullscreen;
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
            this._curVideoObj.idxDisplayMode = this.myCBDisplayMode.selectedDataIndex;
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
         return fps <= 0 ? "@K33UI_FPS_UNLIMITED" : String(fps);
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
         return this._preVideoObj.idxDisplayMode != this._curVideoObj.idxDisplayMode || this._preVideoObj.ui_r_fullscreen != this._curVideoObj.ui_r_fullscreen || this._preVideoObj.ui_r_customMode != this._curVideoObj.ui_r_customMode || this._preVideoObj.ui_r_aspectRatio != this._curVideoObj.ui_r_aspectRatio || this._preVideoObj.ui_r_aaSamples != this._curVideoObj.ui_r_aaSamples || this._preVideoObj.ui_r_displayRefresh != this._curVideoObj.ui_r_displayRefresh || this._preVideoObj.ui_r_vsync != this._curVideoObj.ui_r_vsync || this._preVideoObj.ui_sm_enable != this._curVideoObj.ui_sm_enable || this._preVideoObj.ui_r_specular != this._curVideoObj.ui_r_specular || this._preVideoObj.ui_r_dof_enable != this._curVideoObj.ui_r_dof_enable || this._preVideoObj.ui_r_zFeather != this._curVideoObj.ui_r_zFeather || this._preVideoObj.ui_ragdoll_enable != this._curVideoObj.ui_ragdoll_enable || this._preVideoObj.ui_fx_marks != this._curVideoObj.ui_fx_marks || this._preVideoObj.ui_r_picmip_manual != this._curVideoObj.ui_r_picmip_manual || this._preVideoObj.ui_r_picmip != this._curVideoObj
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
         this._preVideoObj.idxDisplayMode = this._curVideoObj.idxDisplayMode;
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

      // k33 style rows (Language, Button icons, HUD style): see the header comment
      private static const K33_STYLE_KINDS:Array = ["lang","glyph","hud"];

      private static const K33_STYLE_LABELS:Object = {
         "lang":"@K33UI_LANGUAGE",
         "glyph":"@K33UI_GLYPHS",
         "hud":"@K33UI_HUD_STYLE"
      };

      private static const K33_STYLE_HOLD:int = 12; // frames a new choice stays on show before the dvar is read back

      private static const K33_STYLE_POLL:int = 6; // frames between looks at the dvars

      private static const K33_STYLE_X:Number = 12; // the rows' place: under the Graphics check boxes (they end at
                                                    // y 401), above the warning line (y 465), left of the texture
      private static const K33_STYLE_Y:Number = 405; // rows (x 296)

      private static const K33_STYLE_PITCH:Number = 20;

      private static const K33_STYLE_BTN_X:Number = 118;

      private static const K33_STYLE_BTN_W:Number = 170;

      private static const K33_STYLE_BTN_H:Number = 18;

      private static const K33_STYLE_NOTE_Y:Number = 486; // under the warning line, left of the Default button (x 228)

      private var _k33StyleRows:Array = []; // [{kind, label, btn, opts, names, shown, hold}]

      private var _k33StyleNote:TextField = null;

      private var _k33StyleNoteKey:String = "";

      private var _k33StyleSerial:int = -1; // k33_languageSerial as last seen

      private var _k33StyleFrames:int = 0;

      private var _k33StyleOn:Boolean = false;

      // k33 style shared: begin (the same text in MainPause.as and OptionView_video.as; tests/ui_style_rows_test.js)
      private static const K33_GLYPH_NAMES:Array = ["AUTO","XBOX","PS","NINTENDO","GENERIC","TEXT","KBM"]; // gpad_glyphs 0..6

      private static const K33_STYLE_DVARS:Object = {
         "lang":"k33_language",
         "glyph":"gpad_glyphs",
         "hud":"k33_hudStyle"
      };

      private function k33StyleTrim(s:String) : String
      {
         var a:int = 0;
         var b:int = 0;
         if(s == null)
         {
            return "";
         }
         s = String(s);
         b = s.length;
         while(a < b && s.charCodeAt(a) <= 32)
         {
            a++;
         }
         while(b > a && s.charCodeAt(b - 1) <= 32)
         {
            b--;
         }
         return s.substring(a,b);
      }

      // k33: digits only (a style number of k33_hudStyleList)
      private function k33StyleDigits(s:String) : Boolean
      {
         var i:int = 0;
         if(s == null || s.length == 0 || s.length > 6)
         {
            return false;
         }
         while(i < s.length)
         {
            if(s.charCodeAt(i) < 48 || s.charCodeAt(i) > 57)
            {
               return false;
            }
            i++;
         }
         return true;
      }

      // k33: a row's choices, [{v: the value its dvar takes, key: the localize key of its name, raw: the text shown
      // when that key has no entry}]. "lang": the ids of k33_languageList ("en;zh"); "hud": the "number:id" pairs of
      // k33_hudStyleList ("0:stock;2:v26"); "glyph": gpad_glyphs 0..6, when the controller module has published
      // k33_padinfo. An empty list (the dvar is not there: an older Kernel33.dll) gives [], and the row is left out.
      // Blank, malformed and repeated entries are dropped.
      private function k33StyleOptions(kind:String) : Array
      {
         var out:Array = [];
         var list:String = null;
         var parts:Array = null;
         var item:String = null;
         var v:String = null;
         var id:String = null;
         var at:int = 0;
         var i:int = 0;
         var j:int = 0;
         if(kind == "glyph")
         {
            list = CFunctionContainerCommon.Dvar_GetString("k33_padinfo");
            if(list == null || list == "")
            {
               return out;
            }
            while(i < K33_GLYPH_NAMES.length)
            {
               out.push({
                  "v":String(i),
                  "key":"K33UI_GLYPHS_" + K33_GLYPH_NAMES[i],
                  "raw":String(i)
               });
               i++;
            }
            return out;
         }
         list = CFunctionContainerCommon.Dvar_GetString(kind == "lang" ? "k33_languageList" : "k33_hudStyleList");
         if(list == null || list == "")
         {
            return out;
         }
         parts = String(list).split(";");
         while(i < parts.length)
         {
            item = this.k33StyleTrim(parts[i]);
            i++;
            if(kind == "hud")
            {
               at = item.indexOf(":");
               if(at < 1)
               {
                  continue;
               }
               v = this.k33StyleTrim(item.substring(0,at));
               id = this.k33StyleTrim(item.substring(at + 1));
               if(!this.k33StyleDigits(v) || id == "")
               {
                  continue;
               }
               v = String(parseInt(v,10));
            }
            else
            {
               v = item;
               id = item;
               if(id == "")
               {
                  continue;
               }
            }
            j = 0;
            while(j < out.length && out[j].v != v)
            {
               j++;
            }
            if(j < out.length)
            {
               continue;
            }
            out.push({
               "v":v,
               "key":(kind == "hud" ? "K33UI_HUD_STYLE_" : "K33UI_LANG_NAME_") + id.toUpperCase(),
               "raw":id
            });
         }
         return out;
      }

      // k33: a choice's name in the loaded language; C1's translator gives "@KEY" back for a key it does not have
      private function k33StyleName(opt:Object) : String
      {
         var s:String = LocalizeUtil.localize("@" + opt.key);
         if(s == null || s == "" || s.charAt(0) == "@")
         {
            return String(opt.raw);
         }
         return s;
      }

      // k33: what the row's dvar holds, as a choice's v
      private function k33StyleCurrent(kind:String) : String
      {
         var s:String = null;
         if(kind == "lang")
         {
            s = CFunctionContainerCommon.Dvar_GetString(K33_STYLE_DVARS[kind]);
            return s == null ? "" : this.k33StyleTrim(s);
         }
         return String(CFunctionContainerCommon.Dvar_GetInt(K33_STYLE_DVARS[kind]));
      }

      // k33: the choice holding `cur`; a value the list does not have shows the first choice
      private function k33StyleIndex(opts:Array, cur:String) : int
      {
         var i:int = 0;
         while(i < opts.length)
         {
            if(String(opts[i].v).toLowerCase() == String(cur).toLowerCase())
            {
               return i;
            }
            i++;
         }
         return 0;
      }

      // k33: the next (dir 1) or previous (dir -1) choice, wrapping at both ends
      private function k33StyleStep(index:int, count:int, dir:int) : int
      {
         if(count < 1)
         {
            return 0;
         }
         return ((index + dir) % count + count) % count;
      }

      // k33: set the row's dvar to choice `index` (setdvar; nothing when it already holds it) -> whether it was set
      private function k33StyleSet(kind:String, opts:Array, index:int) : Boolean
      {
         if(index < 0 || index >= opts.length || String(opts[index].v).toLowerCase() == this.k33StyleCurrent(kind).toLowerCase())
         {
            return false;
         }
         CFunctionContainerCommon.setdvar(K33_STYLE_DVARS[kind],String(opts[index].v));
         return true;
      }

      // k33: the choices' values in order: a row is built again when this changes
      private function k33StyleSig(opts:Array) : String
      {
         var s:String = "";
         var i:int = 0;
         while(i < opts.length)
         {
            s += opts[i].v + ":" + opts[i].key + ";";
            i++;
         }
         return s;
      }
      // k33 style shared: end

      private function k33StyleText(s:String, size:int, color:uint) : TextField
      {
         var tf:TextField = new TextField();
         var fmt:TextFormat = this.myTFDisplayMode.getTextFormat();
         tf.embedFonts = this.myTFDisplayMode.embedFonts;
         tf.defaultTextFormat = new TextFormat(fmt.font,size > 0 ? size : fmt.size,color);
         tf.selectable = false;
         tf.mouseEnabled = false;
         tf.autoSize = TextFieldAutoSize.LEFT;
         tf.text = s;
         return tf;
      }

      private function k33StyleOver(e:MouseEvent) : void
      {
         (e.currentTarget as Sprite).transform.colorTransform = new ColorTransform(1.4,1.4,1.4);
      }

      private function k33StyleOut(e:MouseEvent) : void
      {
         (e.currentTarget as Sprite).transform.colorTransform = new ColorTransform();
      }

      // the "<  name  >" stepper of a row (as OptionView's Controller tab draws its buttons)
      private function k33StyleButton(kind:String) : Sprite
      {
         var b:Sprite = new Sprite();
         var tf:TextField = this.k33StyleText("",12,14544626);
         b.graphics.lineStyle(1,6272712,0.8);
         b.graphics.beginFill(928307,0.85);
         b.graphics.drawRect(0,0,K33_STYLE_BTN_W,K33_STYLE_BTN_H);
         b.graphics.endFill();
         tf.name = "label";
         b.addChild(tf);
         b.name = kind;
         b.buttonMode = true;
         b.mouseChildren = false;
         b.addEventListener(MouseEvent.CLICK,this.k33OnStyleClick);
         b.addEventListener(MouseEvent.ROLL_OVER,this.k33StyleOver);
         b.addEventListener(MouseEvent.ROLL_OUT,this.k33StyleOut);
         return b;
      }

      private function k33StyleOf(kind:String) : Object
      {
         var i:int = 0;
         while(i < this._k33StyleRows.length)
         {
            if(this._k33StyleRows[i].kind == kind)
            {
               return this._k33StyleRows[i];
            }
            i++;
         }
         return null;
      }

      // show choice `index` on the row without setting its dvar
      private function k33StyleShow(rec:Object, index:int) : void
      {
         var tf:TextField = rec.btn.getChildByName("label") as TextField;
         index = Math.max(0,Math.min(rec.opts.length - 1,index));
         rec.shown = index;
         tf.text = rec.opts.length > 1 ? "<  " + rec.names[index] + "  >" : String(rec.names[index]);
         tf.x = (K33_STYLE_BTN_W - tf.width) / 2;
         tf.y = (K33_STYLE_BTN_H - tf.height) / 2;
      }

      // a click on a row's stepper: the left half steps back, the right half forward (wrapping); the choice is shown
      // and its dvar set at once, and the dvar is read back K33_STYLE_HOLD frames later
      private function k33OnStyleClick(e:MouseEvent) : void
      {
         var b:Sprite = e.currentTarget as Sprite;
         var rec:Object = this.k33StyleOf(b.name);
         var index:int = 0;
         if(rec == null || rec.opts.length < 2)
         {
            return;
         }
         index = this.k33StyleStep(rec.shown,rec.opts.length,b.mouseX > K33_STYLE_BTN_W / 2 ? 1 : -1);
         this.k33StyleShow(rec,index);
         if(this.k33StyleSet(rec.kind,rec.opts,index))
         {
            rec.hold = K33_STYLE_HOLD;
         }
      }

      private function k33StyleRemove() : void
      {
         var i:int = 0;
         while(i < this._k33StyleRows.length)
         {
            if(this._k33StyleRows[i].label.parent != null)
            {
               this._k33StyleRows[i].label.parent.removeChild(this._k33StyleRows[i].label);
            }
            if(this._k33StyleRows[i].btn.parent != null)
            {
               this._k33StyleRows[i].btn.parent.removeChild(this._k33StyleRows[i].btn);
            }
            i++;
         }
         this._k33StyleRows = [];
         if(this._k33StyleNote != null && this._k33StyleNote.parent != null)
         {
            this._k33StyleNote.parent.removeChild(this._k33StyleNote);
         }
         this._k33StyleNote = null;
         this._k33StyleNoteKey = "";
      }

      // the rows are made again from the dvars: one per kind that has choices, top to bottom without gaps
      private function k33StyleBuild() : void
      {
         var opts:Array = null;
         var rec:Object = null;
         var lab:TextField = null;
         var btn:Sprite = null;
         var i:int = 0;
         var j:int = 0;
         var n:int = 0;
         this.k33StyleRemove();
         while(i < K33_STYLE_KINDS.length)
         {
            opts = this.k33StyleOptions(K33_STYLE_KINDS[i]);
            if(opts.length > 0)
            {
               lab = this.k33StyleText(K33_STYLE_LABELS[K33_STYLE_KINDS[i]],0,this.myTFDisplayMode.textColor);
               lab.x = K33_STYLE_X;
               lab.y = K33_STYLE_Y + n * K33_STYLE_PITCH - 1;
               this.addChild(lab);
               btn = this.k33StyleButton(K33_STYLE_KINDS[i]);
               btn.x = K33_STYLE_BTN_X;
               btn.y = K33_STYLE_Y + n * K33_STYLE_PITCH;
               this.addChild(btn);
               rec = {
                  "kind":K33_STYLE_KINDS[i],
                  "label":lab,
                  "btn":btn,
                  "opts":opts,
                  "names":[],
                  "shown":-1,
                  "hold":0
               };
               j = 0;
               while(j < opts.length)
               {
                  rec.names.push(this.k33StyleName(opts[j]));
                  j++;
               }
               if(opts.length > 1)
               {
                  PadSnap.hint(btn,PadSnap.SPLIT_H); // the pad cursor stops on either half
               }
               this._k33StyleRows.push(rec);
               this.k33StyleShow(rec,this.k33StyleIndex(opts,this.k33StyleCurrent(rec.kind)));
               n++;
            }
            i++;
         }
         this._k33StyleNote = this.k33StyleText("",12,10474463);
         this._k33StyleNote.x = K33_STYLE_X;
         this._k33StyleNote.y = K33_STYLE_NOTE_Y;
         this._k33StyleNote.visible = false;
         this.addChild(this._k33StyleNote);
         this.k33StyleNoteUpdate();
      }

      // k33_languageState 1 (switching) / 2 (saved, applies on the next start) as a line under the rows
      private function k33StyleNoteUpdate() : void
      {
         var key:String = "";
         var state:int = 0;
         if(this._k33StyleNote == null)
         {
            return;
         }
         if(this.k33StyleOf("lang") != null)
         {
            state = CFunctionContainerCommon.Dvar_GetInt("k33_languageState");
            key = state == 2 ? "@K33UI_LANG_RESTART" : (state == 1 ? "@K33UI_LANG_SWITCHING" : "");
         }
         if(key != this._k33StyleNoteKey)
         {
            this._k33StyleNoteKey = key;
            this._k33StyleNote.text = key;
            this._k33StyleNote.visible = key != "";
         }
      }

      // the panel is shown (enable) / hidden (disable): the rows follow their dvars only while it is shown
      private function k33StyleWatch(on:Boolean) : void
      {
         if(on == this._k33StyleOn)
         {
            return;
         }
         this._k33StyleOn = on;
         if(on)
         {
            this._k33StyleSerial = CFunctionContainerCommon.Dvar_GetInt("k33_languageSerial");
            this._k33StyleFrames = 0;
            this.addEventListener(Event.ENTER_FRAME,this.k33StyleFrame);
         }
         else
         {
            this.removeEventListener(Event.ENTER_FRAME,this.k33StyleFrame);
         }
      }

      // whether the tab holds choices Apply has not taken yet (they must survive a language switch)
      private function k33StylePending() : Boolean
      {
         var fps:Object = this.myCBAspectRatio.selectedValue;
         if(this._preVideoObj == null || this._curVideoObj == null)
         {
            return true;
         }
         return this.isVideoChanged() || fps != null && int(fps.id) != this._k33FpsApplied || Math.abs(this.myPBBrightness.value - this._preVideoObj.ui_r_brightness) > 0.001 || Math.abs(this.myPBFOV.value - this._preVideoObj.ui_r_fov) > 0.5;
      }

      // a live language switch finished (k33_languageSerial moved): every text of this tab is set again, so C1's
      // translator gives it in the new language. The labels and the style rows always; the combo boxes' lists are
      // filled again (as when the tab is opened) unless the tab holds choices not applied yet - then only Max FPS's
      // list is, keeping its selection, and the others follow when the tab is next opened. The Options window's tab
      // buttons and title are set again too (OptionView's public fields).
      private function k33StyleRelocalize() : void
      {
         var host:Object = this.parent;
         var sel:int = 0;
         var applied:int = 0;
         this.k33Texts();
         try
         {
            if(host != null && "myTabVideo" in host)
            {
               host.myTitleTF.text = LocalizationSetting.OPTION_PAGE;
               host.myTabVideo.label = LocalizationSetting.OPTION_TAB_VIDEO;
               host.myTabAudio.label = LocalizationSetting.OPTION_TAB_AUDIO;
               host.myTabControl.label = LocalizationSetting.OPTION_TAB_CONTROL;
               host.myTabOther.label = LocalizeUtil.localize("@K33UI_PAD_TAB");
            }
         }
         catch(err:Error)
         {
         }
         try
         {
            if(!this.k33StylePending())
            {
               this.videoSettings = this.getSyncDP(SyncDataID.VIDEO_SETTING);
            }
            else
            {
               sel = this.myCBAspectRatio.selectedDataIndex;
               applied = this._k33FpsApplied;
               this.k33FillFps();
               this.myCBAspectRatio.selectedDataIndex = sel;
               this._k33FpsApplied = applied;
            }
         }
         catch(err2:Error)
         {
         }
         this.k33StyleBuild();
      }

      // every K33_STYLE_POLL frames while the tab is shown: a finished language switch translates the tab again; a
      // list that changed makes the rows again; a row shows what its dvar holds
      private function k33StyleFrame(e:Event) : void
      {
         var rec:Object = null;
         var opts:Array = null;
         var serial:int = 0;
         var index:int = 0;
         var i:int = 0;
         while(i < this._k33StyleRows.length)
         {
            if(this._k33StyleRows[i].hold > 0)
            {
               this._k33StyleRows[i].hold--;
            }
            i++;
         }
         this._k33StyleFrames++;
         if(this._k33StyleFrames % K33_STYLE_POLL != 0)
         {
            return;
         }
         try
         {
            serial = CFunctionContainerCommon.Dvar_GetInt("k33_languageSerial");
            if(serial != this._k33StyleSerial)
            {
               this._k33StyleSerial = serial;
               this.k33StyleRelocalize();
               return;
            }
            i = 0;
            while(i < K33_STYLE_KINDS.length)
            {
               opts = this.k33StyleOptions(K33_STYLE_KINDS[i]);
               rec = this.k33StyleOf(K33_STYLE_KINDS[i]);
               i++;
               if(this.k33StyleSig(opts) != (rec != null ? this.k33StyleSig(rec.opts) : ""))
               {
                  this.k33StyleBuild();
                  return;
               }
               if(rec != null && rec.hold <= 0)
               {
                  index = this.k33StyleIndex(rec.opts,this.k33StyleCurrent(rec.kind));
                  if(index != rec.shown)
                  {
                     this.k33StyleShow(rec,index);
                  }
               }
            }
            this.k33StyleNoteUpdate();
         }
         catch(err:Error)
         {
         }
      }
   }
}

