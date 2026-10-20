package codo.controllers
{
   // C1's codo.controllers.StageKeyController (codo.gfx's stage key handler, Main.KEY_DOWN_HANDLE) as
   // decompiled from patch_ui_mp's codo.gfx, with gamepad focus navigation for Kernel33's controller
   // support (controller.c CtlPauseNav). build_voice_ui.py compiles it with JPEXS FFDec and splices the
   // class's DoABC tag back into codo.gfx. Changes from C1's code are marked "k33".
   //   Kernel33 posts these keys only while a match is running and C1's UI key catcher is up (the
   //   in-match pause menu that togglemenu opens): F13 up, F14 down, F11 left, F12 right (D-pad or left
   //   stick), F15 select (A). B / Start are plain Escape, handled by C1's code below as before.
   //   Focus moves between the visible, clickable objects of the screen (SimpleButtons, buttonMode
   //   sprites, anything listening for CLICK), to the nearest one in the pressed direction. The focused
   //   object gets the mouse-over events its own hover look listens for and a highlight frame; select
   //   sends it MOUSE_DOWN / MOUSE_UP / CLICK, as a mouse click would.
   import codo.common.BaseController;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.CodoPageViewDB;
   import codo.constants.SE_ID;
   import codo.constants.SyncDataID;
   import codo.utils.SoundUtil;
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.display.InteractiveObject;
   import flash.display.SimpleButton;
   import flash.display.Sprite;
   import flash.display.Stage;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import flash.events.MouseEvent;
   import flash.geom.Rectangle;
   import flash.text.TextField;

   public class StageKeyController extends BaseController
   {

      private var _enableGlobalKeyCatch:Boolean = true;

      private var k33Focus:InteractiveObject = null; // k33

      private var k33Ring:Sprite = null; // k33

      public function StageKeyController()
      {
         super();
      }

      override protected function onInit() : void
      {
         Main.KEY_DOWN_HANDLE = this.onStageKeyDownHandle;
         this.listen(BCEvent.EVT_DISABLE_GLOBAL_KEY_CATCH,this.onDisableGlobalKeyCatchHandle);
         this.listen(BCEvent.EVT_ENABLE_GLOBAL_KEY_CATCH,this.onEnableGlobalKeyCatchHandle);
      }

      private function onStageKeyDownHandle(cEvt:KeyboardEvent, cStage:Stage) : void
      {
         var isSysmtemMsgShowing:Boolean = false;
         var shouldBroadcast:Boolean = false;
         var tempKeyCode:uint = cEvt.keyCode;
         if(tempKeyCode >= 122 && tempKeyCode <= 126) // k33: pad focus navigation
         {
            this.k33Nav(tempKeyCode,cStage);
            return;
         }
         if(this._enableGlobalKeyCatch == true)
         {
            if(cStage.focus is TextField)
            {
               switch(tempKeyCode)
               {
                  case 8:
                     SoundUtil.playSound(SE_ID.GENERAL_TEXT_DELETE);
                     break;
                  default:
                     SoundUtil.playSound(SE_ID.GENERAL_TEXT_INPUT);
               }
            }
            if(cEvt.keyCode == 27)
            {
               isSysmtemMsgShowing = this.getSyncDP(SyncDataID.IS_SYSTEM_MSG_SHOWING);
               shouldBroadcast = true;
               if(isSysmtemMsgShowing == false && (CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj != CodoPageViewDB.PAGE_POPUP_WEAPON_ZOOMIN || CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == CodoPageViewDB.PAGE_POPUP_WEAPON_ZOOMIN && CodoPageViewDB.PAGE_LAYER_ADV_POPUP.currentPageObj == CodoPageViewDB.PAGE_ADV_POPUP_PURCHASE_CONFIRM))
               {
                  if(CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == CodoPageViewDB.PAGE_POPUP_WEAPON_ZOOMIN || CodoPageViewDB.PAGE_LAYER_ADV_POPUP.currentPageObj == CodoPageViewDB.PAGE_ADV_POPUP_PURCHASE_CONFIRM || CodoPageViewDB.PAGE_LAYER_ADV_POPUP.currentPageObj == CodoPageViewDB.PAGE_ADV_POPUP_PURCHASE_CONFIRM_CYBORG || CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == CodoPageViewDB.PAGE_POPUP_RENEW_ALL || CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == CodoPageViewDB.PAGE_POPUP_SHOPPING_CART || CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == CodoPageViewDB.PAGE_POPUP_LEADERBOARD)
                  {
                     shouldBroadcast = false;
                  }
                  if(CodoPageViewDB.PAGE_LAYER_ADV_POPUP.currentPageObj != null && CodoPageViewDB.PAGE_LAYER_ADV_POPUP.currentPageObj.enableCloseByESC == true || CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj != null && CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj.enableCloseByESC == true || CodoPageViewDB.PAGE_LAYER_ADV_POPUP.currentPageObj == null && CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == null && CodoPageViewDB.PAGE_LAYER_MAIN.currentPageObj != null && CodoPageViewDB.PAGE_LAYER_MAIN.currentPageObj.enableCloseByESC == true)
                  {
                     this.prevPage();
                  }
               }
               if(shouldBroadcast == true)
               {
                  this.broadcast(BCEvent.EVT_PRESS_ESC_CLOSE_WINDOW);
               }
            }
         }
      }

      private function onDisableGlobalKeyCatchHandle(cEvt:ParaEvent) : void
      {
         this._enableGlobalKeyCatch = false;
      }

      private function onEnableGlobalKeyCatchHandle(cEvt:ParaEvent) : void
      {
         this._enableGlobalKeyCatch = true;
      }

      // k33: visible, on screen, every ancestor visible
      private function k33Shown(o:DisplayObject, cStage:Stage) : Boolean
      {
         var p:DisplayObject = o;
         while(p != null && p != cStage)
         {
            if(!p.visible || p.alpha < 0.05)
            {
               return false;
            }
            p = p.parent;
         }
         return p == cStage;
      }

      // k33: the clickable objects under c (a button's own children are not searched)
      private function k33Collect(c:DisplayObjectContainer, out:Array, cStage:Stage) : void
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
               r = io.getBounds(cStage);
               if(r.width > 4 && r.height > 4 && r.right > 0 && r.bottom > 0 && r.left < cStage.stageWidth && r.top < cStage.stageHeight)
               {
                  out.push(io);
                  continue;
               }
            }
            dc = o as DisplayObjectContainer;
            if(dc != null && dc.mouseChildren)
            {
               this.k33Collect(dc,out,cStage);
            }
         }
      }

      private function k33Hover(o:InteractiveObject, over:Boolean) : void
      {
         if(o == null)
         {
            return;
         }
         o.dispatchEvent(new MouseEvent(over ? MouseEvent.ROLL_OVER : MouseEvent.ROLL_OUT,false));
         o.dispatchEvent(new MouseEvent(over ? MouseEvent.MOUSE_OVER : MouseEvent.MOUSE_OUT,true));
      }

      private function k33ShowRing(cStage:Stage) : void
      {
         var r:Rectangle = null;
         if(this.k33Ring == null)
         {
            this.k33Ring = new Sprite();
            this.k33Ring.mouseEnabled = false;
            this.k33Ring.mouseChildren = false;
            this.k33Ring.addEventListener(Event.ENTER_FRAME,this.k33RingFrame);
         }
         cStage.addChild(this.k33Ring);
         this.k33Ring.graphics.clear();
         if(this.k33Focus == null)
         {
            return;
         }
         r = this.k33Focus.getBounds(cStage);
         this.k33Ring.graphics.lineStyle(2,16766976,0.9);
         this.k33Ring.graphics.drawRect(r.x - 3,r.y - 3,r.width + 6,r.height + 6);
         this.k33Ring.visible = true;
      }

      // k33: the frame goes when the focused object does (menu closed, page changed)
      private function k33RingFrame(e:Event) : void
      {
         if(this.k33Focus == null || this.k33Ring.stage == null || !this.k33Shown(this.k33Focus,this.k33Ring.stage))
         {
            this.k33Ring.visible = false;
            this.k33Focus = null;
         }
      }

      private function k33Nav(code:uint, cStage:Stage) : void
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
         this.k33Collect(cStage,list,cStage);
         if(list.length == 0)
         {
            return;
         }
         if(this.k33Focus != null && list.indexOf(this.k33Focus) >= 0)
         {
            cur = this.k33Focus;
         }
         if(code == 126)
         {
            if(cur != null)
            {
               cur.dispatchEvent(new MouseEvent(MouseEvent.MOUSE_DOWN,true));
               cur.dispatchEvent(new MouseEvent(MouseEvent.MOUSE_UP,true));
               cur.dispatchEvent(new MouseEvent(MouseEvent.CLICK,true));
            }
            return;
         }
         if(cur == null)
         {
            i = 0;
            while(i < list.length)
            {
               o = list[i];
               r = o.getBounds(cStage);
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
            if(code == 124)
            {
               dy = -1;
            }
            else if(code == 125)
            {
               dy = 1;
            }
            else if(code == 122)
            {
               dx = -1;
            }
            else
            {
               dx = 1;
            }
            cr = cur.getBounds(cStage);
            i = 0;
            while(i < list.length)
            {
               o = list[i];
               if(o != cur)
               {
                  r = o.getBounds(cStage);
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
         this.k33ShowRing(cStage);
      }
   }
}
