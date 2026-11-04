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
            maxPlayers = super.getSyncDP(SyncDataID.LIST_MAX_PLAYER);
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
            this.myOptionCB_1.dataProvider = super.getSyncDP(SyncDataID.LIST_DIFFICULTY,[this._optionMap.selectedValue]);
            this.myOptionCB_1.selectedDataIndex = selectedIndex >= 0 && selectedIndex < this.myOptionCB_1.dataProvider.length ? selectedIndex : 0;
            this.myOptionCB_1.enabled = this.myOptionCB_1.dataProvider.length > 1;
         }
         this._mapAlbum.currentIndex = this._optionMap.selectedDataIndex;
      }
      
      private function onRoomNameChangedHandle(cEvt:Event) : void
      {
         if(this.myRoomNameTF.text.length > 0)
         {
            this.myRoomNameTF.text = StringUtil.limitStringToRealLength(this.myRoomNameTF.text,STRING_LEN_ROOM_NAME_MAX);
         }
      }
      
      private function onRoomNameSanitizedHandle(cEvt:ParaEvent) : void
      {
         if(cEvt.myPara.type == SanitizedType.ROOM_NAME && Boolean(cEvt.myPara.sanitized))
         {
            this.myRoomNameTF.text = cEvt.myPara.content;
            this.showMsg(LocalizationRoom.NOTE_SANITIZED_ROOM_NAME,MsgBtnCount.ONE);
         }
      }
      
      private function refreshCoopData() : void
      {
         var preSelectedIndex:int = 0;
         this._optionMap = this.myOptionCB_2;
         this._optionMap.addEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         this.myOptionCB_0.removeEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         var coopTypes:Array = super.getSyncDP(SyncDataID.LIST_COOP_TYPE);
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
         this.myOptionCB_0.removeEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         var leagues:Array = super.getSyncDP(SyncDataID.LIST_LEAGUE,[true]);
         if(leagues != null && leagues.length > 0)
         {
            this.myOptionCB_0.dataProvider = leagues;
            this.myOptionCB_0.selectedDataIndex = 0;
         }
         var maxPlayers:Array = super.getSyncDP(SyncDataID.LIST_MAX_PLAYER);
         if(maxPlayers != null && maxPlayers.length > 0)
         {
            this.myOptionCB_1.enabled = true;
            this.myOptionCB_1.dataProvider = maxPlayers;
            this.myOptionCB_1.selectedDataIndex = 0;
         }
         var modeDP:Array = super.getSyncDP(SyncDataID.LIST_MODE,[MatchType.PVP]);
         if(modeDP != null && modeDP.length > 0)
         {
            this.myOptionCB_2.dataProvider = modeDP;
            this.myOptionCB_2.selectedDataIndex = 0;
         }
         this.mapDP = super.getSyncDP(SyncDataID.LIST_MAP,[MatchType.PVP]);
         this.myOptionCB_2.addEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         this.onMapChangedHandle(null);
      }
      
      private function refreshCyborgsData() : void
      {
         this._optionMap = this.myOptionCB_0;
         this._optionMap.addEventListener(CodoComboBox.EVT_CHANGE,this.onMapChangedHandle);
         this.mapDP = super.getSyncDP(SyncDataID.LIST_MAP,[MatchType.ZM]);
      }
      
      private function set mapDP(mapDP:Array) : void
      {
         if(mapDP != null && mapDP.length > 0)
         {
            this._optionMap.dataProvider = mapDP;
            this._optionMap.selectedDataIndex = 0;
         }
      }
      
      private function onMapAlbumChangeHandle(cEvt:ParaEvent) : void
      {
         this._optionMap.selectedDataIndex = cEvt.myPara.index;
      }
      
      private function getRoomCreatorParams() : Object
      {
         var room:Object = {};
         var local:Boolean = this.localCreateOption.visible && this.localCreateOption.selected;
         CFunctionContainerCommon.setdvar("ui_p2p_create",local ? "1" : "0");
         room.name = this.myRoomNameTF.text;
         room.password = this.myPasswordTF.text;
         room.map = this._optionMap.selectedValue.id;
         room.mode = this._currentMatchType == QuickMatchType.CYBORG ? "zm" : (this._currentMatchType == QuickMatchType.COOP ? "coop" : this.myOptionCB_2.selectedValue.id);
         room.difficulty = this._currentMatchType == QuickMatchType.CYBORG ? 0 : (this._currentMatchType == QuickMatchType.COOP ? this.myOptionCB_1.selectedValue.id : this.myOptionCB_0.selectedValue.id);
         room.minPlayer = local ? 1 : this._optionMap.selectedValue.minPlayer;
         room.maxPlayer = this._currentMatchType == QuickMatchType.COOP || this._currentMatchType == QuickMatchType.CYBORG ? this._optionMap.selectedValue.maxPlayer : this.myOptionCB_1.selectedValue.id;
         return {
            "type":QuickMatchType.MP,
            "room":room
         };
      }
   }
}

