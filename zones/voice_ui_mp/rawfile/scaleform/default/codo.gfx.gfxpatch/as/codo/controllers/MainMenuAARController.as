package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.CFunctionContainer;
   import codo.common.CFunctionContainerCommon;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.BCEventHUD;
   import codo.constants.ChallengeTable;
   import codo.constants.CodoPageViewDB;
   import codo.constants.SyncDataID;
   import codo.constants.SyncDataIDHUD;
   import codo.objects.AARChallengeObj;
   import codo.objects.CodoPageObj;
   import codo.objects.RecentMatchStatsObj;
   import codo.services.MatchService;
   import codo.utils.LocalizeUtil;
   
   public class MainMenuAARController extends BaseController
   {
      public static const MAX_CHALLENGES:int = 3;
      
      public static const MAX_RECENT_MATCHES:int = 15;
      
      private static const CHALLENGE_FIELD_VIS:int = 0;
      
      private static const CHALLENGE_FIELD_KEY:int = 1;
      
      private static const CHALLENGE_FIELD_LEVEL:int = 2;
      
      private static const CHALLENGE_FIELD_START:int = 3;
      
      private static const CHALLENGE_FIELD_GAIN:int = 4;
      
      private static const CHALLENGE_FIELD_TARGET:int = 5;
      
      private static const CHALLENGE_FIELD_START_PCT:int = 6;
      
      private static const CHALLENGE_FIELD_GAIN_PCT:int = 7;
      
      private var challenges:Vector.<AARChallengeObj> = null;
      
      public function MainMenuAARController()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         var mapID:String = CFunctionContainerCommon.Dvar_GetString("ui_played_mapname");
         if(mapID == "" || !MatchService.getInstance().isCyborgMap(mapID))
         {
            return;
         }
         this.addSyncDP(SyncDataID.SCALES_LINES_DAT,this.getScalesLinesDat);
         this.addSyncDP(SyncDataID.TOTAL_POINTS_LINE_DAT,this.getTotalPointsLineDat);
         this.addSyncDP(SyncDataIDHUD.MATCH_RESULT,this.getMatchResult);
         this.addSyncDP(SyncDataID.AAR_CHALLENGES,this.getChallenges);
         this.addSyncDP(SyncDataIDHUD.AAR_RECENT_SCORES,this.getRecentMatchStats);
         this.addSyncDP(SyncDataIDHUD.AAR_RECENT_AVG,this.getAverageScore);
         this.addSyncDP(SyncDataIDHUD.EARLY_EXIT_DELAY,this.getEarlyExitDelay);
         this.addSyncDP(SyncDataID.PREV_MATCH_DIFFICULITIES,this.getPrevMatchDifficulties);
         this.listen(BCEvent.EVT_STOP_CONTROLLER_RUMBLE,this.onStopControllerRumbleHandle);
         this.listen(BCEventHUD.BACK_TO_LOBBY_MENU,this.onCloseAARPopup);
         this.listen(BCEvent.EVT_PAGE_CHANGED,this.onPageChangedHandle);
         this.listen(BCEvent.EVT_PRESS_ESC_CLOSE_WINDOW,this.onESC);
      }
      
      private function onPageChangedHandle(cEvt:ParaEvent) : void
      {
         var _currentPageObj:CodoPageObj = cEvt.myPara.pageObj;
         if(_currentPageObj == CodoPageViewDB.PAGE_POPUP_AAR || _currentPageObj == CodoPageViewDB.PAGE_ADV_POPUP_LOBBY_SUMMARY)
         {
            this.setBindData(AsyncTaskID.HUD_SCORE_BOARD,CFunctionContainer.AS_GetScoreBoard_Main());
            this.setBindData(AsyncTaskID.MATCH_STATUS,CFunctionContainer.AS_GetScoreBoard_MapType_Main());
            this.setBindData(AsyncTaskID.ZM_MATCH_GPXPGAINED,this.getXPGPGained());
            if(this.getSyncDP(SyncDataID.IS_CYBORG))
            {
               this.setBindData(AsyncTaskID.OBJECTIVES,CFunctionContainer.AS_GetObjectives_Main());
            }
         }
      }
      
      private function getXPGPGained() : Object
      {
         var zmGameBonus:Object = new Object();
         zmGameBonus.cpGained = CFunctionContainerCommon.Dvar_GetString("zm_eog_cpgained");
         zmGameBonus.charxpGained = CFunctionContainerCommon.Dvar_GetString("zm_eog_charxp");
         zmGameBonus.careerxpGained = CFunctionContainerCommon.Dvar_GetString("zm_eog_xp");
         CFunctionContainerCommon.setdvar("zm_eog_xp","0");
         CFunctionContainerCommon.setdvar("zm_eog_charxp","0");
         CFunctionContainerCommon.setdvar("zm_eog_cpgained","0");
         return zmGameBonus;
      }
      
      private function onCloseAARPopup(cEvt:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("ui_eog_show","0");
         this.prevPage();
      }
      
      private function onESC(cEvt:ParaEvent) : void
      {
         CFunctionContainerCommon.setdvar("ui_eog_show","0");
      }
      
      public function getEarlyExitDelay() : int
      {
         return 0;
      }
      
      private function getPrevMatchDifficulties() : int
      {
         var mapID:String = CFunctionContainerCommon.Dvar_GetString("ui_played_mapname");
         if(mapID != "" && MatchService.getInstance().isCyborgMap(mapID))
         {
            var modeString:String = CFunctionContainerCommon.Dvar_GetString("ui_eog_zm_mode");
            if(modeString == "0")
            {
               return 0;
            }
            if(modeString == "1")
            {
               return 1;
            }
            if(modeString == "2")
            {
               return 2;
            }
            if(modeString == "3")
            {
               return 3;
            }
         }
         var diffString:String = CFunctionContainerCommon.Dvar_GetString("g_difficulty");
         if(diffString == "novice_d")
         {
            return 0;
         }
         if(diffString == "intermediate_d")
         {
            return 1;
         }
         if(diffString == "advanced_d")
         {
            return 2;
         }
         return 0;
      }
      
      private function getMatchResult() : int
      {
         var result:int = CFunctionContainerCommon.Dvar_GetInt("ui_eog_victory");
         if(result > 0)
         {
            return SyncDataIDHUD.MATCH_RESULT_VICTORY;
         }
         if(result == 0)
         {
            return SyncDataIDHUD.MATCH_RESULT_DEFEAT;
         }
         return SyncDataIDHUD.MATCH_RESULT_TIE;
      }
      
      private function initChallengeData() : void
      {
         var str:String = null;
         var fields:Array = null;
         var key:String = null;
         var name:String = null;
         var level:String = null;
         var challenge:AARChallengeObj = null;
         var desc:String = null;
         var target:String = null;
         this.challenges = new Vector.<AARChallengeObj>();
         for(var i:int = 0; i < MAX_CHALLENGES; i++)
         {
            str = CFunctionContainerCommon.Dvar_GetString("ui_eog_ch" + String(i));
            CFunctionContainerCommon.setdvar("ui_eog_ch" + String(i),"");
            Logger.print("MainMenuAARController::getChallenges" + str);
            fields = str.split("|");
            if(!(fields[CHALLENGE_FIELD_VIS] == "0" || fields.length <= 0))
            {
               key = fields[CHALLENGE_FIELD_KEY];
               if(!(key == null || key.length <= 0))
               {
                  name = "@" + CFunctionContainerCommon.StringTableLookup(ChallengeTable.PATH,ChallengeTable.KEY,key,ChallengeTable.NAME);
                  name = LocalizeUtil.localize(name);
                  level = "@" + CFunctionContainerCommon.StringTableLookup(ChallengeTable.PATH,ChallengeTable.KEY,key,ChallengeTable.LEVEL_STR);
                  level += "_" + fields[CHALLENGE_FIELD_LEVEL];
                  level = LocalizeUtil.localize(level);
                  challenge = new AARChallengeObj();
                  challenge.title = name + level;
                  desc = "@" + CFunctionContainerCommon.StringTableLookup(ChallengeTable.PATH,ChallengeTable.KEY,key,ChallengeTable.DESCRIPTION);
                  desc = LocalizeUtil.localize(desc);
                  target = fields[CHALLENGE_FIELD_TARGET];
                  challenge.description = LocalizeUtil.replaceConversionString(desc,[target]);
                  challenge.progressStart = Number(fields[CHALLENGE_FIELD_START_PCT]);
                  challenge.progressGain = Number(fields[CHALLENGE_FIELD_GAIN_PCT]);
                  this.challenges.push(challenge);
               }
            }
         }
      }
      
      private function getChallenges() : Object
      {
         if(this.challenges == null)
         {
            this.initChallengeData();
         }
         return this.challenges;
      }
      
      public function getRecentMatchStats(cGameType:int = 0) : Array
      {
         var currentMatchStats:RecentMatchStatsObj = null;
         var myPosition:Number = NaN;
         var fullRecentMatchStats:Array = CFunctionContainer.AS_GetRecentMatchStats();
         var returnedList:Array = new Array();
         for(var i:uint = uint(fullRecentMatchStats.length - MAX_RECENT_MATCHES); i < fullRecentMatchStats.length; i++)
         {
            currentMatchStats = new RecentMatchStatsObj(fullRecentMatchStats[i]);
            if(currentMatchStats.empty == false)
            {
               if(currentMatchStats.averageScore == 0)
               {
                  returnedList.push(Boolean(currentMatchStats.topScore) ? currentMatchStats.score / currentMatchStats.topScore : 0);
               }
               else
               {
                  myPosition = currentMatchStats.score / currentMatchStats.averageScore;
                  myPosition = myPosition > 2 ? 2 : myPosition;
                  returnedList.push(myPosition / 2);
               }
            }
         }
         return returnedList;
      }
      
      private function getAverageScore() : int
      {
         var currentMatchStats:RecentMatchStatsObj = null;
         var fullRecentMatchStats:Array = CFunctionContainer.AS_GetRecentMatchStats();
         var sum:int = 0;
         var count:int = 0;
         for(var i:uint = uint(fullRecentMatchStats.length - MAX_RECENT_MATCHES); i < fullRecentMatchStats.length; i++)
         {
            currentMatchStats = new RecentMatchStatsObj(fullRecentMatchStats[i]);
            if(currentMatchStats.empty == false)
            {
               sum += currentMatchStats.score;
               count++;
            }
         }
         return count == 0 ? 0 : int(sum / count);
      }
      
      private function onStopControllerRumbleHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainerCommon.execnow("stopControllerRumble");
      }
      
      private function getScalesLinesDat() : String
      {
         return CFunctionContainerCommon.Dvar_GetString("ui_eog_r8");
      }
      
      private function getTotalPointsLineDat() : String
      {
         return CFunctionContainerCommon.Dvar_GetString("ui_eog_r9");
      }
   }
}

