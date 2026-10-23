package codo.k33
{
   // Kernel33's gamepad cursor snapping: the targets (padsnap.c, PADSNAP.md).
   //
   // Kernel33 moves the menu cursor with the left stick. While it asks (dvar k33_snapscan > 0: the pad drove the
   // cursor recently), this class walks the display list ten times a second and publishes every control a click
   // would reach: SimpleButtons, sprites in button mode, objects listening for clicks or presses (list rows,
   // tabs, check boxes, dropdown entries, scroll bars), input text fields, and C1's sliders (ProgressBar) by
   // their knob. A control counts only when it is visible, enabled, on the stage, not clipped away by a
   // mask / scrollRect, and when a click at its point would reach it: the object Flash would hand the click to
   // (topmost hit, mouseEnabled / mouseChildren, hitArea) must be the control or inside it, so a popup or a
   // dialog hides everything under it. Kernel33 does the rest (magnetism, flicks, settling).
   //
   // Published (stage coordinates, integers; no ';' or '"', so it survives any dvar path):
   //   k33_snapn  "<seq> <targets> <chunks> <stageW> <stageH> <mouseX> <mouseY> <ms> <nodes> <hidden> <authoredW>
   //              <authoredH> <how>": stageW x stageH is the visible area, centred on the authored one; how: n/m hit test (getObjectsUnderPoint / walk), M/L movie
   //   k33_snap0..7  targets "x y w h px py kind", comma separated; kind 0 button, 1 text field, 2 slider knob,
   //                 3 / 4 the left / right half of a stepper ("<  value  >", see hint()), 5 a list the mouse wheel
   //                 scrolls (no target: where a D-pad press past the last row may scroll)
   // k33_snapscan 2 also publishes each target's class in k33_snapcls0..7 and what hides the hidden ones in k33_snaphid
   // (gpad_snapdump).
   //
   // The same class is compiled into codo.gfx and codologin.gfx (a new DoABC tag, build_zones.py --store-class).
   // components.CodoUIComponent, the base of every C1 control, starts it from its class initializer (one line
   // added there): the player initializes that class as soon as the movie builds its first control.
   import codo.common.CFunctionContainerCommon;
   import components.CodoUIComponent;
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.display.InteractiveObject;
   import flash.display.SimpleButton;
   import flash.display.Sprite;
   import flash.display.Stage;
   import flash.events.MouseEvent;
   import flash.geom.Point;
   import flash.geom.Rectangle;
   import flash.text.TextField;
   import flash.text.TextFieldType;
   import flash.utils.Dictionary;
   import flash.utils.getQualifiedClassName;
   import flash.utils.getTimer;
   import flash.utils.setInterval;

   public class PadSnap
   {
      public static const NORMAL:int = 0;

      public static const SPLIT_H:int = 1;   // a "<  value  >" stepper: its left and right halves are targets

      public static const SKIP:int = 2;      // never a target itself (its children may be)

      private static const MAX_NODES:int = 6000;

      private static const MAX_TARGETS:int = 240;

      private static const CHUNK:int = 860;

      private static var _hints:Dictionary = new Dictionary(true);

      private static var _timer:int = -1;

      private static var _stage:Stage = null;

      private static var _seq:int = 0;

      private static var _nodes:int = 0;

      private static var _hidden:int = 0;

      private static var _found:Array = null;

      private static var _chunks:int = 0;

      private static var _native:int = -1;

      private static var _lastLeaf:DisplayObject = null;

      private static var _chainMax:int = 5;

      private static var _modal:DisplayObject = null;

      private static var _booted:Boolean = boot();

      public function PadSnap()
      {
         super();
      }

      public static function boot() : Boolean
      {
         if(_timer < 0)
         {
            _timer = setInterval(tick,100);
         }
         return true;
      }

      // A control's snapping hint (NORMAL, SPLIT_H, SKIP), for code-built controls.
      public static function hint(o:DisplayObject, h:int) : void
      {
         boot();
         if(o != null)
         {
            _hints[o] = h;
         }
      }

      private static function tick() : void
      {
         var want:int = 0;
         try
         {
            if(_stage == null)
            {
               _stage = CodoUIComponent.REAL_STAGE;
               if(_stage == null)
               {
                  return;
               }
            }
            want = CFunctionContainerCommon.Dvar_GetInt("k33_snapscan");
            if(want > 0)
            {
               scan(want);
            }
         }
         catch(e:Error)
         {
            CFunctionContainerCommon.setdvar("k33_snaperr",String(e.message).replace(/[;"]/g," ").substr(0,200));
         }
      }

      private static function scan(mode:int) : void
      {
         var t0:int = getTimer();
         var sw:Number = _stage.stageWidth;
         var sh:Number = _stage.stageHeight;
         var aw:Number = _stage.loaderInfo != null ? _stage.loaderInfo.width : sw;
         var ah:Number = _stage.loaderInfo != null ? _stage.loaderInfo.height : sh;
         _nodes = 0;
         _hidden = 0;
         _found = [];
         _modal = null;
         // the visible area: the authored aw x ah centred in it (the stage is wider than authored on a 16:9 window)
         walk(_stage,new Rectangle((aw - sw) / 2,(ah - sh) / 2,sw,sh),sw * sh);
         var parts:Array = [];
         var names:Array = [];
         var hid:Array = [];
         var pts:Array = [];
         var i:int = 0;
         while(i < _found.length && parts.length < MAX_TARGETS)
         {
            var f:Object = _found[i];
            // with a dialog up (C1's modal mask, $layerMask, in front of everything under it) only the dialog's
            // own controls count: those in the mask's layer drawn above it (not even the social bar beside it)
            var p:Point = _modal != null && !(_modal.parent.contains(f.o as DisplayObject) && above(f.o as DisplayObject,_modal)) ? null : reach(f.o as InteractiveObject,f.r as Rectangle,f.k as int);
            if(p != null && f.k == 5)
            {
               p = new Point(f.sig,0);   // a list publishes what it shows (a text signature), so Kernel33 sees it scroll
            }
            if(p == null)
            {
               _hidden++;
               if(mode == 2 && hid.length < 40)
               {
                  var rr:Rectangle = f.r as Rectangle;
                  var who:InteractiveObject = receiver(new Point(rr.x + rr.width / 2,rr.y + rr.height / 2));
                  hid.push(getQualifiedClassName(f.o).replace(/[;" ]/g,"_") + "/" + String(f.o.name).replace(/[;" ]/g,"_") + "@" + int(rr.x) + "_" + int(rr.y) + "_" + int(rr.width) + "_" + int(rr.height) + "_by_" + (who == null ? "nothing" : getQualifiedClassName(who).replace(/[;" ]/g,"_") + "/" + String(who.name).replace(/[;" ]/g,"_")) + "_leaf_" + chainOf(_lastLeaf));
               }
            }
            else if(f.k == 5 || !taken(pts,p))
            {
               if(f.k != 5)
               {
                  pts.push(p);
               }
               var r:Rectangle = f.r as Rectangle;
               parts.push(int(r.x) + " " + int(r.y) + " " + int(r.width) + " " + int(r.height) + " " + int(p.x) + " " + int(p.y) + " " + f.k);
               if(mode == 2 && f.k != 5)
               {
                  names.push(getQualifiedClassName(f.o).replace(/[;" ]/g,"_") + "/" + String(f.o.name).replace(/[;" ]/g,"_"));
               }
            }
            i++;
         }
         var n:int = publish("k33_snap",parts,8);
         if(mode == 2)
         {
            publish("k33_snapcls",names,8);
            publish("k33_snaphid",hid,4);
            var at:Point = new Point(_stage.mouseX,_stage.mouseY);
            var probe:Array = String(CFunctionContainerCommon.Dvar_GetString("k33_snapprobe")).split(" ");
            if(probe.length == 2)
            {
               at = new Point(Number(probe[0]),Number(probe[1]));
            }
            var atWho:InteractiveObject = receiver(at);
            var under:Array = [];
            try
            {
               var objs:Array = _stage.getObjectsUnderPoint(at);
               var j:int = objs.length - 1;
               while(j >= 0 && under.length < 6)
               {
                  _chainMax = 9;
                  under.push(chainOf(objs[j] as DisplayObject));
                  _chainMax = 5;
                  j--;
               }
            }
            catch(e2:Error)
            {
            }
            CFunctionContainerCommon.setdvar("k33_snapat",("receiver " + (atWho == null ? "none" : chainOf(atWho)) + " under " + under.join(" / ")).substr(0,CHUNK));
         }
         _seq++;
         CFunctionContainerCommon.setdvar("k33_snapn",_seq + " " + parts.length + " " + n + " " + int(sw) + " " + int(sh) + " " + int(_stage.mouseX) + " " + int(_stage.mouseY) + " " + (getTimer() - t0) + " " + _nodes + " " + _hidden + " " + int(aw) + " " + int(ah) + " " + (_native == 1 ? "n" : "m") + (_stage.loaderInfo != null && _stage.loaderInfo.url != null && _stage.loaderInfo.url.indexOf("login") >= 0 ? "L" : "M"));
      }

      // A number for the text a list shows: it changes when the list scrolls (its rows are reused, so their positions
      // do not).
      private static function textSig(c:DisplayObjectContainer) : int
      {
         var h:int = 0;
         if(c == null)
         {
            return 0;
         }
         var stack:Array = [c];
         var n:int = 0;
         while(stack.length > 0 && n < 400)
         {
            var d:DisplayObject = stack.pop() as DisplayObject;
            n++;
            if(d is TextField && d.visible)
            {
               var t:String = TextField(d).text;
               var i:int = 0;
               while(i < t.length && i < 40)
               {
                  h = h * 31 + t.charCodeAt(i) & 0x7FFFFFF;
                  i++;
               }
               h = h * 7 + int(d.y) & 0x7FFFFFF;
            }
            var dc:DisplayObjectContainer = d as DisplayObjectContainer;
            if(dc != null && d.visible)
            {
               var j:int = 0;
               while(j < dc.numChildren)
               {
                  stack.push(dc.getChildAt(j));
                  j++;
               }
            }
         }
         return h % 1000000000;
      }

      // Two controls at (almost) one point are one target to the cursor (a list row and its full-row arrow button).
      private static function taken(pts:Array, p:Point) : Boolean
      {
         var i:int = 0;
         while(i < pts.length)
         {
            if(Math.abs(Point(pts[i]).x - p.x) < 8 && Math.abs(Point(pts[i]).y - p.y) < 8)
            {
               return true;
            }
            i++;
         }
         return false;
      }

      // Comma-joined items in chunks of at most CHUNK characters: <prefix>0..<prefix>(max-1); returns the chunk count.
      private static function publish(prefix:String, items:Array, max:int) : int
      {
         var chunk:String = "";
         var c:int = 0;
         var i:int = 0;
         while(i < items.length && c < max)
         {
            var s:String = String(items[i]);
            if(chunk.length > 0 && chunk.length + 1 + s.length > CHUNK)
            {
               CFunctionContainerCommon.setdvar(prefix + c,chunk);
               c++;
               chunk = "";
            }
            if(c < max)
            {
               chunk = chunk.length > 0 ? chunk + "," + s : s;
            }
            i++;
         }
         if(chunk.length > 0 && c < max)
         {
            CFunctionContainerCommon.setdvar(prefix + c,chunk);
            c++;
         }
         return c;
      }

      private static function walk(c:DisplayObjectContainer, clip:Rectangle, stageArea:Number) : void
      {
         var n:int = c.numChildren;
         var i:int = 0;
         while(i < n)
         {
            var d:DisplayObject = null;
            try
            {
               d = c.getChildAt(i);
            }
            catch(e:Error)
            {
               d = null;
            }
            i++;
            if(d == null || !d.visible || d.alpha < 0.02)
            {
               continue;
            }
            if(++_nodes > MAX_NODES)
            {
               return;
            }
            var cl:Rectangle = clip;
            if(d.mask != null)
            {
               cl = cl.intersection(d.mask.getBounds(_stage));
            }
            if(d.scrollRect != null)
            {
               cl = cl.intersection(d.getBounds(_stage));
            }
            if(cl.width < 2 || cl.height < 2)
            {
               continue;
            }
            if(getQualifiedClassName(d) == "$layerMask" && d.parent != null)
            {
               _modal = d;   // C1's page layer puts this mask under a modal page (dialogs, Options): the topmost one wins
            }
            var h:int = _hints[d] != null ? int(_hints[d]) : NORMAL;
            var io:InteractiveObject = d as InteractiveObject;
            // a control listening for the mouse counts even with mouseEnabled off when its children take the click
            // (C1's components set mouseEnabled false on themselves; a slider's track is its child)
            if(io != null && h != SKIP && (io.mouseEnabled || io is DisplayObjectContainer && DisplayObjectContainer(io).mouseChildren))
            {
               var k:int = kind(io);
               if(k >= 0)
               {
                  add(io,k,h,cl,stageArea);
               }
               if(io.hasEventListener(MouseEvent.MOUSE_WHEEL))
               {
                  var wr:Rectangle = io.getBounds(_stage).intersection(cl);
                  if(wr.width >= 8 && wr.height >= 8)
                  {
                     _found.push({"o":io,"r":wr,"k":5,"sig":textSig(io as DisplayObjectContainer)});   // a list the wheel scrolls
                  }
               }
            }
            var dc:DisplayObjectContainer = d as DisplayObjectContainer;
            if(dc != null && dc.mouseChildren && !(d is SimpleButton))
            {
               walk(dc,cl,stageArea);
            }
         }
      }

      // -1 not a control; 0 button / clickable, 1 input text field, 2 slider (by its knob)
      private static function kind(io:InteractiveObject) : int
      {
         if(io is TextField)
         {
            return TextField(io).type == TextFieldType.INPUT ? 1 : -1;
         }
         if(io is SimpleButton)
         {
            return SimpleButton(io).enabled ? 0 : -1;
         }
         if(!enabled(io))
         {
            return -1;
         }
         if(knob(io) != null)
         {
            return 2;
         }
         var s:Sprite = io as Sprite;
         if(s != null && s.buttonMode)
         {
            return 0;
         }
         if(io.hasEventListener(MouseEvent.CLICK) || io.hasEventListener(MouseEvent.MOUSE_DOWN) || io.hasEventListener(MouseEvent.MOUSE_UP) || io.hasEventListener(MouseEvent.DOUBLE_CLICK))
         {
            return 0;
         }
         return -1;
      }

      private static function enabled(o:Object) : Boolean
      {
         try
         {
            if("enable" in o && o["enable"] === false)
            {
               return false;
            }
            if("enabled" in o && o["enabled"] === false)
            {
               return false;
            }
         }
         catch(e:Error)
         {
         }
         return true;
      }

      // C1's slider (components.ProgressBar): a draggable bar with a knob (myPointIco)
      private static function knob(o:Object) : DisplayObject
      {
         try
         {
            if("myPointIco" in o && "draggable" in o && o["draggable"] === true)
            {
               var k:DisplayObject = o["myPointIco"] as DisplayObject;
               if(k != null && k.visible)
               {
                  return k;
               }
            }
         }
         catch(e:Error)
         {
         }
         return null;
      }

      private static function add(io:InteractiveObject, k:int, h:int, clip:Rectangle, stageArea:Number) : void
      {
         var s:Sprite = io as Sprite;
         var full:Rectangle = s != null && s.hitArea != null ? s.hitArea.getBounds(_stage) : io.getBounds(_stage);
         if(full.width < 4 || full.height < 4 || full.width * full.height > stageArea * 0.3)
         {
            return;   // a background or a full-screen blocker, not a control
         }
         if(full.width * full.height > stageArea * 0.08 && !(io is SimpleButton) && !(s != null && s.buttonMode))
         {
            return;   // a large area that only listens for presses: a drag surface (the 3D character view), not a button
         }
         var r:Rectangle = full.intersection(clip);
         if(r.width < 4 || r.height < 4 || r.width * r.height < full.width * full.height * 0.3 && r.height < 12)
         {
            return;   // scrolled or clipped (almost) out of view
         }
         if(h == SPLIT_H)
         {
            _found.push({"o":io,"r":new Rectangle(r.x,r.y,r.width / 2,r.height),"k":3});
            _found.push({"o":io,"r":new Rectangle(r.x + r.width / 2,r.y,r.width / 2,r.height),"k":4});
            return;
         }
         _found.push({"o":io,"r":r,"k":k});
      }

      // The point a click on `io` should land on, or null when nothing of it can be clicked (covered or clipped).
      private static function reach(io:InteractiveObject, r:Rectangle, k:int) : Point
      {
         var pts:Array = [];
         if(k == 2)
         {
            var kb:Rectangle = knob(io).getBounds(_stage);
            pts.push(new Point(kb.x + kb.width / 2,kb.y + kb.height / 2));
         }
         pts.push(new Point(r.x + r.width / 2,r.y + r.height / 2));
         pts.push(new Point(r.x + r.width * 0.25,r.y + r.height / 2));
         pts.push(new Point(r.x + r.width * 0.75,r.y + r.height / 2));
         var i:int = 0;
         while(i < pts.length)
         {
            var p:Point = pts[i] as Point;
            if(p.x >= r.x && p.x <= r.right && p.y >= r.y && p.y <= r.bottom)
            {
               var t:InteractiveObject = receiver(p);
               if(t != null && (t == io || io is DisplayObjectContainer && DisplayObjectContainer(io).contains(t)))
               {
                  return p;
               }
               // a hit area (C1's BaseButton: myHitArea, invisible, so no hit test returns it) takes the click where it
               // covers the point, unless what the click found lies above the control
               var ha:Sprite = io is Sprite ? Sprite(io).hitArea : null;
               if(ha != null && ha.hitTestPoint(p.x,p.y,true) && (t == null || !above(t,io)) && drawnAt(io,p))
               {
                  return p;
               }
            }
            i++;
         }
         return null;
      }

      // Whether anything of `c` is drawn at p (a hit area alone, with nothing visible there, is no control to show).
      private static function drawnAt(c:InteractiveObject, p:Point) : Boolean
      {
         var dc:DisplayObjectContainer = c as DisplayObjectContainer;
         if(dc == null)
         {
            return true;
         }
         try
         {
            var objs:Array = _stage.getObjectsUnderPoint(p);
            var i:int = 0;
            while(i < objs.length)
            {
               if(dc.contains(objs[i] as DisplayObject))
               {
                  return true;
               }
               i++;
            }
            return false;
         }
         catch(e:Error)
         {
         }
         return true;
      }

      // Whether a is drawn above b (later in the display list; an ancestor is below its children).
      private static function above(a:DisplayObject, b:DisplayObject) : Boolean
      {
         var ca:Array = [];
         var cb:Array = [];
         var d:DisplayObject = a;
         while(d != null)
         {
            ca.unshift(d);
            d = d.parent;
         }
         d = b;
         while(d != null)
         {
            cb.unshift(d);
            d = d.parent;
         }
         var i:int = 0;
         while(i < ca.length && i < cb.length && ca[i] == cb[i])
         {
            i++;
         }
         if(i >= ca.length || i >= cb.length || i == 0)
         {
            return i >= cb.length && i < ca.length;   // a inside b: above it; b inside a: below
         }
         var parent:DisplayObjectContainer = ca[i - 1] as DisplayObjectContainer;
         return parent.getChildIndex(ca[i] as DisplayObject) > parent.getChildIndex(cb[i] as DisplayObject);
      }

      // The object a click at p (stage coordinates) goes to, as Flash decides it: the topmost visible object hit, then
      // its nearest mouse-enabled interactive ancestor (or the outermost ancestor with mouseChildren off); a text field
      // with mouseEnabled off, and objects nothing mouse-enabled owns, let the click through to what lies below.
      private static function receiver(p:Point) : InteractiveObject
      {
         if(_native != 0)
         {
            try
            {
               var objs:Array = _stage.getObjectsUnderPoint(p);
               _native = 1;
               var i:int = objs.length - 1;
               while(i >= 0)
               {
                  var o:DisplayObject = objs[i] as DisplayObject;
                  i--;
                  if(o == null || !o.hitTestPoint(p.x,p.y,true))
                  {
                     continue;
                  }
                  var t:InteractiveObject = owner(o,p);
                  if(t != null)
                  {
                     _lastLeaf = o;
                     return t;
                  }
               }
               return null;
            }
            catch(e:Error)
            {
               _native = 0;
            }
         }
         return pick(_stage,p,null,null);
      }

      private static function chainOf(d:DisplayObject) : String
      {
         var out:Array = [];
         while(d != null && !(d is Stage) && out.length < 8)
         {
            var io:InteractiveObject = d as InteractiveObject;
            out.push(getQualifiedClassName(d).replace(/^.*::/,"") + ":" + d.name + (io != null ? (io.mouseEnabled ? "+" : "-") : "") + (d is DisplayObjectContainer ? (DisplayObjectContainer(d).mouseChildren ? "" : "!") : "") + (io != null && (io.hasEventListener(MouseEvent.CLICK) || io.hasEventListener(MouseEvent.MOUSE_DOWN) || io.hasEventListener(MouseEvent.MOUSE_UP)) ? "L" : "") + (d is Sprite && Sprite(d).buttonMode ? "B" : "") + (enabled(d) ? "" : "X"));
            d = d.parent;
         }
         return out.slice(0,_chainMax).join(">").replace(/[;", ]/g,"_");
      }

      // The receiver for a hit on leaf `o`, or null when the click passes through (invisible, masked, or nothing
      // mouse-enabled above it).
      private static function owner(o:DisplayObject, p:Point) : InteractiveObject
      {
         var chain:Array = [];
         var d:DisplayObject = o;
         while(d != null && !(d is Stage))
         {
            if(!d.visible)
            {
               return null;
            }
            if(d.mask != null && !d.mask.hitTestPoint(p.x,p.y,true))
            {
               return null;
            }
            chain.push(d);
            d = d.parent;
         }
         // an ancestor with mouseChildren off takes every hit inside it (its label text included)
         var i:int = chain.length - 1;
         while(i >= 0)
         {
            var c:DisplayObjectContainer = chain[i] as DisplayObjectContainer;
            if(c != null && i > 0 && !c.mouseChildren)
            {
               return c.mouseEnabled ? c : null;
            }
            i--;
         }
         if(o is InteractiveObject && !InteractiveObject(o).mouseEnabled)
         {
            return null;   // an object that ignores the mouse is not hit by its own drawing: the click goes to what is below
         }
         i = 0;
         while(i < chain.length)
         {
            var io:InteractiveObject = chain[i] as InteractiveObject;
            if(io != null && io.mouseEnabled)
            {
               return io;
            }
            i++;
         }
         return null;
      }

      // Without getObjectsUnderPoint: the same decision by a top-down walk (bounds first, then the shapes).
      private static function pick(d:DisplayObject, p:Point, anc:InteractiveObject, forced:InteractiveObject) : InteractiveObject
      {
         if(!d.visible)
         {
            return null;
         }
         if(d.mask != null && !d.mask.hitTestPoint(p.x,p.y,true))
         {
            return null;
         }
         var io:InteractiveObject = d as InteractiveObject;
         var near:InteractiveObject = io != null && io.mouseEnabled ? io : anc;
         var s:Sprite = d as Sprite;
         if(s != null && s.hitArea != null)
         {
            return s.hitArea.hitTestPoint(p.x,p.y,true) ? (forced != null ? forced : near) : null;
         }
         var c:DisplayObjectContainer = d as DisplayObjectContainer;
         if(c != null)
         {
            if(!(d is Stage) && !c.getBounds(_stage).containsPoint(p))
            {
               return null;
            }
            var f:InteractiveObject = forced;
            if(f == null && !(d is Stage) && !c.mouseChildren)
            {
               if(io == null || !io.mouseEnabled)
               {
                  return null;
               }
               f = io;
            }
            var i:int = c.numChildren - 1;
            while(i >= 0)
            {
               var r:InteractiveObject = pick(c.getChildAt(i),p,near,f);
               if(r != null)
               {
                  return r;
               }
               i--;
            }
            return null;
         }
         if(!d.hitTestPoint(p.x,p.y,true) || io != null && !io.mouseEnabled)
         {
            return null;
         }
         return forced != null ? forced : near;
      }
   }
}
