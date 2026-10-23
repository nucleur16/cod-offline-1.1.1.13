package codo.views
{
   import codo.common.BaseView;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.BackToRoomType;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationRoom;
   import codo.constants.PageID;
   import codo.constants.RankedMatchStatus;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.utils.LocalizeUtil;
   import codo.utils.TimeUtil;
   import components.BaseButton;
   import components.CodoGrowButton;
   import components.CodoGrowButtonNoTint;
   import flash.events.Event;
   import flash.text.TextField;
   import flash.text.TextFormat;
   
   public class RankedMatchReadyView extends BaseView
   {
      public static const POS_ORIGIN_X:int = 892;
      
      public static const POS_ORIGIN_Y:int = 1039;
      
      public static const POS_SHOW_Y:int = 884;
      
      public static const POS_ORIGIN_X_NEW:int = 900;
      
      public static const POS_ORIGIN_Y_NEW:int = -140;
      
      public static const POS_SHOW_Y_NEW:int = 25;
      
      private static var _TIMER_TEXT_FORMAT_0:TextFormat = new TextFormat(GameInfo.GAME_FONT_L,18,5028721);
      
      private static var _TIMER_TEXT_FORMAT_1:TextFormat = new TextFormat(GameInfo.GAME_FONT_L,12,5028721);
      
      public var myTFAutoMatchReadyTitle:TextField;
      
      public var myTFAutoMatchReadyDetail:TextField;
      
      public var myTFClickToEnterRoom:TextField;
      
      public var myTFAutoMatchTimer:TextField;
      
      public var myRoomBtn:CodoGrowButtonNoTint;
      
      public var myHomeBtn:CodoGrowButtonNoTint;
      
      public var myLeaveBtn:CodoGrowButton;
      
      private var _currentRankedMatchStatus:String = "idle";
      
      public function RankedMatchReadyView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.myTFAutoMatchReadyTitle.text = "";
         this.myTFAutoMatchTimer.text = "";
         this.myTFClickToEnterRoom.text = LocalizationRoom.AUTO_MATCH_CLICK_TO_ENTER_ROOM;
         this.myTFClickToEnterRoom.mouseEnabled = false;
         this.myTFClickToEnterRoom.alpha = 0;
         this.myTFAutoMatchReadyTitle.mouseEnabled = false;
         this.myTFAutoMatchTimer.mouseEnabled = false;
         this.myTFAutoMatchReadyDetail.mouseEnabled = false;
         this.myTFAutoMatchReadyDetail.text = "";
         this.myRoomBtn.label = LocalizationRoom.BACK_TO_ROOM;
         this.myHomeBtn.label = LocalizationRoom.BACK_TO_MAIN;
         this.myLeaveBtn.label = LocalizationRoom.LEAVE_ROOM;
         this.myLeaveBtn.enable = false;
         this.myHomeBtn.visible = false;
         if(this.getSyncDP(SyncDataID.IS_MATCH_BUTTON_POS_ENABLED) == true)
         {
            this.tweenHide(0,{
               "x":POS_ORIGIN_X_NEW,
               "y":POS_ORIGIN_Y_NEW
            });
         }
         else
         {
            this.tweenHide(0,{
               "x":POS_ORIGIN_X,
               "y":POS_ORIGIN_Y
            });
         }
         this.hideTempContent();
      }
      
      override protected function onInitListener() : void
      {
         this.listen(BCEvent.EVT_RANKED_MATCH_GO_ROOM_TIMER,this.onGoRoomTimerHandle);
         this.listen(BCEvent.EVT_RANKED_MATCH_GO_GAME_TIMER,this.onGoGameTimerHandle);
         this.listen(BCEvent.EVT_RANKED_MATCH_PASSED_TIME_TIMER,this.onRankedMatchPassedTimeHandle);
         this.listen(BCEvent.EVT_RANKED_MATCH_CHANGE_STATUS,this.onRankedMatchStatusChangedHandle);
         this.listen(BCEvent.EVT_RANKED_MATCH_ENABLE_LEAVE_ROOM,this.onRankedMatchEnableLeaveRoomHandle);
         this.listen(BCEvent.EVT_PAGE_ID_CHANGED,this.onPageIDChangedHandle);
         this.myRoomBtn.addEventListener(BaseButton.EVT_CLICK,this.onRoomBtnClickHandle);
         this.myHomeBtn.addEventListener(BaseButton.EVT_CLICK,this.onHomeBtnClickHandle);
         this.myLeaveBtn.addEventListener(BaseButton.EVT_CLICK,this.onLeaveClickHandle);
      }
      
      private function show() : void
      {
         Logger.print("RankedMatchReadyView:::show");
         if(this.getSyncDP(SyncDataID.IS_MATCH_BUTTON_POS_ENABLED) == true)
         {
            this.tweenShow(TweenParam.SPD_ELEMENT_MOVE,{
               "delay":0.6,
               "y":POS_SHOW_Y_NEW
            });
         }
         else
         {
            this.tweenShow(TweenParam.SPD_ELEMENT_MOVE,{
               "delay":0.6,
               "y":POS_SHOW_Y
            });
         }
      }
      
      private function hide() : void
      {
         if(this.getSyncDP(SyncDataID.IS_MATCH_BUTTON_POS_ENABLED) == true)
         {
            this.tweenHide(TweenParam.SPD_ELEMENT_MOVE,{"y":POS_ORIGIN_Y});
         }
         else
         {
            this.tweenHide(TweenParam.SPD_ELEMENT_MOVE,{"y":GameInfo.DEFAULT_HEIGHT});
         }
      }
      
      private function onRoomBtnClickHandle(cEvt:Event) : void
      {
         this.broadcast(BCEvent.EVT_BACK_TO_ROOM,BackToRoomType.RANKED_MATCH);
      }
      
      private function onHomeBtnClickHandle(cEvt:Event) : void
      {
         this.broadcast(BCEvent.EVT_GOTO_HOME);
      }
      
      private function onLeaveClickHandle(cEvt:Event) : void
      {
         this.broadcast(BCEvent.EVT_LEAVE_ROOM);
      }
      
      private function onGoRoomTimerHandle(cEvt:ParaEvent) : void
      {
         this.setRankedMatchTimer(cEvt.myPara);
      }
      
      private function onGoGameTimerHandle(cEvt:ParaEvent) : void
      {
         this.setRankedMatchTimer(cEvt.myPara,false);
      }
      
      private function onRankedMatchPassedTimeHandle(cEvt:ParaEvent) : void
      {
         this.myTFAutoMatchTimer.text = TimeUtil.sec2Str(cEvt.myPara);
      }
      
      private function onRankedMatchStatusChangedHandle(cEvt:ParaEvent) : void
      {
         if(this._currentRankedMatchStatus != cEvt.myPara.status)
         {
            this._currentRankedMatchStatus = cEvt.myPara.status;
            if(this.canShow())
            {
               this.show();
            }
            else
            {
               this.hide();
            }
            this.rankedMatchInfo = cEvt.myPara;
         }
      }
      
      private function onRankedMatchEnableLeaveRoomHandle(cEvt:ParaEvent) : void
      {
         this.myLeaveBtn.enable = cEvt.myPara;
      }
      
      private function set rankedMatchInfo(cInfo:Object) : void
      {
         var pilotQuick:Boolean = false;
         var pilotCount:int = 0;
         var playlists:Array = this.getSyncDP(SyncDataID.LIST_PLAY_LIST_NEW);
         if(playlists != null && cInfo != null && cInfo.playListName == "Quick Match")
         {
            for each(var entry in playlists)
            {
               if(entry != null && entry.name == "Quick Match" && entry.type == 1 && entry.minPartySize == 1 && (entry.maxPartySize == 1 || entry.maxPartySize == 6) && entry.maxRoomSize == 12)
               {
                  pilotCount++;
               }
            }
         }
         pilotQuick = pilotCount == 1;
         this.myTFAutoMatchReadyTitle.text = this._currentRankedMatchStatus == RankedMatchStatus.BACK_FROM_GAME ? LocalizationRoom.AUTO_MATCH_BACK_FROM_GAME : LocalizationRoom.AUTO_MATCH_MP_SUCCESS;
         this.myTFAutoMatchReadyDetail.text = LocalizeUtil.localize(cInfo.leagueName) + " " + LocalizeUtil.localize(cInfo.playListName);
         this.myTFAutoMatchReadyTitle.visible = pilotQuick;
         if(pilotQuick)
         {
            var found:Boolean = this._currentRankedMatchStatus == RankedMatchStatus.READY || this._currentRankedMatchStatus == RankedMatchStatus.READY_IN_ROOM;
            this.myTFAutoMatchReadyTitle.text = found ? "Match found" : "Quick Match";
            this.myTFAutoMatchReadyDetail.text = "Quick Match";
         }
      }
      
      private function setRankedMatchTimer(cTimer:String, cRoom:Boolean = true) : void
      {
         if(cTimer != "" && cTimer != "0")
         {
            this.myTFAutoMatchTimer.text = LocalizeUtil.param(cRoom ? LocalizationRoom.AUTO_MATCH_ENTER_ROOM_TIMER : LocalizationRoom.AUTO_MATCH_ENTER_GAME_TIMER,[cTimer]);
            this.myTFAutoMatchTimer.setTextFormat(_TIMER_TEXT_FORMAT_0,0,cTimer.length);
            this.myTFAutoMatchTimer.setTextFormat(_TIMER_TEXT_FORMAT_1,cTimer.length,this.myTFAutoMatchTimer.length);
         }
         else
         {
            this.myTFAutoMatchTimer.text = "";
         }
      }
      
      private function onPageIDChangedHandle(cEvt:ParaEvent) : void
      {
         if(this.canShow())
         {
            this.show();
         }
         else
         {
            this.hide();
         }
      }
      
      private function canShow() : Boolean
      {
         var pageID:String = this.getSyncDP(SyncDataID.CURRENT_PAGE_ID);
         return pageID != PageID.INROOM && (this._currentRankedMatchStatus == RankedMatchStatus.READY || this._currentRankedMatchStatus == RankedMatchStatus.READY_IN_ROOM || this._currentRankedMatchStatus == RankedMatchStatus.BACK_FROM_GAME);
      }
      
      private function hideTempContent() : void
      {
         this.myRoomBtn.visible = false;
         this.myHomeBtn.visible = false;
         this.myTFAutoMatchReadyTitle.visible = false;
         this.myLeaveBtn.visible = false;
      }
   }
}

