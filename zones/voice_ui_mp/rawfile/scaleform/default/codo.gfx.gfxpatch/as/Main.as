package
{
   import codo.common.AsyncTaskManager;
   import codo.common.BaseMain;
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.CommManager;
   import codo.common.FPSMemCounter;
   import codo.common.PageManagerMain;
   import codo.common.ThreeDAdjuster;
   import codo.constants.AsyncTaskName;
   import codo.constants.CodoPageViewDB;
   import codo.constants.MapTable;
   import codo.constants.MarketSlots;
   import codo.constants.MenuType;
   import codo.constants.ModeTable;
   import codo.objects.CodoPageObj;
   import codo.objects.CodoViewObj;
   import codo.utils.LocalizeUtil;
   import codo.utils.ScaleformUtil;
   import codo.utils.TableUtil;
   import flash.display.Sprite;
   import flash.display.StageAlign;
   import flash.display.StageScaleMode;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import flash.events.MouseEvent;
   import flash.ui.Keyboard;
   import flash.utils.getDefinitionByName;
   
   [SWF(frameRate="30",backgroundColor="#061416",width="1280",height="960")]
   public class Main extends BaseMain
   {
      
      public static var KEY_DOWN_HANDLE:Function;
      
      // Decompilation skipped
      
      private var _consoleInfo:Sprite = null;
      
      private var _start3DAdjust:Boolean = false;
      
      private var _ui3DAdjustMouseDownX:Number;
      
      private var _ui3DAdjustMouseDownY:Number;
      
      private var _adjustType:uint = 0;
      
      private var _shiftDown:Boolean = false;
      
      private var _altDown:Boolean = false;
      
      private var _ctrlDown:Boolean = false;
      
      private var threeDAdjuster:ThreeDAdjuster;
      
      private var theatrePage:CodoPageObj;
      
      private var theatreRoom:Object;
      
      private var theatreMenu:Object;
      
      private var theatreFooter:Object;
      
      private var theatreRevision:String = "";
      
      private var theatreTicks:int = 0;
      
      private var theatreMapNames:Object;
      
      private var theatreModeNames:Object;
      
      public function Main()
      {
         // Decompilation skipped
      }
      
override protected function onBeforeAddToStage() : void
      {
         // Decompilation skipped
      }
      
      override protected function onInit() : void
      {
         // Decompilation skipped
      }
      
      public function addTipText() : void
      {
         // Decompilation skipped
      }
      
      override protected function onStageKeyHandle(cEvt:KeyboardEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onStageMouseDownHandle(cEvt:MouseEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onStageMouseMoveHandle(cEvt:MouseEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onStageMouseUpHandle(cEvt:MouseEvent) : void
      {
         // Decompilation skipped
      }
      
      override protected function resetStageScale() : void
      {
         // Decompilation skipped
      }
      
      private function theatreClone(linkage:String) : CodoViewObj
      {
         // Decompilation skipped
      }
      
      private function initTheatre() : void
      {
         // Decompilation skipped
      }
      
      private function openTheatre() : void
      {
         // Decompilation skipped
      }
      
      private function closeTheatre() : void
      {
         // Decompilation skipped
      }
      
      private function updateTheatre(event:Event) : void
      {
         // Decompilation skipped
      }
      
      private function theatreDisplayName(raw:String, isMap:Boolean) : String
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

