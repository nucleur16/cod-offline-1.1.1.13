package codo.k33
{
   import flash.display.Sprite;
   import codo.utils.LocalizeUtil;
   import flash.events.MouseEvent;
   import flash.text.TextField;
   import flash.text.TextFormat;

   // The private-room creation choice. It does not start hosting until the room exists.
   public class LocalCreateRoomOption extends Sprite
   {
      private var caption:TextField;
      private var chosen:Boolean=false;
      private var blueTheme:Boolean=false;

      public function LocalCreateRoomOption(sample:TextField)
      {
         super();
         var fmt:TextFormat=sample.defaultTextFormat;
         fmt.align="left";
         this.caption=new TextField();
         this.caption.defaultTextFormat=fmt;
         this.caption.embedFonts=sample.embedFonts;
         this.caption.x=37;
         this.caption.y=7;
         this.caption.width=108;
         this.caption.height=25;
         this.caption.text=LocalizeUtil.localize("@K33UI_LOCAL_PLAY");
         this.caption.selectable=false;
         this.caption.mouseEnabled=false;
         this.addChild(this.caption);
         this.buttonMode=true;
         this.mouseChildren=false;
         this.addEventListener(MouseEvent.CLICK,this.onClick);
         this.redraw();
      }

      public function get selected():Boolean
      {
         return this.chosen;
      }

      public function set selected(value:Boolean):void
      {
         this.chosen=value;
         this.redraw();
      }

      public function set multiplayerTheme(value:Boolean):void
      {
         this.blueTheme=value;
         this.caption.textColor=value ? 0xD9F0F5 : 0xE7D3BB;
         this.caption.y=value ? 2 : 7;
         this.redraw();
      }

      private function onClick(e:MouseEvent):void
      {
         this.selected=!this.chosen;
      }

      private function redraw():void
      {
         var top:Number=this.blueTheme ? 5 : 10;
         this.graphics.clear();
         // Use the panel itself as the background, like its stock difficulty options.
         this.graphics.beginFill(0x000000,0.001);
         this.graphics.drawRect(0,0,150,this.blueTheme ? 28 : 40);
         this.graphics.endFill();
         this.graphics.lineStyle(1,this.blueTheme ? (this.chosen ? 0x72D3DE : 0x43899F) : (this.chosen ? 0xBDD65A : 0xA36745));
         this.graphics.beginFill(this.blueTheme ? (this.chosen ? 0x277D98 : 0x102D3A) : (this.chosen ? 0x748B20 : 0x21120D),0.9);
         this.graphics.drawRect(10,top,18,18);
         this.graphics.endFill();
         if(this.chosen)
         {
            this.graphics.lineStyle(2,this.blueTheme ? 0xE9F9FC : 0xF2F5D1);
            this.graphics.moveTo(13,top+9);
            this.graphics.lineTo(17,top+13);
            this.graphics.lineTo(25,top+4);
         }
      }
   }
}
