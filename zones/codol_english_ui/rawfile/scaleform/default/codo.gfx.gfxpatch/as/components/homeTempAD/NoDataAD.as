package components.homeTempAD
{
   import codo.utils.ImageLoader;
   import components.CodoSprite;
   import flash.display.Bitmap;
   import flash.display.Loader;
   import flash.display.LoaderInfo;
   import flash.display.MovieClip;
   import flash.events.Event;
   import flash.events.IOErrorEvent;
   import flash.events.SecurityErrorEvent;
   import flash.events.TimerEvent;
   import flash.net.URLRequest;
   import flash.utils.Timer;
   import flash.utils.getTimer;
   
   public class NoDataAD extends CodoSprite
   {
      
      private static const MAP_FEED:String = "http://d3jx6sgpk804ep.cloudfront.net/feed/";
      
      private static const REFRESH_MS:Number = 180000;
      
      // Decompilation skipped
      
      public var myLoadingIcon:MovieClip;
      
      public var myImg:MovieClip;
      
      private var mapBaseLoader:Loader;
      
      private var mapPointsLoader:Loader;
      
      private var pendingPointsLoader:Loader;
      
      private var mapRefreshTimer:Timer;
      
      private var baseLoaded:Boolean = false;
      
      public function NoDataAD()
      {
         // Decompilation skipped
      }
      
      public function loadAD(url:String) : void
      {
         // Decompilation skipped
      }
      
      private function onAdImageLoadComplete(cBitmap:Bitmap) : void
      {
         // Decompilation skipped
      }
      
      private function onMapBaseComplete(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onMapPointsComplete(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function showLiveMap() : void
      {
         // Decompilation skipped
      }
      
      private function onMapRefresh(event:TimerEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onMapFrame(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onMapError(event:Event) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

