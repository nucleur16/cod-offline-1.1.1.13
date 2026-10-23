package codo.views
{
   import codo.common.BaseView;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationRoom;
   import codo.constants.PageID;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.utils.LocalizeUtil;
   import components.Clock;
   import flash.text.TextField;
   
   public class RankedMatchInProgressView extends BaseView
   {
      public static const POS_ORIGIN_X:uint = 892;
      
      public static const POS_ORIGIN_Y:uint = 1043;
      
      public static const POS_SHOW_Y:uint = 888;
      
      public static const POS_ORIGIN_X_NEW:int = 900;
      
      public static const POS_ORIGIN_Y_NEW:int = -140;
      
      public static const POS_SHOW_Y_NEW:int = 25;
      
      public var myTFAutoMatchInfo:TextField;
      
      public var myTFEstimateLabel:TextField;
      
      public var myPassTime:Clock;
      
      public var myTFEstimate:TextField;
      
      private var _isInRoomWaitForSuccess:Boolean = false;
      
      private var _dotCount:uint = 0;
      
      private var _currentRankedMatchStatus:String = "idle";
      
      public function RankedMatchInProgressView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.myTFAutoMatchInfo.mouseEnabled = false;
         this.myTFAutoMatchInfo.text = "";
         this.myTFEstimateLabel.text = LocalizationRoom.AUTO_MATCH_ESTIMATE_TIME;
         this.myPassTime.time = 0;
         this.myTFEstimate.text = "00:00";
         this.myTFEstimateLabel.visible = false;
         this.myTFEstimate.visible = false;
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
      }
      
      override protected function onInitListener() : void
      {
         this.listen(BCEvent.EVT_RANKED_MATCH_CHANGE_STATUS,this.onRankedMatchStatusChangedHandle);
         this.listen(BCEvent.EVT_RANKED_MATCH_INTERMISSION,this.onRankedMatchIntermissionHandle);
         this.listen(BCEvent.EVT_RANKED_MATCH_PASSED_TIME_TIMER,this.onRankedMatchPassedTimeHandle);
         this.listen(BCEvent.EVT_PAGE_ID_CHANGED,this.onPageIDChangedHandle);
      }
      
      private function show() : void
      {
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
            this.tweenHide(TweenParam.SPD_ELEMENT_MOVE,{"y":POS_ORIGIN_Y_NEW});
         }
         else
         {
            this.tweenHide(TweenParam.SPD_ELEMENT_MOVE,{"y":GameInfo.DEFAULT_HEIGHT});
         }
      }
      
      private function onRankedMatchStatusChangedHandle(cEvt:ParaEvent) : void
      {
         if(this._currentRankedMatchStatus != cEvt.myPara.status)
         {
            this._currentRankedMatchStatus = cEvt.myPara.status;
            this._isInRoomWaitForSuccess = this.canShow() ? this._isInRoomWaitForSuccess : false;
            this.rankedMatchInfo = cEvt.myPara;
            if(this.canShow())
            {
               this.show();
            }
            else
            {
               this.hide();
            }
         }
      }
      
      private function onRankedMatchIntermissionHandle(cEvt:ParaEvent) : void
      {
         if(!this._isInRoomWaitForSuccess)
         {
            this._isInRoomWaitForSuccess = true;
         }
      }
      
      private function onRankedMatchPassedTimeHandle(cEvt:ParaEvent) : void
      {
         this.myPassTime.time = cEvt.myPara;
         ++this._dotCount;
         if(this._dotCount > 3)
         {
            this._dotCount = 0;
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
         this.myTFAutoMatchInfo.text = pilotQuick ? "Searching..." : LocalizeUtil.localize(cInfo.playListName);
      }
      
      private function canShow() : Boolean
      {
         var pageID:String = this.getSyncDP(SyncDataID.CURRENT_PAGE_ID);
         return pageID != PageID.INROOM && this.getSyncDP(SyncDataID.IS_RANKED_MATCH_IN_PROGRESS);
      }
   }
}

