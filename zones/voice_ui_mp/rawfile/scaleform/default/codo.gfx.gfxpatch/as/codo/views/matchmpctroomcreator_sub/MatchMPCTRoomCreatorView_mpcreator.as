package codo.views.matchmpctroomcreator_sub
{
   import codo.common.BaseSubElement;
   import codo.common.CFunctionContainerCommon;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.BCEventCommon;
   import codo.constants.LocalizationRoom;
   import codo.constants.MatchType;
   import codo.constants.MsgBtnCount;
   import codo.constants.QuickMatchType;
   import codo.constants.SanitizedType;
   import codo.constants.SyncDataID;
   import codo.k33.LocalCreateRoomOption;
   import codo.utils.StringUtil;
   import components.Album;
   import components.CodoComboBox;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;
   
   public class MatchMPCTRoomCreatorView_mpcreator extends BaseSubElement
   {
      private static const LIMIT_PASSWORD:uint = 16;
      
      private static const STRING_LEN_ROOM_NAME_MAX:uint = 24;
      
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
      
      private var _optionMap:CodoComboBox;
      
      private var _currentMatchType:int = -1;
      
      private var _mapAlbum:Album;

      private var localCreateOption:LocalCreateRoomOption;
      
      public function MatchMPCTRoomCreatorView_mpcreator()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.myRoomNameTF.text = LocalizationRoom.CREATE_ROOM_NAME;
         this.myRoomNameTF.maxChars = 12;
         this.myPasswordLabelTF.text = LocalizationRoom.CREATE_ROOM_PASSWORD;
         this.myRoomNameLabelTF.text = LocalizationRoom.CREATE_ROOM_NAME;
         this.localCreateOption = new LocalCreateRoomOption(this.myRoomNameLabelTF);
         this.localCreateOption.multiplayerTheme = true;
         this.localCreateOption.x = this.myOptionLabelTF_2.x + 492;
         this.localCreateOption.y = this.myOptionLabelTF_2.y + 65;
         this.localCreateOption.visible = false;
         this.addChild(this.localCreateOption);
         this.myPasswordLabelTF.text = LocalizationRoom.CREATE_ROOM_PASSWORD;
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
         this.myOptionLabelTF_3.visible = this.myOptionCB_3.visible = this.myOptionBg_3.visible = false;
         this.myRoomNameTF.addEventListener(Event.CHANGE,this.onRoomNameChangedHandle);
         this.myPasswordTF.addEventListener(Event.CHANGE,this.onInputChangedHandle);
      }
      
      override public function enable() : void
      {
         this.localCreateOption.selected = false;
         CFunctionContainerCommon.setdvar("ui_p2p_create","0");
         this.addSyncDP(SyncDataID.MATCH_MPCT_ROOMCREATOR_MP_PARAMS,this.getRoomCreatorParams);
         this.listen(BCEventCommon.EVT_ON_STRING_SANITIZED,this.onRoomNameSanitizedHandle);
         this.myRoomNameTF.text = super.getSyncDP(SyncDataID.DEFAULT_ROOM_NAME);
         this.myPasswordTF.text = "";
         var matchType:uint = super.getSyncDP(SyncDataID.CURRENT_MATCH_TYPE);
         Logger.print("MatchMPCTRoomCreatorView_mpcreator:::enable matchType: " + matchType);
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
                  this.myOptionLabelTF_3.visible = this.myOptionCB_3.visible = this.myOptionBg_3.visible = false;
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
         this.localCreateOption.visible = this._currentMatchType == QuickMatchType.MP;
      }
      
      override public function disable() : void
      {
         this.localCreateOption.selected = false;
         CFunctionContainerCommon.setdvar("ui_p2p_create","0");
         this.stopListen(BCEventCommon.EVT_ON_STRING_SANITIZED,this.onRoomNameSanitizedHandle);
      }
      
      public function set active(cSwi:Boolean) : void
      {
         if(cSwi == true)
         {
            this.alpha = 1;
            this.mouseEnabled = true;
            this.mouseChildren = true;
            if(this._mapAlbum != null && this._optionMap != null)
            {
               this._mapAlbum.data = this._optionMap.dataProvider;
               this._mapAlbum.currentIndex = this._optionMap.selectedDataIndex;
            }
         }
         else
         {
            this.alpha = 0.5;
            this.mouseEnabled = false;
            this.mouseChildren = false;
         }
      }
      
      public function set mapAlbum(cAlbum:Album) : void
      {
         this._mapAlbum = cAlbum;
         this._mapAlbum.addEventListener(Album.EVT_INDEX_CHANGED,this.onMapAlbumChangeHandle);
      }
      
      private function onInputChangedHandle(cEvt:Event) : void
      {
         if(this.myPasswordTF.text.length > 0)
         {
            this.myPasswordTF.text = StringUtil.limitStringToRealLength(this.myPasswordTF.text,LIMIT_PASSWORD);
         }
      }
      
      private function onMapChangedHandle(cEvt:ParaEvent) : void
      {
         var selectedIndex:int = 0;
         var modeID:String = "";
         var maxPlayers:Array = null;
         if(this._currentMatchType == QuickMatchType.MP && (cEvt == null || cEvt.currentTarget == this.myOptionCB_2))
         {
            modeID = this.myOptionCB_2.selectedValue != null ? this.myOptionCB_2.selectedValue.id : "";
            this.refreshMPMaps(modeID);
            maxPlayers = this.getSyncDP(SyncDataID.LIST_MAX_PLAYER);
