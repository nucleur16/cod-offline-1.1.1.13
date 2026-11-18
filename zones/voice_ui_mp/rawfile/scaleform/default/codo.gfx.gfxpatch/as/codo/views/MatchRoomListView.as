package codo.views
{
   // the classic build's English MatchRoomListView (FFDec's decompile of its codo.gfx tag, with its theatre list mode) with
   // every string shown as a localize key (K33UI_THEATRE_*, K33UI_PREVIOUS / _NEXT, C1's SF_UI_* where C1 has the text):
   // English in codol_english_ui, Chinese in voice_ui_mp. The theatre has no backend in this project: the list stays empty.
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ListEvent;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationRoom;
   import codo.constants.QuickMatchType;
   import codo.constants.RoomListSortType;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import components.BaseButton;
   import components.CodoGrowButtonNoTint;
   import components.CodoList;
   import flash.display.Sprite;
   import flash.utils.getDefinitionByName;
   import flash.utils.getQualifiedClassName;
   
   public class MatchRoomListView extends BaseView
   {
      
      protected static const POS_X_ORIGIN:int = GameInfo.PANEL_RIGHT_ALIGN;
      
      protected static const POS_Y_ORIGIN:int = GameInfo.PANEL_TOP_ALIGN + 10;
      
      protected static const SORTING_BTN:String = "myRoomLabelBtn_";
      
      public var myHeader:CodoGrowButtonNoTint;
      
      public var myRefreshIcon:Sprite;
      
      public var myRefreshBtn:CodoGrowButtonNoTint;
      
      public var myList:CodoList;
      
      private var _currentRoomObj:Object;
      
      private var _enableRefresh:Boolean = true;
      
      public var theatreOnly:Boolean = false;
      
      private var theatrePrevious:CodoGrowButtonNoTint;
      
      private var theatreNext:CodoGrowButtonNoTint;
      
      private var theatreModes:CodoGrowButtonNoTint;
      
      public function MatchRoomListView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         if(this.theatreOnly)
         {
            this.myHeader.label = "@K33UI_THEATRE_MY_MATCHES";
            this.myHeader.enable = false;
            this.myRefreshBtn.label = "@SF_UI_REFRESH";
            this.myRefreshBtn.icon = this.myRefreshIcon;
            this.initHeader();
            this.myList.doubleClickEnabled = true;
            this.myList.noDataLabel = "@K33UI_THEATRE_EMPTY";
            this.x = POS_X_ORIGIN;
            this.y = POS_Y_ORIGIN;
            this.theatreInitPager();
            this.tweenHide(0,{
               "z":TweenParam.Z_GENERAL,
               "blurFilter":TweenParam.BLUR_GENERAL
            });
            return;
         }
         this.myHeader.label = LocalizationRoom.ROOM_LIST;
         this.myRefreshBtn.label = LocalizationRoom.REFRESH;
         this.myRefreshBtn.icon = this.myRefreshIcon;
         this.initHeader();
         this.myList.doubleClickEnabled = true;
         this.myList.noDataLabel = LocalizationRoom.NO_ROOM;
         this.myHeader.enable = false;
         this.x = POS_X_ORIGIN;
         this.y = POS_Y_ORIGIN;
         this.tweenHide(0,{
            "z":TweenParam.Z_GENERAL,
            "blurFilter":TweenParam.BLUR_GENERAL
         });
      }
      
      protected function initHeader() : void
      {
         if(this.theatreOnly)
         {
            var labels:Array = ["#","@K33UI_THEATRE_RECORDED","@SF_UI_LIST_LABEL_MAP","@SF_UI_LIST_LABEL_MODE","@K33UI_THEATRE_LENGTH","","@K33UI_THEATRE_WATCH",""];
            var column:int = 0;
            while(column < this.sortBtnCount)
            {
               this[SORTING_BTN + column].label = labels[column];
               this[SORTING_BTN + column].enable = false;
               column++;
            }
            return;
         }
         this[SORTING_BTN + 0].label = LocalizationRoom.LIST_LABEL_INDEX;
         this[SORTING_BTN + 1].label = LocalizationRoom.LIST_LABEL_NAME;
         this[SORTING_BTN + 2].label = LocalizationRoom.LIST_LABEL_MAP;
         this[SORTING_BTN + 5].label = LocalizationRoom.LIST_LABEL_PLAYER;
         this[SORTING_BTN + 6].label = LocalizationRoom.LIST_LABEL_STATE;
         this[SORTING_BTN + 7].label = "";
      }
      
      override protected function showView() : void
      {
         if(this.theatreOnly)
         {
            this.tweenShow(TweenParam.SPD_FADE,{
               "delay":0.3,
               "z":0,
               "autoAlpha":1,
               "blurFilter":TweenParam.BLUR_NONE
            },null,null,{
               "autoAlpha":0,
               "z":TweenParam.Z_GENERAL_TOP,
               "blurFilter":TweenParam.BLUR_GENERAL
            });
            return;
         }
         this.broadcast(BCEvent.EVT_MPCT_ROOMLIST_SELECTED,{"selected":false});
         this.addSyncDP(SyncDataID.MATCH_ROOMLIST_SELECTED_ROOM,this.getCurrentSelectedRoom);
         this.tweenShow(TweenParam.SPD_FADE,{
            "delay":0.3,
            "z":0,
            "autoAlpha":1,
            "blurFilter":TweenParam.BLUR_NONE
         },null,null,{
            "autoAlpha":0,
            "z":TweenParam.Z_GENERAL_TOP,
            "blurFilter":TweenParam.BLUR_GENERAL
         });
      }
      
      override protected function hideView() : void
      {
         if(this.theatreOnly)
         {
            this.tweenHide(TweenParam.SPD_FADE,{
               "autoAlpha":0,
               "z":TweenParam.Z_GENERAL,
               "blurFilter":TweenParam.BLUR_GENERAL
            });
            return;
         }
         this.broadcast(BCEvent.EVT_MPCT_ROOMLIST_SELECTED,{"selected":true});
         this.tweenHide(TweenParam.SPD_FADE,{
            "autoAlpha":0,
            "z":TweenParam.Z_GENERAL,
            "blurFilter":TweenParam.BLUR_GENERAL
         });
         this.setBindData(AsyncTaskID.MATCH_MPCT_ENABLE_START_BUTTON,{"enable":true});
      }
      
      protected function get sortBtnCount() : int
      {
         return 8;
      }
      
      override protected function onInitListener() : void
      {
         if(this.theatreOnly)
         {
            this.myRefreshBtn.addEventListener(BaseButton.EVT_CLICK,this.onRefreshBtnClickHandle);
            this.myList.addEventListener(CodoList.EVT_LINE_DOUBLE_CLICK,this.onRoomDoubleClickHandle);
            return;
         }
         this.myRefreshBtn.addEventListener(BaseButton.EVT_CLICK,this.onRefreshBtnClickHandle);
         this.myList.addEventListener(CodoList.EVT_LINE_DOUBLE_CLICK,this.onRoomDoubleClickHandle);
         this.myList.addEventListener(CodoList.EVT_LINE_CLICK,this.onRoomListSelectedChangedHandle);
         var i:int = 0;
         while(i < this.sortBtnCount)
         {
            this[SORTING_BTN + i].addEventListener(BaseButton.EVT_CLICK,this.onSortingBtnClickHandle);
            i++;
         }
         this.listen(BCEvent.EVT_ON_ROOM_JOINED,this.onRoomJoinedHandle);
         this.listen(BCEvent.EVT_ON_ROOM_LISTED,this.onRoomListedHandle);
      }
      
      override protected function enableData() : void
      {
         if(this.theatreOnly)
         {
            return;
         }
         this.myList.cleanUp();
         this.enableListRoom(true);
         var quickMatchType:uint = this.getSyncDP(SyncDataID.CURRENT_MATCH_TYPE);
         if(quickMatchType == QuickMatchType.COOP)
         {
            this[SORTING_BTN + 3].label = LocalizationRoom.MAP_TYPE;
            this[SORTING_BTN + 4].label = LocalizationRoom.ROOM_LEVEL;
         }
         else if(quickMatchType == QuickMatchType.MP)
         {
            this[SORTING_BTN + 3].label = LocalizationRoom.LIST_LABEL_MODE;
            this[SORTING_BTN + 4].label = LocalizationRoom.LIST_LABEL_LEAGUE;
         }
      }
      
      override protected function disableData() : void
      {
         if(this.theatreOnly)
         {
            this.myList.cleanUp();
            return;
         }
         this.enableListRoom(false);
         this.myList.cleanUp();
      }
      
      private function onRefreshBtnClickHandle(cEvt:ParaEvent) : void
      {
         if(this.theatreOnly)
         {
            CFunctionContainerCommon.setdvar("ui_theatreRequest","refresh");
            return;
         }
         if(this._enableRefresh == true)
         {
            this.myList.cleanUp();
            this.enableListRoom(true);
            this.listRoom();
            this._enableRefresh = false;
         }
      }
      
      private function listRoom() : void
      {
         this.broadcast(BCEvent.EVT_LIST_ROOM,{
            "auto":false,
            "mode":0
         });
      }
      
      private function joinRoom(cRoom:Object = null) : void
      {
         if(this.theatreOnly)
         {
            return;
         }
         if(cRoom == null && this.myList.selectedIndex == -1)
         {
            return;
         }
         this.enableListRoom(false);
         this._currentRoomObj = cRoom != null ? cRoom : this.myList.selectedValue;
         this.sendJoinRoomRequest();
      }
      
      private function sendJoinRoomRequest() : void
      {
         if(this.theatreOnly)
         {
            return;
         }
         this.broadcast(BCEvent.EVT_TRY_JOIN_ROOM,{
            "room":this._currentRoomObj,
            "ticket":false
         });
      }
      
      private function enableListRoom(bEnable:Boolean, bImmediate:Boolean = true) : void
      {
         if(this.theatreOnly)
         {
            return;
         }
         this.broadcast(BCEvent.EVT_ENABLE_LIST_ROOM,{
            "enable":bEnable,
            "immediate":bImmediate
         });
      }
      
      private function onRoomJoinedHandle(cEvt:ParaEvent) : void
      {
         if(!cEvt.myPara.success)
         {
            this.enableListRoom(true,false);
         }
      }
      
      private function onRoomListedHandle(cEvt:ParaEvent) : void
      {
         this.myList.dataProvider = cEvt.myPara;
         this.myList.selectedIndex = 0;
         this._enableRefresh = true;
         this.refreshJoinRoomButton();
      }
      
      private function onRoomDoubleClickHandle(cEvt:ListEvent) : void
      {
         if(this.theatreOnly)
         {
            this.theatreWatch();
            return;
         }
         this.joinRoom();
      }
      
      protected function onSortingBtnClickHandle(cEvt:ParaEvent) : void
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
               sortingType = RoomListSortType.LEAGUE;
               break;
            case this[SORTING_BTN + 5]:
               sortingType = RoomListSortType.PLAYER;
               break;
            case this[SORTING_BTN + 6]:
               sortingType = RoomListSortType.STATUS;
               break;
            case this[SORTING_BTN + 7]:
               sortingType = RoomListSortType.PASSWORD;
         }
         this.broadcast(BCEvent.MATCH_ROOM_LIST_SORTING_TYPE_CHANGED,sortingType);
      }
      
      private function onRoomListSelectedChangedHandle(cEvt:ListEvent = null) : void
      {
         this.refreshJoinRoomButton();
      }
      
      private function refreshJoinRoomButton() : void
      {
         if(this.theatreOnly)
         {
            return;
         }
         var tempSelectedRoomDat:Object = this.myList.selectedValue;
         if(tempSelectedRoomDat != null)
         {
            this.broadcast(BCEvent.EVT_MPCT_ROOMLIST_SELECTED,{"selected":true});
         }
         else
         {
            this.broadcast(BCEvent.EVT_MPCT_ROOMLIST_SELECTED,{"selected":false});
         }
      }
      
      private function getCurrentSelectedRoom() : Object
      {
         return this.myList.selectedValue;
      }
      
      private function theatreInitPager() : void
      {
         var cls:Class = getDefinitionByName(getQualifiedClassName(this.myRefreshBtn)) as Class;
         this.theatrePrevious = new cls() as CodoGrowButtonNoTint;
         this.theatreNext = new cls() as CodoGrowButtonNoTint;
         var controls:Array = [this.theatrePrevious,this.theatreNext];
         var labels:Array = ["@K33UI_PREVIOUS","@K33UI_NEXT"];
         var offset:Number = this.myRefreshBtn.x + this.myRefreshBtn.width + 24;
         var i:int = 0;
         while(i < controls.length)
         {
            var button:CodoGrowButtonNoTint = controls[i] as CodoGrowButtonNoTint;
            button.label = labels[i];
            button.x = offset;
            button.y = this.myRefreshBtn.y;
            offset += button.width + 10;
            this.addChild(button);
            i++;
         }
         this.theatrePrevious.addEventListener(BaseButton.EVT_CLICK,this.theatrePreviousPage);
         this.theatreNext.addEventListener(BaseButton.EVT_CLICK,this.theatreNextPage);
      }
      
      private function theatreClose(event:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("ui_theatreClose","1");
      }
      
      private function theatrePreviousPage(event:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("ui_theatreRequest","previous");
      }
      
      private function theatreNextPage(event:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("ui_theatreRequest","next");
      }
      
      private function theatreWatch() : void
      {
         var row:Object = this.myList.selectedValue;
         if(row != null && row.theatreId != null && String(row.theatreId).length == 64 && String(row.theatreId).search(/[^a-f0-9]/) == -1)
         {
            CFunctionContainerCommon.setdvar("ui_theatreRequest","watch:" + row.theatreId);
         }
      }
      
      public function theatrePopulate(rows:Array, page:int, pages:int) : void
      {
         this.myHeader.label = "@K33UI_THEATRE_MY_MATCHES";
         this.myList.dataProvider = rows;
         this.myList.selectedIndex = rows.length > 0 ? 0 : -1;
         this.theatrePrevious.visible = this.theatreNext.visible = pages > 1;
         this.theatrePrevious.enable = page > 1;
         this.theatreNext.enable = page < pages;
      }
   }
}

