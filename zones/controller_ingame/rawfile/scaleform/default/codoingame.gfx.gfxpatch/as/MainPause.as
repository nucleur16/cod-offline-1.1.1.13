package
{
   // codol-classic's English codoingame.gfx MainPause (its theatre-playback pause, FFDec's decompile) with Kernel33's
   // gamepad focus navigation (the "k33" parts, written for controller_ingame) for controller.c CtlPauseNav /
   // CtlDeliverSfKeys: key codes 240 up, 241 down, 242 left, 243 right, 244 select; B / Start send Escape.
   // Focus moves to the nearest visible clickable object in the pressed direction; it gets MOUSE_OVER / ROLL_OVER and
   // select sends it MOUSE_DOWN / MOUSE_UP / CLICK. The movie is shared by both languages (k33_english).
   //   On a pause-menu slider row (InGameMenuSliderBtn: Sensitivity, FOV, Brightness) left / right step the slider
   //   (FOV 5 degrees to the next multiple of 5, the others a twentieth of their range) instead of moving the focus,
   //   and select does nothing (a synthetic click would drag the slider to the click's stage x).
   // k33 FOV row: C1's PauseMenuView (multiplayer, and CyborgPauseMenuView for zombies / co-op) lists Sensitivity
   //   (and Brightness in fullscreen) under Options. Each time it builds that list, an FOV slider row of the same
   //   class is put under Sensitivity (the rows below move down a row, the list background grows): labelled with
   //   C1's own key SF_UI_VIDEO_FOV, ranged by GetFOVRange (65..120 with Kernel33 PatchFov), showing whole degrees,
   //   setting cg_fov (archived in config_mp.cfg) as it moves. PauseMenuView itself stays C1's compiled class.
   import codo.common.AsyncTaskManager;
   import codo.common.BaseMain;
   import codo.common.CFunctionContainerCommon;
   import codo.common.CommManagerPause;
   import codo.common.EventManager;
   import codo.common.FPSMemCounter;
   import codo.common.PageManagerPause;
   import codo.constants.AsyncTaskNamePause;
   import codo.constants.BCEventPause;
   import codo.constants.CodoPageViewDBPause;
   import codo.constants.LocalizationInGame;
   import codo.constants.MenuType;
   import codo.utils.ScaleformUtil;
   import codo.common.CFunctionContainerPause;
   import codo.views.PauseMenuView;
   import components.ProgressBar;
   import components.ingamemenu_part.InGameMenuProgressBar;
   import components.ingamemenu_part.InGameMenuSliderBtn;
   import components.BaseButton;
   import components.ingamemenu_part.InGameMenuBtn;
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.display.Shape;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import scaleform.gfx.MouseEventEx;
   import flash.geom.Rectangle;
   import flash.events.MouseEvent;
   import flash.display.Sprite;
   import flash.display.SimpleButton;
   import flash.display.InteractiveObject;
   
   [SWF(frameRate="30",backgroundColor="#999999",width="1280",height="960")]
   public class MainPause extends BaseMain
   {
      
      private static var originalCyborgPauseClass:Class;
      
      private var _fpsMemCounter:FPSMemCounter;
      
      private var theatreWasOpen:Boolean = false;
      
      public function MainPause()
      {
         super();
      }
      
      public static function prepareTheatrePauseViews() : void
      {
         if(originalCyborgPauseClass == null)
         {
            originalCyborgPauseClass = CodoPageViewDBPause.VIEW_PAUSE_CYBORGS.src;
         }
         CodoPageViewDBPause.VIEW_PAUSE_CYBORGS.src = CFunctionContainerCommon.Dvar_GetString("codol_theatre_playback") == "1" ? CodoPageViewDBPause.VIEW_PAUSE.src : originalCyborgPauseClass;
      }
      
      override protected function onBeforeAddToStage() : void
      {
         EventManager.getInstance().addEventListener(BCEventPause.EVT_ON_PAUSE_MENU_CREATED,this.blockTheatreLoadout,false,1000);
         this.pageManager = PageManagerPause.getInstance();
         this.pageManager.pages = CodoPageViewDBPause.PAGES;
         this.menuType = MenuType.PAUSE;
         this.commManager = CommManagerPause.getInstance();
         this.asyncTaskManager = AsyncTaskManager.getInstance();
      }
      
      override protected function onInit() : void
      {
         AsyncTaskNamePause.init();
         AsyncTaskManager.getInstance().GetTaskClass = AsyncTaskNamePause.getClass;
         this.addMemCounter();
         this.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.onKeyDownHandle);
         this.stage.addEventListener(Event.ADDED,this.k33OnAdded); // k33: FOV row
         this.initTheatrePause();
         if(!ScaleformUtil.isScaleform)
         {
            this.commManager.cpp_activate();
         }
      }
      
      private function onKeyDownHandle(e:KeyboardEvent) : void
      {
         if(e.keyCode >= 240 && e.keyCode <= 244) // k33: pad focus navigation
         {
            this.k33Nav(e.keyCode);
            return;
         }
         switch(e.keyCode)
         {
            case 27:
               if(!CFunctionContainerCommon.IsShipBuild())
               {
                  if(this._fpsMemCounter != null)
                  {
                     this._fpsMemCounter.visible = !this._fpsMemCounter.visible;
                  }
               }
               break;
            case 123:
               if(!CFunctionContainerCommon.IsShipBuild())
               {
                  this.visible = !this.visible;
               }
         }
      }
      
      private function addMemCounter() : void
      {
         if(this._fpsMemCounter == null)
         {
            this._fpsMemCounter = new FPSMemCounter(this.commManager);
            this._fpsMemCounter.visible = false;
            this.addChild(this._fpsMemCounter);
         }
      }
      
      private function initTheatrePause() : void
      {
         this.addEventListener(Event.ENTER_FRAME,this.theatrePauseFrame);
         this.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.theatrePauseKey,true,1000);
      }
      
      private function isTheatrePause() : Boolean
      {
         return CFunctionContainerCommon.Dvar_GetString("codol_theatre_playback") == "1";
      }
      
      private function blockTheatreLoadout(event:Event) : void
      {
         if(this.isTheatrePause())
         {
            event.stopImmediatePropagation();
         }
      }
      
      private function resumeTheatre(event:Event = null) : void
      {
         if(!this.isTheatrePause())
         {
            return;
         }
         if(event != null)
         {
            event.stopImmediatePropagation();
         }
         CommManagerPause.getInstance().closePauseMenu(true);
         CFunctionContainerCommon.setdvar("918","0");
      }
      
      private function leaveTheatre(event:Event) : void
      {
         if(!this.isTheatrePause())
         {
            return;
         }
         event.stopImmediatePropagation();
         CFunctionContainerCommon.setdvar("codol_theatre_request","return");
      }
      
      private function theatrePauseKey(event:KeyboardEvent) : void
      {
         if(event.keyCode == 27 && this.isTheatrePause() && CommManagerPause.getInstance().bIsInGameMenuOpen)
         {
            this.resumeTheatre(event);
         }
      }
      
      private function theatrePauseFrame(event:Event) : void
      {
         if(!this.isTheatrePause())
         {
            return;
         }
         var open:Boolean = CommManagerPause.getInstance().bIsInGameMenuOpen;
         if(open)
         {
            this.layoutTheatrePause(PageManagerPause.getInstance()._container);
         }
         else if(this.theatreWasOpen)
         {
            CFunctionContainerCommon.setdvar("918","0");
         }
         this.theatreWasOpen = open;
      }
      
      private function layoutTheatrePause(container:DisplayObjectContainer) : void
      {
         var view:Object = container;
         if("myMiscListBg" in view && view.myMiscListBg != null)
         {
            var menu:Object = view.getCurrentMenu();
            if(menu == null || !menu.theatre)
            {
               CFunctionContainerCommon.setdvar("codol_theatre_pause_diag","creating");
               view.gotoMenu({
                  "id":"theatre",
                  "theatre":true,
                  "parentMenu":null,
                  "lists":[{
                     "id":"end",
                     "label":LocalizationInGame.BackToLobby,
                     "description":LocalizationInGame.BackToLobbyDesc,
                     "onMiscMenu":true
                  }]
               });
               CFunctionContainerCommon.setdvar("codol_theatre_pause_diag","created");
            }
            if(view.myLoadoutListHeader != null)
            {
               view.myLoadoutListHeader.visible = false;
            }
            if(view.myLoadoutListBg != null)
            {
               view.myLoadoutListBg.visible = false;
            }
            if(view.myLoadoutPanelHolder != null)
            {
               view.myLoadoutPanelHolder.visible = false;
            }
            if(view.myQoSBg != null)
            {
               view.myQoSBg.visible = false;
            }
            if(view.myQoSBtn != null)
            {
               view.myQoSBtn.visible = false;
            }
            if(view.myQoSLabelTF != null)
            {
               view.myQoSLabelTF.visible = false;
            }
            if(view.myBackButton != null)
            {
               view.myBackButton.visible = true;
               view.myBackButton.addEventListener(BaseButton.EVT_CLICK,this.resumeTheatre,false,1000);
            }
            var edge:Number = Number(view.myMiscListBg.x);
            var bottom:Number = Number(view.myMiscListBg.y);
            var i:int = 0;
            while(i < container.numChildren)
            {
               var child:DisplayObject = container.getChildAt(i);
               if(child is InGameMenuBtn)
               {
                  var button:InGameMenuBtn = child as InGameMenuBtn;
                  var leave:Boolean = button.x >= edge;
                  button.visible = leave;
                  if(leave)
                  {
                     button.y = view.myMiscListBg.y;
                     button.addEventListener(BaseButton.EVT_CLICK,this.leaveTheatre,false,1000);
                     bottom = button.y + button.height;
                  }
               }
               else if(child is Shape && child.x < edge)
               {
                  child.visible = false;
               }
               i++;
            }
            view.myMiscListBg.height = bottom - view.myMiscListBg.y + 35;
            CFunctionContainerCommon.setdvar("codol_theatre_pause_diag","ready");
            return;
         }
         var j:int = 0;
         while(j < container.numChildren)
         {
            var nested:DisplayObjectContainer = container.getChildAt(j) as DisplayObjectContainer;
            if(nested != null)
            {
               this.layoutTheatrePause(nested);
            }
            j++;
         }
      }

      private var k33Focus:InteractiveObject = null; // k33

      private var k33Ring:Sprite = null; // k33

      // k33: visible, on screen, every ancestor visible
      private function k33Shown(o:DisplayObject) : Boolean
      {
         var p:DisplayObject = o;
         while(p != null && p != this.stage)
         {
            if(!p.visible || p.alpha < 0.05)
            {
               return false;
            }
            p = p.parent;
         }
         return p == this.stage;
      }

      // k33: the clickable objects under c (a button's own children are not searched)
      private function k33Collect(c:DisplayObjectContainer, out:Array) : void
      {
         var i:int = 0;
         var o:DisplayObject = null;
         var io:InteractiveObject = null;
         var r:Rectangle = null;
         var dc:DisplayObjectContainer = null;
         while(i < c.numChildren)
         {
            o = c.getChildAt(i);
            i++;
            if(o == null || !o.visible || o.alpha < 0.05 || o == this.k33Ring)
            {
               continue;
            }
            io = o as InteractiveObject;
            if(io != null && io.mouseEnabled && (io is SimpleButton || io is Sprite && Sprite(io).buttonMode || io.hasEventListener(MouseEvent.CLICK)))
            {
               r = io.getBounds(this.stage);
               if(r.width > 4 && r.height > 4 && r.right > 0 && r.bottom > 0 && r.left < this.stage.stageWidth && r.top < this.stage.stageHeight)
               {
                  out.push(io);
                  continue;
               }
            }
            dc = o as DisplayObjectContainer;
            if(dc != null && dc.mouseChildren)
            {
               this.k33Collect(dc,out);
            }
         }
      }

      private function k33Hover(o:InteractiveObject, over:Boolean) : void
      {
         if(o == null)
         {
            return;
         }
         o.dispatchEvent(new MouseEventEx(over ? MouseEvent.ROLL_OVER : MouseEvent.ROLL_OUT,false));
         o.dispatchEvent(new MouseEventEx(over ? MouseEvent.MOUSE_OVER : MouseEvent.MOUSE_OUT,true));
      }

      private function k33ShowRing() : void
      {
         var r:Rectangle = null;
         if(this.k33Ring == null)
         {
            this.k33Ring = new Sprite();
            this.k33Ring.mouseEnabled = false;
            this.k33Ring.mouseChildren = false;
            this.k33Ring.addEventListener(Event.ENTER_FRAME,this.k33RingFrame);
         }
         this.stage.addChild(this.k33Ring);
         this.k33Ring.graphics.clear();
         if(this.k33Focus == null)
         {
            return;
         }
         // no frame of our own: the focused button shows its own hover state (k33Hover), as with the mouse
         this.k33Ring.visible = true;
      }

      // k33: the frame goes when the focused object does (menu closed, panel changed)
      private function k33RingFrame(e:Event) : void
      {
         if(this.k33Focus == null || this.stage == null || !this.k33Shown(this.k33Focus))
         {
            this.k33Ring.visible = false;
            this.k33Focus = null;
         }
      }

      // k33: one line per key for Kernel33's gpad_debug (dvar k33nav)
      private function k33Log(code:uint, n:int, cur:InteractiveObject, next:InteractiveObject) : void
      {
         var r:Rectangle = null;
         var msg:String = "k33nav " + code + " candidates " + n + " from " + (cur != null ? cur.name : "-");
         if(next != null)
         {
            r = next.getBounds(this.stage);
            msg += " to " + next.name + " (" + next + ") at " + Math.round(r.x) + "," + Math.round(r.y) + " " + Math.round(r.width) + "x" + Math.round(r.height);
         }
         try
         {
            CFunctionContainerCommon.setdvar("k33nav",msg);
         }
         catch(e:Error)
         {
         }
      }

      private function k33Nav(code:uint) : void
      {
         var list:Array = [];
         var cur:InteractiveObject = null;
         var next:InteractiveObject = null;
         var o:InteractiveObject = null;
         var cr:Rectangle = null;
         var r:Rectangle = null;
         var dx:Number = 0;
         var dy:Number = 0;
         var vx:Number = NaN;
         var vy:Number = NaN;
         var along:Number = NaN;
         var perp:Number = NaN;
         var score:Number = NaN;
         var best:Number = Number.MAX_VALUE;
         var i:int = 0;
         if(this.stage == null)
         {
            return;
         }
         this.k33Collect(this.stage,list);
         if(this.k33Focus != null && list.indexOf(this.k33Focus) >= 0)
         {
            cur = this.k33Focus;
         }
         if(cur is InGameMenuSliderBtn && code >= 242)
         {
            this.k33Log(code,list.length,cur,cur);
            if(code != 244)
            {
               this.k33StepSlider(cur as InGameMenuSliderBtn,code == 243 ? 1 : -1);
            }
            return;
         }
         if(code == 244)
         {
            this.k33Log(code,list.length,cur,cur);
            if(cur != null)
            {
               // MouseEventEx (left button, buttonIdx 0): C1's BaseButton reads cEvt as MouseEventEx
               cur.dispatchEvent(new MouseEventEx(MouseEvent.MOUSE_DOWN,true));
               cur.dispatchEvent(new MouseEventEx(MouseEvent.MOUSE_UP,true));
               cur.dispatchEvent(new MouseEventEx(MouseEvent.CLICK,true));
            }
            return;
         }
         if(cur == null)
         {
            i = 0;
            while(i < list.length)
            {
               o = list[i];
               r = o.getBounds(this.stage);
               score = r.y * 4 + r.x;
               if(score < best)
               {
                  best = score;
                  next = o;
               }
               i++;
            }
         }
         else
         {
            if(code == 240)
            {
               dy = -1;
            }
            else if(code == 241)
            {
               dy = 1;
            }
            else if(code == 242)
            {
               dx = -1;
            }
            else
            {
               dx = 1;
            }
            cr = cur.getBounds(this.stage);
            i = 0;
            while(i < list.length)
            {
               o = list[i];
               if(o != cur)
               {
                  r = o.getBounds(this.stage);
                  vx = r.x + r.width / 2 - (cr.x + cr.width / 2);
                  vy = r.y + r.height / 2 - (cr.y + cr.height / 2);
                  along = vx * dx + vy * dy;
                  if(along > 2)
                  {
                     perp = Math.abs(vx * dy - vy * dx);
                     score = along + perp * 2;
                     if(score < best)
                     {
                        best = score;
                        next = o;
                     }
                  }
               }
               i++;
            }
         }
         this.k33Log(code,list.length,cur,next);
         if(next == null)
         {
            return;
         }
         if(cur != null)
         {
            this.k33Hover(cur,false);
         }
         this.k33Focus = next;
         this.k33Hover(next,true);
         this.k33ShowRing();
      }

      private static const K33_ROW:Number = 30; // k33: InGameMenuBtn's row height (its height getter)

      private var k33FovRow:InGameMenuSliderBtn = null; // k33

      private var k33FovPending:PauseMenuView = null; // k33

      private var k33FovAnchor:InGameMenuSliderBtn = null; // k33: the Sensitivity row the FOV row sits under

      // k33: a row added to a PauseMenuView: put the FOV row in on the next frame, once the whole list is built
      private function k33OnAdded(e:Event) : void
      {
         var row:InGameMenuBtn = e.target as InGameMenuBtn;
         if(row == null || row == this.k33FovRow || !(row.parent is PauseMenuView))
         {
            return;
         }
         if(this.k33FovPending == null)
         {
            this.addEventListener(Event.ENTER_FRAME,this.k33FovFrame);
         }
         this.k33FovPending = row.parent as PauseMenuView;
      }

      private function k33FovFrame(e:Event) : void
      {
         var view:PauseMenuView = this.k33FovPending;
         this.removeEventListener(Event.ENTER_FRAME,this.k33FovFrame);
         this.k33FovPending = null;
         try
         {
            this.k33AddFovRow(view);
         }
         catch(err:Error)
         {
         }
      }

      // k33: the menu rows of the view (C1's _menuInstances are private: its InGameMenuBtn children), top to bottom
      private function k33Rows(view:DisplayObjectContainer) : Array
      {
         var rows:Array = [];
         var i:int = 0;
         var row:InGameMenuBtn = null;
         while(i < view.numChildren)
         {
            row = view.getChildAt(i) as InGameMenuBtn;
            if(row != null && row != this.k33FovRow)
            {
               rows.push(row);
            }
            i++;
         }
         rows.sortOn("y",Array.NUMERIC);
         return rows;
      }

      private function k33AddFovRow(view:PauseMenuView) : void
      {
         var rows:Array = null;
         var sens:InGameMenuSliderBtn = null;
         var row:InGameMenuBtn = null;
         var cls:Class = null;
         var fov:InGameMenuSliderBtn = null;
         var slider:InGameMenuProgressBar = null;
         var range:Object = null;
         var bg:DisplayObject = null;
         var i:int = 0;
         if(view == null || view.stage == null)
         {
            return;
         }
         rows = this.k33Rows(view);
         i = 0;
         while(i < rows.length && sens == null)
         {
            sens = rows[i] as InGameMenuSliderBtn;
            i++;
         }
         if(sens != null && sens == this.k33FovAnchor && this.k33FovRow != null && this.k33FovRow.parent == view)
         {
            return; // this list already has its FOV row
         }
         if(this.k33FovRow != null && this.k33FovRow.parent != null)
         {
            this.k33FovRow.parent.removeChild(this.k33FovRow);
         }
         this.k33FovRow = null;
         this.k33FovAnchor = null;
         if(sens == null)
         {
            return;
         }
         cls = Object(sens).constructor as Class;
         fov = new cls() as InGameMenuSliderBtn;
         if(fov == null)
         {
            return;
         }
         // the rows under Sensitivity in its column move down a row; the list background grows by one
         i = 0;
         while(i < rows.length)
         {
            row = rows[i] as InGameMenuBtn;
            if(row.y > sens.y && Math.abs(row.x - sens.x) < 1)
            {
               row.y += K33_ROW;
            }
            i++;
         }
         bg = view["myMiscListBg"] as DisplayObject;
         if(bg != null && Math.abs(bg.x + 10 - sens.x) < 1)
         {
            bg.height += K33_ROW;
         }
         fov.x = sens.x;
         fov.y = sens.y + K33_ROW;
         this.k33FovRow = fov; // before addChild: its ADDED event must not schedule another FOV row
         this.k33FovAnchor = sens;
         view.addChild(fov);
         fov.label = "@SF_UI_VIDEO_FOV";
         slider = fov.mySlider;
         try
         {
            range = CFunctionContainerPause.GetFOVRange();
         }
         catch(err:Error)
         {
         }
         slider.minVal = range != null ? Number(range.min) : 65;
         slider.maxVal = range != null ? Number(range.max) : 120;
         slider.value = Math.round(CFunctionContainerCommon.Dvar_GetFloat("cg_fov"));
         slider.myCurrentTF.text = String(Math.round(slider.value));
         slider.addEventListener(ProgressBar.EVT_VALUE_CHANGE,this.k33OnFovChange);
      }

      private function k33OnFovChange(e:Event) : void
      {
         var slider:InGameMenuProgressBar = e.target as InGameMenuProgressBar;
         var fov:int = int(Math.round(slider.value));
         slider.myCurrentTF.text = String(fov);
         if(fov != int(Math.round(CFunctionContainerCommon.Dvar_GetFloat("cg_fov"))))
         {
            CFunctionContainerCommon.setdvar("cg_fov",String(fov));
         }
      }

      // k33: pad left / right on a slider row. Its range: FOV from GetFOVRange; C1's own sliders are Sensitivity
      // (the first slider of the list) and Brightness (any other)
      private function k33StepSlider(row:InGameMenuSliderBtn, dir:int) : void
      {
         var range:Object = null;
         var step:Number = NaN;
         var cur:Number = NaN;
         var next:Number = NaN;
         var rows:Array = null;
         var first:InGameMenuSliderBtn = null;
         var i:int = 0;
         if(row == null || row.mySlider == null || row.parent == null)
         {
            return;
         }
         try
         {
            if(row == this.k33FovRow)
            {
               range = CFunctionContainerPause.GetFOVRange();
            }
            else
            {
               rows = this.k33Rows(row.parent);
               while(i < rows.length && first == null)
               {
                  first = rows[i] as InGameMenuSliderBtn;
                  i++;
               }
               range = row == first ? CFunctionContainerPause.GetSensitivityRange() : CFunctionContainerPause.GetGammaRange();
            }
         }
         catch(err:Error)
         {
         }
         if(range == null || !(Number(range.max) > Number(range.min)))
         {
            return;
         }
         cur = row.mySlider.value;
         if(isNaN(cur))
         {
            cur = Number(range.min);
         }
         if(row == this.k33FovRow)
         {
            step = 5;
            next = dir > 0 ? (Math.floor(cur / step + 0.001) + 1) * step : (Math.ceil(cur / step - 0.001) - 1) * step;
         }
         else
         {
            step = (Number(range.max) - Number(range.min)) / 20;
            next = cur + dir * step;
         }
         row.mySlider.value = Math.max(Number(range.min),Math.min(Number(range.max),next));
      }
   }
}
