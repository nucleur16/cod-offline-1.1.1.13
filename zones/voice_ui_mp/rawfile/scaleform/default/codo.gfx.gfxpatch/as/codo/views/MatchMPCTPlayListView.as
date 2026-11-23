package codo.views
{
   import codo.common.BaseView;
   import codo.common.ListEvent;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationRoom;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.objects.ChallengeActionObj;
   import codo.utils.ImageLoader;
   import codo.utils.LocalizeUtil;
   import com.greensock.TimelineLite;
   import com.greensock.TweenLite;
   import com.greensock.easing.Linear;
   import components.CodoGrowButtonNoTint;
   import components.CodoList;
   import flash.display.Bitmap;
   import flash.display.Sprite;
   import flash.geom.Matrix;
   import flash.text.TextField;
   import flash.utils.Timer;
   import flash.utils.setTimeout;
   
   public class MatchMPCTPlayListView extends BaseView
   {
      private static const POS_X_ORIGIN:int = 464;
      
      private static const POS_Y_ORIGIN:int = GameInfo.PANEL_TOP_ALIGN + 10;
      
      public var myHeader:CodoGrowButtonNoTint;
      
      public var myMapContainer:Sprite;
      
      public var myMapContainer2:Sprite;
      
      public var myPlaylistNameTF:TextField;
      
      public var myPlaylistDescTF:TextField;
      
      public var myPlayerLabelTF:TextField;
      
      public var myPlayerTF:TextField;
      
      public var myMapLabelTF:TextField;
      
      public var myModeLabelTF:TextField;
      
      public var myModesTF:TextField;
      
      public var myMapTF:TextField;
      
      public var myXPLabelTF:TextField;
      
      public var myXPTF:TextField;
      
      public var myGPLabelTF:TextField;
      
      public var myGPTF:TextField;
      
      public var myList:CodoList;
      
      public var myOpenLevelLabelTF:TextField;
      
      public var myOpenLevelTF:TextField;
      
      public var myPartyNumTF:TextField;
      
      public var myPartyDesTF:TextField;
      
      public var myPartyUnlockBg:Sprite;
      
      public var myPartyLockBg:Sprite;
      
      private var _currentShowImgArr:Array;
      
      private var _currentShowImgIndex:uint;
      
      private var _currentImgContainer:Sprite;
      
      private var _showImgTimer:Timer;
      
      private var _originMatrix:Matrix;
      
      private var _k33Redirect:Boolean = false;
      
      public function MatchMPCTPlayListView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.myMapContainer.alpha = this.myMapContainer2.alpha = 0;
         this._currentImgContainer = this.myMapContainer;
         this._showImgTimer = new Timer(4000,0);
         this.myPlayerLabelTF.text = LocalizationRoom.LIST_LABEL_PLAYER_MIN_MAX;
         this.myMapLabelTF.text = LocalizationRoom.LIST_LABEL_MAP;
         this.myModeLabelTF.text = LocalizationRoom.CREATE_ROOM_MODE;
         this.myXPLabelTF.text = LocalizationRoom.GAIN_XP_RATE;
         this.myGPLabelTF.text = LocalizationRoom.GAIN_GP_RATE;
         this.myOpenLevelLabelTF.text = LocalizationRoom.PLAYLIST_OPEN_AT;
         this.myHeader.label = LocalizationRoom.LIST_LABEL_PLAYLIST;
         this.myHeader.enable = false;
         this.x = POS_X_ORIGIN;
         this.y = POS_Y_ORIGIN;
         this._originMatrix = this.transform.matrix;
         this.tweenHide(0,{
            "z":TweenParam.Z_GENERAL_TOP,
            "blurFilter":TweenParam.BLUR_GENERAL
         });
      }
      
      override protected function showView() : void
      {
         this.myList.addEventListener(CodoList.EVT_LINE_CLICK,this.onListItemClickHandle);
         this.myList.addEventListener(CodoList.EVT_LINE_ROLLOVER,this.onListRollOverHandle);
         this.myList.addEventListener(CodoList.EVT_LINE_ROLLOUT,this.onListRollOutHandle);
         this.listen(BCEvent.CHALLENGE_PERFORMED,this.onChallengePerformedHandle);
         var tempSelectedDat:Object = this.myList.selectedValue;
         if(tempSelectedDat != null && tempSelectedDat.unlocked == true)
         {
            this.broadcast(BCEvent.EVT_MPCT_PLAYLIST_UNLOCKED,{"unlocked":true});
         }
         else
         {
            this.broadcast(BCEvent.EVT_MPCT_PLAYLIST_UNLOCKED,{"unlocked":false});
         }
         this.tweenShow(TweenParam.SPD_FADE,{
            "delay":0.3,
            "z":0,
            "autoAlpha":1,
            "blurFilter":TweenParam.BLUR_NONE
         },null,this._originMatrix,{
            "autoAlpha":0,
            "z":TweenParam.Z_GENERAL_TOP,
            "blurFilter":TweenParam.BLUR_GENERAL
         });
         this.addSyncDP(SyncDataID.MATCH_MPCT_PLAYLIST_SELECTED_DATA,this.getCurrentPlayListSelectedDat);
         this._showImgTimer.start();
         // k33 offline: there is no Matchmaking. Whatever opens this page (the Multiplayer entry, the Start button,
         // closing a room) goes on to Create Room a frame later, as Create Room's own button does; the page's
         // 0.3 s fade-in never starts.
         this._k33Redirect = true;
         setTimeout(this.k33GotoRoomCreator,1);
      }
      
      private function k33GotoRoomCreator() : void
      {
         if(!this._k33Redirect)
         {
            return;
         }
         this._k33Redirect = false;
         this.setBindData(AsyncTaskID.MATCH_MPCT_ENABLE_START_BUTTON,{"enable":true});
         this.broadcast(BCEvent.EVT_GOTO_MATCH_MPCT_ROOM_CREATOR);
      }
      
      override protected function hideView() : void
      {
         this._k33Redirect = false;
         this.myList.removeEventListener(CodoList.EVT_LINE_CLICK,this.onListItemClickHandle);
         this.myList.removeEventListener(CodoList.EVT_LINE_ROLLOVER,this.onListRollOverHandle);
         this.myList.removeEventListener(CodoList.EVT_LINE_ROLLOUT,this.onListRollOutHandle);
         this.stopListen(BCEvent.CHALLENGE_PERFORMED,this.onChallengePerformedHandle);
         this.broadcast(BCEvent.EVT_MPCT_PLAYLIST_UNLOCKED,{"unlocked":true});
         this.tweenHide(TweenParam.SPD_FADE,{
            "autoAlpha":0,
            "z":TweenParam.Z_GENERAL,
            "blurFilter":TweenParam.BLUR_GENERAL
         });
         this._showImgTimer.stop();
      }
      
      override protected function enableData() : void
      {
         this.bind(AsyncTaskID.GROUP_COUNTS,this.bind_groupCounts);
      }
      
      override protected function disableData() : void
      {
         this.unbind(AsyncTaskID.GROUP_COUNTS,this.bind_groupCounts);
      }
      
      private function setPlayLists(force:Boolean = false) : void
      {
         var playLists:Array = null;
         var lastSelectedIndex:int = 0;
         var i:int = 0;
         if(this.myList.dataProvider.length <= 0 || force)
         {
            playLists = this.getSyncDP(SyncDataID.LIST_PLAY_LIST_NEW);
            if(playLists.length > 0)
            {
               lastSelectedIndex = force && this.myList.dataProvider.length > 0 ? this.myList.selectedIndex : -1;
               if(lastSelectedIndex < 0)
               {
                  for(i = 0; i < playLists.length; i++)
                  {
                     if(playLists[i].unlocked == true)
                     {
                        lastSelectedIndex = i;
                        break;
                     }
                  }
               }
               this.myList.dataProvider = playLists;
               if(lastSelectedIndex >= 0)
               {
                  this.myList.selectedIndex = lastSelectedIndex;
                  this.showPlaylistInfo(this.myList.selectedValue);
               }
            }
         }
      }
      
      private function onListItemClickHandle(cEvt:ListEvent) : void
      {
         this.showPlaylistInfo(this.myList.selectedValue);
      }
      
      private function onListRollOverHandle(cEvt:ListEvent) : void
      {
         this.showPlaylistInfo(this.myList.focusedValue);
      }
      
      private function onListRollOutHandle(cEvt:ListEvent = null) : void
      {
         this.showPlaylistInfo(this.myList.selectedValue);
      }
      
      private function showPlaylistInfo(cDat:Object) : void
      {
         var openLevels:Array = null;
         var openLevelStr:String = null;
         var i:int = 0;
         var tempMapNames:Array = null;
         var tempModeName:Array = null;
         if(cDat == null)
         {
            this.myPlayerTF.text = "";
            this.myPlaylistNameTF.text = "";
            this.myPlaylistDescTF.text = "";
            this.myMapTF.text = "";
            this.myModesTF.text = "";
            this.myPlayerTF.text = "";
            this.myXPTF.text = "";
            this.myGPTF.text = "";
            this.myOpenLevelTF.text = "";
         }
         else
         {
            this.broadcast(BCEvent.EVT_MPCT_PLAYLIST_UNLOCKED,{"unlocked":cDat.unlocked});
            openLevels = cDat.unlockedLevels;
            openLevelStr = "";
            i = 0;
            while(i < openLevels.length)
            {
               openLevelStr += LocalizeUtil.param(LocalizationRoom.PLAYLIST_OPEN_AT_VALUE,[openLevels[i].start,openLevels[i].end]);
               i++;
            }
            if(cDat.minPartySize == 0 && cDat.maxPartySize == 0)
            {
               this.myPartyLockBg.visible = true;
               this.myPartyUnlockBg.visible = false;
               this.myPartyNumTF.visible = false;
               this.myPartyDesTF.text = LocalizationRoom.LIST_LOCKPARTY;
            }
            else
            {
               this.myPartyLockBg.visible = false;
               this.myPartyUnlockBg.visible = true;
               this.myPartyNumTF.visible = true;
               this.myPartyNumTF.text = cDat.maxPartySize;
               this.myPartyDesTF.text = LocalizationRoom.LIST_UNLOCKPARTY;
            }
            this.myOpenLevelTF.text = openLevelStr;
            this.myPlayerTF.text = "";
            this.myPlaylistNameTF.text = cDat.name;
            this.myPlaylistDescTF.text = cDat.description;
            this.myPlayerTF.text = cDat.name == "Quick Match" && cDat.type == 1 && cDat.minPartySize == 1 && cDat.maxPartySize == 6 && cDat.maxRoomSize == 12 ? "2 ~ 12" : cDat.minRoomSize + " ~ " + cDat.maxRoomSize;
            tempMapNames = [];
            tempModeName = [];
            if(cDat.maps.length > 0)
            {
               this._currentShowImgIndex = 0;
               this._currentShowImgArr = cDat.maps;
               this._showImgTimer.reset();
               this._showImgTimer.start();
               this.myMapContainer.scaleX = this.myMapContainer.scaleY = this.myMapContainer2.scaleX = this.myMapContainer2.scaleY = 1;
               this.showNextMapImgPPT();
            }
            i = 0;
            while(i < cDat.maps.length)
            {
               tempMapNames.push(LocalizeUtil.localize(cDat.maps[i].name));
               i++;
            }
            i = 0;
            while(i < cDat.modes.length)
            {
               tempModeName.push(LocalizeUtil.localize(cDat.modes[i].name));
               i++;
            }
            this.myMapTF.text = tempMapNames.join(", ");
            this.myModesTF.text = tempModeName.join(", ");
            var pilotQuick:Boolean = cDat.name == "Quick Match" && cDat.type == 1 && cDat.minPartySize == 1 && (cDat.maxPartySize == 1 || cDat.maxPartySize == 6) && cDat.maxRoomSize == 12;
            this.myMapContainer.visible = this.myMapContainer2.visible = !pilotQuick;
            if(pilotQuick)
            {
               this.myMapTF.text = "Random";
               this.myModesTF.text = "Mixed modes";
               this._showImgTimer.stop();
            }
            this.myXPTF.text = this.getSyncDP(SyncDataID.XP_LEAGUE_FACTOR,[cDat.combatTrainingType,cDat.leagueID,cDat]) * 100 + "%";
            this.myGPTF.text = this.getSyncDP(SyncDataID.GP_LEAGUE_FACTOR,[cDat.combatTrainingType,cDat.leagueID,cDat]) * 100 + "%";
         }
      }
      
      private function showNextMapImgPPT() : void
      {
         ImageLoader.loadImg(this._currentShowImgArr[this._currentShowImgIndex].imageURL,this._currentImgContainer,this.onImageLoadComplete);
         TweenLite.killTweensOf(this._currentImgContainer);
         var timeline:TimelineLite = new TimelineLite();
         this._currentImgContainer.scaleX = this._currentImgContainer.scaleY = 1;
         timeline.append(new TweenLite(this._currentImgContainer,TweenParam.MAP_IMG_PPT_FADE,{
            "alpha":1,
            "scaleX":1.024,
            "scaleY":1.024,
            "ease":Linear.easeNone
         }));
         timeline.append(new TweenLite(this._currentImgContainer,TweenParam.MAP_IMG_PPT,{
            "scaleX":1.1,
            "scaleY":1.1,
            "ease":Linear.easeNone,
            "onComplete":this.onShowNextMapImgComplete
         }));
         timeline.play();
         if(this._currentImgContainer == this.myMapContainer)
         {
            TweenLite.killTweensOf(this.myMapContainer2);
            TweenLite.to(this.myMapContainer2,TweenParam.MAP_IMG_PPT,{"alpha":0});
         }
         else
         {
            TweenLite.killTweensOf(this.myMapContainer);
            TweenLite.to(this.myMapContainer,TweenParam.MAP_IMG_PPT,{"alpha":0});
         }
      }
      
      private function getCurrentPlayListSelectedDat() : Object
      {
         return this.myList.selectedValue;
      }
      
      private function bind_groupCounts(cRes:Object) : void
      {
         this.setPlayLists(true);
      }
      
      private function onChallengePerformedHandle(cEvt:ParaEvent) : void
      {
         var actionObj:ChallengeActionObj = cEvt.myPara;
         if(actionObj.playlist == null)
         {
            return;
         }
         var myListArray:Array = this.myList.dataProvider;
         for(var i:int = 0; i < myListArray.length; i++)
         {
            if(myListArray[i] != null)
            {
               if(myListArray[i].id == actionObj.playlist.id)
               {
                  this.myList.selectedIndex = i;
                  break;
               }
            }
         }
         this.showPlaylistInfo(this.myList.selectedValue);
      }
      
      private function onShowNextMapImgComplete() : void
      {
         if(this._currentImgContainer == this.myMapContainer)
         {
            this._currentImgContainer = this.myMapContainer2;
         }
         else
         {
            this._currentImgContainer = this.myMapContainer;
         }
         ++this._currentShowImgIndex;
         if(this._currentShowImgIndex >= this._currentShowImgArr.length)
         {
            this._currentShowImgIndex = 0;
         }
         this.showNextMapImgPPT();
      }
      
      private function onImageLoadComplete(cBMP:Bitmap) : void
      {
         cBMP.width = 326;
         cBMP.height = 234;
         cBMP.x = -cBMP.width / 2;
         cBMP.y = -cBMP.height / 2;
      }
   }
}

