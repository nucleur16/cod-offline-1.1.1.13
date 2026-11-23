package codo.views
{
   // FFDec's decompile of C1's MatchMPCTRoomCreatorView (codo.gfx), with the k33 offline changes marked "k33".
   import codo.common.BaseView;
   import codo.common.ParaEvent;
   import codo.constants.LocalizationRoom;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.views.matchmpctroomcreator_sub.MatchMPCTRoomCreatorView_ctcreator;
   import codo.views.matchmpctroomcreator_sub.MatchMPCTRoomCreatorView_mpcreator;
   import com.greensock.layout.ScaleMode;
   import components.Album;
   import components.BaseButton;
   import components.CodoComboBox;
   import components.CodoGrowButton;
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.display.InteractiveObject;
   import flash.events.Event;
   
   public class MatchMPCTRoomCreatorView extends BaseView
   {
      
      private static const POS_X_ORIGIN:int = 464;
      
      private static const POS_Y_ORIGIN:int = 148;
      
      public var myTabMP:CodoGrowButton;
      
      public var myTabCT:CodoGrowButton;
      
      public var myAlbum:Album;
      
      public var myMPRoomCreator:MatchMPCTRoomCreatorView_mpcreator;
      
      public var myCTRoomCreator:MatchMPCTRoomCreatorView_ctcreator;
      
      private var _currentTabBtn:CodoGrowButton;
      
      public function MatchMPCTRoomCreatorView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.myTabMP.label = LocalizationRoom.CREATE_ROOM_MP;
         this.myTabCT.label = LocalizationRoom.CREATE_ROOM_CT;
         this.myAlbum.setImgSize(705,200);
         this.myAlbum.fitParams = {"scaleMode":ScaleMode.PROPORTIONAL_OUTSIDE};
         this.myAlbum.dataField = "imageURL";
         this.myMPRoomCreator.mapAlbum = this.myAlbum;
         this.myCTRoomCreator.mapAlbum = this.myAlbum;
         this.changeTabSelect(this.myTabMP);
         this.myMPRoomCreator.enable();
         this.myCTRoomCreator.enable();
         this.k33OfflineLayout();
         this.x = POS_X_ORIGIN;
         this.y = POS_Y_ORIGIN;
         this.tweenHide(0,{
            "z":TweenParam.Z_GENERAL_TOP,
            "blurFilter":TweenParam.BLUR_GENERAL
         });
      }
      
      override protected function onInitListener() : void
      {
         this.myTabMP.addEventListener(BaseButton.EVT_CLICK,this.onTabClickHandle);
         this.myTabCT.addEventListener(BaseButton.EVT_CLICK,this.onTabClickHandle);
      }
      
      override protected function showView() : void
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
         this.myMPRoomCreator.mapAlbum = this.myAlbum;
         this.myCTRoomCreator.mapAlbum = this.myAlbum;
      }
      
      override protected function hideView() : void
      {
         this.tweenHide(TweenParam.SPD_FADE,{
            "autoAlpha":0,
            "z":TweenParam.Z_GENERAL,
            "blurFilter":TweenParam.BLUR_GENERAL
         });
      }
      
      override protected function enableData() : void
      {
         this.addSyncDP(SyncDataID.MATCH_MPCT_ROOMCREATOR_PARAMS,this.getRoomCreatorParams);
      }
      
      override protected function disableData() : void
      {
         this.removeSyncDP(SyncDataID.MATCH_MPCT_ROOMCREATOR_PARAMS,this.getRoomCreatorParams);
      }
      
      private function onTabClickHandle(cEvt:ParaEvent) : void
      {
         var targetTab:CodoGrowButton = cEvt.target as CodoGrowButton;
         this.changeTabSelect(targetTab);
      }
      
      private function changeTabSelect(cTargetBtn:CodoGrowButton) : void
      {
         if(cTargetBtn != this._currentTabBtn)
         {
            switch(cTargetBtn)
            {
               case this.myTabMP:
                  this.myMPRoomCreator.active = true;
                  this.myCTRoomCreator.active = false;
                  break;
               case this.myTabCT:
                  this.myMPRoomCreator.active = false;
                  this.myCTRoomCreator.active = true;
            }
            if(this._currentTabBtn != null)
            {
               this._currentTabBtn.selected = false;
            }
            this._currentTabBtn = cTargetBtn;
            this._currentTabBtn.selected = true;
         }
      }
      
      // k33 offline: no Combat Training. Its tab is hidden, so every room is a Multiplayer room (the tab this view
      // starts on). Of the Combat Training panel only the map description stays, at full strength; it describes the
      // multiplayer map chosen (MatchMPCTRoomCreatorView_mpcreator.k33MapDescription). Its XP / GP rates and its
      // Easy / Normal / Hard difficulty are hidden.
      private function k33OfflineLayout() : void
      {
         var ct:Object = this.myCTRoomCreator;
         var hidden:Array = [this.myTabCT,ct.myExpRateLabelTF,ct.myGPRateLabelTF,ct.myExpRate,ct.myGPRate,ct.myExpBar,ct.myGPBar,ct.myLevelSelector];
         var i:int = 0;
         while(i < hidden.length)
         {
            var item:DisplayObject = hidden[i] as DisplayObject;
            if(item != null)
            {
               item.visible = false;
               if(item is InteractiveObject)
               {
                  InteractiveObject(item).mouseEnabled = false;
                  InteractiveObject(item).tabEnabled = false;
               }
               if(item is DisplayObjectContainer)
               {
                  DisplayObjectContainer(item).mouseChildren = false;
                  DisplayObjectContainer(item).tabChildren = false;
               }
            }
            i++;
         }
         this.myCTRoomCreator.alpha = 1;
         this.myMPRoomCreator.addEventListener("k33MapInfo",this.k33ShowMapInfo);
         // the hidden Combat Training map list still writes its own map's description when it changes: put the
         // multiplayer map's back (this listener runs after the panel's own)
         this.myCTRoomCreator.myOptionCB_1.addEventListener(CodoComboBox.EVT_CHANGE,this.k33ShowMapInfo);
         this.k33ShowMapInfo(null);
      }
      
      private function k33ShowMapInfo(cEvt:Event) : void
      {
         var text:Object = Object(this.myCTRoomCreator).myMapTF;
         if(text != null)
         {
            text.text = Object(this.myMPRoomCreator).k33MapDescription();
         }
      }
      
      private function getRoomCreatorParams() : Object
      {
         var tempParams:Object = null;
         switch(this._currentTabBtn)
         {
            case this.myTabMP:
               tempParams = this.getSyncDP(SyncDataID.MATCH_MPCT_ROOMCREATOR_MP_PARAMS);
               break;
            case this.myTabCT:
               tempParams = this.getSyncDP(SyncDataID.MATCH_MPCT_ROOMCREATOR_CT_PARAMS);
         }
         return tempParams;
      }
   }
}

