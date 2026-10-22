package codo.views
{
   import codo.common.BaseView;
   import codo.constants.AsyncTaskID;
   import codo.objects.ResultObj;
   import codo.objects.ScoreboardObj;
   import components.list_part.HudScoreboardItemRenderer;
   import components.scoreboard_part.MatchStatusBar;
   import components.scoreboard_part.ScoreboardTeamPanel;
   
   public class ScoreboardBaseView extends BaseView
   {
      
      // Decompilation skipped
      
      public var myHeader:HudScoreboardItemRenderer;
      
      public var myTeamBlue:ScoreboardTeamPanel;
      
      public var myTeamRed:ScoreboardTeamPanel;
      
      public var myFooter:MatchStatusBar;
      
      private var _team2Offset:Number;
      
      private var _footerOffset:Number;
      
      private var _modeInited:Boolean;
      
      public function ScoreboardBaseView()
      {
         // Decompilation skipped
      }
      
      override protected function onInit() : void
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
      
      protected function bind_hudScoreboard(cResult:ResultObj) : void
      {
         // Decompilation skipped
      }
      
      protected function bind_matchStatus(cResult:ResultObj) : void
      {
         // Decompilation skipped
      }
      
      private function PanelBottom(panel:ScoreboardTeamPanel) : Number
      {
         // Decompilation skipped
      }
      
      protected function IsCyborgsAsset() : Boolean
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

