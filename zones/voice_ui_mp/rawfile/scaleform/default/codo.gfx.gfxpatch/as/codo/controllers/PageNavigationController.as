package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.CFunctionContainer;
   import codo.common.CFunctionContainerCommon;
   import codo.common.Logger;
   import codo.common.PageManagerMain;
   import codo.common.ParaEvent;
   import codo.constants.BCEvent;
   import codo.constants.BackToRoomType;
   import codo.constants.CodoPageViewDB;
   import codo.constants.Common;
   import codo.constants.CommonPageID;
   import codo.constants.MusicID;
   import codo.constants.RankedMatchStatus;
   import codo.constants.SyncDataID;
   import codo.constants.ViewState;
   import codo.objects.CodoPageObj;
   import codo.utils.CommonUtil;
   import codo.utils.SoundUtil;
   import codo.utils.UILogUtil;
   import flash.utils.Dictionary;
   
   public class PageNavigationController extends BaseController
   {
      private var _currentMainPageObj:CodoPageObj;
      
      private var _prevMainPageID:String = "";
      
      private var _queueDict:Dictionary = new Dictionary();
      
      public function PageNavigationController()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.addSyncDP(SyncDataID.CURRENT_PAGE_ID,this.getCurrentPageID);
         this.addSyncDP(SyncDataID.PREV_PAGE_ID,this.getPrevPageID);
         this.addSyncDP(SyncDataID.HAS_POPUP,this.hasPopup);
         this.addSyncDP(SyncDataID.IS_WEAPON_ZOOMIN_OPEN,this.isZoomInPageOpened);
         this.listen(BCEvent.GOTO_PAGE_BY_PAGEOBJ,this.onGotoPageByPageObjHandle);
         this.listen(BCEvent.GOTO_PREV_PAGE,this.onGotoPrevPageHandle);
         this.listen(BCEvent.EVT_RANKED_MATCH_CHANGE_STATUS,this.onRankedMatchStatusChangedHandle);
      }
      
      public function gotoPageByPageObj(cPageObj:CodoPageObj, cParams:Object = null, cCallBeforeGoto:Function = null, cCallAfterGoto:Function = null) : void
      {
         Logger.print("PageNavigationController::gotoPageByPageObj cPageObj: " + cPageObj.id);
         if(cPageObj == null)
         {
            cPageObj = CodoPageViewDB.PAGE_MAIN_HOME;
         }
         if(Boolean(this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH)) && cPageObj != CodoPageViewDB.PAGE_MAIN_HOME && cPageObj != CodoPageViewDB.PAGE_MAIN_IN_ROOM && cPageObj != CodoPageViewDB.PAGE_MAIN_LOGIN && cPageObj != CodoPageViewDB.PAGE_POPUP_SUMMARY && cPageObj != CodoPageViewDB.PAGE_POPUP_AAR && cPageObj != CodoPageViewDB.PAGE_ADV_POPUP_LOBBY_SUMMARY && cPageObj != CodoPageViewDB.PAGE_QUIT && cPageObj.owner != CodoPageViewDB.PAGE_LAYER_SYS_MSG)
         {
            return;
         }
         this.broadcast(BCEvent.EVT_PAGE_START_CHANGE,{"pageObj":cPageObj});
         var isPageChanged:Boolean = true;
         var isMainPageChanged:Boolean = false;
         if((this._currentMainPageObj == null || this._currentMainPageObj.owner == CodoPageViewDB.PAGE_LAYER_MAIN) && cPageObj.owner == CodoPageViewDB.PAGE_LAYER_MAIN)
         {
            isMainPageChanged = true;
         }
         if(cCallBeforeGoto != null)
         {
            if(cParams == null)
            {
               cCallBeforeGoto();
            }
            else
            {
               cCallBeforeGoto(cParams);
            }
         }
         switch(cPageObj)
         {
            case CodoPageViewDB.PAGE_MAIN_HOME:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_NEW_FLOW);
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_HOME);
               this.setUIViewerMode(Common.UIVIEWER_MODE_CHARACTER_LOADOUT,Common.UIVIEWER_POS_CENTER);
               this.setPageID(CommonPageID.MAIN);
               break;
            case CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_NEW_FLOW);
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP);
               this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER);
               break;
            case CodoPageViewDB.PAGE_MAIN_MATCH_MPCT_PLAYLIST:
            case CodoPageViewDB.PAGE_MAIN_MATCH_MPCT_ROOMLIST:
            case CodoPageViewDB.PAGE_MAIN_MATCH_MPCT_ROOMCTEATOR:
            case CodoPageViewDB.PAGE_MAIN_ROOM_CYBORGS:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_NEW_FLOW);
               PageManagerMain.getInstance().showPage(cPageObj);
               this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER);
               break;
            case CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP_TUTORIAL:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_LOGO_TUTORIAL);
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP_TUTORIAL);
               this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER);
               break;
            case CodoPageViewDB.PAGE_MAIN_HOME_CHARACTER:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_HOME_CHARACTER);
               this.setUIViewerMode(Common.UIVIEWER_MODE_CHARACTER_LOADOUT,Common.UIVIEWER_POS_CENTER);
               break;
            case CodoPageViewDB.PAGE_MAIN_HOME_WEAPON:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_HOME_WEAPON);
               this.setUIViewerMode(Common.UIVIEWER_MODE_CHARACTER_LOADOUT,Common.UIVIEWER_POS_CENTER);
               break;
            case CodoPageViewDB.PAGE_MAIN_CHARACTER:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_CHARACTER);
               this.setUIViewerMode(Common.UIVIEWER_MODE_CHARACTER_LOADOUT,Common.UIVIEWER_POS_RIGHT);
               break;
            case CodoPageViewDB.PAGE_MAIN_HOME_LOADOUT:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_HOME_LOADOUT);
               this.setUIViewerMode(Common.UIVIEWER_MODE_CHARACTER_LOADOUT,Common.UIVIEWER_POS_CENTER);
               break;
            case CodoPageViewDB.PAGE_MAIN_ITEM:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_ITEM);
               this.setUIViewerMode(Common.UIVIEWER_MODE_CHARACTER_LOADOUT,Common.UIVIEWER_POS_CENTER);
               break;
            case CodoPageViewDB.PAGE_POPUP_WEAPON_ZOOMIN:
               PageManagerMain.getInstance().cleanPageLayer(CodoPageViewDB.PAGE_LAYER_MAIN);
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_POPUP_WEAPON_ZOOMIN,false);
               this.setUIViewerMode(Common.UIVIEWER_MODE_WEAPON_LOADOUT);
               break;
            case CodoPageViewDB.PAGE_MAIN_IN_ROOM:
               if(this.getSyncDP(SyncDataID.IS_MATCH_BUTTON_POS_ENABLED) == true)
               {
                  PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_NEW_FLOW_WITHOUT_BOTTOM_MARKETGROUP);
               }
               else
               {
                  PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_NEW_FLOW_MINIOPTION);
               }
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_IN_ROOM);
               this.closeWeaponZoomIn();
               this.setUIViewerMode(Common.UIVIEWER_MODE_TEAM);
               break;
            case CodoPageViewDB.PAGE_MAIN_ROOM_COOP:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_NEW_FLOW);
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_ROOM_COOP);
               this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER);
               break;
            case CodoPageViewDB.PAGE_POPUP_SUMMARY:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_POPUP_SUMMARY);
               break;
            case CodoPageViewDB.PAGE_MAIN_LOGIN:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_LOGIN);
               this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER,Common.UIVIEWER_GAME_MP);
               break;
            case CodoPageViewDB.PAGE_MAIN_CHARACTER_CREATION:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_LOGO);
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_CHARACTER_CREATION);
               this.setUIViewerMode(Common.UIVIEWER_MODE_CHARACTER_CREATION);
               SoundUtil.playMusic(MusicID.CREATION,MusicID.CREATION_AMB);
               break;
            case CodoPageViewDB.PAGE_KEYBOARD_CHOOSER:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_KEYBOARD_CHOOSER);
               this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER,Common.UIVIEWER_GAME_MP);
               break;
            case CodoPageViewDB.PAGE_MAIN_LOGIN:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_LOGIN);
               this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER,Common.UIVIEWER_GAME_MP);
               break;
            case CodoPageViewDB.PAGE_QUIT:
               CommonUtil.quit();
               break;
            case CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_LOGO_NEW);
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW);
               this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER,Common.UIVIEWER_GAME_MP);
               break;
            case CodoPageViewDB.PAGE_POPUP_AAR:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_POPUP_AAR);
               break;
            case CodoPageViewDB.PAGE_ADV_POPUP_PURCHASE_CONFIRM:
            case CodoPageViewDB.PAGE_ADV_POPUP_PURCHASE_CONFIRM_CYBORG:
               if(this.getSyncDP(SyncDataID.IS_CYBORG) == true)
               {
                  PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_ADV_POPUP_PURCHASE_CONFIRM_CYBORG);
               }
               else
               {
                  PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_ADV_POPUP_PURCHASE_CONFIRM);
               }
               break;
            case CodoPageViewDB.PAGE_MAIN_WEAPON:
               PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MAIN_WEAPON);
               if(this.getSyncDP(SyncDataID.IS_CYBORG))
               {
                  this.setUIViewerMode(Common.UIVIEWER_MODE_CHARACTER_LOADOUT,Common.UIVIEWER_POS_LEFT);
               }
               else
               {
                  this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER);
               }
               break;
            case CodoPageViewDB.PAGE_TUTORIAL_CHOOSER:
               if(this.getSyncDP(SyncDataID.IS_CYBORG) == true)
               {
                  PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_LOGO_CYBORG);
               }
               else
               {
                  PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_MENU_LOGO);
               }
               PageManagerMain.getInstance().showPage(cPageObj);
               this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER);
               break;
            default:
               PageManagerMain.getInstance().showPage(cPageObj);
               if(cPageObj.owner == CodoPageViewDB.PAGE_LAYER_MAIN)
               {
                  this.setUIViewerMode(Common.UIVIEWER_MODE_NONE,Common.UIVIEWER_POS_CENTER);
               }
         }
         if(cCallAfterGoto != null)
         {
            if(cParams == null)
            {
               cCallAfterGoto();
            }
            else
            {
               cCallAfterGoto(cParams);
            }
         }
         if(isPageChanged)
         {
            UILogUtil.UILog_currentPageID(cPageObj.id);
            this.broadcast(BCEvent.EVT_PAGE_CHANGED,{"pageObj":cPageObj});
            if(cPageObj.owner == CodoPageViewDB.PAGE_LAYER_MAIN)
            {
               if(this._currentMainPageObj != null)
               {
                  this._prevMainPageID = this._currentMainPageObj.id;
               }
               this._currentMainPageObj = cPageObj;
            }
            if(this._currentMainPageObj != null)
            {
               this.broadcast(BCEvent.EVT_PAGE_ID_CHANGED,{"id":this._currentMainPageObj.id});
            }
            if(isMainPageChanged == true)
            {
               this.broadcast(BCEvent.EVT_MAIN_PAGE_CHANGED,{
                  "id":this._currentMainPageObj.id,
                  "prev_id":this._prevMainPageID
               });
            }
         }
      }
      
      public function gotoFirstPage() : void
      {
         PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_BG);
         PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_BACKGROUND_POPUP,true,ViewState.BACKGROUND);
         PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_SOCIAL);
         PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_R_MOUSE_MENU,false,ViewState.BACKGROUND);
         PageManagerMain.getInstance().showPage(CodoPageViewDB.PAGE_TIP);
         this.broadcast(BCEvent.EVT_BACK_TO_ROOM,BackToRoomType.FIRST_PAGE);
         this.broadcast(BCEvent.EVT_INIT_AT_FIRST_SIGHT_OF_MAIN_MENU);
      }
      
      private function onGotoPageByPageObjHandle(cEvt:ParaEvent) : void
      {
         if(cEvt.myPara.pageObj.owner != null && cEvt.myPara.pageObj.owner.queue == true)
         {
            if(this._queueDict[cEvt.myPara.pageObj.owner] == null)
            {
               this._queueDict[cEvt.myPara.pageObj.owner] = [];
            }
            this._queueDict[cEvt.myPara.pageObj.owner].push(cEvt.myPara);
            if(this._queueDict[cEvt.myPara.pageObj.owner].length == 1)
            {
               this.gotoPageByPageObj(cEvt.myPara.pageObj,cEvt.myPara.params,cEvt.myPara.callBeforeGoto,cEvt.myPara.callAfterGoto);
            }
         }
         else
         {
            this.gotoPageByPageObj(cEvt.myPara.pageObj,cEvt.myPara.params,cEvt.myPara.callBeforeGoto,cEvt.myPara.callAfterGoto);
         }
      }
      
      private function onGotoPrevPageHandle(cEvt:ParaEvent) : void
      {
         var targetPageObj:CodoPageObj = null;
         var tempParams:Object = null;
         var tempCallBeforeGoto:Function = null;
         var tempCallAfterGoto:Function = null;
         if(Boolean(this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH)) && CodoPageViewDB.PAGE_LAYER_SYS_MSG.currentPageObj == null && CodoPageViewDB.PAGE_LAYER_ADV_POPUP.currentPageObj == null && CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == null)
         {
            return;
         }
         if(CodoPageViewDB.PAGE_LAYER_SYS_MSG.currentPageObj != null)
         {
            PageManagerMain.getInstance().tryCloseAllSysMsg();
            PageManagerMain.getInstance().showMask(false);
         }
         else if(CodoPageViewDB.PAGE_LAYER_ADV_POPUP.currentPageObj != null)
         {
            PageManagerMain.getInstance().cleanPageLayer(CodoPageViewDB.PAGE_LAYER_ADV_POPUP);
            PageManagerMain.getInstance().showMask(true);
         }
         else if(CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj != null && this._queueDict[CodoPageViewDB.PAGE_LAYER_POPUP] != null && this._queueDict[CodoPageViewDB.PAGE_LAYER_POPUP].length > 0)
         {
            this._queueDict[CodoPageViewDB.PAGE_LAYER_POPUP].splice(0,1);
            if(this._queueDict[CodoPageViewDB.PAGE_LAYER_POPUP].length > 0)
            {
               targetPageObj = this._queueDict[CodoPageViewDB.PAGE_LAYER_POPUP][0].pageObj;
               tempParams = this._queueDict[CodoPageViewDB.PAGE_LAYER_POPUP][0].params;
               tempCallBeforeGoto = this._queueDict[CodoPageViewDB.PAGE_LAYER_POPUP][0].callBeforeGoto;
               tempCallAfterGoto = this._queueDict[CodoPageViewDB.PAGE_LAYER_POPUP][0].callAfterGoto;
               PageManagerMain.getInstance().cleanPageLayer(CodoPageViewDB.PAGE_LAYER_POPUP);
            }
            else if(CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == CodoPageViewDB.PAGE_POPUP_WEAPON_ZOOMIN)
            {
               targetPageObj = PageManagerMain.getInstance().showPrevPage(false);
               PageManagerMain.getInstance().cleanPageLayer(CodoPageViewDB.PAGE_LAYER_POPUP);
               PageManagerMain.getInstance().showMask(true);
            }
            else
            {
               PageManagerMain.getInstance().cleanPageLayer(CodoPageViewDB.PAGE_LAYER_POPUP);
               PageManagerMain.getInstance().showMask(true);
            }
         }
         else
         {
            targetPageObj = PageManagerMain.getInstance().showPrevPage(false);
            if(targetPageObj == CodoPageViewDB.PAGE_MAIN_MATCH_QUICK_COOP_TUTORIAL)
            {
               targetPageObj = CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW;
            }
            else if(targetPageObj == null)
            {
               targetPageObj = CodoPageViewDB.PAGE_MAIN_PLAY_MODE_SELECTION_NEW;
            }
            PageManagerMain.getInstance().deleteLastHistory();
         }
         if(targetPageObj != null)
         {
            Logger.print("PageNavigationController:::onGotoPrevPageHandle targetPageObj: " + targetPageObj.id);
            this.gotoPageByPageObj(targetPageObj,tempParams,tempCallBeforeGoto,tempCallAfterGoto);
         }
      }
      
      private function onRankedMatchStatusChangedHandle(cEvt:ParaEvent) : void
      {
         switch(cEvt.myPara.status)
         {
            case RankedMatchStatus.READY:
            case RankedMatchStatus.READY_IN_ROOM:
               this.closeWeaponZoomIn();
         }
      }
      
      private function setPageID(id:uint) : void
      {
         CFunctionContainerCommon.SetPageID(id);
      }
      
      private function getCurrentPageID() : String
      {
         return this._currentMainPageObj != null ? this._currentMainPageObj.id : "";
      }
      
      private function getPrevPageID() : String
      {
         var targetPageObj:CodoPageObj = PageManagerMain.getInstance().showPrevPage(false);
         return targetPageObj != null ? targetPageObj.id : null;
      }
      
      private function hasPopup() : Boolean
      {
         return CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj != null || CodoPageViewDB.PAGE_LAYER_ADV_POPUP.currentPageObj != null;
      }
      
      private function closeWeaponZoomIn() : void
      {
         var mode:uint = 0;
         if(CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == CodoPageViewDB.PAGE_POPUP_WEAPON_ZOOMIN)
         {
            PageManagerMain.getInstance().cleanPageLayer(CodoPageViewDB.PAGE_LAYER_POPUP);
            PageManagerMain.getInstance().showMask(true);
            this._queueDict[CodoPageViewDB.PAGE_LAYER_POPUP].splice(0,1);
            mode = Common.UIVIEWER_MODE_NONE;
            if(CodoPageViewDB.PAGE_LAYER_MAIN.currentPageObj == CodoPageViewDB.PAGE_MAIN_IN_ROOM || CodoPageViewDB.PAGE_LAYER_MAIN.currentPageObj == CodoPageViewDB.PAGE_MAIN_IN_ROOM_CYBORGS)
            {
               mode = Common.UIVIEWER_MODE_TEAM;
            }
            else if(CodoPageViewDB.PAGE_LAYER_MAIN.currentPageObj == CodoPageViewDB.PAGE_MAIN_HOME)
            {
               mode = Common.UIVIEWER_MODE_CHARACTER_LOADOUT;
            }
            else if(CodoPageViewDB.PAGE_LAYER_MAIN.currentPageObj == null)
            {
               this.gotoPage(CodoPageViewDB.PAGE_MAIN_HOME);
            }
            if(mode != Common.UIVIEWER_MODE_NONE)
            {
               this.setUIViewerMode(mode,Common.UIVIEWER_POS_CENTER);
            }
         }
      }
      
      private function setUIViewerMode(cMode:uint, cStatus:uint = 0, cGameType:int = -1) : void
      {
         if(cGameType == -1)
         {
            cGameType = this.getSyncDP(SyncDataID.IS_CYBORG) ? int(Common.UIVIEWER_GAME_ZM) : int(Common.UIVIEWER_GAME_MP);
         }
         this.broadcast(BCEvent.EVT_SET_UIVIEWER_MODE,{
            "mode":cMode,
            "status":cStatus,
            "gametype":cGameType
         });
      }
      
      private function isZoomInPageOpened() : Boolean
      {
         return CodoPageViewDB.PAGE_LAYER_POPUP.currentPageObj == CodoPageViewDB.PAGE_POPUP_WEAPON_ZOOMIN;
      }
   }
}

