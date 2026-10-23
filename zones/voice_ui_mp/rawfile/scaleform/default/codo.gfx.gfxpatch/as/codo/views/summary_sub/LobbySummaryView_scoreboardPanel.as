package codo.views.summary_sub
{
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.SyncDataID;
   import codo.objects.ResultObj;
   import codo.objects.ScoreboardObj;
   import components.list_part.HudScoreboardItemRenderer;
   import components.scoreboard_part.ScoreboardTeamPanel;
   import flash.utils.clearTimeout;
   import flash.utils.setTimeout;
   
   public class LobbySummaryView_scoreboardPanel extends LobbySummaryView_basePanel
   {
      
      private static const DISPLAY_TIME:int = 3000;
      
      // Decompilation skipped
      
      public var myTeamBlue:ScoreboardTeamPanel;
      
      public var myTeamRed:ScoreboardTeamPanel;
      
      public var myHeader:HudScoreboardItemRenderer;
      
      private var _team2Offset:Number;
      
      private var _modeInited:Boolean;
      
      private var _timeoutTimer:uint = 4294967295;
      
      public function LobbySummaryView_scoreboardPanel()
      {
         // Decompilation skipped
      }
      
      override protected function onInit() : void
      {
         // Decompilation skipped
      }
      
      override public function preEnable() : Boolean
      {
         // Decompilation skipped
      }
      
      override public function enable() : void
      {
         // Decompilation skipped
      }
      
      private function showComplete() : void
      {
         // Decompilation skipped
      }
      
      override public function disable() : void
      {
         // Decompilation skipped
      }
      
      protected function bind_hudScoreboard(cResult:ResultObj) : void
      {
         // Decompilation skipped
      }
      
      private function PanelBottom(panel:ScoreboardTeamPanel) : Number
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

