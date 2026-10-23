package codo.views
{
   // codol_english_ui: the English codo.gfx's copy of voice_ui_mp's hand-written class (same source, its strings in English).
   // A change to voice_ui_mp's file must be made here too: with English on (k33_english 1) this zone loads
   // ahead of voice_ui_mp and its codo.gfx is the one C1 uses.
   // codo.views.OptionView from C1's codo.gfx (decompiled) with Kernel33's Options > Controller tab ("k33"):
   // C1's hidden "Other" tab (myTabOther) is shown as "Controller" and opens a panel built here in code:
   //   a line-art diagram of the connected pad (Kernel33's family: Xbox 360 / PS3 / Wii U / generic, from
   //   k33_padinfo "family|connected|name"; glyphs/make_diagrams.py) with a callout per button showing what
   //   it does (k33_padbinds "A=cmd;B=cmd;..."), live;
   //   the action list: click an action, then press a pad button (k33_padcapture = command; Kernel33 binds
   //   the next pad button to it, unbinds the command's other pad buttons and sets gpad_binds_custom, so the
   //   bind repair leaves the player's binds alone); "clear" (k33_padclear = command);
   //   settings: swap sticks (gpad_swap_sticks), swap triggers (gpad_swaptriggers: the two trigger binds
   //   swap), look feel (gpad_lookpreset), look sensitivity, invert Y, aim assist, deadzone, dive with the
   //   crouch button while sprinting (gpad_dive), and "default" (gpad_defaultbinds).
   import codo.common.BaseSubElement;
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.CodoVersion;
   import codo.constants.LocalizationSetting;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.views.option_sub.OptionView_audio;
   import codo.views.option_sub.OptionView_control;
   import codo.views.option_sub.OptionView_video;
   import com.greensock.TweenLite;
   import components.BaseButton;
   import components.CodoButton;
   import components.CodoGrowButtonNoTint;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.events.MouseEvent;
   import flash.geom.ColorTransform;
   import flash.text.TextField;
   import flash.text.TextFieldAutoSize;
   import flash.text.TextFormat;

   public class OptionView extends BaseView
   {

      private static var POS_X_ORIGIN:int = 640;

      private static var POS_Y_ORIGIN:int = 480;

      private static const K33_ACTIONS:Array = [["+gostand","Jump"],["+stance","Crouch/Prone"],["+reload","Reload"],["+activate","Use/Pick up"],["weapnext","Switch Weapon"],["weapalt","Attachment"],["+melee","Melee"],["+breath_sprint","Sprint/Hold Breath"],["+frag","Lethal"],["+smoke","Tactical"],["+tact","Special"],["gpad_ksnext","Select Killstreak"],["gpad_ksuse","Use Killstreak"],["togglemenu","Menu"],["+scores","Scoreboard"],["+talk","Talk"]];

      private static const K33_BUTTONS:Array = ["A","B","X","Y","LB","RB","START","BACK","LS","RS","UP","DOWN","LEFT","RIGHT","LT","RT"];

      private static const K33_DS:Number = 0.62;

      public var myIconCloseBtn:CodoButton;

      public var myTitleTF:TextField;

      public var myVideoPanel:OptionView_video;

      public var myAudioPanel:OptionView_audio;

      public var myControlPanel:OptionView_control;

      public var myTabVideo:CodoGrowButtonNoTint;

      public var myTabAudio:CodoGrowButtonNoTint;

      public var myTabControl:CodoGrowButtonNoTint;

      public var myTabOther:CodoGrowButtonNoTint;

      private var _currentTab:CodoGrowButtonNoTint;

      private var _currentPanel:BaseSubElement;

      private var _k33Panel:Sprite = null;

      private var _k33Diagram:Sprite = null;

      private var _k33Callouts:Sprite = null;

      private var _k33List:Sprite = null;

      private var _k33Settings:Sprite = null;

      private var _k33Status:TextField = null;

      private var _k33Font:String = null;

      private var _k33Embed:Boolean = false;

      private var _k33Info:String = "?";

      private var _k33Binds:String = "?";

      private var _k33Capture:String = "";

      private var _k33CaptureFrames:int = 0;

      private var _k33Frames:int = 0;

      private var _k33ClearMode:Boolean = false;

      private var _k33Fam:int = 3;

      public function OptionView()
      {
         super();
      }

      override protected function onInit() : void
      {
         this.myVideoPanel.disable();
         this.myAudioPanel.disable();
         this.myControlPanel.disable();
         this.myTitleTF.text = LocalizationSetting.OPTION_PAGE;
         this.myTitleTF.visible = false;
         this.myTabVideo.label = LocalizationSetting.OPTION_TAB_VIDEO;
         this.myTabAudio.label = LocalizationSetting.OPTION_TAB_AUDIO;
         this.myTabControl.label = LocalizationSetting.OPTION_TAB_CONTROL;
         this.myTabOther.label = "Controller";
         this.myTabOther.visible = true;
         switch(CodoVersion.currentVersion)
         {
            case CodoVersion.CE2:
            case CodoVersion.CE3:
            case CodoVersion.ALPHA_2:
            case CodoVersion.GAPP:
            case CodoVersion.ALPHA_1:
               this.myTabAudio.visible = false;
         }
         this.x = POS_X_ORIGIN;
         this.y = POS_Y_ORIGIN;
         TweenLite.to(this,0,{
            "autoAlpha":0,
            "scaleX":TweenParam.SCALE_POPUP,
            "scaleY":TweenParam.SCALE_POPUP
         });
      }

      override protected function onInitListener() : void
      {
         this.myTabVideo.addEventListener(BaseButton.EVT_CLICK,this.onTabRadioBtnClickHandle);
         this.myTabAudio.addEventListener(BaseButton.EVT_CLICK,this.onTabRadioBtnClickHandle);
         this.myTabControl.addEventListener(BaseButton.EVT_CLICK,this.onTabRadioBtnClickHandle);
         this.myTabOther.addEventListener(BaseButton.EVT_CLICK,this.onTabRadioBtnClickHandle);
         this.myIconCloseBtn.addEventListener(BaseButton.EVT_CLICK,this.onCloseBtnClick);
      }

      override protected function showView() : void
      {
         TweenLite.to(this,TweenParam.SPD_SCALE,{
            "autoAlpha":1,
            "scaleX":1,
            "scaleY":1
         });
         this.myTabVideo.enable = !this.getSyncDP(SyncDataID.IS_IN_ROOM) && !this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH);
         if(this.myTabVideo.enable)
         {
            this.currentTab = this._currentTab == null ? this.myTabVideo : this._currentTab;
         }
         else
         {
            this.currentTab = this._currentTab == null || this._currentTab == this.myTabVideo ? this.myTabControl : this._currentTab;
         }
         this.listen(BCEvent.EVT_PRESS_ESC_CLOSE_WINDOW,this.onOptionESCPressed);
         this.listen(BCEvent.EVT_ENABLE_VIDEO,this.onVedioEnable);
      }

      override protected function hideView() : void
      {
         TweenLite.to(this,TweenParam.SPD_SCALE_OUT,{
            "autoAlpha":0,
            "scaleX":TweenParam.SCALE_POPUP,
            "scaleY":TweenParam.SCALE_POPUP
         });
         this.myVideoPanel.disable();
         this.myAudioPanel.disable();
         this.myControlPanel.disable();
         this.k33Show(false);
         this.stopListen(BCEvent.EVT_PRESS_ESC_CLOSE_WINDOW,this.onOptionESCPressed);
         this.stopListen(BCEvent.EVT_ENABLE_VIDEO,this.onVedioEnable);
      }

      private function set currentTab(cTab:CodoGrowButtonNoTint) : void
      {
         if(this._currentTab != null)
         {
            this._currentTab.selected = false;
         }
         if(this._currentPanel != null)
         {
            this._currentPanel.disable();
         }
         this._currentPanel = null;
         this._currentTab = cTab;
         this._currentTab.selected = true;
         switch(this._currentTab)
         {
            case this.myTabVideo:
               this._currentPanel = this.myVideoPanel;
               break;
            case this.myTabAudio:
               this._currentPanel = this.myAudioPanel;
               break;
            case this.myTabControl:
               this._currentPanel = this.myControlPanel;
         }
         this.k33Show(this._currentTab == this.myTabOther);
         if(this._currentPanel != null)
         {
            this._currentPanel.enable();
         }
      }

      private function onTabRadioBtnClickHandle(cEvt:Event) : void
      {
         if(this._currentTab != cEvt.target)
         {
            if(this._currentTab == this.myTabControl)
            {
               if(Boolean(this.getSyncDP(SyncDataID.VITAL_COMMANDS_BOUNDED)))
               {
                  this.currentTab = cEvt.target as CodoGrowButtonNoTint;
               }
            }
            else
            {
               this.currentTab = cEvt.target as CodoGrowButtonNoTint;
            }
         }
      }

      private function onCloseBtnClick(cEvt:Event) : void
      {
         this.broadcast(BCEvent.EVT_CLOSE_OPTION);
      }

      private function onOptionESCPressed(cEvt:ParaEvent) : void
      {
         if(this._k33Capture != "")
         {
            this.k33CancelCapture();
            return;
         }
         this.broadcast(BCEvent.EVT_CLOSE_OPTION);
      }

      private function onVedioEnable(cEvt:ParaEvent) : void
      {
         if(cEvt.myPara.data == true)
         {
            this.myTabVideo.enable = true;
         }
         else
         {
            this.myTabVideo.enable = false;
            this.currentTab = this.myTabControl;
         }
      }

      private function k33Show(on:Boolean) : void
      {
         if(on)
         {
            if(this._k33Panel == null)
            {
               this.k33Build();
            }
            this._k33Panel.visible = true;
            this._k33Info = "?";
            this._k33Binds = "?";
            this.k33Refresh();
            this._k33Panel.addEventListener(Event.ENTER_FRAME,this.k33OnFrame);
         }
         else if(this._k33Panel != null)
         {
            this.k33CancelCapture();
            this._k33Panel.visible = false;
            this._k33Panel.removeEventListener(Event.ENTER_FRAME,this.k33OnFrame);
         }
      }

      private function k33Text(s:String, size:int, color:uint) : TextField
      {
         var tf:TextField = new TextField();
         tf.embedFonts = this._k33Embed;
         tf.defaultTextFormat = new TextFormat(this._k33Font,size,color);
         tf.selectable = false;
         tf.mouseEnabled = false;
         tf.autoSize = TextFieldAutoSize.LEFT;
         tf.text = s;
         return tf;
      }

      private function k33Button(label:String, w:Number, h:Number, fn:Function) : Sprite
      {
         var b:Sprite = new Sprite();
         b.graphics.lineStyle(1,6272712,0.8);
         b.graphics.beginFill(928307,0.85);
         b.graphics.drawRect(0,0,w,h);
         b.graphics.endFill();
         var tf:TextField = this.k33Text(label,13,14544626);
         tf.x = (w - tf.width) / 2;
         tf.y = (h - tf.height) / 2;
         tf.name = "label";
         b.addChild(tf);
         b.buttonMode = true;
         b.mouseChildren = false;
         b.addEventListener(MouseEvent.CLICK,fn);
         b.addEventListener(MouseEvent.ROLL_OVER,this.k33Over);
         b.addEventListener(MouseEvent.ROLL_OUT,this.k33Out);
         return b;
      }

      private function k33Over(e:MouseEvent) : void
      {
         (e.currentTarget as Sprite).transform.colorTransform = new ColorTransform(1.4,1.4,1.4);
      }

      private function k33Out(e:MouseEvent) : void
      {
         (e.currentTarget as Sprite).transform.colorTransform = new ColorTransform();
      }

      private function k33SetLabel(b:Sprite, s:String) : void
      {
         if(b == null)
         {
            return;
         }
         var tf:TextField = b.getChildByName("label") as TextField;
         if(tf != null && tf.text != s)
         {
            tf.text = s;
            tf.x = (b.width - tf.width) / 2;
         }
      }

      private function k33Build() : void
      {
         this._k33Font = this.myTitleTF.getTextFormat().font;
         this._k33Embed = this.myTitleTF.embedFonts;
         this._k33Panel = new Sprite();
         this._k33Panel.x = this.myControlPanel.x;
         this._k33Panel.y = this.myControlPanel.y;
         this.addChild(this._k33Panel);
         var head:TextField = this.k33Text("Controller",18,16777215);
         head.x = 0;
         head.y = 0;
         this._k33Panel.addChild(head);
         this._k33Status = this.k33Text("",14,10474463);
         this._k33Status.x = 60;
         this._k33Status.y = 3;
         this._k33Panel.addChild(this._k33Status);
         this._k33Diagram = new Sprite();
         this._k33Diagram.x = 128;
         this._k33Diagram.y = 40;
         this._k33Diagram.scaleX = this._k33Diagram.scaleY = K33_DS;
         this._k33Panel.addChild(this._k33Diagram);
         this._k33Callouts = new Sprite();
         this._k33Callouts.y = 40;
         this._k33Panel.addChild(this._k33Callouts);
         this._k33List = new Sprite();
         this._k33List.x = 20;
         this._k33List.y = 40;
         this._k33List.visible = false;
         this._k33Panel.addChild(this._k33List);
         var pg1:Sprite = this.k33Button("Pad Layout",110,24,this.k33OnPage);
         pg1.name = "page0";
         pg1.x = 330;
         pg1.y = 0;
         this._k33Panel.addChild(pg1);
         var pg2:Sprite = this.k33Button("Button Binds",110,24,this.k33OnPage);
         pg2.name = "page1";
         pg2.x = 450;
         pg2.y = 0;
         this._k33Panel.addChild(pg2);
         this._k33Settings = new Sprite();
         this._k33Settings.y = 270;
         this._k33Panel.addChild(this._k33Settings);
         var title:TextField = this.k33Text("Click an action, then press the pad button for it (triggers and sticks cannot be bound)",13,10474463);
         this._k33List.addChild(title);
         var i:int = 0;
         while(i < K33_ACTIONS.length)
         {
            var holder:Sprite = new Sprite();
            var nm:TextField = this.k33Text(K33_ACTIONS[i][1],13,14544626);
            nm.y = 3;
            holder.addChild(nm);
            var row:Sprite = this.k33Button("",145,20,this.k33OnAction);
            row.name = "act" + i;
            row.x = 100;
            holder.addChild(row);
            holder.x = i < 8 ? 0 : 300;
            holder.y = 28 + (i % 8) * 26;
            this._k33List.addChild(holder);
            i++;
         }
         var clr:Sprite = this.k33Button("Clear Binding",130,24,this.k33OnClearMode);
         clr.name = "clear";
         clr.x = 100;
         clr.y = 28 + 8 * 26 + 14;
         this._k33List.addChild(clr);
         var dflt:Sprite = this.k33Button("Defaults",130,24,this.k33OnDefault);
         dflt.x = 400;
         dflt.y = clr.y;
         this._k33List.addChild(dflt);
         var s:Array = [["swapsticks","Swap sticks (lefty)"],["swaptriggers","Swap triggers (LT fire)"],["preset","Look feel"],["sens","Look sensitivity"],["invert","Invert Y axis"],["aim","Aim assist"],["deadzone","Stick deadzone"],["dive","Dive from sprint"]];
         var j:int = 0;
         while(j < s.length)
         {
            var lab:TextField = this.k33Text(s[j][1],13,14544626);
            lab.x = j % 2 * 290;
            lab.y = int(j / 2) * 28 + 3;
            this._k33Settings.addChild(lab);
            var val:Sprite = this.k33Button("",110,22,this.k33OnSetting);
            val.name = s[j][0];
            val.x = j % 2 * 290 + 160;
            val.y = int(j / 2) * 28;
            this._k33Settings.addChild(val);
            j++;
         }
      }

      private function k33OnPage(e:MouseEvent) : void
      {
         var list:Boolean = (e.currentTarget as Sprite).name == "page1";
         this._k33List.visible = list;
         this._k33Diagram.visible = !list;
         this._k33Callouts.visible = !list;
         this._k33Settings.visible = !list;
         if(!list)
         {
            this.k33CancelCapture();
         }
      }
      
      private function k33Bind(tag:String) : String
      {
         var parts:Array = this._k33Binds.split(";");
         var i:int = 0;
         while(i < parts.length)
         {
            var kv:Array = String(parts[i]).split("=");
            if(kv.length == 2 && kv[0] == tag)
            {
               return kv[1];
            }
            i++;
         }
         return "";
      }

      private function k33ButtonName(tag:String, fam:int) : String
      {
         var xb:Object = {
            "A":"A","B":"B","X":"X","Y":"Y","LB":"LB","RB":"RB","START":"START","BACK":"BACK","LS":"LS","RS":"RS",
            "UP":"D-pad Up","DOWN":"D-pad Down","LEFT":"D-pad Left","RIGHT":"D-pad Right","LT":"LT","RT":"RT","HOLDX":"Hold X"};
         var ps:Object = {"A":"×","B":"○","X":"□","Y":"△","LB":"L1","RB":"R1","BACK":"SELECT","LS":"L3","RS":"R3","LT":"L2","RT":"R2","HOLDX":"Hold □"};
         var wu:Object = {"A":"B","B":"A","X":"Y","Y":"X","LB":"L","RB":"R","START":"+","BACK":"-","LS":"L Stick","RS":"R Stick","LT":"ZL","RT":"ZR","HOLDX":"Hold Y"};
         var ge:Object = {"A":"Down","B":"Right","X":"Left","Y":"Up","LB":"L1","RB":"R1","START":"Start","BACK":"Back","LS":"L Stick","RS":"R Stick","LT":"L2","RT":"R2","HOLDX":"Hold Left"};
         var t:Object = fam == 1 ? ps : (fam == 2 ? wu : (fam == 3 ? ge : xb));
         return t[tag] != null ? String(t[tag]) : String(xb[tag]);
      }

      private function k33ActionName(cmd:String) : String
      {
         if(cmd == "+attack")
         {
            return "Fire";
         }
         if(cmd == "+speed_throw" || cmd == "+toggleads_throw")
         {
            return "Aim";
         }
         var i:int = 0;
         while(i < K33_ACTIONS.length)
         {
            if(K33_ACTIONS[i][0] == cmd)
            {
               return K33_ACTIONS[i][1];
            }
            i++;
         }
         return cmd == "" ? "-" : cmd;
      }

      private function k33Refresh() : void
      {
         var info:String = CFunctionContainerCommon.Dvar_GetString("k33_padinfo");
         var binds:String = CFunctionContainerCommon.Dvar_GetString("k33_padbinds");
         if(info == null)
         {
            info = "";
         }
         if(binds == null)
         {
            binds = "";
         }
         var parts:Array = info.split("|");
         var fam:int = parts.length > 0 ? int(parts[0]) : 3;
         var conn:Boolean = parts.length > 1 && parts[1] == "1";
         var forced:Boolean = parts.length > 3 && parts[3] == "1";
         if(info != this._k33Info)
         {
            this._k33Info = info;
            this._k33Fam = conn || forced ? fam : 3;
            this._k33Status.text = conn ? (parts.length > 2 ? String(parts[2]) : "") : "No controller connected";
            this.k33DrawDiagram(this._k33Fam);
            this._k33Binds = "?";
         }
         if(binds != this._k33Binds)
         {
            this._k33Binds = binds;
            this.k33DrawCallouts(this._k33Fam);
            this.k33UpdateList(this._k33Fam);
         }
         this.k33UpdateSettings();
      }

      private function k33OnFrame(e:Event) : void
      {
         this._k33Frames++;
         if(this._k33Capture != "")
         {
            this._k33CaptureFrames++;
            var pending:String = CFunctionContainerCommon.Dvar_GetString("k33_padcapture");
            if(this._k33CaptureFrames > 5 && (pending == null || pending == ""))
            {
               this._k33Capture = "";
               this._k33Binds = "?";
            }
            else if(this._k33CaptureFrames > 600)
            {
               this.k33CancelCapture();
            }
         }
         if(this._k33Frames % 10 == 0)
         {
            this.k33Refresh();
         }
      }

      private function k33DrawDiagram(fam:int) : void
      {
         var d:Object = this.k33Diagram(fam);
         while(this._k33Diagram.numChildren > 0)
         {
            this._k33Diagram.removeChildAt(0);
         }
         this._k33Diagram.graphics.clear();
         var pts:Array = d.body;
         var n:int = int(pts.length / 2);
         this._k33Diagram.graphics.lineStyle(2,12575724,0.95);
         this._k33Diagram.graphics.beginFill(1061946,0.55);
         this._k33Diagram.graphics.moveTo((pts[(n - 1) * 2] + pts[0]) / 2,(pts[(n - 1) * 2 + 1] + pts[1]) / 2);
         var i:int = 0;
         while(i < n)
         {
            var cx:Number = pts[i * 2];
            var cy:Number = pts[i * 2 + 1];
            var nx:Number = pts[(i + 1) % n * 2];
            var ny:Number = pts[(i + 1) % n * 2 + 1];
            this._k33Diagram.graphics.curveTo(cx,cy,(cx + nx) / 2,(cy + ny) / 2);
            i++;
         }
         this._k33Diagram.graphics.endFill();
         var shapes:Array = d.shapes;
         i = 0;
         while(i < shapes.length)
         {
            var s:Array = shapes[i];
            var lbl:String = String(s[s.length - 1]);
            this._k33Diagram.graphics.lineStyle(2,12575724,0.95);
            this._k33Diagram.graphics.beginFill(1984082,0.9);
            // Scaleform strokes a line from the pen to where drawCircle / drawRoundRect start: move there first
            if(s[0] == "c")
            {
               this._k33Diagram.graphics.moveTo(s[1] + s[3],s[2]);
               this._k33Diagram.graphics.drawCircle(s[1],s[2],s[3]);
               this._k33Diagram.graphics.endFill();
               this.k33DiagramLabel(lbl,s[1],s[2]);
            }
            else if(s[0] == "r")
            {
               this._k33Diagram.graphics.moveTo(s[1] + s[5] / 2,s[2]);
               this._k33Diagram.graphics.drawRoundRect(s[1],s[2],s[3],s[4],s[5],s[5]);
               this._k33Diagram.graphics.endFill();
               this.k33DiagramLabel(lbl,s[1] + s[3] / 2,s[2] + s[4] / 2);
            }
            else if(s[0] == "s")
            {
               this._k33Diagram.graphics.moveTo(s[1] + s[3],s[2]);
               this._k33Diagram.graphics.drawCircle(s[1],s[2],s[3]);
               this._k33Diagram.graphics.endFill();
               this._k33Diagram.graphics.moveTo(s[1] + s[3] * 0.6,s[2]);
               this._k33Diagram.graphics.drawCircle(s[1],s[2],s[3] * 0.6);
               this.k33DiagramLabel(lbl,s[1],s[2]);
            }
            else
            {
               var a:Number = s[3];
               var w:Number = a * 0.62;
               var x0:Number = s[1];
               var y0:Number = s[2];
               this._k33Diagram.graphics.moveTo(x0 - w / 2,y0 - a);
               this._k33Diagram.graphics.lineTo(x0 + w / 2,y0 - a);
               this._k33Diagram.graphics.lineTo(x0 + w / 2,y0 - w / 2);
               this._k33Diagram.graphics.lineTo(x0 + a,y0 - w / 2);
               this._k33Diagram.graphics.lineTo(x0 + a,y0 + w / 2);
               this._k33Diagram.graphics.lineTo(x0 + w / 2,y0 + w / 2);
               this._k33Diagram.graphics.lineTo(x0 + w / 2,y0 + a);
               this._k33Diagram.graphics.lineTo(x0 - w / 2,y0 + a);
               this._k33Diagram.graphics.lineTo(x0 - w / 2,y0 + w / 2);
               this._k33Diagram.graphics.lineTo(x0 - a,y0 + w / 2);
               this._k33Diagram.graphics.lineTo(x0 - a,y0 - w / 2);
               this._k33Diagram.graphics.lineTo(x0 - w / 2,y0 - w / 2);
               this._k33Diagram.graphics.lineTo(x0 - w / 2,y0 - a);
               this._k33Diagram.graphics.endFill();
            }
            i++;
         }
      }

      private function k33DiagramLabel(s:String, x:Number, y:Number) : void
      {
         if(s == "")
         {
            return;
         }
         var tf:TextField = this.k33Text(s,13,16777215);
         tf.x = x - tf.width / 2;
         tf.y = y - tf.height / 2;
         this._k33Diagram.addChild(tf);
      }

      private function k33DrawCallouts(fam:int) : void
      {
         var d:Object = this.k33Diagram(fam);
         while(this._k33Callouts.numChildren > 0)
         {
            this._k33Callouts.removeChildAt(0);
         }
         this._k33Callouts.graphics.clear();
         var left:Array = [];
         var right:Array = [];
         var i:int = 0;
         while(i < K33_BUTTONS.length)
         {
            var tag:String = K33_BUTTONS[i];
            var a:Array = d.anchors[tag];
            if(a != null)
            {
               var item:Object = {
                  "tag":tag,
                  "x":this._k33Diagram.x + a[0] * K33_DS,
                  "y":a[1] * K33_DS
               };
               if(a[0] < 220)
               {
                  left.push(item);
               }
               else
               {
                  right.push(item);
               }
            }
            i++;
         }
         left.sortOn("y",Array.NUMERIC);
         right.sortOn("y",Array.NUMERIC);
         this.k33PlaceCallouts(left,true,fam);
         this.k33PlaceCallouts(right,false,fam);
      }

      private function k33PlaceCallouts(list:Array, onLeft:Boolean, fam:int) : void
      {
         var step:Number = list.length > 1 ? Math.min(28,240 / (list.length - 1)) : 0;
         var i:int = 0;
         while(i < list.length)
         {
            var it:Object = list[i];
            var ly:Number = i * step;
            var txt:String = this.k33ButtonName(it.tag,fam) + " " + this.k33ActionName(this.k33Bind(it.tag));
            if(it.tag == "X" && this.k33Bind("HOLDX") != "")
            {
               txt += ", hold " + this.k33ActionName(this.k33Bind("HOLDX"));
            }
            var tf:TextField = this.k33Text(txt,12,15398648);
            var lx:Number = onLeft ? 124 - tf.width : this._k33Diagram.x + 440 * K33_DS + 8;
            tf.x = lx;
            tf.y = ly - tf.height / 2;
            this._k33Callouts.addChild(tf);
            // the line is a thin filled bar, rotated (fills render reliably here; strokes did not)
            var sx:Number = lx - 4;
            if(onLeft)
            {
               sx = 128;
            }
            var dx:Number = it.x - sx;
            var dy:Number = it.y - ly;
            var ln:Sprite = new Sprite();
            ln.graphics.beginFill(8374998,0.75);
            ln.graphics.drawRect(0,-0.5,Math.sqrt(dx * dx + dy * dy),1);
            ln.graphics.endFill();
            ln.x = sx;
            ln.y = ly;
            ln.rotation = Math.atan2(dy,dx) * 180 / Math.PI;
            ln.mouseEnabled = false;
            this._k33Callouts.addChild(ln);
            var dot:Sprite = new Sprite();
            dot.graphics.beginFill(8374998,0.95);
            dot.graphics.drawCircle(0,0,2.5);
            dot.graphics.endFill();
            dot.x = it.x;
            dot.y = it.y;
            dot.mouseEnabled = false;
            this._k33Callouts.addChild(dot);
            i++;
         }
      }

      private function k33UpdateList(fam:int) : void
      {
         var i:int = 0;
         while(i < K33_ACTIONS.length)
         {
            var holder:Sprite = this._k33List.getChildAt(i + 1) as Sprite;
            var row:Sprite = holder.getChildByName("act" + i) as Sprite;
            var cmd:String = K33_ACTIONS[i][0];
            var keys:Array = [];
            var tags:Array = K33_BUTTONS.concat(["HOLDX"]);
            var j:int = 0;
            while(j < tags.length)
            {
               if(this.k33Bind(tags[j]) == cmd)
               {
                  keys.push(this.k33ButtonName(tags[j],fam));
               }
               j++;
            }
            this.k33SetLabel(row,this._k33Capture == cmd ? "Press a pad button..." : (keys.length > 0 ? keys.join(", ") : "Unbound"));
            i++;
         }
      }

      private function k33UpdateSettings() : void
      {
         var s:Sprite = this._k33Settings;
         var start:Number = CFunctionContainerCommon.Dvar_GetFloat("gpad_view_accel_start");
         this.k33SetLabel(s.getChildByName("swapsticks") as Sprite,CFunctionContainerCommon.Dvar_GetInt("gpad_swap_sticks") != 0 ? "On" : "Off");
         this.k33SetLabel(s.getChildByName("swaptriggers") as Sprite,this.k33Bind("LT") == "+attack" ? "On" : "Off");
         this.k33SetLabel(s.getChildByName("preset") as Sprite,start > 0.5 ? "Responsive" : (start > 0.2 ? "Smooth" : "Classic"));
         this.k33SetLabel(s.getChildByName("sens") as Sprite,"<  " + CFunctionContainerCommon.Dvar_GetFloat("gpad_view_sensitivity").toFixed(1) + "  >");
         this.k33SetLabel(s.getChildByName("invert") as Sprite,CFunctionContainerCommon.Dvar_GetInt("gpad_invert_y") != 0 ? "On" : "Off");
         this.k33SetLabel(s.getChildByName("aim") as Sprite,CFunctionContainerCommon.Dvar_GetFloat("gpad_aimassist") > 0 ? "On" : "Off");
         this.k33SetLabel(s.getChildByName("deadzone") as Sprite,"<  " + CFunctionContainerCommon.Dvar_GetFloat("gpad_stick_deadzone_min").toFixed(2) + "  >");
         this.k33SetLabel(s.getChildByName("dive") as Sprite,CFunctionContainerCommon.Dvar_GetInt("gpad_dive") != 0 ? "On" : "Off");
      }

      private function k33OnAction(e:MouseEvent) : void
      {
         var row:Sprite = e.currentTarget as Sprite;
         var i:int = int(row.name.substr(3));
         var cmd:String = K33_ACTIONS[i][0];
         if(this._k33ClearMode)
         {
            this._k33ClearMode = false;
            this.k33SetLabel(this._k33List.getChildByName("clear") as Sprite,"Clear Binding");
            CFunctionContainerCommon.setdvar("k33_padclear",cmd);
            this._k33Binds = "?";
            return;
         }
         if(this._k33Capture == cmd)
         {
            this.k33CancelCapture();
            return;
         }
         this._k33Capture = cmd;
         this._k33CaptureFrames = 0;
         CFunctionContainerCommon.setdvar("k33_padcapture",cmd);
         this._k33Binds = "?";
         this.k33Refresh();
      }

      private function k33CancelCapture() : void
      {
         if(this._k33Capture != "")
         {
            this._k33Capture = "";
            CFunctionContainerCommon.setdvar("k33_padcapture","");
            this._k33Binds = "?";
         }
      }

      private function k33OnClearMode(e:MouseEvent) : void
      {
         this._k33ClearMode = !this._k33ClearMode;
         this.k33SetLabel(e.currentTarget as Sprite,this._k33ClearMode ? "Click the action to clear" : "Clear Binding");
      }

      private function k33OnDefault(e:MouseEvent) : void
      {
         this.k33CancelCapture();
         CFunctionContainerCommon.execnow("gpad_defaultbinds");
         this._k33Binds = "?";
      }

      private function k33OnSetting(e:MouseEvent) : void
      {
         var b:Sprite = e.currentTarget as Sprite;
         var right:Boolean = b.mouseX > b.width / 2;
         var v:Number = NaN;
         switch(b.name)
         {
            case "swapsticks":
               CFunctionContainerCommon.setdvar("gpad_swap_sticks",CFunctionContainerCommon.Dvar_GetInt("gpad_swap_sticks") != 0 ? "0" : "1");
               break;
            case "swaptriggers":
               CFunctionContainerCommon.execnow("gpad_swaptriggers");
               this._k33Binds = "?";
               break;
            case "preset":
               v = CFunctionContainerCommon.Dvar_GetFloat("gpad_view_accel_start");
               CFunctionContainerCommon.execnow("gpad_lookpreset " + (v > 0.5 ? "smooth" : (v > 0.2 ? "classic" : "snappy")));
               break;
            case "sens":
               v = CFunctionContainerCommon.Dvar_GetFloat("gpad_view_sensitivity") + (right ? 0.1 : -0.1);
               v = Math.max(0.1,Math.min(5,Math.round(v * 10) / 10));
               CFunctionContainerCommon.setdvar("gpad_view_sensitivity",String(v));
               break;
            case "invert":
               CFunctionContainerCommon.setdvar("gpad_invert_y",CFunctionContainerCommon.Dvar_GetInt("gpad_invert_y") != 0 ? "0" : "1");
               break;
            case "aim":
               CFunctionContainerCommon.setdvar("gpad_aimassist",CFunctionContainerCommon.Dvar_GetFloat("gpad_aimassist") > 0 ? "0" : "1");
               break;
            case "deadzone":
               v = CFunctionContainerCommon.Dvar_GetFloat("gpad_stick_deadzone_min") + (right ? 0.02 : -0.02);
               v = Math.max(0,Math.min(0.4,Math.round(v * 100) / 100));
               CFunctionContainerCommon.setdvar("gpad_stick_deadzone_min",String(v));
               break;
            case "dive":
               CFunctionContainerCommon.setdvar("gpad_dive",CFunctionContainerCommon.Dvar_GetInt("gpad_dive") != 0 ? "0" : "1");
         }
         this.k33UpdateSettings();
      }

      // @K33DIAGRAMS@
   }
}
