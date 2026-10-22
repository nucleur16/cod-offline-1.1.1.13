package codo.views
{
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationDict;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.objects.GroupCountsObj;
   import components.BaseButton;
   import components.PlayModeSelectionBtn;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.geom.Matrix;
   import flash.geom.Rectangle;
   import flash.text.TextField;
   import flash.text.TextFormat;
   import flash.utils.getDefinitionByName;
   
   public class MatchSelectModeHomeView extends BaseView
   {
      
      private static var POS_X_ORIGIN:int = 640;
      
      private static var POS_Y_ORIGIN:int;
      
      // Decompilation skipped
      
      public var myCoopBtn:PlayModeSelectionBtn;
      
      public var myMpBtn:PlayModeSelectionBtn;
      
      public var myCyborgsBtn:PlayModeSelectionBtn;
      
      public var mySurvivalBtn:BaseButton;
      
      private var _originMatrix_coop:Matrix;
      
      private var _originMatrix_mp:Matrix;
      
      private var _originMatrix_cyborg:Matrix;
      
      private var theatreModeContainer:Sprite;
      
      private var survivalModeContainer:Sprite;
      
      public function MatchSelectModeHomeView()
      {
         // Decompilation skipped
      }
      
      override protected function onInit() : void
      {
         // Decompilation skipped
      }
      
      override protected function onInitListener() : void
      {
         // Decompilation skipped
      }
      
      override protected function showView() : void
      {
         // Decompilation skipped
      }
      
      override protected function hideView() : void
      {
         // Decompilation skipped
      }
      
      override protected function enableData() : void
      {
         // Decompilation skipped
      }
      
      override protected function disableData() : void
      {
         // Decompilation skipped
      }
      
      private function bind_groupCounts(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onCoopClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onCyborgsClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onSurvivalClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onMPClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onEnableCyborgHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onEnableSurvivalHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function set enableCyborg(enable:Boolean) : void
      {
         // Decompilation skipped
      }
      
      private function set enableSurvival(enable:Boolean) : void
      {
         // Decompilation skipped
      }
      
      private function bottomModeControl(label:String, xPos:Number, yPos:Number) : Sprite
      {
         // Decompilation skipped
      }
      
      private function initTheatreModeCards() : void
      {
         // Decompilation skipped
      }
      
      private function showTheatreModeCard() : void
      {
         // Decompilation skipped
      }
      
      private function hideTheatreModeCard() : void
      {
         // Decompilation skipped
      }
      
      private function onTheatreModeClick(event:ParaEvent) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

