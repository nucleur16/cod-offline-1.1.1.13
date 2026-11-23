package codo.views
{
   // FFDec's decompile of C1's CyborgMatchRoomListView (codo.gfx), with the k33 offline change marked "k33".
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.LocalizationRoom;
   import codo.constants.RoomListSortType;
   
   public class CyborgMatchRoomListView extends MatchRoomListView
   {
      
      public function CyborgMatchRoomListView()
      {
         super();
      }
      
      // k33 offline: there are no other players' rooms to list, so the Zombies room list (its column headers, "No
      // Rooms", Refresh) is never shown. Without it nothing is ever selected for the Join Room button, which
      // MainMenuController hides on this page. The list's data side (enableData / disableData) is as before.
      override protected function showView() : void
      {
         this.visible = false;
         this.mouseEnabled = false;
         this.mouseChildren = false;
         this.tabChildren = false;
      }
      
      override protected function get sortBtnCount() : int
      {
         return 7;
      }
      
      override protected function initHeader() : void
      {
         this[SORTING_BTN + 0].label = LocalizationRoom.LIST_LABEL_INDEX;
         this[SORTING_BTN + 1].label = LocalizationRoom.LIST_LABEL_NAME;
         this[SORTING_BTN + 2].label = LocalizationRoom.LIST_LABEL_MAP;
         this[SORTING_BTN + 3].label = LocalizationRoom.ROOM_LEVEL;
         this[SORTING_BTN + 4].label = LocalizationRoom.LIST_LABEL_PLAYER;
         this[SORTING_BTN + 5].label = LocalizationRoom.LIST_LABEL_STATE;
         this[SORTING_BTN + 6].label = "";
      }
      
      override protected function onSortingBtnClickHandle(cEvt:ParaEvent) : void
      {
         var sortingType:uint = RoomListSortType.NONE;
         switch(cEvt.target)
         {
            case this[SORTING_BTN + 0]:
               sortingType = RoomListSortType.NUMBER;
               break;
            case this[SORTING_BTN + 1]:
               sortingType = RoomListSortType.NAME;
               break;
            case this[SORTING_BTN + 2]:
               sortingType = RoomListSortType.MAP;
               break;
            case this[SORTING_BTN + 3]:
               sortingType = RoomListSortType.MODE;
               break;
            case this[SORTING_BTN + 4]:
               sortingType = RoomListSortType.PLAYER;
               break;
            case this[SORTING_BTN + 5]:
               sortingType = RoomListSortType.STATUS;
               break;
            case this[SORTING_BTN + 6]:
               sortingType = RoomListSortType.PASSWORD;
         }
         this.broadcast(BCEvent.MATCH_ROOM_LIST_SORTING_TYPE_CHANGED,sortingType);
      }
   }
}

