package codo.views
{
   // the classic build's English MatchMPCTMenuView (FFDec's decompile of its codo.gfx tag, with its theatre list mode)
   // with every string shown as a localize key (K33UI_THEATRE_*, or C1's own SF_UI_* where C1 has the text): English
   // in codol_english_ui, Chinese in voice_ui_mp. The theatre needs the classic build's theatre backend, which this
   // project does not have: the screen opens but lists nothing.
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationRoom;
   import codo.constants.TweenParam;
   import codo.utils.LocalizeUtil;
   import components.BaseButton;
   import components.CodoGrowButton;
   import components.CodoGrowButtonNoTint;
   import flash.display.Bitmap;
   import flash.display.BitmapData;
   import flash.display.DisplayObjectContainer;
   import flash.events.Event;
   import flash.geom.Matrix;
   import flash.text.TextField;
   import flash.utils.getDefinitionByName;
   
   public class MatchMPCTMenuView extends BaseView
   {
      
      private static const POS_X_ORIGIN:int = GameInfo.PANEL_GAP;
      
      private static const POS_Y_ORIGIN:int = GameInfo.PANEL_TOP_ALIGN;
      
      public var myHeader:CodoGrowButtonNoTint;
      
      public var myPlayListBtn:CodoGrowButton;
      
      public var myRoomListBtn:CodoGrowButton;
      
      public var myRoomCreatorBtn:CodoGrowButton;
      
      private var _currentMenuBtn:CodoGrowButton = null;
      
      private var _originMatrix:Matrix;
      
      public var theatreOnly:Boolean = false;
      
      private var theatreHelp:Array = [];
      
      public function MatchMPCTMenuView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         if(this.theatreOnly)
         {
            this.myHeader.label = "@K33UI_THEATRE";
            this.myHeader.enable = false;
            this.myPlayListBtn.label = "@K33UI_THEATRE_ALL";
            this.myRoomListBtn.label = "@SF_UI_MAIN_MENU_LABEL_QUICKMATCH_MP";
            this.myRoomCreatorBtn.label = "@SF_UI_MAIN_MENU_LABEL_QUICKMATCH_ZOMBIE";
            this.theatreZombieArt();
            this.x = POS_X_ORIGIN;
            this.y = POS_Y_ORIGIN;
            this._originMatrix = this.transform.matrix;
            this.tweenHide(0,{
               "z":TweenParam.Z_GENERAL,
               "blurFilter":TweenParam.BLUR_GENERAL
            });
            return;
         }
         this.myHeader.label = LocalizationRoom.LIST_LABEL_LEAGUE;
         this.myHeader.enable = false;
         this.myPlayListBtn.label = LocalizationRoom.LIST_LABEL_PLAYLIST;
         this.myRoomListBtn.label = LocalizationRoom.ROOM_LIST;
         this.myRoomCreatorBtn.label = LocalizationRoom.CREATE;
         // k33 offline: no online channel. The "Channel" header, Matchmaking (the playlists) and Rooms (the room
         // browser) are hidden; Create Room is the only entry and moves up to the first button's place.
         this.k33OfflineHide(this.myHeader);
         this.k33OfflineHide(this.myPlayListBtn);
         this.k33OfflineHide(this.myRoomListBtn);
         this.myRoomCreatorBtn.y = this.myPlayListBtn.y;
         this.x = POS_X_ORIGIN;
         this.y = POS_Y_ORIGIN;
         this._originMatrix = this.transform.matrix;
         this.tweenHide(0,{
            "z":TweenParam.Z_GENERAL,
            "blurFilter":TweenParam.BLUR_GENERAL
         });
      }
      
      override protected function onInitListener() : void
      {
         if(this.theatreOnly)
         {
            this.myPlayListBtn.addEventListener(BaseButton.EVT_CLICK,this.theatreFilter);
            this.myRoomListBtn.addEventListener(BaseButton.EVT_CLICK,this.theatreFilter);
            this.myRoomCreatorBtn.addEventListener(BaseButton.EVT_CLICK,this.theatreFilter);
            return;
         }
         this.myRoomCreatorBtn.addEventListener(BaseButton.EVT_CLICK,this.onMenuBtnClickHandle);
         this.bind(AsyncTaskID.GROUP_COUNTS,this.bind_groupCounts);
      }

      private function k33OfflineHide(item:Object) : void
      {
         item.visible = false;
         item.mouseEnabled = false;
         item.mouseChildren = false;
         item.tabEnabled = false;
         item.tabChildren = false;
      }
      
      override protected function showView() : void
      {
         this.tweenShow(TweenParam.SPD_FADE,{
            "delay":0.3,
            "z":0,
            "autoAlpha":1,
            "blurFilter":TweenParam.BLUR_NONE
         },null,this._originMatrix);
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
         this.tweenHide(TweenParam.SPD_FADE,{
            "autoAlpha":0,
            "z":TweenParam.Z_GENERAL,
            "blurFilter":TweenParam.BLUR_GENERAL
         });
         this.setBindData(AsyncTaskID.MATCH_MPCT_ENABLE_START_BUTTON,{"enable":true});
      }
      
      override protected function enableData() : void
      {
         if(this.theatreOnly)
         {
            this.theatreSelect("all");
            return;
         }
         // k33 offline: the Matchmaking page sends the player on to Create Room (MatchMPCTPlayListView)
         this.changeCurrentMenu(this.myRoomCreatorBtn,false);
      }
      
      private function bind_groupCounts(cRes:Object) : void
      {
      }
      
      private function onMenuBtnClickHandle(cEvt:Event) : void
      {
         this.changeCurrentMenu(cEvt.target as CodoGrowButton);
      }
      
      private function changeCurrentMenu(cTargetBtn:CodoGrowButton, cBC:Boolean = true) : void
      {
         if(cBC == true)
         {
            switch(cTargetBtn)
            {
               case this.myPlayListBtn:
                  this.broadcast(BCEvent.EVT_GOTO_MATCH_MPCT_PLAY_LIST);
                  break;
               case this.myRoomListBtn:
                  this.broadcast(BCEvent.EVT_GOTO_MATCH_MPCT_ROOM_LIST);
                  break;
               case this.myRoomCreatorBtn:
                  this.setBindData(AsyncTaskID.MATCH_MPCT_ENABLE_START_BUTTON,{"enable":true});
                  this.broadcast(BCEvent.EVT_GOTO_MATCH_MPCT_ROOM_CREATOR);
            }
         }
         if(this._currentMenuBtn != null)
         {
            this._currentMenuBtn.selected = false;
         }
         this._currentMenuBtn = cTargetBtn;
         this._currentMenuBtn.selected = true;
      }
      
      private function theatreZombieArt() : void
      {
         var portraitClass:Class = getDefinitionByName("playModeCyborgBtn") as Class;
         var portraitButton:Object = new portraitClass();
         portraitButton.stop();
         var portrait:Bitmap = portraitButton.myShaderBg.getChildAt(0) as Bitmap;
         if(portrait == null || portrait.bitmapData == null)
         {
            return;
         }
         var photo:BitmapData = portrait.bitmapData;
         var states:Array = ["myBgNormal","myBgFocus","myBgDisable"];
         var i:int = 0;
         while(i < states.length)
         {
            var background:Object = this.myRoomCreatorBtn[states[i]];
            if(!(background == null || background.numChildren == 0))
            {
               var image:Bitmap = background.getChildAt(0) as Bitmap;
               if(image != null)
               {
                  var original:BitmapData = image.bitmapData;
                  var w:int = original.width;
                  var h:int = original.height;
                  var composite:BitmapData = new BitmapData(w,h,true,0);
                  var row:int = 0;
                  while(row < h)
                  {
                     var col:int = 0;
                     while(col < w)
                     {
                        composite.setPixel32(col,row,original.getPixel32(col,row));
                        col++;
                     }
                     row++;
                  }
                  var y:int = 0;
                  while(y < 56 && y + 6 < h)
                  {
                     var base:uint = original.getPixel32(168,y + 6);
                     var x:int = 0;
                     while(x < 160)
                     {
                        var pixel:uint = photo.getPixel32(85 + int(x * 1.5),45 + int(y * 1.5));
                        var fade:Number = Math.max(0,Math.min(1,x / 4,(159 - x) / 30,y / 3,(55 - y) / 3));
                        var a:Number = (pixel >>> 24) / 255 * fade;
                        var red:uint = (pixel >>> 16 & 0xFF) * a + (base >>> 16 & 0xFF) * (1 - a);
                        var green:uint = (pixel >>> 8 & 0xFF) * a + (base >>> 8 & 0xFF) * (1 - a);
                        var blue:uint = (pixel & 0xFF) * a + (base & 0xFF) * (1 - a);
                        composite.setPixel32(x + 8,y + 6,base & 0xFF000000 | red << 16 | green << 8 | blue);
                        x++;
                     }
                     y++;
                  }
                  image.bitmapData = composite;
               }
            }
            i++;
         }
      }
      
      private function theatreFindHelp(container:DisplayObjectContainer) : void
      {
         var i:int = 0;
         while(i < container.numChildren)
         {
            var child:Object = container.getChildAt(i);
            if(child is TextField)
            {
               var value:String = TextField(child).text.toLowerCase();
               // k33: the party help (K33UI_CODO_1466) in any language, or the classic build's English words
               if(TextField(child).text == LocalizeUtil.localize("@K33UI_CODO_1466") || value.indexOf("party") >= 0 || value.indexOf("friend") >= 0)
               {
                  this.theatreHelp.push(child);
               }
            }
            else if(child is DisplayObjectContainer)
            {
               this.theatreFindHelp(child as DisplayObjectContainer);
            }
            i++;
         }
      }
      
      public function theatreInfo(status:String, page:int, pages:int) : void
      {
         if(this.theatreHelp.length == 0)
         {
            this.theatreFindHelp(this);
         }
         status = status == null ? "" : status.replace(/^Page\s+\d+\s+of\s+\d+\s*-\s*/i,"");
         var details:String = LocalizeUtil.replace(LocalizeUtil.localize("@K33UI_THEATRE_PAGE"),[page,pages]);
         if(status != "")
         {
            details += "\n\n" + status;
         }
         if(this.theatreHelp.length == 1)
         {
            TextField(this.theatreHelp[0]).text = LocalizeUtil.localize("@K33UI_THEATRE_HELP") + "\n\n" + details;
            return;
         }
         if(this.theatreHelp.length > 0)
         {
            TextField(this.theatreHelp[0]).text = LocalizeUtil.localize("@K33UI_THEATRE_HELP");
         }
         if(this.theatreHelp.length > 1)
         {
            TextField(this.theatreHelp[1]).text = details;
         }
         var i:int = 2;
         while(i < this.theatreHelp.length)
         {
            TextField(this.theatreHelp[i]).text = "";
            TextField(this.theatreHelp[i]).visible = false;
            i++;
         }
      }
      
      private function theatreFilter(event:Event) : void
      {
         var mode:String = event.currentTarget == this.myRoomListBtn ? "mp" : (event.currentTarget == this.myRoomCreatorBtn ? "zm" : "all");
         CFunctionContainerCommon.setdvar("ui_theatreRequest","filter:" + mode);
      }
      
      public function theatreSelect(mode:String) : void
      {
         var selected:CodoGrowButton = mode == "mp" ? this.myRoomListBtn : (mode == "zm" ? this.myRoomCreatorBtn : this.myPlayListBtn);
         if(this._currentMenuBtn == selected)
         {
            return;
         }
         if(this._currentMenuBtn != null)
         {
            this._currentMenuBtn.selected = false;
         }
         this._currentMenuBtn = selected;
         selected.selected = true;
      }
   }
}

