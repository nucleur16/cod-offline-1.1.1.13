package codo.views
{
   // codol-classic's English MatchSelectModeHomeView (FFDec's decompile of its codo.gfx tag): the main screen's
   // SURVIVAL and THEATRE buttons, built in code. k33: their captions are localize keys (K33UI_SURVIVAL, K33UI_THEATRE),
   // localized before they are set: a code-made text field given "@KEY" loses its embedded font when C1's translator
   // replaces the text, and shows boxes.
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.utils.LocalizeUtil;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationDict;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.objects.GroupCountsObj;
   import components.BaseButton;
   import components.PlayModeSelectionBtn;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.geom.Matrix;
   import flash.geom.Rectangle;
   import flash.text.TextField;
   import flash.text.TextFormat;
   import flash.utils.getDefinitionByName;
   
   public class MatchSelectModeHomeView extends BaseView
   {
      
      private static var POS_X_ORIGIN:int = 640;
      
      private static var POS_Y_ORIGIN:int = GameInfo.PANEL_TOP_ALIGN;
      
      public var myCoopBtn:PlayModeSelectionBtn;
      
      public var myMpBtn:PlayModeSelectionBtn;
      
      public var myCyborgsBtn:PlayModeSelectionBtn;
      
      public var mySurvivalBtn:BaseButton;
      
      private var _originMatrix_coop:Matrix;
      
      private var _originMatrix_mp:Matrix;
      
      private var _originMatrix_cyborg:Matrix;
      
      private var theatreModeContainer:Sprite;
      
      private var survivalModeContainer:Sprite;
      
      public function MatchSelectModeHomeView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.myCoopBtn.label = LocalizationDict.MAIN_MENU_LABEL_QUICKMATCH_COOP;
         this.myMpBtn.label = LocalizationDict.MAIN_MENU_LABEL_QUICKMATCH_MP;
         this.myCyborgsBtn.label = LocalizationDict.MAIN_MENU_LABEL_QUICKMATCH_ZOMBIE;
         this.myCoopBtn.description = LocalizationDict.QUICK_MATCH_COOP_INTRO;
         this.myMpBtn.description = LocalizationDict.QUICK_MATCH_MP_INTRO;
         this.myCyborgsBtn.description = LocalizationDict.QUICK_MATCH_CYBORG_INTRO;
         this.x = POS_X_ORIGIN;
         this.y = POS_Y_ORIGIN;
         this.initTheatreModeCards();
         this._originMatrix_coop = this.myCoopBtn.transform.matrix;
         this._originMatrix_mp = this.myMpBtn.transform.matrix;
         this._originMatrix_cyborg = this.myCyborgsBtn.transform.matrix;
         this.tweenHide(0,{
            "autoAlpha":0,
            "z":TweenParam.Z_MATCH_SELECT_MODE,
            "blurFilter":TweenParam.BLUR_MATCH_SELECT_MODE
         },this.myCoopBtn);
         this.tweenHide(0,{
            "autoAlpha":0,
            "z":TweenParam.Z_MATCH_SELECT_MODE,
            "blurFilter":TweenParam.BLUR_MATCH_SELECT_MODE
         },this.myMpBtn);
         this.tweenHide(0,{
            "autoAlpha":0,
            "z":TweenParam.Z_MATCH_SELECT_MODE,
            "blurFilter":TweenParam.BLUR_MATCH_SELECT_MODE
         },this.myCyborgsBtn);
         this.myCoopBtn.playerAmount = "0";
         this.myMpBtn.playerAmount = "0";
         this.myCyborgsBtn.playerAmount = "0";
      }
      
      override protected function onInitListener() : void
      {
         this.myCoopBtn.addEventListener(BaseButton.EVT_CLICK,this.onCoopClickHandle);
         this.myMpBtn.addEventListener(BaseButton.EVT_CLICK,this.onMPClickHandle);
         this.myCyborgsBtn.addEventListener(BaseButton.EVT_CLICK,this.onCyborgsClickHandle);
         this.mySurvivalBtn.addEventListener(BaseButton.EVT_CLICK,this.onSurvivalClickHandle);
      }
      
      override protected function showView() : void
      {
         this.showTheatreModeCard();
         this.tweenShow(TweenParam.SPD_FADE,{
            "autoAlpha":1,
            "delay":0.3,
            "z":0,
            "blurFilter":TweenParam.BLUR_NONE
         },this.myCoopBtn,this._originMatrix_coop);
         this.tweenShow(TweenParam.SPD_FADE + 0.1,{
            "autoAlpha":1,
            "delay":0.3,
            "z":0,
            "blurFilter":TweenParam.BLUR_NONE
         },this.myMpBtn,this._originMatrix_mp);
         this.myCoopBtn.startShaderEffect();
         this.myMpBtn.startShaderEffect();
         this.enableCyborg = this.getSyncDP(SyncDataID.IS_CYBORG_ENABLED);
         this.enableSurvival = this.getSyncDP(SyncDataID.IS_SURVIVAL_ENABLED);
      }
      
      override protected function hideView() : void
      {
         this.hideTheatreModeCard();
         this.tweenHide(TweenParam.SPD_FADE_OUT,{
            "autoAlpha":0,
            "z":TweenParam.Z_MATCH_SELECT_MODE,
            "blurFilter":TweenParam.BLUR_MATCH_SELECT_MODE
         },this.myCoopBtn);
         this.tweenHide(TweenParam.SPD_FADE_OUT,{
            "autoAlpha":0,
            "z":TweenParam.Z_MATCH_SELECT_MODE,
            "blurFilter":TweenParam.BLUR_MATCH_SELECT_MODE
         },this.myMpBtn);
         this.myCoopBtn.stopShaderEffect();
         this.myMpBtn.stopShaderEffect();
         this.enableCyborg = false;
         this.enableSurvival = false;
      }
      
      override protected function enableData() : void
      {
         this.listen(BCEvent.UI_ENABLE_CYBORG,this.onEnableCyborgHandle);
         this.listen(BCEvent.UI_ENABLE_SURVIVAL,this.onEnableSurvivalHandle);
         this.bind(AsyncTaskID.GROUP_COUNTS,this.bind_groupCounts);
      }
      
      override protected function disableData() : void
      {
         this.stopListen(BCEvent.UI_ENABLE_CYBORG,this.onEnableCyborgHandle);
         this.stopListen(BCEvent.UI_ENABLE_SURVIVAL,this.onEnableSurvivalHandle);
         this.unbind(AsyncTaskID.GROUP_COUNTS,this.bind_groupCounts);
      }
      
      private function bind_groupCounts(cRes:Object) : void
      {
         if(cRes.data == null || !(cRes.data is GroupCountsObj))
         {
            return;
         }
         this.myCoopBtn.playerAmount = cRes.data.coop;
         this.myMpBtn.playerAmount = cRes.data.mp;
         this.myCyborgsBtn.playerAmount = cRes.data.cyborg;
      }
      
      private function onCoopClickHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("codol_survival_direct_home","0");
         this.broadcast(BCEvent.EVT_GOTO_MAIN_COOP);
      }
      
      private function onCyborgsClickHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("codol_survival_direct_home","0");
         this.broadcast(BCEvent.EVT_GOTO_MAIN_CYBORG);
      }
      
      private function onSurvivalClickHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("codol_survival_direct_home","1");
         this.broadcast(BCEvent.EVT_GOTO_SURVIVAL_QUICK_MATCH);
      }
      
      private function onMPClickHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("codol_survival_direct_home","0");
         this.broadcast(BCEvent.EVT_GOTO_MAIN_MP);
      }
      
      private function onEnableCyborgHandle(cEvt:ParaEvent) : void
      {
         this.enableCyborg = cEvt.myPara;
      }
      
      private function onEnableSurvivalHandle(cEvt:ParaEvent) : void
      {
         this.enableSurvival = cEvt.myPara;
      }
      
      private function set enableCyborg(enable:Boolean) : void
      {
         if(enable)
         {
            this.tweenShow(TweenParam.SPD_FADE + 0.2,{
               "autoAlpha":1,
               "delay":0.3,
               "z":0,
               "blurFilter":TweenParam.BLUR_NONE
            },this.myCyborgsBtn,this._originMatrix_cyborg);
            this.myCyborgsBtn.startShaderEffect();
         }
         else
         {
            this.tweenHide(TweenParam.SPD_FADE_OUT,{
               "autoAlpha":0,
               "z":TweenParam.Z_MATCH_SELECT_MODE,
               "blurFilter":TweenParam.BLUR_MATCH_SELECT_MODE
            },this.myCyborgsBtn);
            this.myCyborgsBtn.stopShaderEffect();
         }
      }
      
      private function set enableSurvival(enable:Boolean) : void
      {
         this.mySurvivalBtn.enable = enable;
         this.survivalModeContainer.visible = enable;
      }
      
      private function bottomModeControl(label:String, xPos:Number, yPos:Number) : Sprite
      {
         var container:Sprite;
         var artwork:Sprite;
         var headerClass:Class;
         var header:Object;
         var format:TextFormat;
         var caption:TextField;
         var buttonClass:Class = getDefinitionByName("generalBtn_ani_blue2") as Class;
         var button:Object = new buttonClass();
         button.label = "";
         button.enable = true;
         container = new Sprite();
         container.x = xPos;
         container.y = yPos;
         artwork = new Sprite();
         artwork.addChild(button as Sprite);
         container.addChild(artwork);
         headerClass = getDefinitionByName("raven_listHeader") as Class;
         header = new headerClass();
         format = header.txt_header.defaultTextFormat;
         format.size = 24;
         format.color = 16777215;
         format.align = "left";
         format.leftMargin = format.rightMargin = format.indent = format.blockIndent = 0;
         caption = new TextField();
         caption.defaultTextFormat = format;
         caption.embedFonts = header.txt_header.embedFonts;
         caption.autoSize = "left";
         caption.text = LocalizeUtil.localize(label); // k33
         caption.x = (300 - caption.width) / 2;
         caption.y = 12;
         caption.mouseEnabled = false;
         caption.selectable = false;
         container.addChild(caption);
         container.addEventListener(Event.ENTER_FRAME,function(event:Event):void
         {
            var bounds:Rectangle = button.getBounds(button);
            if(bounds.width > 0 && bounds.height > 0)
            {
               artwork.scaleX = 300 / bounds.width;
               artwork.scaleY = 48 / bounds.height;
               button.x = -bounds.x;
               button.y = -bounds.y;
            }
         });
         return container;
      }
      
      private function initTheatreModeCards() : void
      {
         var oldSurvival:BaseButton = this.mySurvivalBtn;
         var bottomY:Number = oldSurvival.y;
         this.survivalModeContainer = bottomModeControl("@K33UI_SURVIVAL",-315,bottomY);
         this.theatreModeContainer = bottomModeControl("@K33UI_THEATRE",15,bottomY);
         this.mySurvivalBtn = (this.survivalModeContainer.getChildAt(0) as Sprite).getChildAt(0) as BaseButton;
         var theatreButton:BaseButton = (this.theatreModeContainer.getChildAt(0) as Sprite).getChildAt(0) as BaseButton;
         theatreButton.addEventListener(BaseButton.EVT_CLICK,this.onTheatreModeClick);
         this.addChild(this.survivalModeContainer);
         this.addChild(this.theatreModeContainer);
         this.survivalModeContainer.visible = false;
         this.theatreModeContainer.visible = false;
         if(oldSurvival.parent != null)
         {
            oldSurvival.parent.removeChild(oldSurvival);
         }
      }
      
      private function showTheatreModeCard() : void
      {
         this.theatreModeContainer.visible = true;
      }
      
      private function hideTheatreModeCard() : void
      {
         this.theatreModeContainer.visible = false;
      }
      
      private function onTheatreModeClick(event:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("codol_theatre_open","1");
      }
   }
}

