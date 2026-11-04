package codo.k33
{
   import codo.common.CFunctionContainerCommon;
   import flash.display.Sprite;

   // The existing room view creates this invisible bridge after Create Room succeeds.
   public class LocalMatchControls extends Sprite
   {
      private var room:Object;
      private var lastContext:String="";
      private var localSelection:Boolean=false;

      public function LocalMatchControls()
      {
         super();
         this.visible=false;
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
