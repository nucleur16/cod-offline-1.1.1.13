package codo.views
{
   import codo.common.BaseView;
   import codo.common.ParaEvent;
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
         this.myRoomNameTF.text = this.getSyncDP(SyncDataID.DEFAULT_ROOM_NAME);
         this.myPasswordTF.text = "";
         var matchType:uint = this.getSyncDP(SyncDataID.CURRENT_MATCH_TYPE);
         if(this._currentMatchType != matchType)
         {
            this._currentMatchType = matchType;
            if(this._optionMap != null)
            {
               this._optionMap.removeEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
            }
            switch(this._currentMatchType)
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
         this.mapDP = this.getSyncDP(SyncDataID.LIST_MAP,[MatchType.COOP,false,this.myOptionCB_0.selectedValue.id]);
      }
      
      protected function onMapChangedHandle(cEvt:ParaEvent) : void
      {
         var selectedIndex:int = 0;
         var modeID:String = "";
         var maxPlayers:Array = null;
         if(this._currentMatchType == QuickMatchType.MP && (cEvt == null || cEvt.currentTarget == this.myOptionCB_2))
         {
            modeID = this.myOptionCB_2.selectedValue != null ? this.myOptionCB_2.selectedValue.id : "";
            maxPlayers = this.getSyncDP(SyncDataID.LIST_MAX_PLAYER);
            if(modeID == "gun" || modeID == "oic" || modeID == "shrp")
            {
               this.myOptionCB_1.dataProvider = [{
                  "name":6,
                  "id":6
               }];
               this.myOptionCB_1.selectedDataIndex = 0;
               this.myOptionCB_1.enabled = false;
            }
            else if(modeID == "infect" && maxPlayers != null && maxPlayers.length > 0)
            {
               this.myOptionCB_1.dataProvider = maxPlayers;
               this.myOptionCB_1.selectedDataIndex = maxPlayers.length - 1;
               this.myOptionCB_1.enabled = true;
            }
            else if(maxPlayers != null && this.myOptionCB_1.dataProvider.length != maxPlayers.length)
            {
               this.myOptionCB_1.dataProvider = maxPlayers;
               this.myOptionCB_1.selectedDataIndex = 0;
               this.myOptionCB_1.enabled = true;
            }
         }
         if(this._currentMatchType == QuickMatchType.COOP)
         {
            selectedIndex = this.myOptionCB_1.selectedDataIndex;
            this.myOptionCB_1.dataProvider = this.getSyncDP(SyncDataID.LIST_DIFFICULTY,[this._optionMap.selectedValue]);
            this.myOptionCB_1.selectedDataIndex = selectedIndex >= 0 && selectedIndex < this.myOptionCB_1.dataProvider.length ? selectedIndex : 0;
            this.myOptionCB_1.enabled = this.myOptionCB_1.dataProvider.length > 1;
         }
         this.showMapInfo(this._optionMap.selectedValue);
      }
      
      private function onRoomNameChangedHandle(cEvt:Event) : void
      {
         if(this.myRoomNameTF.text.length > 0)
         {
            this.myRoomNameTF.text = StringUtil.limitStringToRealLength(this.myRoomNameTF.text,STRING_LEN_ROOM_NAME_MAX);
         }
      }
      
      private function onBackBtnClickHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.EVT_CLOSE_ROOM_CREATOR);
      }
      
      protected function onCreateRoomBtnClickHandle(cEvt:ParaEvent) : void
      {
         var room:Object = {};
         room.name = this.myRoomNameTF.text;
         room.password = this.myPasswordTF.text;
         room.map = this._optionMap.selectedValue.id;
         room.minPlayer = this._optionMap.selectedValue.minPlayer;
         if(this._currentMatchType == QuickMatchType.COOP)
         {
            room.mode = MatchType.COOP;
            room.difficulty = this.myOptionCB_1.selectedValue.id;
            room.maxPlayer = this._optionMap.selectedValue.maxPlayer;
         }
         else
         {
            room.mode = this.myOptionCB_2.selectedValue.id;
            room.difficulty = this.myOptionCB_0.selectedValue.id;
            room.maxPlayer = this.myOptionCB_1.selectedValue.id;
         }
         this.broadcast(BCEvent.EVT_CREATE_ROOM,room);
      }
      
      private function onRoomNameSanitizedHandle(cEvt:ParaEvent) : void
      {
         if(cEvt.myPara.type == SanitizedType.ROOM_NAME && Boolean(cEvt.myPara.sanitized))
         {
            this.myRoomNameTF.text = cEvt.myPara.content;
            this.showMsg(LocalizationRoom.NOTE_SANITIZED_ROOM_NAME,MsgBtnCount.ONE);
         }
      }
      
      private function onPreMapBtnClickHandle(cEvt:ParaEvent) : void
      {
         if(this._optionMap.selectedDataIndex > 0)
         {
            this._optionMap.selectPrev();
         }
         else
         {
            this._optionMap.selectedDataIndex = this._optionMap.dataProvider.length - 1;
         }
      }
      
      private function onNextMapBtnClickHandle(cEvt:ParaEvent) : void
      {
         if(this._optionMap.selectedDataIndex < this._optionMap.dataProvider.length - 1)
         {
            this._optionMap.selectNext();
         }
         else
         {
            this._optionMap.selectedDataIndex = 0;
         }
      }
      
      protected function showMapInfo(cMap:Object) : void
      {
         if(cMap != null)
         {
            ImageLoader.loadImg(cMap.imageURL,this.myMapImg,this.onMapImageLoadComplete);
         }
      }
      
      private function onMapImageLoadComplete(cBMP:Bitmap) : void
      {
         TweenLite.killTweensOf(this.myMapImg);
         this.myMapImg.alpha = 0;
         TweenLite.to(this.myMapImg,TweenParam.SPD_FADE,{"alpha":1});
         cBMP.width = 326;
         cBMP.height = 234;
      }
      
      private function refreshCoopData() : void
      {
         var preSelectedIndex:int = 0;
         this._optionMap = this.myOptionCB_2;
         this._optionMap.addEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         this.myOptionCB_0.addEventListener(CodoComboBox.EVT_CHANGE,this.onCoopTypeChangedHandle);
         this.myOptionCB_0.removeEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         var coopTypes:Array = this.getSyncDP(SyncDataID.LIST_COOP_TYPE);
         if(coopTypes != null && coopTypes.length > 0)
         {
            preSelectedIndex = this.myOptionCB_0.selectedDataIndex;
            this.myOptionCB_0.dataProvider = coopTypes;
            this.myOptionCB_0.selectedDataIndex = preSelectedIndex >= 0 && preSelectedIndex < this.myOptionCB_0.dataProvider.length ? preSelectedIndex : 0;
         }
      }
      
      private function refreshMPData() : void
      {
         this._optionMap = this.myOptionCB_3;
         this._optionMap.addEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         this.myOptionCB_0.removeEventListener(CodoComboBox.EVT_CHANGE,this.onCoopTypeChangedHandle);
         this.myOptionCB_0.removeEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         var leagues:Array = this.getSyncDP(SyncDataID.LIST_LEAGUE,[true]);
         if(leagues != null && leagues.length > 0)
         {
            this.myOptionCB_0.dataProvider = leagues;
            this.myOptionCB_0.selectedDataIndex = 0;
         }
         var maxPlayers:Array = this.getSyncDP(SyncDataID.LIST_MAX_PLAYER);
         if(maxPlayers != null && maxPlayers.length > 0)
         {
            this.myOptionCB_1.enabled = true;
            this.myOptionCB_1.dataProvider = maxPlayers;
            this.myOptionCB_1.selectedDataIndex = 0;
         }
         var modeDP:Array = this.getSyncDP(SyncDataID.LIST_MODE,[MatchType.PVP]);
         if(modeDP != null && modeDP.length > 0)
         {
            this.myOptionCB_2.dataProvider = modeDP;
            this.myOptionCB_2.selectedDataIndex = 0;
         }
         this.mapDP = this.getSyncDP(SyncDataID.LIST_MAP,[MatchType.PVP]);
         this.myOptionCB_2.addEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         this.onMapChangedHandle(null);
      }
      
      private function refreshCyborgsData() : void
      {
         this._optionMap = this.myOptionCB_0;
         this._optionMap.addEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         this.myOptionCB_0.removeEventListener(CodoComboBox.EVT_CHANGE,this.onCoopTypeChangedHandle);
         this.mapDP = this.getSyncDP(SyncDataID.LIST_MAP,[MatchType.ZM]);
      }
      
      private function set mapDP(mapDP:Array) : void
      {
         if(mapDP != null && mapDP.length > 0)
         {
            this._optionMap.dataProvider = mapDP;
            this._optionMap.selectedDataIndex = 0;
         }
      }
   }
}

