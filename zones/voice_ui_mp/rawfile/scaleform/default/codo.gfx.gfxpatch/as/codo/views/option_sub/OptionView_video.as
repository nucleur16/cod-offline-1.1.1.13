package codo.views.option_sub
{
   // the classic build's English movie's display-mode fix is in (Brightness follows fullscreen; a display-mode change counts
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
   //   Game tab (docs/UI_STYLE_OPTIONS_20261112.md "Game tab"): a fifth tab of the Options window, made here in
   //             code (k33GameSetup: another instance of OptionView's own tab button class, right of the Controller
   //             tab, and a panel of rows where this panel sits, on the window's card, which every tab shares: it is
   //             OptionView's first child, not part of any tab), holding the UI / HUD rows, each a label and a
   //             drop-down like the Graphics tab's own (CodoComboBox): another instance of the class this tab's combo
   //             boxes are (codoComboBox_l2, or codoComboBoxCyborgs on the other panel; taken from myCBAspectRatio,
   //             as CodoList makes its item renderers), top to bottom:
   //               Language      loc_languageName (below)
   //               Button icons  gpad_glyphs 0..6 (shown when the controller module is there: gpad_info is set)
   //               HUD style     cg_hudStyle, one of the numbers of cg_hudStyleList ("0:stock;2:v26")
   //               HUD sway      cg_hudSway, Off / On (with the HUD style row: hud_style.c registers both)
   //             OptionView itself is not changed (its class is pinned by the selector producer): the Game tab
   //             listens to OptionView's four tab buttons; opening it hides whichever of OptionView's panels is shown
   //             and closing it shows that panel again: on any other tab (its listener runs ahead of OptionView's), when
   //             the window closes (OptionView calls this panel's disable()) and on EVT_ENABLE_VIDEO. Until 16 November
   //             these rows were under the Graphics check boxes.
   //               Language      loc_languageName, one of loc_languageList ("en;zh"); names K33UI_LANG_NAME_<ID>.
   //                             Switched by K33_STYLE_LANGUAGE_ROW (on since the language zone reloads live).
   //             A row whose list is empty (an older Kernel33.dll) is hidden; a name without a localize entry shows
   //             the raw id; a value the list lacks shows the first choice and sets nothing. Unlike the rest of the
   //             tab these apply at once (setdvar, on the player's pick only: what the code selects is not a pick)
   //             and take no part in Apply / Cancel / Default: the box shows what the dvar holds, read back a few
   //             frames after a change. With the Language row, loc_languageState 1 / 2 shows a line under the rows
   //             (switching / restart to apply). When loc_languageSerial changes (a live switch finished) the tab's
   //             texts are set again (k33StyleRelocalize), the rows' labels and lists with them. The texts onInit
   //             sets are in k33Texts for that.
   //             Texts of code-made fields are translated before they are set (LocalizeUtil.localize): C1's
   //             translator answers a field that is set to "@KEY" with the text in the field's authored format, and
   //             a field made in code has none (Times, 12, black), which an embedded-font field draws as black boxes.
   //             The k33Style* functions between "k33 style shared" are the same text as in codoingame.gfx's
   //             MainPause (tests/ui_style_rows_test.js runs both).
   import codo.common.BaseSubElement;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.LocalizationDict;
   import codo.constants.LocalizationSetting;
   import codo.constants.SyncDataID;
   import codo.objects.VideoObj;
   import codo.utils.LocalizeUtil;
   import components.BaseButton;
   import components.CheckBox;
   import components.CodoComboBox;
   import components.CodoGrowButtonNoTint;
   import components.ProgressBar;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;
   import flash.text.TextFieldAutoSize;
   import flash.text.TextFormat;
   import flash.utils.getDefinitionByName;
   import flash.utils.getQualifiedClassName;
   
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
         this.k33GameSetup(); // k33: the Game tab
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
         // k33: a language switch since the tab was last shown translates its texts again first; the frame watch
         // runs while this tab or the Game tab is shown
         if(this._k33StyleSerial >= 0 && CFunctionContainerCommon.Dvar_GetInt("loc_languageSerial") != this._k33StyleSerial)
         {
            this.k33StyleRelocalize();
         }
         this._k33VideoOn = true;
         this.k33StyleWatch(this._k33VideoOn || this._k33GameOpen);
      }
      
      override public function disable() : void
      {
         super.disable();
         this._k33VideoOn = false; // k33: OptionView calls this on every change of tab and when the window closes
         this.k33GameClose();
         this.k33StyleWatch(this._k33GameOpen);
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
         if(tempCB != null && tempCB.parent != null)
         {
            tempCB.parent.setChildIndex(tempCB,tempCB.parent.numChildren - 1);
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

      // k33 style rows (Button icons, HUD style, HUD sway, and Language when it is switched on): see the header comment
      private static const K33_STYLE_KINDS:Array = ["lang","glyph","hud","sway","scene"];

      private static const K33_STYLE_LABELS:Object = {
         "lang":"@K33UI_LANGUAGE",
         "glyph":"@K33UI_GLYPHS",
         "hud":"@K33UI_HUD_STYLE",
         "sway":"@K33UI_HUD_SWAY",
         "scene":"@K33UI_LOBBY_SCENE"
      };

      private static const K33_STYLE_HOLD:int = 12; // frames a new choice stays on show before the dvar is read back

      private static const K33_STYLE_POLL:int = 6; // frames between looks at the dvars

      // the Game tab's rows, in this panel's coordinates (the Game panel sits where this panel does): labels near
      // the left edge, boxes in a column of their own, one row per K33_STYLE_PITCH under the tab's title
      private static const K33_STYLE_X:Number = 24;

      private static const K33_STYLE_BOX_X:Number = 170;

      private static const K33_STYLE_Y:Number = 40;

      private static const K33_STYLE_PITCH:Number = 34;

      private static const K33_STYLE_NOTE_GAP:Number = 10; // the language line, under the last row


      private static const K33_HOST_TABS:Array = ["myTabVideo","myTabAudio","myTabControl","myTabOther"]; // OptionView's

      private var _k33Game:Sprite = null; // the Game tab's panel (a child of OptionView, over its panels)

      private var _k33GameTab:CodoGrowButtonNoTint = null; // the Game tab's button (a child of OptionView)

      private var _k33GameTitle:TextField = null;

      private var _k33GameOpen:Boolean = false;

      private var _k33GamePrev:CodoGrowButtonNoTint = null; // OptionView's tab that was selected when Game opened

      private var _k33GameFix:int = 0; // frames until the tab selection is checked after Game closed

      private var _k33VideoOn:Boolean = false; // this (Graphics) panel is shown

      private var _k33GameCard:Object = null; // the window's card (OptionView's first child), in this panel's coordinates

      private var _k33GameFailed:Boolean = false; // k33GameSetup failed: no Game tab, no rows

      private var _k33GameHidden:Array = null; // OptionView's panels hidden while Game is open

      private var _k33StyleRows:Array = []; // the rows on show: [{kind, label, combo, opts, names, shown, hold}]

      private var _k33StyleCombos:Object = {}; // kind -> its CodoComboBox, made once

      private var _k33StyleLabels:Object = {}; // kind -> its label, made once

      private var _k33StyleOpen:CodoComboBox = null; // the style box whose list is open

      private var _k33StyleSync:Boolean = false; // the code is selecting: not the player's pick, nothing is set

      private var _k33StyleDirty:Boolean = false; // the rows are to be made once the tab is on the stage

      private var _k33StyleNote:TextField = null;

      private var _k33StyleNoteKey:String = "";

      private var _k33StyleSerial:int = -1; // loc_languageSerial as last seen

      private var _k33StyleFrames:int = 0;

      private var _k33StyleOn:Boolean = false;

      // k33 style shared: begin (the same text in MainPause.as and OptionView_video.as; tests/ui_style_rows_test.js)
      private static const K33_GLYPH_NAMES:Array = ["AUTO","XBOX","PS","NINTENDO","GENERIC","TEXT","KBM"]; // gpad_glyphs 0..6

      private static const K33_STYLE_DVARS:Object = {
         "lang":"loc_languageName",
         "glyph":"gpad_glyphs",
         "hud":"cg_hudStyle",
         "sway":"cg_hudSway",
         "scene":"ui_vlobbyEnv"
      };

      // k33: the Language row is switched off here, in one place for both movies: with false the row and its
      // "Switching language..." / "Restart the game to apply the language" line are not made and none of the
      // loc_language* dvars is read for a row (the language is still changed with loc_languageName / loc_setLanguage
      // in the console, and a finished switch still translates the remaining rows: loc_languageSerial). true
      // brings the row back; its keys (K33UI_LANGUAGE, K33UI_LANG_*) stay in the string zones for that.
      private static const K33_STYLE_LANGUAGE_ROW:Boolean = true;

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

      // k33: digits only (a style number of cg_hudStyleList)
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
      // when that key has no entry}]. "lang": the ids of loc_languageList ("en;zh"); "hud": the "number:id" pairs of
      // cg_hudStyleList ("0:stock;2:v26"); "glyph": gpad_glyphs 0..6, when the controller module has published
      // gpad_info. An empty list (the dvar is not there: an older Kernel33.dll) gives [], and the row is left out.
      // "lang" gives [] while K33_STYLE_LANGUAGE_ROW is false.
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
         if(kind == "lang" && !K33_STYLE_LANGUAGE_ROW)
         {
            return out; // the Language row is switched off
         }
         if(kind == "sway")
         {
            // cg_hudSway 0 / 1 (hud_style.c: the HUD moves with the view, movement and camera shake, every style).
            // It is an int dvar: C1's Dvar_GetString reads an int's value as a string pointer (live: a crash at
            // 0x007739B0 reading 0x00000001), so the row comes with the HUD style row, whose cg_hudStyleList the
            // same hud_style.c registers: a Kernel33.dll without HUD styles has neither
            if(this.k33StyleOptions("hud").length == 0)
            {
               return out;
            }
            out.push({
               "v":"0",
               "key":"K33UI_HUD_SWAY_OFF",
               "raw":"off"
            });
            out.push({
               "v":"1",
               "key":"K33UI_HUD_SWAY_ON",
               "raw":"on"
            });
            return out;
         }
         if(kind == "scene")
         {
            // ui_vlobbyEnvList (vlobby_env.c): the lobby scenes, "center,v0_menu,v3_lobby,..." (comma separated: a queued
            // set drops ';'). With only C1's own scene (no merged lobby zone, ui_viewer_vlobby_mp.ff) there is no row.
            list = CFunctionContainerCommon.Dvar_GetString("ui_vlobbyEnvList");
            if(list == null || list == "")
            {
               return out;
            }
            parts = String(list).split(",");
            while(i < parts.length)
            {
               v = this.k33StyleTrim(parts[i]).toLowerCase();
               i++;
               if(v == "" || v.length > 40 || !/^[a-z0-9_]+$/.test(v))
               {
                  continue;
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
                  "key":"K33UI_VLOBBY_" + v.toUpperCase(),
                  "raw":v
               });
            }
            return out.length > 1 ? out : [];
         }
         if(kind == "glyph")
         {
            list = CFunctionContainerCommon.Dvar_GetString("gpad_info");
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
         list = CFunctionContainerCommon.Dvar_GetString(kind == "lang" ? "loc_languageList" : "cg_hudStyleList");
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
         if(kind == "scene")
         {
            // a string dvar; "" (or "default") is C1's own scene, "center"
            s = CFunctionContainerCommon.Dvar_GetString(K33_STYLE_DVARS[kind]);
            s = s == null ? "" : this.k33StyleTrim(s).toLowerCase();
            return s == "" || s == "default" ? "center" : s;
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

      // a text field in the format of one of the tab's own (`src`), for text that is already translated. Never
      // "@KEY": C1's translator would answer with the text in this field's authored format, and a field made in code
      // has none (Times, 12, black: black boxes in an embedded-font field).
      private function k33StyleText(src:TextField, s:String, size:int, color:uint) : TextField
      {
         var tf:TextField = new TextField();
         var fmt:TextFormat = src.getTextFormat();
         tf.embedFonts = src.embedFonts;
         tf.defaultTextFormat = new TextFormat(fmt.font,size > 0 ? size : int(fmt.size),color);
         tf.selectable = false;
         tf.mouseEnabled = false;
         tf.autoSize = TextFieldAutoSize.LEFT;
         tf.text = s;
         return tf;
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

      // a row's drop-down, made once: one more instance of the class this panel's own combo boxes are (the symbol
      // brings its button, list and skin), opening upwards as the Graphics tab's do. Like the others it comes to
      // the front as it opens. Made only on the stage: a CodoSprite builds itself when it gets there.
      private function k33StyleCombo(kind:String) : CodoComboBox
      {
         var cb:CodoComboBox = this._k33StyleCombos[kind] as CodoComboBox;
         var cls:Class = null;
         if(cb == null)
         {
            cls = getDefinitionByName(getQualifiedClassName(this.myCBAspectRatio)) as Class;
            cb = new cls() as CodoComboBox;
            cb.name = "k33Style_" + kind;
            cb.x = K33_STYLE_BOX_X;
            cb.visible = false;
            this.k33StyleHost().addChild(cb);
            cb.setDropDownAtRuntime(true); // the Game rows start at the top of the panel: their lists open downwards
            cb.addEventListener(CodoComboBox.EVT_CHANGE,this.k33OnStyleChange);
            cb.addEventListener(CodoComboBox.EVT_PRE_OPEN,this.onK33ComboPreOpen);
            cb.addEventListener(CodoComboBox.EVT_OPEN,this.k33OnStyleOpen);
            cb.addEventListener(CodoComboBox.EVT_CLOSE,this.k33OnStyleClose);
            this._k33StyleCombos[kind] = cb;
         }
         return cb;
      }

      // a row's label, made once, in the format of the Graphics rows' labels
      private function k33StyleLabel(kind:String) : TextField
      {
         var tf:TextField = this._k33StyleLabels[kind] as TextField;
         if(tf == null)
         {
            tf = this.k33StyleText(this.myTFSSAO,"",0,this.myTFSSAO.textColor);
            tf.x = K33_STYLE_X;
            tf.visible = false;
            this.k33StyleHost().addChild(tf);
            this._k33StyleLabels[kind] = tf;
         }
         return tf;
      }

      private function k33OnStyleOpen(e:Event) : void
      {
         this._k33StyleOpen = e.currentTarget as CodoComboBox;
      }

      private function k33OnStyleClose(e:Event) : void
      {
         if(this._k33StyleOpen == e.currentTarget)
         {
            this._k33StyleOpen = null;
         }
      }

      // show choice `index` in the row's box without setting its dvar
      private function k33StyleShow(rec:Object, index:int) : void
      {
         index = Math.max(0,Math.min(rec.opts.length - 1,index));
         rec.shown = index;
         if(rec.combo.selectedDataIndex != index)
         {
            this._k33StyleSync = true;
            rec.combo.selectedDataIndex = index;
            this._k33StyleSync = false;
         }
      }

      // a style box changed. The player's pick is shown (the box did that) and its dvar set at once; the dvar is
      // read back K33_STYLE_HOLD frames later. What the code selected (a new list, the dvar's value) sets nothing.
      private function k33OnStyleChange(e:Event) : void
      {
         var rec:Object = null;
         var index:int = 0;
         var i:int = 0;
         if(this._k33StyleSync)
         {
            return;
         }
         while(i < this._k33StyleRows.length)
         {
            if(this._k33StyleRows[i].combo == e.currentTarget)
            {
               rec = this._k33StyleRows[i];
            }
            i++;
         }
         if(rec == null)
         {
            return;
         }
         index = rec.combo.selectedDataIndex;
         if(index < 0 || index >= rec.opts.length)
         {
            return;
         }
         rec.shown = index;
         if(this.k33StyleSet(rec.kind,rec.opts,index))
         {
            rec.hold = K33_STYLE_HOLD;
         }
      }

      // every row off the tab (the boxes and labels are kept for the next build)
      private function k33StyleRemove() : void
      {
         var kind:String = null;
         var cb:CodoComboBox = null;
         var i:int = 0;
         while(i < K33_STYLE_KINDS.length)
         {
            kind = K33_STYLE_KINDS[i];
            cb = this._k33StyleCombos[kind] as CodoComboBox;
            if(cb != null)
            {
               if(cb == this._k33StyleOpen)
               {
                  cb.close();
               }
               cb.visible = false;
            }
            if(this._k33StyleLabels[kind] != null)
            {
               this._k33StyleLabels[kind].visible = false;
            }
            i++;
         }
         this._k33StyleOpen = null;
         this._k33StyleRows = [];
         if(this._k33StyleNote != null && this._k33StyleNote.parent != null)
         {
            this._k33StyleNote.parent.removeChild(this._k33StyleNote);
         }
         this._k33StyleNote = null;
         this._k33StyleNoteKey = "";
      }

      // the rows are made again from the dvars: one per kind that has choices, top to bottom without gaps; labels
      // and names in the loaded language
      private function k33StyleBuild() : void
      {
         var kinds:Array = [];
         var lists:Array = [];
         var opts:Array = null;
         var rec:Object = null;
         var lab:TextField = null;
         var cb:CodoComboBox = null;
         var dp:Array = null;
         var i:int = 0;
         var j:int = 0;
         if(this._k33GameFailed)
         {
            return; // the rows live on the Game tab only
         }
         if(this.stage == null)
         {
            this._k33StyleDirty = true; // k33StyleFrame makes them
            return;
         }
         this._k33StyleDirty = false;
         this.k33StyleRemove();
         while(i < K33_STYLE_KINDS.length)
         {
            opts = this.k33StyleOptions(K33_STYLE_KINDS[i]);
            if(opts.length > 0)
            {
               kinds.push(K33_STYLE_KINDS[i]);
               lists.push(opts);
            }
            i++;
         }
         i = 0;
         while(i < kinds.length)
         {
            opts = lists[i];
            cb = this.k33StyleCombo(kinds[i]);
            lab = this.k33StyleLabel(kinds[i]);
            // in this panel's coordinates (the Game panel sits where this panel does), inside the window's card
            cb.x = K33_STYLE_BOX_X;
            cb.y = K33_STYLE_Y + i * K33_STYLE_PITCH;
            lab.x = K33_STYLE_X;
            lab.text = LocalizeUtil.localize(K33_STYLE_LABELS[kinds[i]]);
            lab.y = cb.y + (this.myTFSSAO.y - this.myCBSSAO.y);
            rec = {
               "kind":kinds[i],
               "label":lab,
               "combo":cb,
               "opts":opts,
               "names":[],
               "shown":-1,
               "hold":0
            };
            dp = [];
            j = 0;
            while(j < opts.length)
            {
               rec.names.push(this.k33StyleName(opts[j]));
               dp.push({
                  "label":rec.names[j],
                  "id":opts[j].v
               });
               j++;
            }
            rec.shown = this.k33StyleIndex(opts,this.k33StyleCurrent(rec.kind));
            this._k33StyleSync = true; // filling the box selects; that is not the player's pick
            cb.dataProvider = dp;
            cb.selectedDataIndex = rec.shown;
            this._k33StyleSync = false;
            lab.visible = true;
            cb.visible = true;
            this._k33StyleRows.push(rec);
            i++;
         }
         if(K33_STYLE_LANGUAGE_ROW)
         {
            this._k33StyleNote = this.k33StyleText(this.myTFSSAO,"",12,10474463);
            this._k33StyleNote.x = K33_STYLE_X;
            this._k33StyleNote.y = K33_STYLE_Y + kinds.length * K33_STYLE_PITCH + K33_STYLE_NOTE_GAP;
            this._k33StyleNote.visible = false;
            this.k33StyleHost().addChild(this._k33StyleNote);
            this.k33StyleNoteUpdate();
         }
      }

      // loc_languageState 1 (switching) / 2 (saved, applies on the next start) as a line under the rows
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
            state = CFunctionContainerCommon.Dvar_GetInt("loc_languageState");
            key = state == 2 ? "@K33UI_LANG_RESTART" : (state == 1 ? "@K33UI_LANG_SWITCHING" : "");
         }
         if(key != this._k33StyleNoteKey)
         {
            this._k33StyleNoteKey = key;
            this._k33StyleNote.text = key == "" ? "" : LocalizeUtil.localize(key); // translated first: k33StyleText
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
            this._k33StyleSerial = CFunctionContainerCommon.Dvar_GetInt("loc_languageSerial");
            this._k33StyleFrames = 0;
            this.k33StyleHost().addEventListener(Event.ENTER_FRAME,this.k33StyleFrame);
         }
         else
         {
            this.k33StyleHost().removeEventListener(Event.ENTER_FRAME,this.k33StyleFrame);
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

      // k33 Game tab: where the rows live (the Game panel; this panel when there is no Game panel, e.g. a test)
      private function k33StyleHost() : Sprite
      {
         return this._k33Game != null ? this._k33Game : this;
      }

      // k33 Game tab: made once, from onInit. OptionView's display list (measured live, 16 November): its first child
      // is the window's card, which every tab shares (a MovieClip at -310,-277, 599 x 544 in OptionView's
      // coordinates), then the three panels, the tab bar's header line, the tab buttons, the close button and the
      // title; OptionView_video has no card of its own (its myBg is null: the card is not on this panel's timeline).
      // So the Game tab draws no background: the button is another instance of OptionView's tab button class, one tab
      // pitch right of the Controller tab, and the panel holds only this panel's section header (another instance of
      // its frame_window_title, at the same place), the tab's name over it and the rows, where this panel sits.
      // Opening Game hides the panel that is shown (k33OnGameTab), so only the card is under the rows.
      private function k33GameSetup() : void
      {
         var host:Object = this.parent;
         var cls:Class = null;
         var head:Sprite = null;
         var d:Object = null;
         var name:String = null;
         var i:int = 0;
         if(this._k33Game != null || host == null || !("myTabOther" in host) || !("myTabControl" in host))
         {
            return;
         }
         try
         {
            cls = getDefinitionByName(getQualifiedClassName(host.myTabOther)) as Class;
            this._k33GameTab = new cls() as CodoGrowButtonNoTint;
            this._k33GameTab.name = "k33TabGame";
            this._k33GameTab.x = host.myTabOther.x + (host.myTabOther.x - host.myTabControl.x);
            this._k33GameTab.y = host.myTabOther.y;
            host.addChild(this._k33GameTab);
            this._k33GameTab.label = LocalizeUtil.localize("@K33UI_GAME_TAB");
            this._k33GameTab.selected = false;
            this._k33GameTab.addEventListener(BaseButton.EVT_CLICK,this.k33OnGameTab);
            for each(name in K33_HOST_TABS)
            {
               if(name in host && host[name] != null)
               {
                  // ahead of OptionView's own listener: the hidden panel is shown again before OptionView changes tab
                  host[name].addEventListener(BaseButton.EVT_CLICK,this.k33OnHostTab,false,100);
               }
            }
            this._k33Game = new Sprite();
            this._k33Game.name = "k33PanelGame";
            this._k33Game.x = this.x;
            this._k33Game.y = this.y;
            this._k33Game.scaleX = this.scaleX;
            this._k33Game.scaleY = this.scaleY;
            this._k33Game.visible = false;
            // this panel's first section header ("Basic"), as the Game tab's header
            while(i < this.numChildren)
            {
               d = this.getChildAt(i);
               if(head == null && getQualifiedClassName(d) == "frame_window_title")
               {
                  cls = getDefinitionByName(getQualifiedClassName(d)) as Class;
                  head = new cls() as Sprite;
                  if(head != null)
                  {
                     head.x = d.x;
                     head.y = d.y;
                     head.scaleX = d.scaleX;
                     head.scaleY = d.scaleY;
                     head.mouseEnabled = false;
                     head.mouseChildren = false;
                     this._k33Game.addChild(head);
                  }
               }
               i++;
            }
            name = LocalizeUtil.localize("@K33UI_GAME_TAB"); // translated before it is set (k33StyleText)
            this._k33GameTitle = this.k33StyleText(this.myTFBase,name,0,this.myTFBase.textColor);
            this._k33GameTitle.x = this.myTFBase.x;
            this._k33GameTitle.y = this.myTFBase.y;
            this._k33Game.addChild(this._k33GameTitle);
            host.addChild(this._k33Game);
            // the window's card, for the rows' bounds (tests/ui_style_rows_test.js): OptionView's first child
            this._k33GameCard = host.getChildAt(0).getBounds(this);
         }
         catch(err:Error)
         {
            // no Game tab at all: the half-made button goes, and the rows are not made on the Graphics tab either
            this._k33GameFailed = true;
            if(this._k33GameTab != null && this._k33GameTab.parent != null)
            {
               this._k33GameTab.parent.removeChild(this._k33GameTab);
            }
            if(this._k33Game != null && this._k33Game.parent != null)
            {
               this._k33Game.parent.removeChild(this._k33Game);
            }
            this._k33GameTab = null;
            this._k33Game = null;
         }
      }

      // k33 Game tab: whether one of OptionView's children is a panel (hidden while Game is open): not the card
      // (first child), the tab bar's header line, a tab, the close button, the title or the Game panel
      private function k33IsHostPanel(host:Object, d:Object) : Boolean
      {
         var n:String = getQualifiedClassName(d);
         if(d == null || d == this._k33Game || d == this._k33GameTab || host.getChildIndex(d) == 0)
         {
            return false;
         }
         if(n == "header_window" || n.indexOf("iconTab") == 0 || d is TextField || d == host.myIconCloseBtn)
         {
            return false;
         }
         return d is Sprite;
      }

      // k33 Game tab: open it in place of the shown panel (OptionView's tabs are shown unselected meanwhile)
      private function k33OnGameTab(e:Event) : void
      {
         var host:Object = this.parent;
         var name:String = null;
         var d:Object = null;
         var i:int = 0;
         if(this._k33GameOpen || this._k33Game == null)
         {
            return;
         }
         this._k33GamePrev = null;
         for each(name in K33_HOST_TABS)
         {
            if(name in host && host[name] != null && host[name].selected)
            {
               this._k33GamePrev = host[name];
               host[name].selected = false;
            }
         }
         this._k33GameHidden = [];
         while(i < host.numChildren)
         {
            d = host.getChildAt(i);
            if(d.visible && this.k33IsHostPanel(host,d))
            {
               d.visible = false;
               this._k33GameHidden.push(d);
            }
            i++;
         }
         this._k33GameOpen = true;
         this._k33GameTab.selected = true;
         this._k33Game.visible = true;
         this.k33StyleBuild();
         this.k33StyleWatch(true);
      }

      // k33 Game tab: one of OptionView's tabs was clicked (this listener runs before OptionView's). Game closes and
      // the panel it hid is shown again; then OptionView changes tab as usual, or ignores a click on its current tab,
      // which is selected again here; a change OptionView refuses (Controls with unbound commands) is caught two frames
      // later: no tab selected -> the one that was
      private function k33OnHostTab(e:Event) : void
      {
         if(!this._k33GameOpen)
         {
            return;
         }
         this.k33GameClose();
         if(this._k33GamePrev != null && e.currentTarget == this._k33GamePrev)
         {
            this._k33GamePrev.selected = true;
         }
         this._k33GameFix = 2;
         this.k33StyleHost().addEventListener(Event.ENTER_FRAME,this.k33GameFixFrame);
      }

      private function k33GameFixFrame(e:Event) : void
      {
         var host:Object = this.parent;
         var name:String = null;
         var any:Boolean = false;
         this._k33GameFix--;
         if(this._k33GameFix > 0)
         {
            return;
         }
         this.k33StyleHost().removeEventListener(Event.ENTER_FRAME,this.k33GameFixFrame);
         for each(name in K33_HOST_TABS)
         {
            if(name in host && host[name] != null && host[name].selected)
            {
               any = true;
            }
         }
         if(!any && this._k33GamePrev != null && !this._k33GameOpen)
         {
            this._k33GamePrev.selected = true;
         }
      }

      // k33 Game tab: close it (an open list first) and show the panel it hid
      private function k33GameClose() : void
      {
         var d:Object = null;
         if(!this._k33GameOpen)
         {
            return;
         }
         this._k33GameOpen = false;
         if(this._k33StyleOpen != null)
         {
            this._k33StyleOpen.close();
            this._k33StyleOpen = null;
         }
         this._k33Game.visible = false;
         this._k33GameTab.selected = false;
         if(this._k33GameHidden != null)
         {
            for each(d in this._k33GameHidden)
            {
               d.visible = true;
            }
            this._k33GameHidden = null;
         }
         this.k33StyleWatch(this._k33VideoOn);
      }

      // a live language switch finished (loc_languageSerial moved): every text of this tab is set again, so C1's
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
            if(this._k33GameTab != null)
            {
               this._k33GameTab.label = LocalizeUtil.localize("@K33UI_GAME_TAB");
            }
            if(this._k33GameTitle != null)
            {
               this._k33GameTitle.text = LocalizeUtil.localize("@K33UI_GAME_TAB");
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
      // list that changed makes the rows again; a row's box shows what its dvar holds
      private function k33StyleFrame(e:Event) : void
      {
         var rec:Object = null;
         var opts:Array = null;
         var serial:int = 0;
         var index:int = 0;
         var i:int = 0;
         if(this._k33StyleDirty)
         {
            this.k33StyleBuild();
         }
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
            serial = CFunctionContainerCommon.Dvar_GetInt("loc_languageSerial");
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
               if(rec != null && rec.hold <= 0 && rec.combo != this._k33StyleOpen) // an open list is left alone
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

