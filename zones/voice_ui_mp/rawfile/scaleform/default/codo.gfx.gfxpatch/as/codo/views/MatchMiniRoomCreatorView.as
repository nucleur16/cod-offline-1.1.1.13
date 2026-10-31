package codo.views
{
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ParaEvent;
   import codo.k33.LocalCreateRoomOption;
   import codo.constants.BCEvent;
   import codo.constants.BCEventCommon;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationDict;
   import codo.constants.LocalizationRoom;
   import codo.constants.MatchType;
   import codo.constants.MsgBtnCount;
   import codo.constants.QuickMatchType;
   import codo.constants.SanitizedType;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.utils.ImageLoader;
   import codo.utils.StringUtil;
   import com.greensock.TweenLite;
   import components.BaseButton;
   import components.CodoComboBox;
   import components.CodoFlipButton;
   import components.CodoGrowButton;
   import components.CodoGrowButtonNoTint;
   import flash.display.Bitmap;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;
   
   public class MatchMiniRoomCreatorView extends BaseView
   {
      private static const LIMIT_PASSWORD:uint = 16;
      
      private static var ALPHA_ORIGIN:int = 0;
      
      private static var ALPHA_OPEN:int = 1;
      
      private static const STRING_LEN_ROOM_NAME_MAX:uint = 24;
      
      private static const POS_X_ORIGIN:int = 870;
      
      private static const POS_X_OPEN:int = GameInfo.PANEL_GAP;
      
      private static const POS_Y_ORIGIN:int = GameInfo.PANEL_TOP_ALIGN;
      
      public var myFlipBtn:CodoFlipButton;
      
      public var myHeader:CodoGrowButtonNoTint;
      
      public var myCreateBtn:CodoGrowButtonNoTint;
      
      public var myMapPretBtn:CodoGrowButton;
      
      public var myMapNextBtn:CodoGrowButton;
      
      public var myRoomNameTF:TextField;
      
      public var myRoomNameLabelTF:TextField;
      
      public var myPasswordLabelTF:TextField;
      
      public var myPasswordTF:TextField;
      
      public var myOptionLabelTF_0:TextField;
      
      public var myOptionCB_0:CodoComboBox;
      
      public var myOptionBg_0:Sprite;
      
      public var myOptionLabelTF_1:TextField;
      
      public var myOptionCB_1:CodoComboBox;
      
      public var myOptionBg_1:Sprite;
      
      public var myOptionLabelTF_2:TextField;
      
      public var myOptionCB_2:CodoComboBox;
      
      public var myOptionBg_2:Sprite;
      
      public var myOptionLabelTF_3:TextField;
      
      public var myOptionCB_3:CodoComboBox;
      
      public var myOptionBg_3:Sprite;
      
      public var myMapImg:Sprite;
      
      protected var _optionMap:CodoComboBox;
      
      protected var _currentMatchType:int = -1;

      private var localCreateOption:LocalCreateRoomOption;
      
      public function MatchMiniRoomCreatorView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.x = POS_X_ORIGIN;
         this.y = POS_Y_ORIGIN;
         this.alpha = ALPHA_ORIGIN;
         this.myHeader.label = LocalizationDict.CREATE_ROOM_TITLE;
         this.myFlipBtn.flipLabel = LocalizationDict.BACK;
         this.myRoomNameTF.text = LocalizationRoom.CREATE_ROOM_NAME;
         this.myRoomNameTF.maxChars = 12;
         this.myPasswordLabelTF.text = LocalizationRoom.CREATE_ROOM_PASSWORD;
         this.myRoomNameLabelTF.text = LocalizationRoom.CREATE_ROOM_NAME;
         this.myPasswordLabelTF.text = LocalizationRoom.CREATE_ROOM_PASSWORD;
         this.myCreateBtn.label = LocalizationRoom.CREATE;
         this.myHeader.enable = false;
         localCreateOption = new LocalCreateRoomOption(this.myRoomNameLabelTF);
         localCreateOption.x = this.myCreateBtn.x - 163;
         localCreateOption.y = this.myCreateBtn.y + 6;
         localCreateOption.visible = false;
         this.addChild(localCreateOption);
         var fitFieldPassword:Object = this.myPasswordLabelTF;
         if(fitFieldPassword != null && fitFieldPassword.text == "Room Password" && fitFieldPassword.width > 4)
         {
            var fitFormatPassword:Object = fitFieldPassword.getTextFormat();
            var fitSizePassword:Number = Number(fitFieldPassword.defaultTextFormat.size);
            if(fitSizePassword >= 10 && fitSizePassword <= 72)
            {
               fitFormatPassword.size = fitSizePassword;
               fitFieldPassword.setTextFormat(fitFormatPassword);
               while(fitFieldPassword.textWidth > fitFieldPassword.width - 4 && fitSizePassword > 10)
               {
                  fitSizePassword = Math.max(10,fitSizePassword - 1);
                  fitFormatPassword.size = fitSizePassword;
                  fitFieldPassword.setTextFormat(fitFormatPassword);
               }
            }
         }
      }
      
      override protected function onInitListener() : void
      {
         this.myFlipBtn.addEventListener(BaseButton.EVT_CLICK,this.onBackBtnClickHandle);
         this.myCreateBtn.addEventListener(BaseButton.EVT_CLICK,this.onLocalCreateChosen);
         this.myCreateBtn.addEventListener(BaseButton.EVT_CLICK,this.onCreateRoomBtnClickHandle);
         this.myMapPretBtn.addEventListener(BaseButton.EVT_CLICK,this.onPreMapBtnClickHandle);
         this.myMapNextBtn.addEventListener(BaseButton.EVT_CLICK,this.onNextMapBtnClickHandle);
         this.myRoomNameTF.addEventListener(Event.CHANGE,this.onRoomNameChangedHandle);
         this.myPasswordTF.addEventListener(Event.CHANGE,this.onInputChangedHandle);
      }
      
      override protected function showView() : void
      {
         this.tweenShow(TweenParam.SPD_ELEMENT_MOVE,{
            "x":POS_X_OPEN,
            "alpha":ALPHA_OPEN
         });
      }
      
      override protected function hideView() : void
      {
         this.tweenHide(TweenParam.SPD_ELEMENT_MOVE,{
            "x":POS_X_ORIGIN,
            "alpha":ALPHA_ORIGIN
         });
      }
      
      override protected function enableData() : void
      {
         this.listen(BCEventCommon.EVT_ON_STRING_SANITIZED,this.onRoomNameSanitizedHandle);
         localCreateOption.selected = false;
         CFunctionContainerCommon.setdvar("ui_p2p_create","0");
         this.myRoomNameTF.text = super.getSyncDP(SyncDataID.DEFAULT_ROOM_NAME);
         this.myPasswordTF.text = "";
         var matchType:uint = super.getSyncDP(SyncDataID.CURRENT_MATCH_TYPE);
         if(_currentMatchType != matchType)
         {
            _currentMatchType = matchType;
            if(_optionMap != null)
            {
               _optionMap.removeEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
            }
            switch(_currentMatchType)
            {
               case QuickMatchType.COOP:
                  this.myOptionLabelTF_0.text = LocalizationRoom.MAP_TYPE;
                  this.myOptionLabelTF_1.text = LocalizationRoom.ROOM_LEVEL;
                  this.myOptionLabelTF_2.text = LocalizationRoom.LIST_LABEL_MAP;
                  this.myOptionLabelTF_1.visible = this.myOptionCB_1.visible = this.myOptionBg_1.visible = true;
                  this.myOptionLabelTF_2.visible = this.myOptionCB_2.visible = this.myOptionBg_2.visible = true;
                  this.myOptionLabelTF_3.visible = this.myOptionCB_3.visible = this.myOptionBg_3.visible = false;
                  this.refreshCoopData();
                  break;
               case QuickMatchType.MP:
                  this.myOptionLabelTF_0.text = LocalizationRoom.LIST_LABEL_LEAGUE;
                  this.myOptionLabelTF_1.text = LocalizationRoom.GAME_OPTION_MAX_PLAYER;
                  this.myOptionLabelTF_2.text = LocalizationRoom.CREATE_ROOM_MODE;
                  this.myOptionLabelTF_3.text = LocalizationRoom.LIST_LABEL_MAP;
                  this.myOptionLabelTF_1.visible = this.myOptionCB_1.visible = this.myOptionBg_1.visible = true;
                  this.myOptionLabelTF_2.visible = this.myOptionCB_2.visible = this.myOptionBg_2.visible = true;
                  this.myOptionLabelTF_3.visible = this.myOptionCB_3.visible = this.myOptionBg_3.visible = true;
                  this.refreshMPData();
                  break;
               case QuickMatchType.CYBORG:
                  this.myOptionLabelTF_0.text = LocalizationRoom.LIST_LABEL_MAP;
                  this.myOptionLabelTF_1.visible = this.myOptionCB_1.visible = this.myOptionBg_1.visible = false;
                  this.myOptionLabelTF_2.visible = this.myOptionCB_2.visible = this.myOptionBg_2.visible = false;
                  this.myOptionLabelTF_3.visible = this.myOptionCB_3.visible = this.myOptionBg_3.visible = false;
                  this.refreshCyborgsData();
            }
         }
         this.updateLocalOption();
      }
      
      override protected function disableData() : void
      {
         this.stopListen(BCEventCommon.EVT_ON_STRING_SANITIZED,this.onRoomNameSanitizedHandle);
      }
      
      private function onInputChangedHandle(cEvt:Event) : void
      {
         if(this.myPasswordTF.text.length > 0)
         {
            this.myPasswordTF.text = StringUtil.limitStringToRealLength(this.myPasswordTF.text,LIMIT_PASSWORD);
         }
      }
      
      private function onCoopTypeChangedHandle(cEvt:ParaEvent) : void
      {
         this.mapDP = super.getSyncDP(SyncDataID.LIST_MAP,[MatchType.COOP,false,this.myOptionCB_0.selectedValue.id]);
      }
      
      protected function onMapChangedHandle(cEvt:ParaEvent) : void
      {
         var selectedIndex:int = 0;
         var modeID:String = "";
         var maxPlayers:Array = null;
         if(_currentMatchType == QuickMatchType.MP && (cEvt == null || cEvt.currentTarget == this.myOptionCB_2))
         {
            modeID = this.myOptionCB_2.selectedValue != null ? this.myOptionCB_2.selectedValue.id : "";
            this.refreshMPMaps(modeID);
            maxPlayers = this.getSyncDP(SyncDataID.LIST_MAX_PLAYER);
