package codo.k33
{
   import codo.common.CFunctionContainerCommon;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.events.MouseEvent;
   import flash.text.TextField;
   import flash.text.TextFormat;
   import flash.utils.getDefinitionByName;
   import flash.utils.getTimer;

   // One room-owner toggle. The existing game lobby owns gameplay settings.
   public class LocalMatchControls extends Sprite
   {
      private var room:Object;
      private var toggle:Sprite;
      private var caption:TextField;
      private var status:TextField;
      private var lastContext:String = "";
      private var lastPoll:int = 0;

      public function LocalMatchControls()
      {
         super();
         var cls:Class = getDefinitionByName("raven_listHeader") as Class;
         var sample:Object = new cls();
         var fmt:TextFormat = sample.txt_header.defaultTextFormat;
         fmt.size=14; fmt.color=0xEAF1F6; fmt.align="left";
         fmt.leftMargin=fmt.rightMargin=fmt.indent=fmt.blockIndent=0;
         this.toggle=new Sprite();
         this.toggle.buttonMode=true; this.toggle.mouseChildren=false;
         this.toggle.graphics.beginFill(0x28475B);
         this.toggle.graphics.lineStyle(1,0x6A8FA4);
         this.toggle.graphics.drawRect(0,0,322,30);
         this.toggle.graphics.endFill();
         this.caption=new TextField();
         this.caption.defaultTextFormat=fmt;
         this.caption.embedFonts=sample.txt_header.embedFonts;
         this.caption.x=8; this.caption.y=3; this.caption.width=306; this.caption.height=26;
         this.caption.selectable=false; this.caption.mouseEnabled=false;
         this.toggle.addChild(this.caption);
         this.toggle.addEventListener(MouseEvent.CLICK,this.toggleLocal);
         this.addChild(this.toggle);
         this.status=new TextField();
         fmt.size=12;
         this.status.defaultTextFormat=fmt;
         this.status.embedFonts=sample.txt_header.embedFonts;
         this.status.y=34; this.status.width=322; this.status.height=44;
         this.status.wordWrap=true; this.status.selectable=false; this.status.mouseEnabled=false;
         this.addChild(this.status);
         this.visible=false;
         this.addEventListener(Event.ENTER_FRAME,this.poll);
         this.refreshStatus();
      }

      private function mapID():String
      {
         return this.room != null && this.room.currentMap != null ? String(this.room.currentMap.id) : "";
      }
      private function category():String
      {
         var map:String=this.mapID();
         var gt:String=this.room != null && this.room.currentMatchMode != null ? String(this.room.currentMatchMode.id) : "";
         if(gt=="survival" || map.indexOf("coop_survival_")==0) return "survival";
         if(gt=="zm" || map.indexOf("zm_")==0) return "zm";
         if(map.indexOf("coop_")==0) return "coop";
         return "mp";
      }
      private function gameType():String
      {
         var cat:String=this.category();
         if(cat=="survival" || cat=="zm") return cat;
         return this.room != null && this.room.currentMatchMode != null ? String(this.room.currentMatchMode.id) : "";
      }
      private function owner():Boolean
      {
         return this.room != null && this.room.forces != null && this.room.forces.isHost;
      }
      public function setRoom(value:Object):void
      {
         this.room=value;
         this.visible=this.owner() && this.category()!="coop";
         if(!this.visible) { this.lastContext=""; return; }
         var map:String=this.mapID();
         var gt:String=this.gameType();
         if(!/^[a-z0-9_]+$/.test(map) || !/^[a-z0-9_]+$/.test(gt)) { this.visible=false; this.lastContext=""; return; }
         var context:String=map+" "+gt+" "+this.category();
         if(context!=this.lastContext)
         {
            this.lastContext=context;
            // Publishing context alone never starts a disabled local host.
            CFunctionContainerCommon.execnow("p2p_room "+context);
         }
         this.refreshStatus();
      }
      private function toggleLocal(e:MouseEvent):void
      {
         if(!this.owner() || this.category()=="coop") return;
         var enabled:Boolean=CFunctionContainerCommon.Dvar_GetString("ui_p2p_enabled")=="1";
         CFunctionContainerCommon.execnow("p2p_localhost "+(enabled ? "0" : "1"));
         this.refreshStatus();
      }
      private function refreshStatus():void
      {
         var enabled:Boolean=CFunctionContainerCommon.Dvar_GetString("ui_p2p_enabled")=="1";
         this.caption.text="LOCAL HOSTING: "+(enabled ? "ENABLED" : "DISABLED");
         var error:String=CFunctionContainerCommon.Dvar_GetString("ui_p2p_error");
         var state:String=CFunctionContainerCommon.Dvar_GetString("ui_p2p_state");
         this.status.text=error!="" ? error : (enabled && state!="" ? state : "");
      }
      private function poll(e:Event):void
      {
         var now:int=getTimer();
         if(now-this.lastPoll<250 || !this.visible) return;
         this.lastPoll=now;
         this.refreshStatus();
      }
   }
}
