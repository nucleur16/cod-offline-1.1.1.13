package codo.views
{
   import codo.common.BaseView;
   import codo.k33.LocalMatchControls;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.ChattingChannelType;
   import codo.constants.LocalizationChatting;
   import codo.constants.LocalizationRoom;
   import codo.constants.RightClickMenuType;
   import codo.constants.SyncDataID;
   import codo.constants.ViewState;
   import codo.objects.ChatChannelObj;
   import codo.objects.RoomInfoObj;
   import codo.objects.RoomObj;
   import codo.utils.StringUtil;
   import components.BaseButton;
   import components.CodoGrowButton;
   import components.CodoGrowButtonNoTint;
   import components.List;
   import components.ScrollBar2;
   import components.room_part.MatchInfo;
   import components.room_part.RoomForce;
   import components.room_part.RoomInfoBase;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.events.FocusEvent;
   import flash.events.KeyboardEvent;
   import flash.text.TextField;
   
   public class InRoomBaseView extends BaseView
   {
      
      private static const STRING_LEN_MAX:uint = 64;
      
      public var myForce_0:RoomForce;
      
      public var myMatchInfo:MatchInfo;
      
      public var myRoomInfo:RoomInfoBase;
      
      public var myDevDSRoomList:List;
      
      public var myDevPanel:Sprite;
      
      public var myRefreshDSBtn:CodoGrowButton;
      
      public var myStartDSBtn:CodoGrowButtonNoTint;
      
      public var myStartLSBtn:CodoGrowButtonNoTint;
      
      public var myChatTitleTF:TextField;
      
      public var mySendBtn:CodoGrowButtonNoTint;
      
      public var myHistoryTF:TextField;
      
      public var myInputTF:TextField;
      
      public var myScrollBar:ScrollBar2;
      
      public var myChatBg:Sprite;
      
      private var localMatchControls:LocalMatchControls;

      protected var keysDown:Object = new Object();
      
      protected var _roomObj:RoomObj = null;
      
      protected var _channelObj:ChatChannelObj = null;
      
      protected var _isRefreshingRoomInfo:Boolean = false;
      
      protected var _scrollBarWorking:Boolean = false;
      
      public function InRoomBaseView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.localMatchControls = new LocalMatchControls();
         this.localMatchControls.x = 900;
         this.localMatchControls.y = 790;
         this.addChild(this.localMatchControls);
         this.myMatchInfo.startVisualByState();
         this.myStartDSBtn.label = LocalizationRoom.START_DS;
         this.myStartLSBtn.label = LocalizationRoom.START_LS;
         this.myHistoryTF.visible = false;
         this.mySendBtn.visible = false;
         this.myInputTF.visible = false;
         this.myChatBg.visible = false;
         this.myScrollBar.visible = false;
         this.myScrollBar.alpha = 0;
         this.myChatTitleTF.visible = false;
         this.myChatTitleTF.text = LocalizationRoom.INROOM_CHATTING;
         this.mySendBtn.label = LocalizationChatting.SEND_MESSAGE;
      }
      
      override protected function onInitListener() : void
      {
         this.myHistoryTF.addEventListener(Event.SCROLL,this.onHistoryScrollHandle);
         this.myInputTF.addEventListener(FocusEvent.FOCUS_IN,this.onInputFocusInHandle);
         this.myInputTF.addEventListener(FocusEvent.FOCUS_OUT,this.onInputFocusOutHandle);
         this.myInputTF.addEventListener(Event.CHANGE,this.onInputTFChangedHandle);
         this.myScrollBar.addEventListener(Event.SCROLL,this.onScrollBarScrolledHandle);
         this.mySendBtn.addEventListener(BaseButton.EVT_CLICK,this.onSendClickedHandle);
         this.myStartDSBtn.addEventListener(BaseButton.EVT_CLICK,this.onStartDSClickHandle);
         this.myStartLSBtn.addEventListener(BaseButton.EVT_CLICK,this.onStartLSClickHandle);
         this.myDevDSRoomList.addEventListener(List.LINE_DOUBLE_CLICK,this.onDedicatedServerListDoubleClickHandle);
         this.myRefreshDSBtn.addEventListener(BaseButton.EVT_CLICK,this.onRefreshDSBtnClickHandle);
         this.myRoomInfo.addEventListener(RoomInfoBase.EVT_MAP_CHANGED,this.onMapChangedHandle);
         this.myForce_0.myForceList.addEventListener(List.LINE_RIGHT_CLICK,this.onRightClickHandle);
         this.listen(BCEvent.ROOM_INFO,this.onRoomInfoHandle);
         this.listen(BCEvent.ROOM_FORCES,this.onRoomForcesHandle);
         this.listen(BCEvent.ROOM_STATE_CHANGED,this.onRoomStateChangedHandle);
         this.listen(BCEvent.EVT_ON_ROOM_LEFT,this.onRoomLeftHandle);
         this.listen(BCEvent.EVT_ON_DEDICATED_SERVER_LISTED,this.onDedicatedServerListedHandle);
         this.listen(BCEvent.EVT_ON_CHAT_CHANNEL_ADDED,this.onChatChannelAddedHandle);
      }
      
      override protected function enableData() : void
      {
         if(this._roomObj == null)
         {
            this._roomObj = this.getSyncDP(SyncDataID.ROOM_INFO);
         }
         this.refreshRoomInfo();
         this.broadcast(BCEvent.EVT_CONTROLLER_CHECK_AVATAR_REFRESH_IN_TEAM);
      }
      
      protected function onStartDSClickHandle(cEvt:Event) : void
      {
         var selectedIndex:int = this.myDevDSRoomList.selectedDataIndex;
         if(selectedIndex >= 0)
         {
            this.startMatch({
               "useDS":true,
               "index":selectedIndex
            });
         }
      }
      
      protected function onStartLSClickHandle(cEvt:Event) : void
      {
         this.startMatch({
            "useDS":false,
            "index":-1
         });
      }
      
      protected function onDedicatedServerListDoubleClickHandle(cEvt:ParaEvent) : void
      {
         this.startMatch({
            "useDS":true,
            "index":this.myDevDSRoomList.selectedDataIndex
         });
      }
      
      protected function startMatch(dsObj:Object) : void
      {
         this.broadcast(BCEvent.START_MATCH,dsObj);
      }
      
      protected function onRoomLeftHandle(cEvt:ParaEvent) : void
      {
         this._roomObj = null;
         if(this.localMatchControls != null) this.localMatchControls.setRoom(null);
         this._channelObj = null;
         this.myHistoryTF.htmlText = "";
         this.myScrollBar.visible = false;
         this.myInputTF.text = "";
         this.myRoomInfo.myMapAlbum.reset();
      }
      
      protected function listDedicatedServer(bAuto:Boolean) : void
      {
         this.broadcast(BCEvent.EVT_LIST_DEDICATED_SERVER,bAuto);
      }
      
      protected function onRefreshDSBtnClickHandle(cEvt:Event) : void
      {
         this.listDedicatedServer(false);
      }
      
      protected function onRoomInfoHandle(cEvt:ParaEvent) : void
      {
         this._roomObj = cEvt.myPara;
         if(this.state == ViewState.ACTIVATED)
         {
            this.refreshRoomInfo();
         }
      }
      
      protected function onRoomForcesHandle(cEvt:ParaEvent) : void
      {
         if(this.localMatchControls != null) this.localMatchControls.setRoom(this._roomObj);
         var isForce0:Boolean = cEvt.myPara.team == 0;
         if(isForce0)
         {
            this.myForce_0.playerDP = {
               "dp":cEvt.myPara.forces.force0,
               "maxPlayer":cEvt.myPara.forces.forceMaxPlayer
            };
         }
      }
      
      protected function onRoomStateChangedHandle(cEvt:ParaEvent) : void
      {
         if(this.localMatchControls != null) this.localMatchControls.setRoom(this._roomObj);
         this.myRoomInfo.myMapAlbum.controllable = cEvt.myPara.forces.enableChangeMapBtn;
      }
      
      protected function onDedicatedServerListedHandle(cEvt:ParaEvent) : void
      {
         this.myDevDSRoomList.dataProvider = cEvt.myPara;
         this.myDevDSRoomList.selectedDataIndex = cEvt.myPara.length > 0 ? 0 : -1;
      }
      
      protected function onChatChannelAddedHandle(cEvt:ParaEvent) : void
      {
         if(this._channelObj == null && cEvt.myPara != null && cEvt.myPara.id == ChattingChannelType.ROOM)
         {
            this._channelObj = cEvt.myPara;
         }
      }
      
      protected function onMapChangedHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.CHANGE_ROOM_INFO,{
            "type":"map",
            "data":cEvt.myPara.data
         });
      }
      
      protected function onRightClickHandle(cEvt:ParaEvent) : void
      {
         this.showRightClickMenu(RightClickMenuType.IN_ROOM,[cEvt.myPara.targetLine.getData()]);
      }
      
      protected function onDifficultyChangedHandle(cEvt:ParaEvent) : void
      {
         if(!this._isRefreshingRoomInfo)
         {
            this.broadcast(BCEvent.CHANGE_ROOM_INFO,{
               "type":"difficulty",
               "data":this.getSelectedDifficulty()
            });
         }
      }
      
      protected function getSelectedDifficulty() : *
      {
         return 0;
      }
      
      protected function onInputFocusInHandle(cEvt:FocusEvent) : void
      {
         this.myInputTF.addEventListener(KeyboardEvent.KEY_DOWN,this.onKeyDownHandle);
         this.myInputTF.addEventListener(KeyboardEvent.KEY_UP,this.onKeyUpHandle);
      }
      
      protected function onInputFocusOutHandle(cEvt:FocusEvent) : void
      {
         this.myInputTF.removeEventListener(KeyboardEvent.KEY_DOWN,this.onKeyDownHandle);
      }
      
      protected function onInputTFChangedHandle(cEvt:Event) : void
      {
         if(this.myInputTF.text.length > 0)
         {
            this.myInputTF.text = StringUtil.limitStringToRealLength(this.myInputTF.text,STRING_LEN_MAX);
         }
      }
      
      protected function onKeyDownHandle(cEvt:KeyboardEvent) : void
      {
         if(this.isDown(cEvt.keyCode) == false)
         {
            switch(cEvt.keyCode)
            {
               case 13:
               case 108:
                  this.sendMsg();
            }
         }
         this.keysDown[cEvt.keyCode] = true;
      }
      
      protected function isDown(keyCode:uint) : Boolean
      {
         return Boolean(keyCode in this.keysDown);
      }
      
      protected function onKeyUpHandle(cEvt:KeyboardEvent) : void
      {
         if(cEvt.keyCode in this.keysDown)
         {
            delete this.keysDown[cEvt.keyCode];
         }
      }
      
      protected function onScrollBarScrolledHandle(cEvt:Event) : void
      {
         var targetScrollV:Number = Math.floor(this.myScrollBar.curPos * this.myHistoryTF.numLines) + 1;
         if(this.myHistoryTF.scrollV != targetScrollV)
         {
            this._scrollBarWorking = true;
            this.myHistoryTF.scrollV = targetScrollV;
         }
      }
      
      protected function onSendClickedHandle(cEvt:Event) : void
      {
         this.sendMsg();
      }
      
      protected function onHistoryScrollHandle(cEvt:Event) : void
      {
         if(this._scrollBarWorking == false)
         {
            this.myScrollBar.tweenToTargetPos(this.myHistoryTF.scrollV == 1 ? 0 : this.myHistoryTF.scrollV / this.myHistoryTF.numLines,0);
         }
         else
         {
            this._scrollBarWorking = false;
         }
      }
      
      protected function showDedicatedServerPanel(bShow:Boolean) : void
      {
         var isShip:Boolean = this.getSyncDP(SyncDataID.IS_SHIP_BUILD);
         bShow = isShip ? false : bShow;
         if(this.myDevDSRoomList.visible == bShow)
         {
            return;
         }
         if(bShow)
         {
            this.listDedicatedServer(true);
         }
         if(this.myDevPanel != null)
         {
            this.myDevPanel.visible = bShow;
         }
         this.myDevDSRoomList.visible = bShow;
         this.myStartDSBtn.visible = bShow;
         this.myStartLSBtn.visible = bShow;
         this.myRefreshDSBtn.visible = bShow;
      }
      
      protected function refreshRoomInfo() : void
      {
         if(this.localMatchControls != null) this.localMatchControls.setRoom(this._roomObj);
         if(this._roomObj == null)
         {
            return;
         }
         this._isRefreshingRoomInfo = true;
         this.myRoomInfo.data = this._roomObj;
         this.showDedicatedServerPanel(this._roomObj.forces.isHost);
         var roomInfo:RoomInfoObj = this._roomObj.forces;
         this.myForce_0.playerDP = {
            "dp":roomInfo.force0,
            "maxPlayer":roomInfo.forceMaxPlayer
         };
         this.myRoomInfo.myMapAlbum.visible = roomInfo.showMapAlbum;
         if(roomInfo.showMapAlbum)
         {
            if(roomInfo.maps != null && roomInfo.maps.length > 0)
            {
               this.myRoomInfo.myMapAlbum.data = roomInfo.maps;
               this.myRoomInfo.myMapAlbum.currentIndex = roomInfo.selectedMapIndex;
               this.myRoomInfo.myMapAlbum.controllable = roomInfo.enableChangeMapBtn;
            }
         }
         this._isRefreshingRoomInfo = false;
      }
      
      protected function sendMsg() : void
      {
         this.broadcast(BCEvent.EVT_SEND_CHAT_MSG,{
            "channel":this._channelObj,
            "message":this.myInputTF.text
         });
         this.myInputTF.text = "";
      }
   }
}

