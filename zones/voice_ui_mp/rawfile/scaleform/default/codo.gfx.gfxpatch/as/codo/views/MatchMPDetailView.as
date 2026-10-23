package codo.views
{
   import codo.common.BaseView;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationRoom;
   import codo.constants.TweenParam;
   import codo.utils.ImageLoader;
   import codo.utils.LocalizeUtil;
   import com.greensock.TimelineLite;
   import com.greensock.TweenLite;
   import com.greensock.easing.Linear;
   import components.BaseButton;
   import components.CodoFlipButton;
   import components.CodoGrowButtonNoTint;
   import flash.display.Bitmap;
   import flash.display.Sprite;
   import flash.text.TextField;
   import flash.utils.Timer;
   
   public class MatchMPDetailView extends BaseView
   {
      private static var ALPHA_ORIGIN:int = 0;
      
      private static var ALPHA_OPEN:int = 1;
      
      private static var POS_X_ORIGIN:int = 455;
      
      private static var POS_X_OPEN:int = GameInfo.PANEL_RIGHT_ALIGN;
      
      private static var POS_Y_ORIGIN:int = GameInfo.PANEL_TOP_ALIGN;
      
      public var myFlipBtn:CodoFlipButton;
      
      public var myStartBtn:CodoGrowButtonNoTint;
      
      public var myPlaylistNameTF:TextField;
      
      public var myPlaylistDescTF:TextField;
      
      public var myPlayerLabelTF:TextField;
      
      public var myPlayerTF:TextField;
      
      public var myMapLabelTF:TextField;
      
      public var myModeLabelTF:TextField;
      
      public var myModesTF:TextField;
      
      public var myMapTF:TextField;
      
      public var myPartyNumTF:TextField;
      
      public var myPartyDesTF:TextField;
      
      public var myPartyUnlockBg:Sprite;
      
      public var myPartyLockBg:Sprite;
      
      public var myMapContainer2:Sprite;
      
      public var myMapContainer:Sprite;
      
      private var _playlistDat:Object;
      
      private var _currentShowImgs:Array;
      
      private var _currentShowImgIndex:uint;
      
      private var _currentImgContainer:Sprite;
      
      private var _showImgTimer:Timer;
      
      public function MatchMPDetailView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.myPlayerLabelTF.text = LocalizationRoom.LIST_LABEL_PLAYER_MIN_MAX;
         this.myMapLabelTF.text = LocalizationRoom.LIST_LABEL_MAP;
         this.myModeLabelTF.text = LocalizationRoom.CREATE_ROOM_MODE;
         this.myStartBtn.label = LocalizationRoom.AUTO_MATCH;
         this.myFlipBtn.label = LocalizationRoom.ROOM_LIST;
         this.myMapContainer.alpha = this.myMapContainer2.alpha = 0;
         this._currentImgContainer = this.myMapContainer;
         this._showImgTimer = new Timer(4000,0);
         this.tweenHide(0,{
            "x":POS_X_ORIGIN,
            "y":POS_Y_ORIGIN,
            "alpha":ALPHA_ORIGIN
         });
      }
      
      override protected function onInitListener() : void
      {
         this.myStartBtn.addEventListener(BaseButton.EVT_CLICK,this.onStartBtnClickHandle);
         this.myFlipBtn.addEventListener(BaseButton.EVT_CLICK,this.onFlipBtnClickedHandle);
         this.listen(BCEvent.EVT_SHOW_MP_PLAYLIST_INFO,this.onShowMpPlaylistInfoHandle);
      }
      
      override protected function showView() : void
      {
         this.tweenShow(TweenParam.SPD_ELEMENT_MOVE,{
            "x":POS_X_OPEN,
            "alpha":ALPHA_OPEN
         });
      }
      
      override protected function hideView() : void
      {
         this.tweenHide(TweenParam.SPD_ELEMENT_MOVE,{
            "x":POS_X_ORIGIN,
            "alpha":ALPHA_ORIGIN
         });
      }
      
      private function onShowMpPlaylistInfoHandle(cEvt:ParaEvent) : void
      {
         var tempMapNames:Array = null;
         var tempModeName:Array = null;
         var i:uint = 0;
         this._playlistDat = cEvt.myPara;
         this.myStartBtn.enable = this._playlistDat != null;
         if(this._playlistDat == null)
         {
            this.myPlayerTF.text = "";
            this.myPlaylistNameTF.text = "";
            this.myPlaylistDescTF.text = "";
            this.myMapTF.text = "";
            this.myModesTF.text = "";
            this.myPlayerTF.text = "";
            TweenLite.killTweensOf(this.myMapContainer);
            TweenLite.killTweensOf(this.myMapContainer2);
            this.myMapContainer.alpha = this.myMapContainer2.alpha = 0;
            this.myPartyLockBg.visible = this.myPartyUnlockBg.visible = this.myPartyNumTF.visible = this.myPartyDesTF.visible = false;
         }
         else
         {
            this.myPlayerTF.text = "";
            this.myPlaylistNameTF.text = this._playlistDat.name;
            this.myPlaylistDescTF.text = this._playlistDat.description;
            this.myPlayerTF.text = this._playlistDat.name == "Quick Match" && this._playlistDat.type == 1 && this._playlistDat.minPartySize == 1 && this._playlistDat.maxPartySize == 6 && this._playlistDat.maxRoomSize == 12 ? "2 ~ 12" : this._playlistDat.minRoomSize + " ~ " + this._playlistDat.maxRoomSize;
            tempMapNames = [];
            tempModeName = [];
            if(this._playlistDat.maps.length > 0)
            {
               this._currentShowImgIndex = 0;
               this._currentShowImgs = this._playlistDat.maps;
               this._showImgTimer.reset();
               this._showImgTimer.start();
               this.myMapContainer.scaleX = this.myMapContainer.scaleY = this.myMapContainer2.scaleX = this.myMapContainer2.scaleY = 1;
               this.showNextMapImgPPT();
            }
            i = 0;
            while(i < this._playlistDat.maps.length)
            {
               tempMapNames.push(LocalizeUtil.localize(this._playlistDat.maps[i].name));
               i++;
            }
            i = 0;
            while(i < this._playlistDat.modes.length)
            {
               tempModeName.push(LocalizeUtil.localize(this._playlistDat.modes[i].name));
               i++;
            }
            this.myMapTF.text = tempMapNames.join(", ");
            this.myModesTF.text = tempModeName.join(", ");
            if(this._playlistDat.minPartySize == 0 && this._playlistDat.maxPartySize == 0)
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
               this.myPartyNumTF.visible = this.myPartyDesTF.visible = true;
               this.myPartyNumTF.text = this._playlistDat.maxPartySize;
               this.myPartyDesTF.text = LocalizationRoom.LIST_UNLOCKPARTY;
            }
            this.myStartBtn.enable = this._playlistDat.unlocked;
         }
      }
      
      private function showNextMapImgPPT() : void
      {
         ImageLoader.loadImg(this._currentShowImgs[this._currentShowImgIndex].imageURL,this._currentImgContainer,this.onImageLoadComplete);
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
         if(this._currentShowImgIndex >= this._currentShowImgs.length)
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
      
      private function onStartBtnClickHandle(cEvt:ParaEvent) : void
      {
         if(this._playlistDat != null && Boolean(this._playlistDat.unlocked))
         {
            this.broadcast(BCEvent.EVT_START_QUICK_MATCH_MP,{"data":this._playlistDat});
         }
      }
      
      private function onFlipBtnClickedHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.EVT_GOTO_ROOMLIST_MP);
      }
   }
}

