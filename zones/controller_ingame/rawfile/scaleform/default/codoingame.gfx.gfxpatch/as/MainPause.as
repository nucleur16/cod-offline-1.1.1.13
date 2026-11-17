package
{
   // codol-classic's English codoingame.gfx MainPause (its theatre-playback pause, FFDec's decompile) with Kernel33's
   // gamepad focus navigation (the "k33" parts, written for controller_ingame) for controller.c CtlPauseNav /
   // CtlDeliverSfKeys: key codes 240 up, 241 down, 242 left, 243 right, 244 select; B / Start send Escape.
   // Focus moves to the nearest visible clickable object in the pressed direction; it gets MOUSE_OVER / ROLL_OVER and
   // select sends it MOUSE_DOWN / MOUSE_UP / CLICK. The movie is shared by both languages (loc_english).
   //   On a pause-menu slider row (InGameMenuSliderBtn: Sensitivity, FOV, Brightness) left / right step the slider
   //   (FOV 5 degrees to the next multiple of 5, the others a twentieth of their range) instead of moving the focus,
   //   and select does nothing (a synthetic click would drag the slider to the click's stage x).
   //   Mouse: C1's slider drag does not follow the mouse (InGameMenuProgressBar's move handler uses a constant x),
   //   so pressing or dragging on a pause-menu slider row sets the value from the mouse's place on the bar here.
   // k33 FOV row: C1's PauseMenuView (multiplayer, and CyborgPauseMenuView for zombies / co-op) lists Sensitivity
   //   (and Brightness in fullscreen) under Options. Each time it builds that list, an FOV slider row of the same
   //   class is put under Sensitivity (the rows below move down a row, the list background grows): labelled with
   //   C1's own key SF_UI_VIDEO_FOV, ranged by GetFOVRange (65..120 with Kernel33 PatchFov), showing whole degrees,
   //   setting cg_fov (archived in config_mp.cfg) as it moves. PauseMenuView itself stays C1's compiled class.
   // k33 style rows (docs/UI_STYLE_OPTIONS_20261112.md): under the last slider of that list go Button icons and HUD
   //   style (and Language, which K33_STYLE_LANGUAGE_ROW switches off for now: its row and its line under the list are
   //   not made), each a slider row of the same class used as a stepped chooser (one stop per choice, the choice's
   //   name where a slider shows its number). Each reads and sets a Kernel33 dvar (CFunctionContainerCommon):
   //     Language      loc_languageName, one of loc_languageList ("en;zh"); names K33UI_LANG_NAME_<ID>
   //     Button icons  gpad_glyphs 0..6 (shown when the controller module is there: gpad_info is set); K33UI_GLYPHS_*
   //     HUD style     cg_hudStyle, one of the numbers of cg_hudStyleList ("0:stock;2:v26"); K33UI_HUD_STYLE_<ID>
   //     HUD sway      cg_hudSway, Off / On (shown when the dvar is there); K33UI_HUD_SWAY_OFF / _ON
   //   A row whose list is empty (an older Kernel33.dll) is not added. A name without a localize entry shows the raw
   //   id. A value the list does not have shows the first choice. The rows apply at once (setdvar) and show what the
   //   dvar holds: it is read back a few frames after a change, so a value the engine refused does not stay on show.
   //   Mouse: drag or click along the bar; a click on the row's label or on the name steps to the next choice.
   //   Pad: left / right step (wrapping), select steps forward.
   //   With the Language row, loc_languageState 1 / 2 shows a line under the list (switching / restart to apply);
   //   its text is translated before it is set (a code-made field set to "@KEY" comes back from C1's translator in
   //   the default format and draws as black boxes). When loc_languageSerial
   //   changes (a live switch finished) the open list is built again (PauseMenuView.gotoMenu of its current menu) so
   //   every row is translated again, with the view's fixed labels; the pad focus stays on the row it was on.
   //   The same k33Style* functions are in codo.gfx's OptionView_video (tests/ui_style_rows_test.js runs both).
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
   import codo.utils.LocalizeUtil;
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
   import flash.text.TextField;
   import flash.text.TextFieldAutoSize;
   
   [SWF(frameRate="30",backgroundColor="#999999",width="1280",height="960")]
   public class MainPause extends BaseMain
   {
      
      private static var originalCyborgPauseClass:Class;
      
      private var _fpsMemCounter:FPSMemCounter;
      
      private var theatreWasOpen:Boolean = false;

      private var survivalClassMenuSeenOpen:Boolean = false;
      
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
         CodoPageViewDBPause.VIEW_PAUSE_CYBORGS.src = CFunctionContainerCommon.Dvar_GetString("ui_theatrePlayback") == "1" ? CodoPageViewDBPause.VIEW_PAUSE.src : originalCyborgPauseClass;
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
         this.stage.addEventListener(MouseEvent.MOUSE_DOWN,this.k33SliderMouseDown,true,1000); // k33: slider drag
         this.initTheatrePause();
         this.addEventListener(Event.ENTER_FRAME,this.survivalClassMenuFrame);
         this.addEventListener(Event.ENTER_FRAME,this.k33StyleFrame); // k33: style rows
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
         return CFunctionContainerCommon.Dvar_GetString("ui_theatrePlayback") == "1";
      }

      // The existing pause controller owns class display, selection and its
      // normal immediate indicator. Only report a cancelled paid selector:
      // closing this Scaleform menu otherwise has no GSC menu response.
      private function survivalClassMenuFrame(event:Event) : void
      {
         if(CFunctionContainerCommon.Dvar_GetString("g_gametype") != "survival" ||
            CFunctionContainerCommon.Dvar_GetString("survival_class_purchase") != "1")
         {
            this.survivalClassMenuSeenOpen = false;
            return;
         }
         if(CommManagerPause.getInstance().bIsInGameMenuOpen)
         {
            this.survivalClassMenuSeenOpen = true;
         }
         else if(this.survivalClassMenuSeenOpen)
         {
            this.survivalClassMenuSeenOpen = false;
            CFunctionContainerCommon.setdvar("survival_class_purchase","0");
            CFunctionContainerCommon.ScriptMenuResponse("changeclass","back");
         }
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
         CFunctionContainerCommon.setdvar("ui_theatreRequest","return");
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
               CFunctionContainerCommon.setdvar("ui_theatrePauseDiag","creating");
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
               CFunctionContainerCommon.setdvar("ui_theatrePauseDiag","created");
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
            CFunctionContainerCommon.setdvar("ui_theatrePauseDiag","ready");
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

      // k33: one line per key for Kernel33's gpad_debug (dvar gpad_navLog)
      private function k33Log(code:uint, n:int, cur:InteractiveObject, next:InteractiveObject) : void
      {
         var r:Rectangle = null;
         var msg:String = "gpad_navLog " + code + " candidates " + n + " from " + (cur != null ? cur.name : "-");
         if(next != null)
         {
            r = next.getBounds(this.stage);
            msg += " to " + next.name + " (" + next + ") at " + Math.round(r.x) + "," + Math.round(r.y) + " " + Math.round(r.width) + "x" + Math.round(r.height);
         }
         try
         {
            CFunctionContainerCommon.setdvar("gpad_navLog",msg);
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
            if(code != 244 || this.k33StyleOf(cur) != null) // k33: select on a style row steps forward
            {
               this.k33StepSlider(cur as InGameMenuSliderBtn,code == 242 ? -1 : 1);
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
         if(row == null || row == this.k33FovRow || this.k33StyleBusy || !(row.parent is PauseMenuView))
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
         try
         {
            this.k33AddStyleRows(view); // k33: style rows, under the FOV row
         }
         catch(err2:Error)
         {
            this.k33StyleBusy = false;
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
            if(row != null && row != this.k33FovRow && this.k33StyleOf(row) == null)
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

      // k33: a slider row's range. FOV from GetFOVRange; C1's own sliders are Sensitivity (the first slider of the
      // list) and Brightness (any other)
      private function k33SliderRange(row:InGameMenuSliderBtn) : Object
      {
         var range:Object = null;
         var rows:Array = null;
         var first:InGameMenuSliderBtn = null;
         var i:int = 0;
         if(row == null || row.mySlider == null || row.parent == null)
         {
            return null;
         }
         try
         {
            if(row == this.k33FovRow)
            {
               range = CFunctionContainerPause.GetFOVRange();
            }
            else if(this.k33StyleOf(row) != null) // k33: a style row has one stop per choice
            {
               range = {
                  "min":0,
                  "max":Math.max(1,this.k33StyleOf(row).opts.length - 1)
               };
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
            return null;
         }
         return range;
      }

      // k33: pad left / right on a slider row: FOV 5 degrees to the next multiple of 5, the others a twentieth
      private function k33StepSlider(row:InGameMenuSliderBtn, dir:int) : void
      {
         var range:Object = this.k33SliderRange(row);
         var step:Number = NaN;
         var cur:Number = NaN;
         var next:Number = NaN;
         var style:Object = this.k33StyleOf(row);
         if(style != null) // k33: a style row steps one choice, wrapping
         {
            this.k33StylePick(style,this.k33StyleStep(this.k33StyleShown(style),style.opts.length,dir));
            return;
         }
         if(range == null)
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

      private var k33DragRow:InGameMenuSliderBtn = null; // k33

      // k33: mouse on a pause-menu slider row. C1's InGameMenuProgressBar drag ignores the mouse position (its
      // move handler uses a constant x), so the bar does not follow the mouse; press and drag are handled here
      // instead: the value is the mouse's position along the bar.
      private function k33SliderMouseDown(e:MouseEvent) : void
      {
         var o:DisplayObject = e.target as DisplayObject;
         var row:InGameMenuSliderBtn = null;
         while(o != null && o != this.stage && row == null)
         {
            row = o as InGameMenuSliderBtn;
            o = o.parent;
         }
         if(row == null || row.mySlider == null || !(row.parent is PauseMenuView) || this.k33SliderRange(row) == null)
         {
            return;
         }
         e.stopImmediatePropagation();
         if(this.k33StyleOf(row) != null && row.mySlider.myBg != null) // k33: off the bar, a click steps to the next choice
         {
            var bar:Rectangle = row.mySlider.myBg.getBounds(this.stage);
            if(e.stageX < bar.x - 4 || e.stageX > bar.right + 4)
            {
               this.k33StepSlider(row,1);
               return;
            }
         }
         this.k33DragRow = row;
         this.stage.addEventListener(MouseEvent.MOUSE_MOVE,this.k33SliderMouseMove,true,1000);
         this.stage.addEventListener(MouseEvent.MOUSE_UP,this.k33SliderMouseUp,true,1000);
         this.addEventListener(Event.ENTER_FRAME,this.k33SliderDragFrame);
         this.k33SliderFromMouse(e.stageX);
      }

      // k33: while the button is held the bar follows the cursor every frame (in a match C1 does not always pass
      // the movie mouse-move events)
      private function k33SliderDragFrame(e:Event) : void
      {
         if(this.k33DragRow == null || this.k33DragRow.stage == null)
         {
            this.k33SliderDragEnd();
            return;
         }
         this.k33SliderFromMouse(this.stage.mouseX);
      }

      private function k33SliderDragEnd() : void
      {
         this.stage.removeEventListener(MouseEvent.MOUSE_MOVE,this.k33SliderMouseMove,true);
         this.stage.removeEventListener(MouseEvent.MOUSE_UP,this.k33SliderMouseUp,true);
         this.removeEventListener(Event.ENTER_FRAME,this.k33SliderDragFrame);
         this.k33DragRow = null;
      }

      private function k33SliderMouseMove(e:MouseEvent) : void
      {
         e.stopImmediatePropagation();
         this.k33SliderFromMouse(e.stageX);
      }

      private function k33SliderMouseUp(e:MouseEvent) : void
      {
         e.stopImmediatePropagation();
         this.k33SliderFromMouse(e.stageX);
         this.k33SliderDragEnd();
      }

      private function k33SliderFromMouse(stageX:Number) : void
      {
         var row:InGameMenuSliderBtn = this.k33DragRow;
         var range:Object = row != null ? this.k33SliderRange(row) : null;
         var bar:Rectangle = null;
         var f:Number = NaN;
         var v:Number = NaN;
         if(range == null || row.mySlider.myBg == null)
         {
            return;
         }
         bar = row.mySlider.myBg.getBounds(this.stage);
         if(!(bar.width > 1))
         {
            return;
         }
         f = Math.max(0,Math.min(1,(stageX - bar.x) / bar.width));
         v = Number(range.min) + f * (Number(range.max) - Number(range.min));
         if(row == this.k33FovRow)
         {
            v = Math.round(v);
         }
         if(this.k33StyleOf(row) != null) // k33: the nearest choice
         {
            this.k33StylePick(this.k33StyleOf(row),Math.round(v));
            return;
         }
         row.mySlider.value = v;
      }

      // k33 style rows (Language, Button icons, HUD style, HUD sway): see the header comment
      private static const K33_STYLE_KINDS:Array = ["lang","glyph","hud","sway"]; // k33

      private static const K33_STYLE_LABELS:Object = {
         "lang":"@K33UI_LANGUAGE",
         "glyph":"@K33UI_GLYPHS",
         "hud":"@K33UI_HUD_STYLE",
         "sway":"@K33UI_HUD_SWAY"
      }; // k33

      private static const K33_STYLE_HOLD:int = 12; // k33: frames a new choice stays on show before the dvar is read back

      private static const K33_STYLE_POLL:int = 6; // k33: frames between looks at the dvars

      private static const K33_SLIDER_W:Number = 195; // k33: menuSliderButton's mySlider.width (bar + value text)

      private static const K33_STYLE_BAR:Number = 60; // k33: the shortest bar a long name may leave

      private var k33StyleRows:Array = []; // k33: [{kind, row, opts, names, shown, hold}]

      private var k33StyleBusy:Boolean = false; // k33: our own rows are being added

      private var k33StyleSync:Boolean = false; // k33: a slider is being moved to what its dvar holds

      private var k33StyleAnchor:InGameMenuSliderBtn = null; // k33: the slider row the style rows sit under

      private var k33StyleView:PauseMenuView = null; // k33

      private var k33StyleNote:TextField = null; // k33: "restart to apply" under the list

      private var k33StyleNoteKey:String = ""; // k33

      private var k33StyleSerial:int = -1; // k33: loc_languageSerial as last seen

      private var k33StyleFrames:int = 0; // k33

      private var k33StyleRefocus:String = ""; // k33: the kind whose row gets the pad focus back after a rebuild

      // k33 style shared: begin (the same text in MainPause.as and OptionView_video.as; tests/ui_style_rows_test.js)
      private static const K33_GLYPH_NAMES:Array = ["AUTO","XBOX","PS","NINTENDO","GENERIC","TEXT","KBM"]; // gpad_glyphs 0..6

      private static const K33_STYLE_DVARS:Object = {
         "lang":"loc_languageName",
         "glyph":"gpad_glyphs",
         "hud":"cg_hudStyle",
         "sway":"cg_hudSway"
      };

      // k33: the Language row is switched off here, in one place for both movies: with false the row and its
      // "Switching language..." / "Restart the game to apply the language" line are not made and none of the
      // loc_language* dvars is read for a row (the language is still changed with loc_languageName / loc_setLanguage
      // in the console, and a finished switch still translates the remaining rows: loc_languageSerial). true
      // brings the row back; its keys (K33UI_LANGUAGE, K33UI_LANG_*) stay in the string zones for that.
      private static const K33_STYLE_LANGUAGE_ROW:Boolean = true;

      private function k33StyleTrim(s:String) : String
      {
         var a:int = 0;
         var b:int = 0;
         if(s == null)
         {
            return "";
         }
         s = String(s);
         b = s.length;
         while(a < b && s.charCodeAt(a) <= 32)
         {
            a++;
         }
         while(b > a && s.charCodeAt(b - 1) <= 32)
         {
            b--;
         }
         return s.substring(a,b);
      }

      // k33: digits only (a style number of cg_hudStyleList)
      private function k33StyleDigits(s:String) : Boolean
      {
         var i:int = 0;
         if(s == null || s.length == 0 || s.length > 6)
         {
            return false;
         }
         while(i < s.length)
         {
            if(s.charCodeAt(i) < 48 || s.charCodeAt(i) > 57)
            {
               return false;
            }
            i++;
         }
         return true;
      }

      // k33: a row's choices, [{v: the value its dvar takes, key: the localize key of its name, raw: the text shown
      // when that key has no entry}]. "lang": the ids of loc_languageList ("en;zh"); "hud": the "number:id" pairs of
      // cg_hudStyleList ("0:stock;2:v26"); "glyph": gpad_glyphs 0..6, when the controller module has published
      // gpad_info. An empty list (the dvar is not there: an older Kernel33.dll) gives [], and the row is left out.
      // "lang" gives [] while K33_STYLE_LANGUAGE_ROW is false.
      // Blank, malformed and repeated entries are dropped.
      private function k33StyleOptions(kind:String) : Array
      {
         var out:Array = [];
         var list:String = null;
         var parts:Array = null;
         var item:String = null;
         var v:String = null;
         var id:String = null;
         var at:int = 0;
         var i:int = 0;
         var j:int = 0;
         if(kind == "lang" && !K33_STYLE_LANGUAGE_ROW)
         {
            return out; // the Language row is switched off
         }
         if(kind == "sway")
         {
            // cg_hudSway 0 / 1 (hud_style.c: the HUD moves with the view, movement and camera shake, every style).
            // It is an int dvar: C1's Dvar_GetString reads an int's value as a string pointer (live: a crash at
            // 0x007739B0 reading 0x00000001), so the row comes with the HUD style row, whose cg_hudStyleList the
            // same hud_style.c registers: a Kernel33.dll without HUD styles has neither
            if(this.k33StyleOptions("hud").length == 0)
            {
               return out;
            }
            out.push({
               "v":"0",
               "key":"K33UI_HUD_SWAY_OFF",
               "raw":"off"
            });
            out.push({
               "v":"1",
               "key":"K33UI_HUD_SWAY_ON",
               "raw":"on"
            });
            return out;
         }
         if(kind == "glyph")
         {
            list = CFunctionContainerCommon.Dvar_GetString("gpad_info");
            if(list == null || list == "")
            {
               return out;
            }
            while(i < K33_GLYPH_NAMES.length)
            {
               out.push({
                  "v":String(i),
                  "key":"K33UI_GLYPHS_" + K33_GLYPH_NAMES[i],
                  "raw":String(i)
               });
               i++;
            }
            return out;
         }
         list = CFunctionContainerCommon.Dvar_GetString(kind == "lang" ? "loc_languageList" : "cg_hudStyleList");
         if(list == null || list == "")
         {
            return out;
         }
         parts = String(list).split(";");
         while(i < parts.length)
         {
            item = this.k33StyleTrim(parts[i]);
            i++;
            if(kind == "hud")
            {
               at = item.indexOf(":");
               if(at < 1)
               {
                  continue;
               }
               v = this.k33StyleTrim(item.substring(0,at));
               id = this.k33StyleTrim(item.substring(at + 1));
               if(!this.k33StyleDigits(v) || id == "")
               {
                  continue;
               }
               v = String(parseInt(v,10));
            }
            else
            {
               v = item;
               id = item;
               if(id == "")
               {
                  continue;
               }
            }
            j = 0;
            while(j < out.length && out[j].v != v)
            {
               j++;
            }
            if(j < out.length)
            {
               continue;
            }
            out.push({
               "v":v,
               "key":(kind == "hud" ? "K33UI_HUD_STYLE_" : "K33UI_LANG_NAME_") + id.toUpperCase(),
               "raw":id
            });
         }
         return out;
      }

      // k33: a choice's name in the loaded language; C1's translator gives "@KEY" back for a key it does not have
      private function k33StyleName(opt:Object) : String
      {
         var s:String = LocalizeUtil.localize("@" + opt.key);
         if(s == null || s == "" || s.charAt(0) == "@")
         {
            return String(opt.raw);
         }
         return s;
      }

      // k33: what the row's dvar holds, as a choice's v
      private function k33StyleCurrent(kind:String) : String
      {
         var s:String = null;
         if(kind == "lang")
         {
            s = CFunctionContainerCommon.Dvar_GetString(K33_STYLE_DVARS[kind]);
            return s == null ? "" : this.k33StyleTrim(s);
         }
         return String(CFunctionContainerCommon.Dvar_GetInt(K33_STYLE_DVARS[kind]));
      }

      // k33: the choice holding `cur`; a value the list does not have shows the first choice
      private function k33StyleIndex(opts:Array, cur:String) : int
      {
         var i:int = 0;
         while(i < opts.length)
         {
            if(String(opts[i].v).toLowerCase() == String(cur).toLowerCase())
            {
               return i;
            }
            i++;
         }
         return 0;
      }

      // k33: the next (dir 1) or previous (dir -1) choice, wrapping at both ends
      private function k33StyleStep(index:int, count:int, dir:int) : int
      {
         if(count < 1)
         {
            return 0;
         }
         return ((index + dir) % count + count) % count;
      }

      // k33: set the row's dvar to choice `index` (setdvar; nothing when it already holds it) -> whether it was set
      private function k33StyleSet(kind:String, opts:Array, index:int) : Boolean
      {
         if(index < 0 || index >= opts.length || String(opts[index].v).toLowerCase() == this.k33StyleCurrent(kind).toLowerCase())
         {
            return false;
         }
         CFunctionContainerCommon.setdvar(K33_STYLE_DVARS[kind],String(opts[index].v));
         return true;
      }

      // k33: the choices' values in order: a row is built again when this changes
      private function k33StyleSig(opts:Array) : String
      {
         var s:String = "";
         var i:int = 0;
         while(i < opts.length)
         {
            s += opts[i].v + ":" + opts[i].key + ";";
            i++;
         }
         return s;
      }
      // k33 style shared: end

      // k33: the style row record of a row (null: not one of ours)
      private function k33StyleOf(o:Object) : Object
      {
         var i:int = 0;
         while(i < this.k33StyleRows.length)
         {
            if(this.k33StyleRows[i].row == o)
            {
               return this.k33StyleRows[i];
            }
            i++;
         }
         return null;
      }

      private function k33StyleShown(rec:Object) : int
      {
         return int(rec.shown);
      }

      // k33: show choice `index` on the row without setting its dvar
      private function k33StyleShow(rec:Object, index:int) : void
      {
         var slider:InGameMenuProgressBar = rec.row.mySlider;
         index = Math.max(0,Math.min(rec.opts.length - 1,index));
         this.k33StyleSync = true;
         try
         {
            slider.value = index;
         }
         catch(err:Error)
         {
         }
         this.k33StyleSync = false;
         rec.shown = index;
         slider.myCurrentTF.text = String(rec.names[index]);
      }

      // k33: the player's choice: show it and set the dvar; the dvar is read back K33_STYLE_HOLD frames later
      private function k33StylePick(rec:Object, index:int) : void
      {
         index = Math.max(0,Math.min(rec.opts.length - 1,index));
         this.k33StyleShow(rec,index);
         if(this.k33StyleSet(rec.kind,rec.opts,index))
         {
            rec.hold = K33_STYLE_HOLD;
         }
      }

      // k33: a style slider moved. Our own moves (k33StyleSync) are finished by k33StyleShow; any other (C1's own
      // slider handler) takes the nearest choice
      private function k33OnStyleChange(e:Event) : void
      {
         var slider:InGameMenuProgressBar = e.target as InGameMenuProgressBar;
         var rec:Object = slider != null ? this.k33StyleOf(slider.parent) : null;
         if(rec == null || this.k33StyleSync)
         {
            return;
         }
         this.k33StylePick(rec,int(Math.round(slider.value)));
      }

      private function k33StyleRemove() : void
      {
         var i:int = 0;
         var row:InGameMenuSliderBtn = null;
         while(i < this.k33StyleRows.length)
         {
            row = this.k33StyleRows[i].row;
            if(row.mySlider != null)
            {
               row.mySlider.removeEventListener(ProgressBar.EVT_VALUE_CHANGE,this.k33OnStyleChange);
            }
            if(row.parent != null)
            {
               row.parent.removeChild(row);
            }
            i++;
         }
         this.k33StyleRows = [];
         if(this.k33StyleNote != null && this.k33StyleNote.parent != null)
         {
            this.k33StyleNote.parent.removeChild(this.k33StyleNote);
         }
         this.k33StyleNote = null;
         this.k33StyleNoteKey = "";
         this.k33StyleAnchor = null;
         this.k33StyleView = null;
      }

      // k33: the names of the row's choices; the bar is shortened so the longest fits in the row (the slider is
      // K33_SLIDER_W wide in all: bar, gap, value text), one stop per choice, and the dvar's choice is shown
      private function k33StyleSetup(rec:Object) : void
      {
         var slider:InGameMenuProgressBar = rec.row.mySlider;
         var longest:String = "";
         var name:String = null;
         var w:Number = -1;
         var total:Number = K33_SLIDER_W;
         var off:Number = NaN;
         var i:int = 0;
         rec.names = [];
         while(i < rec.opts.length)
         {
            name = this.k33StyleName(rec.opts[i]);
            rec.names.push(name);
            slider.myCurrentTF.text = name;
            if(slider.myCurrentTF.width > w)
            {
               w = slider.myCurrentTF.width;
               longest = name;
            }
            i++;
         }
         slider.myCurrentTF.text = longest;
         off = slider.myCurrentTF.x - slider.myBg.width;
         if(total - off - slider.myCurrentTF.width < K33_STYLE_BAR)
         {
            total = K33_STYLE_BAR + off + slider.myCurrentTF.width;
         }
         slider.width = total;
         slider.minVal = 0;
         slider.maxVal = Math.max(1,rec.opts.length - 1);
         rec.shown = -1;
         rec.hold = 0;
         this.k33StyleShow(rec,this.k33StyleIndex(rec.opts,this.k33StyleCurrent(rec.kind)));
      }

      // k33: the style rows go under the lowest slider row of the Options list (C1's Brightness, else the FOV row);
      // the rows below move down, the list background grows. A list without sliders (a submenu) gets none.
      private function k33AddStyleRows(view:PauseMenuView) : void
      {
         var rows:Array = null;
         var first:InGameMenuSliderBtn = null;
         var anchor:InGameMenuSliderBtn = null;
         var s:InGameMenuSliderBtn = null;
         var row:InGameMenuBtn = null;
         var cls:Class = null;
         var made:InGameMenuSliderBtn = null;
         var bg:DisplayObject = null;
         var kinds:Array = [];
         var lists:Array = [];
         var opts:Array = null;
         var rec:Object = null;
         var tf:TextField = null;
         var i:int = 0;
         if(view == null || view.stage == null)
         {
            return;
         }
         rows = this.k33Rows(view);
         while(i < rows.length)
         {
            s = rows[i] as InGameMenuSliderBtn;
            if(s != null && first == null)
            {
               first = s;
            }
            if(s != null && Math.abs(s.x - first.x) < 1 && (anchor == null || s.y > anchor.y))
            {
               anchor = s;
            }
            i++;
         }
         if(anchor != null && this.k33FovRow != null && this.k33FovRow.parent == view && this.k33FovRow.y > anchor.y)
         {
            anchor = this.k33FovRow;
         }
         if(anchor != null && anchor == this.k33StyleAnchor && view == this.k33StyleView)
         {
            return; // this list already has its style rows
         }
         this.k33StyleRemove();
         if(anchor == null)
         {
            return;
         }
         i = 0;
         while(i < K33_STYLE_KINDS.length)
         {
            opts = this.k33StyleOptions(K33_STYLE_KINDS[i]);
            if(opts.length > 0)
            {
               kinds.push(K33_STYLE_KINDS[i]);
               lists.push(opts);
            }
            i++;
         }
         this.k33StyleAnchor = anchor; // also with no rows: the frame handler adds them when a list turns up
         this.k33StyleView = view;
         if(this.k33StyleSerial < 0)
         {
            this.k33StyleSerial = CFunctionContainerCommon.Dvar_GetInt("loc_languageSerial");
         }
         if(kinds.length == 0)
         {
            return;
         }
         cls = Object(anchor).constructor as Class;
         i = 0;
         while(i < rows.length)
         {
            row = rows[i] as InGameMenuBtn;
            if(row.y > anchor.y && Math.abs(row.x - anchor.x) < 1)
            {
               row.y += K33_ROW * kinds.length;
            }
            i++;
         }
         bg = view["myMiscListBg"] as DisplayObject;
         if(bg != null && Math.abs(bg.x + 10 - anchor.x) < 1)
         {
            bg.height += K33_ROW * kinds.length;
         }
         this.k33StyleBusy = true; // their ADDED events must not schedule another pass
         i = 0;
         while(i < kinds.length)
         {
            made = new cls() as InGameMenuSliderBtn;
            if(made != null)
            {
               made.x = anchor.x;
               made.y = anchor.y + K33_ROW * (i + 1);
               view.addChild(made);
               made.label = K33_STYLE_LABELS[kinds[i]];
               rec = {
                  "kind":kinds[i],
                  "row":made,
                  "opts":lists[i],
                  "names":[],
                  "shown":-1,
                  "hold":0
               };
               this.k33StyleRows.push(rec);
               this.k33StyleSetup(rec);
               made.mySlider.addEventListener(ProgressBar.EVT_VALUE_CHANGE,this.k33OnStyleChange);
            }
            i++;
         }
         this.k33StyleBusy = false;
         if(K33_STYLE_LANGUAGE_ROW)
         {
            // the language line, under the list, in the rows' own font (only with the Language row)
            tf = new TextField();
            tf.embedFonts = anchor.myLabel.embedFonts;
            tf.defaultTextFormat = anchor.myLabel.getTextFormat();
            tf.textColor = 14544626;
            tf.selectable = false;
            tf.mouseEnabled = false;
            tf.multiline = true;
            tf.wordWrap = true;
            tf.width = 300;
            tf.autoSize = TextFieldAutoSize.LEFT;
            tf.x = anchor.x + 2;
            tf.y = bg != null ? bg.y + bg.height + 4 : anchor.y + K33_ROW * (kinds.length + 3);
            tf.visible = false;
            view.addChild(tf);
            this.k33StyleNote = tf;
            this.k33StyleNoteUpdate();
         }
         i = 0;
         while(i < this.k33StyleRows.length)
         {
            if(this.k33StyleRefocus != "" && this.k33StyleRows[i].kind == this.k33StyleRefocus)
            {
               this.k33Focus = this.k33StyleRows[i].row;
               this.k33Hover(this.k33Focus,true);
               this.k33ShowRing();
            }
            i++;
         }
         this.k33StyleRefocus = "";
      }

      // k33: loc_languageState 1 (switching) / 2 (saved, applies on the next start) as a line under the list
      private function k33StyleNoteUpdate() : void
      {
         var key:String = "";
         var state:int = 0;
         var i:int = 0;
         if(this.k33StyleNote == null)
         {
            return;
         }
         while(i < this.k33StyleRows.length)
         {
            if(this.k33StyleRows[i].kind == "lang")
            {
               state = CFunctionContainerCommon.Dvar_GetInt("loc_languageState");
               key = state == 2 ? "@K33UI_LANG_RESTART" : (state == 1 ? "@K33UI_LANG_SWITCHING" : "");
            }
            i++;
         }
         if(key != this.k33StyleNoteKey)
         {
            this.k33StyleNoteKey = key;
            // translated here, not by the field: C1's translator gives a code-made field that is set to "@KEY" its
            // text in the default format (Times, 12, black), which an embedded-font field draws as black boxes
            this.k33StyleNote.text = key == "" ? "" : LocalizeUtil.localize(key);
            this.k33StyleNote.visible = key != "";
         }
      }

      // k33: build the open list again (its rows are translated as their labels are set; the FOV and style rows
      // follow on the next frame); `texts`: after a language switch, C1's fixed labels of the view are set again
      private function k33StyleRebuild(view:PauseMenuView, texts:Boolean) : void
      {
         var rec:Object = this.k33StyleOf(this.k33Focus);
         var o:Object = view;
         this.k33StyleRefocus = rec != null ? rec.kind : "";
         if(texts)
         {
            try
            {
               view.myQoSLabelTF.text = LocalizationInGame.QOS_TITLE;
               view.myQoSBtn.label = LocalizationInGame.QOS_BTN;
            }
            catch(err1:Error)
            {
            }
            try
            {
               if(view.myBackButton != null && "label" in view.myBackButton)
               {
                  o.myBackButton.label = LocalizationInGame.BackToGame;
               }
               if(view.myLoadoutListHeader != null && view.myLoadoutListHeader.visible)
               {
                  o.myLoadoutListHeader.SetLabel(LocalizationInGame.ChooseClass);
               }
            }
            catch(err2:Error)
            {
            }
         }
         try
         {
            if(view.getCurrentMenu() != null)
            {
               view.gotoMenu(view.getCurrentMenu());
            }
         }
         catch(err3:Error)
         {
         }
      }

      // k33: every K33_STYLE_POLL frames while the list is open: a finished language switch (loc_languageSerial)
      // builds the list again; a list that changed builds it again; a row shows what its dvar holds
      private function k33StyleFrame(e:Event) : void
      {
         var view:PauseMenuView = this.k33StyleView;
         var rec:Object = null;
         var opts:Array = null;
         var serial:int = 0;
         var index:int = 0;
         var count:int = 0;
         var i:int = 0;
         if(view == null || this.k33StyleAnchor == null || view.stage == null || !this.k33Shown(view))
         {
            return;
         }
         while(i < this.k33StyleRows.length)
         {
            if(this.k33StyleRows[i].hold > 0)
            {
               this.k33StyleRows[i].hold--;
            }
            i++;
         }
         this.k33StyleFrames++;
         if(this.k33StyleFrames % K33_STYLE_POLL != 0)
         {
            return;
         }
         try
         {
            serial = CFunctionContainerCommon.Dvar_GetInt("loc_languageSerial");
            if(serial != this.k33StyleSerial)
            {
               this.k33StyleSerial = serial;
               this.k33StyleRebuild(view,true);
               return;
            }
            i = 0;
            while(i < K33_STYLE_KINDS.length)
            {
               opts = this.k33StyleOptions(K33_STYLE_KINDS[i]);
               rec = count < this.k33StyleRows.length && this.k33StyleRows[count].kind == K33_STYLE_KINDS[i] ? this.k33StyleRows[count] : null;
               i++;
               if(rec != null)
               {
                  count++;
                  if(rec.row == this.k33DragRow)
                  {
                     continue;
                  }
               }
               if(this.k33StyleSig(opts) != (rec != null ? this.k33StyleSig(rec.opts) : ""))
               {
                  this.k33StyleRebuild(view,false);
                  return;
               }
               if(rec != null && rec.hold <= 0)
               {
                  index = this.k33StyleIndex(rec.opts,this.k33StyleCurrent(rec.kind));
                  if(index != rec.shown)
                  {
                     this.k33StyleShow(rec,index);
                  }
               }
            }
            this.k33StyleNoteUpdate();
         }
         catch(err:Error)
         {
         }
      }
   }
}
