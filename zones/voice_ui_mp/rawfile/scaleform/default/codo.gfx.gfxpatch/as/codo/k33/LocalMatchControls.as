package codo.k33
{
   import codo.common.CFunctionContainerCommon;
   import flash.display.Sprite;
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.events.Event;
   import flash.events.MouseEvent;
   import flash.geom.Rectangle;
   import flash.text.TextField;
   import flash.text.TextFormat;

   // The existing room view creates this invisible bridge after Create Room succeeds.
   public class LocalMatchControls extends Sprite
   {
      private var room:Object;
      private var lastContext:String="";
      private var localSelection:Boolean=false;

      private var choice:LocalCreateRoomOption;
      private var choiceCallback:Function;
      private var observedProtocol:String="";
      private var pendingChoice:Boolean=false;
      private var lastRoomKey:String="";

      public function LocalMatchControls(sample:TextField=null,onChoice:Function=null)
      {
         super();
         this.visible=false;
         if(sample==null){sample=new TextField();sample.defaultTextFormat=new TextFormat("_sans",14,0xFFFFFF);}
         this.choice=new LocalCreateRoomOption(sample);this.addChild(this.choice);
         this.choiceCallback=onChoice;
         this.choice.addEventListener(MouseEvent.CLICK,this.onLocalChoice);
         this.addEventListener(Event.ENTER_FRAME,this.onProtocolFrame);
      }

      public function anchorBesideReadyArea(area:DisplayObject,container:DisplayObjectContainer):void
      {
         // Only the new control moves. Stock Ready, its label and all siblings
         // retain their transforms. Real multi-resolution adjacency needs render QA.
         if(area==null || container==null)return;
         var bounds:Rectangle=area.getBounds(container);
         this.x=bounds.right+12;this.y=bounds.bottom-32;
      }

      private function onProtocolFrame(event:Event):void
      {
         var mode:String=CFunctionContainerCommon.Dvar_GetString("ui_p2p_startup_protocol");
         if(mode!=this.observedProtocol){this.observedProtocol=mode;this.setRoom(this.room);}
         if(this.pendingChoice && this.localSelection && this.room!=null &&
            CFunctionContainerCommon.Dvar_GetString("ui_p2p_enabled")=="1" &&
            CFunctionContainerCommon.Dvar_GetString("ui_p2p_map")==this.nativeMapID())
         {
            this.pendingChoice=false;
            if(this.choiceCallback!=null)this.choiceCallback(true);
         }
      }

      private function resetProductionChoice():void
      {
         if(this.localSelection)CFunctionContainerCommon.execnow("p2p_localhost 0");
         this.localSelection=false;this.lastContext="";this.choice.selected=false;this.pendingChoice=false;
      }

      private function nativeMapID():String
      {
         var map:String=this.mapID();
         return map.indexOf("coop_survival_")==0 ? map.substr(14) : map;
      }

      private function intentRoomKey(value:Object):String
      {
         if(value==null || value.index==null || value.name==null)return "";
         var index:String=String(value.index),name:String=String(value.name);
         return /^(0|[1-9][0-9]{0,19})$/.test(index) && name.length>0 ? index+"|"+name.length+":"+name : "";
      }

      private function onLocalChoice(event:MouseEvent):void
      {
         if(CFunctionContainerCommon.Dvar_GetString("ui_p2p_startup_protocol")!="3" || this.room==null || this.lastContext=="")return;
         this.localSelection=this.choice.selected;
         CFunctionContainerCommon.execnow("p2p_room "+this.lastContext);
         CFunctionContainerCommon.execnow("p2p_localhost "+(this.localSelection?"1":"0"));
         this.pendingChoice=this.localSelection;
         if(!this.localSelection && this.choiceCallback!=null)this.choiceCallback(false);
      }

      private function setProductionRoom(value:Object):void
      {
         var roomKey:String=this.intentRoomKey(value);
         if(this.lastRoomKey!="" && roomKey!=this.lastRoomKey)this.resetProductionChoice();
         this.lastRoomKey=roomKey;
         this.room=value;
         if(value==null || roomKey=="" || value.isPlayList==true){this.resetProductionChoice();this.visible=false;return;}
         var map:String=this.mapID(),gt:String=this.gameType(),intent:String=this.difficultyIntent();
         if(!/^[a-z][a-z0-9_]{0,63}$/.test(map) || !/^[a-z][a-z0-9_]{0,31}$/.test(gt) || intent=="")
         {this.resetProductionChoice();this.visible=false;return;}
         var context:String=map+" "+gt+" "+this.category()+" "+intent;
         if(this.lastContext!="" && context!=this.lastContext)this.resetProductionChoice();
         if(this.lastContext=="")
         {
            // A genuine local Create Room choice may seed its own host view.
            // Guests start unchecked and require their own explicit click.
            this.localSelection=this.owner() && CFunctionContainerCommon.Dvar_GetString("ui_p2p_create")=="1";
            CFunctionContainerCommon.setdvar("ui_p2p_create","0");
            CFunctionContainerCommon.execnow("p2p_room "+context);
            if(this.localSelection)CFunctionContainerCommon.execnow("p2p_localhost 1");
         }
         this.lastContext=context;this.choice.selected=this.localSelection;this.visible=true;
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

      private function difficultyIntent():String
      {
         // MP currentLevel is a league. A UI slot is only a hint: the SDK
         // compares its independently authenticated, current room selection.
         if(this.category()=="mp") return "0 -1";
         if(this.room==null || !("currentLevel" in this.room) || typeof this.room.currentLevel!="number") return "";
         var tier:Number=Number(this.room.currentLevel);
         if(!isFinite(tier) || tier<0 || tier>3 || int(tier)!=tier) return "";
         return "1 "+int(tier);
      }

      public function setRoom(value:Object):void
      {
         if(CFunctionContainerCommon.Dvar_GetString("ui_p2p_startup_protocol")=="3"){this.setProductionRoom(value);return;}
         this.room=value;
         this.visible=false;
         if(!this.owner() || this.room.isPlayList==true || this.category()=="coop")
         {
            if(this.localSelection && CFunctionContainerCommon.Dvar_GetString("ui_p2p_enabled")=="1")
               CFunctionContainerCommon.execnow("p2p_localhost 0");
            this.localSelection=false;
            this.lastContext="";
            return;
         }
         var map:String=this.mapID();
         var gt:String=this.gameType();
         if(!/^[a-z0-9_]+$/.test(map) || !/^[a-z0-9_]+$/.test(gt))
         {
            var protocol:String=CFunctionContainerCommon.Dvar_GetString("ui_p2p_startup_protocol");
            if(protocol.length>0 && protocol!="0" && this.localSelection)
            {
               CFunctionContainerCommon.execnow("p2p_cancel_start");
               return;
            }
            this.lastContext="";
            return;
         }
         var context:String=map+" "+gt+" "+this.category();
         if(CFunctionContainerCommon.Dvar_GetString("ui_p2p_startup_protocol")=="2")
         {
            var intent:String=this.difficultyIntent();
            if(intent=="")
            {
               if(this.localSelection) CFunctionContainerCommon.execnow("p2p_cancel_start");
               return;
            }
            context+=" "+intent;
         }
         if(context==this.lastContext) return;
         var first:Boolean=this.lastContext=="";
         if(first)
         {
            this.localSelection=CFunctionContainerCommon.Dvar_GetString("ui_p2p_create")=="1";
            CFunctionContainerCommon.setdvar("ui_p2p_create","0");
            // A fresh room defaults to the dedicated route even after an earlier local room.
            if(!this.localSelection && CFunctionContainerCommon.Dvar_GetString("ui_p2p_enabled")=="1")
               CFunctionContainerCommon.execnow("p2p_localhost 0");
         }
         this.lastContext=context;
         CFunctionContainerCommon.execnow("p2p_room "+context);
         if(first && this.localSelection)
            CFunctionContainerCommon.execnow("p2p_localhost 1");
      }
   }
}
