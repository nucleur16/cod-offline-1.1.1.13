package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.PageManagerMain;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.BackToRoomType;
   import codo.constants.CodoPageViewDB;
   import codo.constants.CombatType;
   import codo.constants.Common;
   import codo.constants.GlobalBackBtnStatus;
   import codo.constants.GlobalBottomBtnStatus;
   import codo.constants.LocalizationParty;
   import codo.constants.MsgBtnCount;
   import codo.constants.PageID;
   import codo.constants.PlayListType;
   import codo.constants.QuickMatchType;
   import codo.constants.RankedMatchStatus;
   import codo.constants.RoomListType;
   import codo.constants.SyncDataID;
   import codo.constants.Table.UINotifications;
   import codo.objects.QuickMatchFilterObj;
   import codo.services.MatchService;
   import codo.utils.ScaleformUtil;
   
   public class MainMenuController extends BaseController
   {
      private var _currentPageID:String;
      
      private var _currentRankedMatchStatus:String;
      
      private var _currentBackBtnStatus:uint;
      
      private var _isInRoomReady:Boolean = false;
      
      private var _currentBottomBtnStatus:uint;
      
      private var _prePageIsMP:Boolean = true;
      
      public function MainMenuController()
      {
         super();
      }
      
      override protected function onInitialized() : void
      {
         this.addSyncDP(SyncDataID.IS_CALLSIGN_EXPAND,this.getIsCallsignExpand);
         this.addSyncDP(SyncDataID.LOGO_CLIACKABLE,this.getLogoIsClickable);
         this.listen(BCEvent.EVT_QUICK_MATCH_BACK,this.onQuickMatchBackHandle);
         this.listen(BCEvent.EVT_GOTO_PLAY_MODE_SELECTION,this.onGotoPlayModeSelectionHandle);
         this.listen(BCEvent.EVT_GOTO_HOME,this.onGotoHomeHandle);
         this.listen(BCEvent.ON_LOGO_BTN_CLICKED,this.onLogoBtnClickedHandle);
         this.listen(BCEvent.EVT_GOTO_QUICKMATCH_MP,this.onGotoQuickMatchMPHandle);
         this.listen(BCEvent.EVT_GOTO_ROOMLIST_MP,this.onGotoRoomListMPHandle);
         this.listen(BCEvent.EVT_GOTO_ROOMLIST_COOP,this.onGotoRoomListCoopHandle);
         this.listen(BCEvent.EVT_GOTO_QUICKMATCH_COOP,this.onGotoQuickMatchCoopHandle);
         this.listen(BCEvent.EVT_GOTO_QUICKMATCH_ZOMBIE,this.onGotoQuickMatchZombieHandle);
         this.listen(BCEvent.EVT_GOTO_QUICKMATCH_COMBAT_TRAINING_NEW,this.onGotoQuickMatchCombatTrainingNewHandle);
         this.listen(BCEvent.EVT_GOTO_WEAPONLOADOUT,this.onGotoWeaponLoadoutHandle);
         this.listen(BCEvent.EVT_GOTO_CHARACTERLOADOUT,this.onGotoCharacterLoadoutHandle);
         this.listen(BCEvent.EVT_GOTO_ITEM,this.onGotoItemHandle);
         this.listen(BCEvent.EVT_GOTO_QUIT,this.onGotoQuitHandle);
         this.listen(BCEvent.EVT_GOTO_QUICKMATCH_LOBBY,this.onGotoQuickMatchLobbyHandle);
         this.listen(BCEvent.EVT_GOTO_MARKET,this.onGotoMarketHandle);
         this.listen(BCEvent.EVT_GOTO_SHOW_CLAIMREWARDS,this.onShowClaimRewards);
         this.listen(BCEvent.TRY_OPEN_MY_CAREER,this.onSwitchMyUserInfoHandle);
         this.listen(BCEvent.TRY_OPEN_LEADERBOARD,this.onSwitchLeaderboardHandle);
         this.listen(BCEvent.EVT_LOBBY_SUMMARY,this.onGotoLobbySummaryHandle);
         this.listen(BCEvent.EVT_GOTO_MAIN_COOP,this.onGotoNewMainHandle);
         this.listen(BCEvent.EVT_GOTO_MAIN_CYBORG,this.onGotoNewMainHandle);
         this.listen(BCEvent.EVT_GOTO_MAIN_MP,this.onGotoNewMainHandle);
         this.listen(BCEvent.EVT_GOTO_MAIN_CT,this.onGotoNewMainHandle);
         this.listen(BCEvent.EVT_GLOBAL_BACK_BTN_CLICK,this.onGlobalBackHandle);
         this.listen(BCEvent.EVT_BOTTOM_START_GAME,this.onBottomStartBtnClickHandle);
         this.listen(BCEvent.EVT_BOTTOM_LEFT_BUTTON,this.onBottomLeftBtnClickHandle);
         this.listen(BCEvent.EVT_MPCT_PLAYLIST_UNLOCKED,this.onChangeStartBtnStatusHandle);
         this.listen(BCEvent.EVT_MPCT_ROOMLIST_SELECTED,this.onChangeStartBtnStatusHandle);
         this.listen(BCEvent.EVT_PAGE_ID_CHANGED,this.onPageIDChangedHandle);
         this.listen(BCEvent.EVT_RANKED_MATCH_CHANGE_STATUS,this.onRankedMatchStatusChangedHandle);
         this.listen(BCEvent.ROOM_INFO,this.onRoomForcesHandle);
         this.listen(BCEvent.ROOM_FORCES,this.onRoomForcesHandle);
         this.listen(BCEvent.EVT_ON_ROOM_LEFT,this.onRoomLeftHandle);
         this.listen(BCEvent.TRY_OPEN_MAIL,this.onTryOpenMailHandle);
         this.listen(BCEvent.TRY_OPEN_FRIENDLIST,this.onTryOpenFriendlistHandle);
         this.listen(BCEvent.TRY_OPEN_MESSAGEBOX,this.onTryOpenMessageBoxHandle);
         this.listen(BCEvent.SOCIAL_EXPAND_CHANGE,this.onSocialExpandChangeHandle);
      }
      
      private function onPageIDChangedHandle(cEvt:ParaEvent) : void
      {
         this._currentPageID = cEvt.myPara.id;
         this.refreshGlobalBackStatus();
         this.refreshBottomButtonStatus();
      }
      
      private function onRoomForcesHandle(cEvt:ParaEvent) : void
      {
         if(!this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH))
         {
            this._isInRoomReady = cEvt.myPara.forces.isReady;
            this.refreshBottomButtonStatus();
         }
      }
      
      private function onRoomLeftHandle(cEvt:ParaEvent) : void
      {
         this.refreshBottomButtonStatus();
      }
      
      private function onRankedMatchStatusChangedHandle(cEvt:ParaEvent) : void
      {
         this._currentRankedMatchStatus = cEvt.myPara.status;
         this.refreshGlobalBackStatus();
         this.refreshBottomButtonStatus();
      }
      
      private function refreshGlobalBackStatus() : void
      {
         var tempIsInRoom:Boolean = this.getSyncDP(SyncDataID.IS_IN_ROOM);
         var tempEnable:Boolean = true;
         switch(this._currentPageID)
         {
            case PageID.HOME_NEW:
               break;
            case PageID.HOME:
               if(tempIsInRoom == true && this._currentRankedMatchStatus != RankedMatchStatus.IN_PROGRESS && this._currentRankedMatchStatus != RankedMatchStatus.IN_PROGRESS_IN_ROOM)
               {
                  this._currentBackBtnStatus = GlobalBackBtnStatus.STATUS_BACK_TO_INROOM;
               }
               else if(this._currentRankedMatchStatus == RankedMatchStatus.IN_PROGRESS_IN_ROOM)
               {
                  this._currentBackBtnStatus = GlobalBackBtnStatus.STATUS_BACK_TO_INROOM;
                  tempEnable = false;
               }
               else
               {
                  this._currentBackBtnStatus = GlobalBackBtnStatus.STATUS_CHANGE_GAME_TYPE;
               }
               break;
            case PageID.INROOM:
               this._currentBackBtnStatus = GlobalBackBtnStatus.STATUS_LEAVE_ROOM;
               if(this._currentRankedMatchStatus == RankedMatchStatus.READY_IN_ROOM || this._currentRankedMatchStatus == RankedMatchStatus.READY)
               {
                  tempEnable = false;
               }
               break;
            case PageID.QUICK_MATCH_COOP:
            case PageID.ROOM_COOP:
               this._currentBackBtnStatus = GlobalBackBtnStatus.STATUS_CHANGE_GAME_TYPE;
               break;
            default:
               this._currentBackBtnStatus = GlobalBackBtnStatus.STATUS_BACK_TO_LANDING_PAGE;
         }
         this.setBindData(AsyncTaskID.CURRRENT_TOP_BACK_BTN_STATUS,{
            "status":this._currentBackBtnStatus,
            "enable":tempEnable
         });
      }
      
      private function refreshBottomButtonStatus() : void
      {
         var tempIsInRoom:Boolean = this.getSyncDP(SyncDataID.IS_IN_ROOM);
         var tempIsRankedMatch:Boolean = this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH);
         var tempIsCyborg:Boolean = this.getSyncDP(SyncDataID.CURRENT_MATCH_TYPE) == QuickMatchType.CYBORG;
         if(tempIsInRoom || tempIsRankedMatch)
         {
            switch(this._currentPageID)
            {
               case PageID.INROOM:
                  if(tempIsRankedMatch)
                  {
                     if(this._currentRankedMatchStatus == RankedMatchStatus.BACK_FROM_GAME)
                     {
                        this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_NOTIDLE_RANKED_INROOM;
                     }
                     else
                     {
                        this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_NOTIDLE_RANKED_INROOM_CANT_LEAVE;
                     }
                  }
                  else if(this._isInRoomReady == true)
                  {
                     this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_NOTIDLE_INROOM_READY;
                  }
                  else
                  {
                     this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_NOTIDLE_INROOM_NOT_READY;
                  }
                  break;
               default:
                  if(this.getSyncDP(SyncDataID.IS_RANKED_MATCH_IN_PROGRESS))
                  {
                     this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_NOTIDLE_RANKED_INPROGRESS;
                  }
                  else if(this.getSyncDP(SyncDataID.IS_RANKED_MATCH_READY) == true)
                  {
                     this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_NOTIDLE_NOT_INROOM_CANT_LEAVE;
                  }
                  else
                  {
                     this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_NOTIDLE_NOT_INROOM;
                  }
            }
         }
         else
         {
            switch(this._currentPageID)
            {
               case PageID.HOME:
                  if(tempIsCyborg == true)
                  {
                     this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_IDLE_CYBORG_LANDING;
                  }
                  else
                  {
                     this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_IDLE_MP_LANDING;
                  }
                  break;
               case PageID.QUICK_MATCH_MPCT_PLAYLIST:
                  this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_IDLE_MP_PLAYLIST_CHOOSE;
                  break;
               case PageID.QUICK_MATCH_MPCT_ROOMLIST:
                  this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_IDLE_MP_ROOM_JOIN;
                  break;
               case PageID.QUICK_MATCH_MPCT_ROOMCREATOR:
                  this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_IDLE_MP_ROOM_CREATOR;
                  break;
               case PageID.QUICK_MATCH_COOP:
                  this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_IDLE_COOP_STAGE_CHOOSE;
                  break;
               case PageID.ROOM_COOP:
                  // k33 offline: the Spec Ops room list is never shown (MatchRoomListView), so nothing can be selected
                  // to join: this page's Join button has no label and is not shown.
                  GlobalBottomBtnStatus.LOCALIZATION[GlobalBottomBtnStatus.STATUS_IDLE_COOP_ROOM_JOIN] = [null,null,null];
                  this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_IDLE_COOP_ROOM_JOIN;
                  break;
               case PageID.ROOMLIST_ZOMBIE:
                  // k33 offline: the Zombies room list is never shown (CyborgMatchRoomListView), so nothing can be
                  // selected to join: this page's Join Room button has no label and is not shown.
                  GlobalBottomBtnStatus.LOCALIZATION[GlobalBottomBtnStatus.STATUS_IDLE_CYBORG_ROOM_JOIN] = [null,null,null];
                  this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_IDLE_CYBORG_ROOM_JOIN;
                  break;
               default:
                  this._currentBottomBtnStatus = GlobalBottomBtnStatus.STATUS_IDLE_DEFAULT;
            }
         }
         this.setBindData(AsyncTaskID.CURRRENT_BOTTOM_BTN_STATUS,{
            "status":this._currentBottomBtnStatus,
            "isCyborg":tempIsCyborg
         });
      }
      
      private function onQuickMatchBackHandle(cEvt:ParaEvent) : void
      {
         switch(this.getSyncDP(SyncDataID.CURRENT_PAGE_ID))
         {
            case PageID.PLAY_MODE_SELECTION:
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
               break;
            case PageID.QUICK_MATCH_COOP:
            case PageID.QUICK_MATCH_COOP_TUTORIAL:
            case PageID.QUICK_MATCH_CT:
            case PageID.QUICK_MATCH_MP:
            case PageID.ROOM_COOP:
            case PageID.ROOM_MP:
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW);
               break;
            case PageID.ROOM_CREATOR:
               this.prevPage();
         }
      }
      
      private function onGotoPlayModeSelectionHandle(cEvt:ParaEvent) : void
      {
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW);
      }
      
      private function onGotoHomeHandle(cEvt:ParaEvent) : void
      {
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
      }
      
      private function onLogoBtnClickedHandle(cEvt:ParaEvent) : void
      {
         var tempPageID:String = this.getSyncDP(SyncDataID.CURRENT_PAGE_ID);
         if(ScaleformUtil.isTempContentEnabled_1() == true)
         {
            if(this.getSyncDP(SyncDataID.HAS_MP_CHARACTER))
            {
               if(this.getSyncDP(SyncDataID.IS_IN_ROOM) || this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH))
               {
                  if(this.getSyncDP(SyncDataID.CURRENT_MATCH_TYPE) != QuickMatchType.COOP && tempPageID != PageID.INROOM && tempPageID != PageID.HOME)
                  {
                     this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
                  }
               }
               else if(tempPageID != PageID.HOME_NEW)
               {
                  this.gotoPage(CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW);
               }
            }
         }
         else if(this.getSyncDP(SyncDataID.HAS_MP_CHARACTER) && tempPageID != PageID.HOME)
         {
            this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
         }
      }
      
      private function getLogoIsClickable() : Boolean
      {
         var tempPageID:String = this.getSyncDP(SyncDataID.CURRENT_PAGE_ID);
         var result:Boolean = false;
         if(ScaleformUtil.isTempContentEnabled_1() == true)
         {
            if(this.getSyncDP(SyncDataID.HAS_MP_CHARACTER))
            {
               if(this.getSyncDP(SyncDataID.IS_IN_ROOM) || this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH))
               {
                  if(this.getSyncDP(SyncDataID.CURRENT_MATCH_TYPE) != QuickMatchType.COOP && tempPageID != PageID.INROOM && tempPageID != PageID.HOME)
                  {
                     result = true;
                  }
               }
               else if(tempPageID != PageID.HOME_NEW)
               {
                  result = true;
               }
            }
         }
         else if(this.getSyncDP(SyncDataID.HAS_MP_CHARACTER) && tempPageID != PageID.HOME)
         {
            result = true;
         }
         return result;
      }
      
      private function setQuickMatchType(type:uint) : void
      {
         if(Boolean(this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH)))
         {
            return;
         }
         this.broadcast(BCEvent.EVT_SET_MATCH_TYPE,type);
      }
      
      private function onGotoQuickMatchMPHandle(cEvt:ParaEvent) : void
      {
         this.setQuickMatchType(QuickMatchType.MP);
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_MATCH_MPCT_PLAYLIST);
      }
      
      private function onGotoRoomListMPHandle(cEvt:ParaEvent) : void
      {
         this.setQuickMatchType(QuickMatchType.MP);
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_ROOM_MP);
      }
      
      private function onGotoQuickMatchCoopHandle(cEvt:ParaEvent) : void
      {
         this.setQuickMatchType(QuickMatchType.COOP);
         var isAct1Complete:Boolean = MatchService.getInstance().isActComplete(1);
         var hasCharacter:Boolean = this.getSyncDP(SyncDataID.HAS_MP_CHARACTER);
         if(hasCharacter || isAct1Complete)
         {
            this.gotoPage(CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP);
         }
         else
         {
            this.gotoPage(CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP_TUTORIAL);
         }
      }
      
      private function onGotoRoomListCoopHandle(cEvt:ParaEvent) : void
      {
         this.setQuickMatchType(QuickMatchType.COOP);
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_ROOM_COOP);
      }
      
      private function onGotoQuickMatchZombieHandle(cEvt:ParaEvent) : void
      {
         this.setQuickMatchType(QuickMatchType.CYBORG);
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_ROOM_CYBORGS);
      }
      
      private function onGotoQuickMatchCombatTrainingNewHandle(cEvt:ParaEvent) : void
      {
         this.setQuickMatchType(QuickMatchType.TRAINING);
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_CT);
      }
      
      private function onGotoWeaponLoadoutHandle(cEvt:ParaEvent) : void
      {
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_WEAPON);
      }
      
      private function onGotoCharacterLoadoutHandle(cEvt:ParaEvent) : void
      {
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_CHARACTER);
      }
      
      private function onGotoItemHandle(cEvt:ParaEvent) : void
      {
         this.gotoPage(CodoPageViewDB.PAGE_MAIN_ITEM);
      }
      
      private function onGotoLobbySummaryHandle(cEvt:ParaEvent) : void
      {
         this.gotoPage(CodoPageViewDB.PAGE_ADV_POPUP_LOBBY_SUMMARY);
      }
      
      private function onGotoQuitHandle(cEvt:ParaEvent) : void
      {
         this.gotoPage(CodoPageViewDB.PAGE_POPUP_SUMMARY);
      }
      
      private function onGotoMarketHandle(cEvt:ParaEvent) : void
      {
         if(this.getSyncDP(SyncDataID.CURRENT_PAGE_ID) == PageID.MARKET)
         {
            this.prevPage();
         }
         else
         {
            this.gotoPage(CodoPageViewDB.PAGE_MAIN_MARKET);
         }
      }
      
      private function onGotoQuickMatchLobbyHandle(cEvt:ParaEvent) : void
      {
         if(Boolean(this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH)))
         {
            return;
         }
         var isInParty:Boolean = this.getSyncDP(SyncDataID.IS_IN_PARTY);
         var isPartyHost:Boolean = this.getSyncDP(SyncDataID.IS_PARTY_HOST);
         if(isInParty && !isPartyHost)
         {
            this.showMsg(LocalizationParty.NOTE_IN_A_PARTY,MsgBtnCount.ONE);
            return;
         }
         this.setQuickMatchType(cEvt.myPara.data.type);
         switch(cEvt.myPara.data.type)
         {
            case QuickMatchType.COOP:
               if(isInParty)
               {
                  this.showMsg(LocalizationParty.NOTE_IN_A_PARTY,MsgBtnCount.ONE);
                  return;
               }
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_ROOM_MP);
               this.broadcast(BCEvent.EVT_HIDE_COOP_GO_LOBBY,{"data":cEvt.myPara.data});
               break;
            case QuickMatchType.MP:
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_ROOM_MP);
               this.broadcast(BCEvent.EVT_HIDE_MP_GO_LOBBY,{"data":cEvt.myPara.data});
               break;
            case QuickMatchType.TRAINING:
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_ROOM_MP);
               break;
            case QuickMatchType.CYBORG:
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_ROOM_MP);
               this.broadcast(BCEvent.EVT_HIDE_MP_GO_LOBBY,{"data":cEvt.myPara.data});
         }
      }
      
      private function onSwitchMyUserInfoHandle(cEvt:ParaEvent) : void
      {
         if(CodoPageViewDB.PAGE_LAYER_MAIN.currentPageObj != CodoPageViewDB.PAGE_MAIN_CAREER)
         {
            if(CodoPageViewDB.PAGE_LAYER_MAIN.currentPageObj == CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP || CodoPageViewDB.PAGE_LAYER_MAIN.currentPageObj == CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP_TUTORIAL)
            {
               this._prePageIsMP = false;
            }
            else
            {
               this._prePageIsMP = true;
            }
            this.gotoPage(CodoPageViewDB.PAGE_MAIN_CAREER);
         }
         else if(this._prePageIsMP)
         {
            this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
         }
         else
         {
            this.gotoPage(CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP);
         }
         this.broadcast(BCEvent.CAREER_EXPAND_CHANGED,{"expand":cEvt.myPara.expand});
      }
      
      private function getIsCallsignExpand() : Boolean
      {
         var tempResult:Boolean = false;
         if(CodoPageViewDB.PAGE_LAYER_MAIN.currentPageObj == CodoPageViewDB.PAGE_MAIN_CAREER)
         {
            tempResult = true;
         }
         return tempResult;
      }
      
      private function onSwitchLeaderboardHandle(cEvt:ParaEvent) : void
      {
         if(cEvt.myPara.expand == true)
         {
            this.gotoPage(CodoPageViewDB.PAGE_POPUP_LEADERBOARD);
         }
         else
         {
            this.prevPage();
         }
      }
      
      private function onShowClaimRewards(event:ParaEvent) : void
      {
         var rewards:Array = event.myPara;
         var rewardsStr:String = "";
         for(var i:int = 0; i < rewards.length; i++)
         {
            rewardsStr += rewards[i] + "\n";
         }
         this.showMsg(rewardsStr,MsgBtnCount.ONE);
      }
      
      private function onGotoNewMainHandle(cEvt:ParaEvent) : void
      {
         if(Boolean(this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH)))
         {
            return;
         }
         switch(cEvt.type)
         {
            case BCEvent.EVT_GOTO_MAIN_COOP:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_NEW_FLOW);
               this.setQuickMatchType(QuickMatchType.COOP);
               this.broadcast(BCEvent.EVT_GOTO_QUICKMATCH_COOP);
               if(!this.getSyncDP(SyncDataID.IS_NOTIFIED,[UINotifications.SP_ONBOARD]))
               {
                  this.broadcast(BCEvent.ADD_NEW_UNLOCK,UINotifications.SP_ONBOARD);
               }
               break;
            case BCEvent.EVT_GOTO_MAIN_CT:
               this.setQuickMatchType(QuickMatchType.TRAINING);
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
               break;
            case BCEvent.EVT_GOTO_MAIN_MP:
               this.setQuickMatchType(QuickMatchType.MP);
               if(!this.getSyncDP(SyncDataID.IS_MP_TUTORIAL_COMPLETE))
               {
                  this.setBindData(AsyncTaskID.TUTORIAL_TYPE,Common.TUTORIAL_MP);
                  this.gotoPage(CodoPageViewDB.PAGE_TUTORIAL_CHOOSER);
               }
               else
               {
                  if(!this.getSyncDP(SyncDataID.IS_NOTIFIED,[UINotifications.MP_ONBOARD]))
                  {
                     this.broadcast(BCEvent.ADD_NEW_UNLOCK,UINotifications.MP_ONBOARD);
                  }
                  this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
               }
               break;
            case BCEvent.EVT_GOTO_MAIN_CYBORG:
               this.setQuickMatchType(QuickMatchType.CYBORG);
               if(!this.getSyncDP(SyncDataID.IS_NOTIFIED,[UINotifications.ZM_INTRO_VIDEO]))
               {
                  this.broadcast(BCEvent.EVT_UI_NOTIFICATION,{
                     "notification":UINotifications.ZM_INTRO_VIDEO,
                     "clean":false
                  });
                  this.broadcast(BCEvent.EVT_PLAY_VIDEO,{"videoURL":"cyborg_intro"});
               }
               if(!this.getSyncDP(SyncDataID.IS_ZM_TUTORIAL_COMPLETE))
               {
                  this.setBindData(AsyncTaskID.TUTORIAL_TYPE,Common.TUTORIAL_ZM);
                  this.gotoPage(CodoPageViewDB.PAGE_TUTORIAL_CHOOSER);
               }
               else
               {
                  if(!this.getSyncDP(SyncDataID.IS_NOTIFIED,[UINotifications.ZM_ONBOARD]))
                  {
                     this.broadcast(BCEvent.ADD_NEW_UNLOCK,UINotifications.ZM_ONBOARD);
                  }
                  this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
               }
         }
      }
      
      private function onChangeStartBtnStatusHandle(cEvt:ParaEvent) : void
      {
         switch(cEvt.type)
         {
            case BCEvent.EVT_MPCT_ROOMLIST_SELECTED:
               this.setBindData(AsyncTaskID.MATCH_MPCT_ENABLE_START_BUTTON,{"enable":cEvt.myPara.selected});
               break;
            case BCEvent.EVT_MPCT_PLAYLIST_UNLOCKED:
               this.setBindData(AsyncTaskID.MATCH_MPCT_ENABLE_START_BUTTON,{"enable":cEvt.myPara.unlocked});
         }
      }
      
      private function onBottomStartBtnClickHandle(cEvt:ParaEvent) : void
      {
         var playListObj:Object = null;
         var tempRoomListSelectedRoom:Object = null;
         var tempRoomCreatorParams:Object = null;
         var filter:QuickMatchFilterObj = null;
         var currentMatchType:uint = this.getSyncDP(SyncDataID.CURRENT_MATCH_TYPE);
         switch(this._currentBottomBtnStatus)
         {
            case GlobalBottomBtnStatus.STATUS_IDLE_COOP_STAGE_CHOOSE:
               this.broadcast(BCEvent.EVT_QUICK_MATCH,{"filter":this.getSyncDP(SyncDataID.MATCH_COOP_FILTER_DATA)});
               break;
            case GlobalBottomBtnStatus.STATUS_IDLE_MP_PLAYLIST_CHOOSE:
               playListObj = this.getSyncDP(SyncDataID.MATCH_MPCT_PLAYLIST_SELECTED_DATA);
               if(playListObj != null)
               {
                  if(playListObj.type == PlayListType.COMBAT && playListObj.combatTrainingType <= CombatType.CT)
                  {
                     filter = new QuickMatchFilterObj();
                     filter.type = RoomListType.MP;
                     filter.mode = "combat";
                     filter.league = playListObj.leagueID;
                     filter.playListID = playListObj.id;
                     filter.minPlayer = playListObj.minRoomSize;
                     filter.maxPlayer = playListObj.maxRoomSize;
                     this.broadcast(BCEvent.EVT_QUICK_MATCH,{"filter":filter});
                  }
                  else
                  {
                     this.broadcast(BCEvent.EVT_SET_MATCH_TYPE,QuickMatchType.MP);
                     this.broadcast(BCEvent.EVT_START_QUICK_MATCH_MP,{"data":playListObj});
                  }
               }
               break;
            case GlobalBottomBtnStatus.STATUS_IDLE_MP_ROOM_JOIN:
            case GlobalBottomBtnStatus.STATUS_IDLE_COOP_ROOM_JOIN:
            case GlobalBottomBtnStatus.STATUS_IDLE_CYBORG_ROOM_JOIN:
               tempRoomListSelectedRoom = this.getSyncDP(SyncDataID.MATCH_ROOMLIST_SELECTED_ROOM);
               if(tempRoomListSelectedRoom != null && tempRoomListSelectedRoom.selectedDataIndex != -1)
               {
                  this.broadcast(BCEvent.EVT_ENABLE_LIST_ROOM,{
                     "enable":false,
                     "immediate":true
                  });
                  this.broadcast(BCEvent.EVT_TRY_JOIN_ROOM,{
                     "room":tempRoomListSelectedRoom,
                     "ticket":false
                  });
               }
               break;
            case GlobalBottomBtnStatus.STATUS_IDLE_MP_ROOM_CREATOR:
               tempRoomCreatorParams = this.getSyncDP(SyncDataID.MATCH_MPCT_ROOMCREATOR_PARAMS);
               this.broadcast(BCEvent.EVT_SET_MATCH_TYPE,tempRoomCreatorParams.type);
               switch(tempRoomCreatorParams.type)
               {
                  case QuickMatchType.MP:
                     this.broadcast(BCEvent.EVT_CREATE_ROOM,tempRoomCreatorParams.room);
                     break;
                  case QuickMatchType.TRAINING:
                     this.broadcast(BCEvent.EVT_QUICK_MATCH,{"filter":tempRoomCreatorParams.filter});
               }
               break;
            default:
               switch(currentMatchType)
               {
                  case QuickMatchType.COOP:
                     this.broadcast(BCEvent.EVT_GOTO_QUICKMATCH_COOP);
                     break;
                  case QuickMatchType.MP:
                     this.broadcast(BCEvent.EVT_GOTO_QUICKMATCH_MP);
                     break;
                  case QuickMatchType.CYBORG:
                     this.broadcast(BCEvent.EVT_GOTO_QUICKMATCH_ZOMBIE);
               }
         }
      }
      
      private function onBottomLeftBtnClickHandle(cEvt:ParaEvent) : void
      {
         switch(this._currentBottomBtnStatus)
         {
            case GlobalBottomBtnStatus.STATUS_NOTIDLE_INROOM_NOT_READY:
            case GlobalBottomBtnStatus.STATUS_NOTIDLE_INROOM_READY:
               this.broadcast(BCEvent.EVT_CHANGE_READY_STATE);
               break;
            case GlobalBottomBtnStatus.STATUS_NOTIDLE_NOT_INROOM:
            case GlobalBottomBtnStatus.STATUS_NOTIDLE_NOT_INROOM_CANT_LEAVE:
               this.backToRoom();
               break;
            case GlobalBottomBtnStatus.STATUS_NOTIDLE_RANKED_INPROGRESS:
               this.broadcast(BCEvent.EVT_LEAVE_ROOM);
         }
      }
      
      private function onGlobalBackHandle(cEvt:ParaEvent) : void
      {
         if(CodoPageViewDB.PAGE_LAYER_ADV_POPUP.currentPageObj == null && CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == null && CodoPageViewDB.PAGE_LAYER_SYS_MSG.currentPageObj == null)
         {
            switch(this._currentBackBtnStatus)
            {
               case GlobalBackBtnStatus.STATUS_BACK_TO_INROOM:
                  this.backToRoom();
                  break;
               case GlobalBackBtnStatus.STATUS_BACK_TO_LANDING_PAGE:
                  if(this.getSyncDP(SyncDataID.CURRENT_MATCH_TYPE) == QuickMatchType.COOP)
                  {
                     this.broadcast(BCEvent.EVT_GOTO_QUICKMATCH_COOP);
                  }
                  else
                  {
                     this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
                  }
                  break;
               case GlobalBackBtnStatus.STATUS_CHANGE_GAME_TYPE:
                  this.gotoPage(CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW);
                  break;
               case GlobalBackBtnStatus.STATUS_LEAVE_ROOM:
                  this.broadcast(BCEvent.EVT_LEAVE_ROOM);
            }
         }
      }
      
      private function backToRoom() : void
      {
         this.broadcast(BCEvent.EVT_BACK_TO_ROOM,this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH) ? BackToRoomType.RANKED_MATCH : BackToRoomType.UNRANKED_MATCH);
      }
      
      private function onTryOpenMailHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.SHOW_MAIL);
      }
      
      private function onTryOpenFriendlistHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.SHOW_FRIENDLIST);
      }
      
      private function onTryOpenMessageBoxHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.SHOW_MESSAGEBOX);
      }
      
      private function onSocialExpandChangeHandle(cEvt:ParaEvent) : void
      {
         if(CodoPageViewDB.PAGE_LAYER_SYS_MSG.currentPageObj != null)
         {
            PageManagerMain.getInstance().showMask(false);
         }
         else
         {
            PageManagerMain.getInstance().showMask(true);
         }
      }
   }
}

