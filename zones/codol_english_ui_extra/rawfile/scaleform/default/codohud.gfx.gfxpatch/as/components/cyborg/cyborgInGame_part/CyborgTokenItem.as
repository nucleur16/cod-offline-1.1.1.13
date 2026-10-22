package components.cyborg.cyborgInGame_part
{
   import codo.constants.LocalizationInGame;
   import codo.utils.ImageLoader;
   import codo.utils.LocalizeUtil;
   import com.greensock.TweenLite;
   import components.CodoSprite;
   import flash.display.Bitmap;
   import flash.display.MovieClip;
   import flash.display.Sprite;
   import flash.text.TextField;
   
   public class CyborgTokenItem extends CodoSprite
   {
      
      private static const TOKEN_TIMER:uint = 500;
      
      public static const STATE_EMPTY:int = 0;
      
      public static const STATE_SPENT:int = 1;
      
      public static const STATE_OWNED:int = 2;
      
      private static const IMG_SRC_EMPTY:String = "tokenEmpty";
      
      private static const IMG_SRC_SPENT:String = "tokenSpent";
      
      private static const IMG_SRC_OWNED:String = "tokenUnspent";
      
      // Decompilation skipped
      
      public var myCostTF:TextField;
      
      public var myKeyTipTF:TextField;
      
      public var myImageContainer:Sprite;
      
      public var myBgMask:Sprite;
      
      public var myBgProgress:Sprite;
      
      public var myBg:MovieClip;
      
      private var _imgWidth:Number;
      
      private var _imgHeight:Number;
      
      public function CyborgTokenItem()
      {
         // Decompilation skipped
      }
      
      public function startProgress() : void
      {
         // Decompilation skipped
      }
      
      public function stopProgress() : void
      {
         // Decompilation skipped
      }
      
      public function setTokenState(state:int, hasFocus:Boolean = false) : void
      {
         // Decompilation skipped
      }
      
      private function onIconLoadComplete(img:Bitmap) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

