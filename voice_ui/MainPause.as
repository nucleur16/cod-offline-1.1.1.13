package
{
   // C1's MainPause (document class of scaleform/default/codoingame.gfx, the in-match pause menu) as decompiled
   // from patch_common_mp's codoingame.gfx, with gamepad focus navigation for Kernel33's controller support
   // (controller.c CtlPauseNav / CtlDeliverSfKeys). build_ingame_ui.py compiles it with JPEXS FFDec and splices
   // the class's DoABC tag back into codoingame.gfx. Changes from C1's code are marked "k33".
   //   Kernel33 hands these key codes straight to this movie (C1 routes in-match keys to the HUD movie, so
   //   stock C1 gives this menu no keyboard at all; no keyboard produces these codes): 240 up, 241 down,
   //   242 left, 243 right (D-pad or left stick), 244 select (A). B / Start send Escape, which C1's
   //   PauseMenuView already treats as back / close.
   //   Focus moves between the visible, clickable objects (buttonMode sprites such as C1's BaseButton,
   //   SimpleButtons, anything listening for clicks), to the nearest one in the pressed direction. The focused
   //   object gets MOUSE_OVER / ROLL_OVER (its own hover look) and a frame; select sends it MOUSE_DOWN /
   //   MOUSE_UP / CLICK, as a mouse click would.
   import codo.common.AsyncTaskManager;
   import codo.common.BaseMain;
   import codo.common.CFunctionContainerCommon;
   import codo.common.CommManagerPause;
   import codo.common.FPSMemCounter;
   import codo.common.PageManagerPause;
   import codo.constants.AsyncTaskNamePause;
   import codo.constants.CodoPageViewDBPause;
   import codo.constants.MenuType;
   import codo.utils.ScaleformUtil;
   import flash.events.KeyboardEvent;
   import scaleform.gfx.MouseEventEx;
   import flash.geom.Rectangle;
   import flash.events.MouseEvent;
   import flash.events.Event;
   import flash.display.Sprite;
   import flash.display.SimpleButton;
   import flash.display.InteractiveObject;
   import flash.display.DisplayObjectContainer;
   import flash.display.DisplayObject;
   
   [SWF(width="1280",height="960",backgroundColor="#999999",frameRate="30")]
   public class MainPause extends BaseMain
   {
      
      private var _fpsMemCounter:FPSMemCounter;
      
      public function MainPause()
      {
         super();
      }
      
      override protected function onBeforeAddToStage() : void
      {
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
   }
}

