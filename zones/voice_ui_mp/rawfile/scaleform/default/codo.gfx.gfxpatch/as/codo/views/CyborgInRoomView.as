package codo.views
{
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.LocalizationRoom;
   import codo.constants.SyncDataID;
   import codo.objects.RoomInfoObj;
   import components.BaseButton;
   import components.CodoGrowButton;
   import components.LevelChooser;
   import flash.text.TextField;
   
   public class CyborgInRoomView extends InRoomBaseView
   {
      private static const POS_X_SUM_TOP:int = 926;
      
      private static const POS_Y_SUM_TOP:int = 25;
      
      private static const POS_X_SUM_BOTTOM:int = 888;
      
      private static const POS_Y_SUM_BOTTOM:int = 887;
      
      public var myDifficultySelector:LevelChooser;
      
      public var mySummaryBtn:CodoGrowButton;
      
      public var myRoomIDTF:TextField;
      
      public var myRoomNameTF:TextField;
      
      public var myDifficultyTF:TextField;
      
      public function CyborgInRoomView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         super.onInit();
         this.mySummaryBtn.label = LocalizationRoom.LOBBY_SUMMARY;
         this.myDifficultySelector.labels = LocalizationRoom.ROOM_LEVELS.concat();
         this.myDifficultyTF.text = this.myRoomIDTF.text = this.myRoomNameTF.text = "";
         this.mySummaryBtn.visible = false;
         if(this.getSyncDP(SyncDataID.IS_MATCH_BUTTON_POS_ENABLED) == true)
         {
            this.mySummaryBtn.x = POS_X_SUM_TOP;
            this.mySummaryBtn.y = POS_Y_SUM_TOP;
         }
         else
         {
            this.mySummaryBtn.x = POS_X_SUM_BOTTOM;
            this.mySummaryBtn.y = POS_Y_SUM_BOTTOM;
         }
      }
      
      override protected function onInitListener() : void
      {
         super.onInitListener();
         this.myDifficultySelector.addEventListener(LevelChooser.EVT_CHANGED,this.onDifficultyChangedHandle);
         this.mySummaryBtn.addEventListener(BaseButton.EVT_CLICK,this.onSummaryClick);
         this.listen(BCEvent.EVT_ENABLE_PREV_MATCH_SUMMARY,this.onEnablePrevMatchSummary);
         this.listen(BCEvent.EVT_LEAVE_ROOM,this.onLeaveRoom);
      }
      
      override protected function getSelectedDifficulty() : *
      {
         return this._roomObj.forces.difficulties[this.myDifficultySelector.selectedIndex].id;
      }
      
      override protected function refreshRoomInfo() : void
      {
         if(this._roomObj == null)
         {
            return;
         }
         super.refreshRoomInfo();
         var roomInfo:RoomInfoObj = this._roomObj.forces;
         this.myDifficultySelector.diffData = roomInfo.difficulties;
         this.myDifficultySelector.visible = roomInfo.showDifficulty;
         if(this.myDifficultySelector.visible)
         {
            this.myDifficultySelector.selectedIndex = roomInfo.selectedDifficultyIndex;
         }
         var idxDifficulty:int = 0;
         var i:int = 0;
         while(i < LocalizationRoom.ROOM_LEVELS.length)
         {
            if(roomInfo.difficulties != null && roomInfo.difficulties[roomInfo.selectedDifficultyIndex] != null && int(roomInfo.difficulties[roomInfo.selectedDifficultyIndex].id) == i)
            {
               idxDifficulty = i;
               break;
            }
            i++;
         }
         this.myDifficultyTF.text = LocalizationRoom.ROOM_LEVELS[idxDifficulty];
         this.myRoomIDTF.text = String(this._roomObj.index);
         this.myRoomNameTF.text = this._roomObj.name;
      }
      
      private function onSummaryClick(event:ParaEvent) : void
      {
         this.broadcast(BCEvent.EVT_LOBBY_SUMMARY);
      }
      
      private function onEnablePrevMatchSummary(cEvt:ParaEvent) : void
      {
         if(cEvt.myPara.isCyborg == true)
         {
            this.mySummaryBtn.visible = true;
         }
      }
      
      private function onLeaveRoom(cEvt:ParaEvent) : void
      {
         this.mySummaryBtn.visible = false;
      }
      
      override protected function onRoomLeftHandle(cEvt:ParaEvent) : void
      {
         super.onRoomLeftHandle(cEvt);
         this.mySummaryBtn.visible = false;
      }
   }
}

