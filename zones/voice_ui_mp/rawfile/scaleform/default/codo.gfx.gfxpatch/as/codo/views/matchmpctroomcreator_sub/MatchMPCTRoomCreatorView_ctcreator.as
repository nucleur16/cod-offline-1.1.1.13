package codo.views.matchmpctroomcreator_sub
{
   import codo.common.BaseSubElement;
   import codo.common.ParaEvent;
   import codo.constants.CombatType;
   import codo.constants.LocalizationRoom;
   import codo.constants.MatchType;
   import codo.constants.QuickMatchType;
   import codo.constants.RoomListType;
   import codo.constants.SyncDataID;
   import codo.objects.QuickMatchFilterObj;
   import components.Album;
   import components.CodoComboBox;
   import components.LevelChooser;
   import components.VolumeChart;
   import flash.display.Sprite;
   import flash.text.TextField;

   public class MatchMPCTRoomCreatorView_ctcreator extends BaseSubElement
   {
      public var myLevelSelector:LevelChooser;

      public var myGPBar:VolumeChart;

      public var myExpBar:VolumeChart;

      public var myExpRateLabelTF:TextField;

      public var myGPRateLabelTF:TextField;

      public var myExpRate:TextField;

      public var myGPRate:TextField;

      public var myMapTF:TextField;

      public var myOptionCB_1:CodoComboBox;

      public var myOptionLabelTF_1:TextField;

      public var myOptionBg_1:Sprite;

      private var _mapAlbum:Album;

      private var _difficultiesDat:Array;

      public function MatchMPCTRoomCreatorView_ctcreator()
      {
         super();
      }

      override protected function onInit() : void
      {
         this.myOptionLabelTF_1.text = LocalizationRoom.LIST_LABEL_MAP;
         this.myExpRateLabelTF.text = LocalizationRoom.GAIN_XP_RATE;
         this.myGPRateLabelTF.text = LocalizationRoom.GAIN_GP_RATE;
         this.myLevelSelector.labels = LocalizationRoom.ROOM_LEVELS;
      }

      override protected function onInitListener() : void
      {
         this.myOptionCB_1.visible = this.myOptionLabelTF_1.visible = this.myOptionBg_1.visible = false;
         this.myOptionCB_1.addEventListener(CodoComboBox.EVT_CHANGE,this.onMapCBChangedHandle);
         this.myLevelSelector.addEventListener(LevelChooser.EVT_CHANGED,this.onDifficultyChangedHandle);
      }

      override public function enable() : void
      {
         this.addSyncDP(SyncDataID.MATCH_MPCT_ROOMCREATOR_CT_PARAMS,this.getRoomCreatorParams);
         this.setMaps();
         this.setDifficulties();
      }

      public function set active(cSwi:Boolean) : void
      {
         if(cSwi == true)
         {
            this.alpha = 1;
            this.mouseEnabled = true;
            this.mouseChildren = true;
            if(this._mapAlbum != null)
            {
               this._mapAlbum.data = this.myOptionCB_1.dataProvider;
               this._mapAlbum.currentIndex = this.myOptionCB_1.selectedDataIndex;
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

      private function setMaps() : void
      {
         var tempMapDat:Array = null;
         if(this.myOptionCB_1.dataProvider.length <= 0)
         {
            tempMapDat = this.getSyncDP(SyncDataID.LIST_MAP,[MatchType.PVP,false,0,false,"combat"]);
            this.myOptionCB_1.dataProvider = tempMapDat;
            if(this.mouseEnabled && this._mapAlbum != null)
            {
               this._mapAlbum.data = tempMapDat;
               this._mapAlbum.currentIndex = this.myOptionCB_1.selectedDataIndex;
            }
         }
      }

      private function setDifficulties() : void
      {
         if(this._difficultiesDat == null)
         {
            this._difficultiesDat = this.getSyncDP(SyncDataID.LIST_DIFFICULTY_CT_CREATOR);
            this.myLevelSelector.selectedIndex = 1;
            this.refreshEXPAndGPRate();
         }
      }

      private function refreshEXPAndGPRate() : void
      {
         var rate:Number = NaN;
         var idxDiff:uint = this.myLevelSelector.selectedIndex;
         rate = this.getSyncDP(SyncDataID.XP_LEAGUE_FACTOR,[CombatType.CT,this._difficultiesDat[idxDiff].id]);
         this.myExpRate.text = String(rate * 100) + "%";
         this.myExpBar.rate = rate;
         rate = this.getSyncDP(SyncDataID.GP_LEAGUE_FACTOR,[CombatType.CT,this._difficultiesDat[idxDiff].id]);
         this.myGPRate.text = String(rate * 100) + "%";
         this.myGPBar.rate = rate;
      }

      private function onMapCBChangedHandle(cEvt:ParaEvent) : void
      {
         this.myMapTF.text = this.myOptionCB_1.selectedValue != null ? this.myOptionCB_1.selectedValue.description : "";
         if(this.mouseEnabled && this._mapAlbum != null)
         {
            this._mapAlbum.currentIndex = this.myOptionCB_1.selectedDataIndex;
         }
      }

      private function onDifficultyChangedHandle(cEvt:ParaEvent) : void
      {
         this.refreshEXPAndGPRate();
      }

      private function onMapAlbumChangeHandle(cEvt:ParaEvent) : void
      {
         if(!this.mouseEnabled || cEvt.myPara.index < 0 || cEvt.myPara.index >= this.myOptionCB_1.dataProvider.length)
         {
            return;
         }
         this.myOptionCB_1.selectedDataIndex = cEvt.myPara.index;
      }

      private function getRoomCreatorParams() : Object
      {
         var filter:QuickMatchFilterObj = new QuickMatchFilterObj();
         filter.type = RoomListType.MP;
         filter.mode = "combat";
         filter.league = this._difficultiesDat[this.myLevelSelector.selectedIndex].id;
         filter.map = this.myOptionCB_1.selectedValue.id;
         filter.minPlayer = this.myOptionCB_1.selectedValue.minPlayer;
         filter.maxPlayer = this.myOptionCB_1.selectedValue.maxPlayer;
         return {
            "type":QuickMatchType.TRAINING,
            "filter":filter
         };
      }
   }
}
