package codo.views
{
   import codo.common.ParaEvent;
   import codo.common.CFunctionContainerCommon;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationRoom;
   import codo.constants.MatchType;
   import codo.constants.QuickMatchType;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import components.LevelChooser;
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.display.InteractiveObject;
   
   public class MatchMiniRoomCreatorCyborgsView extends MatchMiniRoomCreatorView
   {
      public var myDifficultySelector:LevelChooser;
      
      // k33 offline: with no room list beside it (CyborgMatchRoomListView stays hidden) the panel opens in the
      // middle of the screen, where the multiplayer Create Room panel is, instead of at the left edge.
      private static const K33_POS_X_OPEN:int = GameInfo.PANEL_MiDLEFT_ALIGN;
      
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
         this.k33OfflineLayout();
      }
      
      override protected function showView() : void
      {
         this.tweenShow(TweenParam.SPD_ELEMENT_MOVE,{
            "x":K33_POS_X_OPEN,
            "alpha":1
         });
      }
      
      // k33 offline: the Room Name and Room Password rows are hidden and the rows under them (Map, the difficulty)
      // move up into their place; the Create Room button stays where it is. The room is still named after the
      // profile (DEFAULT_ROOM_NAME, set in enableData) and has no password.
      private function k33OfflineLayout() : void
      {
         var shift:Number = this.myOptionLabelTF_0.y - this.myRoomNameLabelTF.y;
         var top:Number = this.myOptionBg_0.y - 1;
         var from:Number = top - shift - 1;
         var bottom:Number = this.myCreateBtn.y - 1;
         var i:int = 0;
         while(i < this.numChildren)
         {
            var child:DisplayObject = this.getChildAt(i);
            if(child.y >= from && child.y < top)
            {
               child.visible = false;
               if(child is InteractiveObject)
               {
                  InteractiveObject(child).mouseEnabled = false;
                  InteractiveObject(child).tabEnabled = false;
               }
               if(child is DisplayObjectContainer)
               {
                  DisplayObjectContainer(child).mouseChildren = false;
                  DisplayObjectContainer(child).tabChildren = false;
               }
            }
            else if(child.y >= top && child.y < bottom)
            {
               child.y -= shift;
            }
            i++;
         }
      }
      
      override protected function onMapChangedHandle(cEvt:ParaEvent) : void
      {
         var selectedIndex:int = 0;
         if(_currentMatchType == QuickMatchType.CYBORG)
         {
            selectedIndex = this.myOptionCB_1.selectedDataIndex;
            this.myDifficultySelector.diffData = super.getSyncDP(SyncDataID.LIST_DIFFICULTY,[_optionMap.selectedValue]);
            this.myDifficultySelector.selectedIndex = selectedIndex >= 0 && selectedIndex < this.myDifficultySelector.diffData.length ? uint(selectedIndex) : 0;
            this.myDifficultySelector.enabled = this.myDifficultySelector.diffData.length > 1;
         }
         this.showMapInfo(_optionMap.selectedValue);
      }
      
      override protected function onCreateRoomBtnClickHandle(cEvt:ParaEvent) : void
      {
         var room:Object = {};
         room.name = this.myRoomNameTF.text;
         room.password = this.myPasswordTF.text;
         room.map = _optionMap.selectedValue.id;
         room.mode = MatchType.ZM;
         super.getSyncDP(SyncDataID.LIST_DIFFICULTY,[_optionMap.selectedValue]);
         room.difficulty = int(this.myDifficultySelector.diffData[this.myDifficultySelector.selectedIndex].id);
         room.minPlayer = CFunctionContainerCommon.Dvar_GetString("ui_p2p_create") == "1" ? 1 : _optionMap.selectedValue.minPlayer;
         room.maxPlayer = _optionMap.selectedValue.maxPlayer;
         this.broadcast(BCEvent.EVT_CREATE_ROOM,room);
      }
   }
}

