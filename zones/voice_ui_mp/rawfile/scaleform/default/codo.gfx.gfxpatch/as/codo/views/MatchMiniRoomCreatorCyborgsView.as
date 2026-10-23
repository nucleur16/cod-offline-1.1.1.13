package codo.views
{
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.LocalizationRoom;
   import codo.constants.MatchType;
   import codo.constants.QuickMatchType;
   import codo.constants.SyncDataID;
   import components.LevelChooser;
   
   public class MatchMiniRoomCreatorCyborgsView extends MatchMiniRoomCreatorView
   {
      public var myDifficultySelector:LevelChooser;
      
      public function MatchMiniRoomCreatorCyborgsView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         super.onInit();
         this.myDifficultySelector.labels = LocalizationRoom.ROOM_LEVELS.concat();
         this.myDifficultySelector.selectedIndex = 0;
         this.myFlipBtn.visible = false;
      }
      
      override protected function onMapChangedHandle(cEvt:ParaEvent) : void
      {
         var selectedIndex:int = 0;
         if(this._currentMatchType == QuickMatchType.CYBORG)
         {
            selectedIndex = this.myOptionCB_1.selectedDataIndex;
            this.myDifficultySelector.diffData = this.getSyncDP(SyncDataID.LIST_DIFFICULTY,[this._optionMap.selectedValue]);
            this.myDifficultySelector.selectedIndex = selectedIndex >= 0 && selectedIndex < this.myDifficultySelector.diffData.length ? uint(selectedIndex) : 0;
            this.myDifficultySelector.enabled = this.myDifficultySelector.diffData.length > 1;
         }
         this.showMapInfo(this._optionMap.selectedValue);
      }
      
      override protected function onCreateRoomBtnClickHandle(cEvt:ParaEvent) : void
      {
         var room:Object = {};
         room.name = this.myRoomNameTF.text;
         room.password = this.myPasswordTF.text;
         room.map = this._optionMap.selectedValue.id;
         room.mode = MatchType.ZM;
         this.getSyncDP(SyncDataID.LIST_DIFFICULTY,[this._optionMap.selectedValue]);
         room.difficulty = int(this.myDifficultySelector.diffData[this.myDifficultySelector.selectedIndex].id);
         room.minPlayer = this._optionMap.selectedValue.minPlayer;
         room.maxPlayer = this._optionMap.selectedValue.maxPlayer;
         this.broadcast(BCEvent.EVT_CREATE_ROOM,room);
      }
   }
}

