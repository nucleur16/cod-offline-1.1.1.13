package codo.views
{
   import codo.common.BaseView;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.LocalizationDict;
   import codo.constants.PageID;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.constants.ViewState;
   import com.greensock.TweenLite;
   import com.greensock.easing.Elastic;
   import com.greensock.easing.RoughEase;
   import components.BaseButton;
   import components.CodoGrowButtonNoTint;
   import components.CodoSocialBtn;
   import flash.display.MovieClip;
   import flash.display.Sprite;
   import flash.events.KeyboardEvent;
   import flash.events.MouseEvent;
   import flash.text.TextField;
   import flash.ui.Keyboard;
   import flash.utils.Dictionary;
   import flash.utils.getDefinitionByName;
   
   public class SocialSideBarView extends BaseView
   {
      private static var POS_ORIGIN_X:int = 1280 + POS_OFFSET_X;
      
      private static const POS_ORIGIN_Y:int = 100;
      
      private static var POS_SHOW_X:int = 730;
      
      private static const POS_SIZE_EXPAND:int = 550;
      
      private static const POS_OFFSET_X:int = 3;
      
      private static var TITLE_ICON_INDEX:Dictionary = new Dictionary();
      
      public var myTitleCategoryIcon:MovieClip;
      
      public var myTitleTF:TextField;
      
      public var myCloseBtn:CodoGrowButtonNoTint;
      
      public var myBtnMail:CodoSocialBtn;
      
      public var myBtnQT:CodoSocialBtn;
      
      public var myBtnClan:CodoSocialBtn;
      
      public var myBtnParty:CodoSocialBtn;
      
      public var myBtnMessage:CodoSocialBtn;
      
      public var myBtnFriend:CodoSocialBtn;
      
      public var myBtnsBg:Sprite;
      
      public var myBg:Sprite;
      
      public var myBgGrow:Sprite;
      
      private var _mailView:MailView;
      
      private var _friendListView:FriendListView;
      
      private var _messageBoxView:MessageBoxView;
      
      private var _partyCreatorView:PartyCreatorView;
      
      private var _partyListView:PartyListView;
      
      private var _clanCreatorView:ClanCreatorView;
      
      private var _clanListView:ClanListView;
      
      private var _currentSelectedView:BaseView;
      
      private var _currentSelectedBtn:CodoSocialBtn;
      
      private var _expanded:Boolean = false;
      
      public function SocialSideBarView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         TITLE_ICON_INDEX[this.myBtnMail] = {
            "frame":1,
            "title":LocalizationDict.MAIN_MENU_LABEL_MAIL
         };
         TITLE_ICON_INDEX[this.myBtnFriend] = {
            "frame":2,
            "title":LocalizationDict.MAIN_MENU_LABEL_CONTACT
         };
         TITLE_ICON_INDEX[this.myBtnMessage] = {
            "frame":3,
            "title":LocalizationDict.MAIN_MENU_LABEL_MESSAGEBOX
         };
         TITLE_ICON_INDEX[this.myBtnParty] = {
            "frame":4,
            "title":LocalizationDict.MAIN_MENU_LABEL_PARTY
         };
         TITLE_ICON_INDEX[this.myBtnClan] = {
            "frame":5,
            "title":LocalizationDict.MAIN_MENU_LABEL_CLAN
         };
         TITLE_ICON_INDEX[this.myBtnQT] = {
            "frame":6,
            "title":LocalizationDict.MAIN_MENU_LABEL_QT
         };
         this.myTitleCategoryIcon.stop();
         this.myBg.mouseEnabled = this.myBg.mouseChildren = false;
         this.myBtnMail.label = LocalizationDict.MAIN_MENU_LABEL_MAIL_SHORT_NAME;
         this.myBtnFriend.label = LocalizationDict.MAIN_MENU_LABEL_CONTACT_SHORT_NAME;
         this.myBtnMessage.label = LocalizationDict.MAIN_MENU_LABEL_MESSAGEBOX_SHORT_NAME;
         this.myBtnParty.label = LocalizationDict.MAIN_MENU_LABEL_PARTY_SHORT_NAME;
         this.myBtnClan.label = LocalizationDict.MAIN_MENU_LABEL_CLAN_SHORT_NAME;
         this.myBtnQT.label = LocalizationDict.MAIN_MENU_LABEL_QT_SHORT_NAME;
         this._mailView = new (getDefinitionByName("mailView") as Class)();
         this.addChild(this._mailView);
         this._mailView.init();
         this._mailView.state = ViewState.DEACTIVATED;
         this._mailView.x = 3;
         this._mailView.y = 26;
         this._friendListView = new (getDefinitionByName("friendListView") as Class)();
         this.addChild(this._friendListView);
         this._friendListView.init();
         this._friendListView.state = ViewState.DEACTIVATED;
         this._friendListView.x = 7;
         this._friendListView.y = 30;
         this._messageBoxView = new (getDefinitionByName("messageBoxView") as Class)();
         this.addChild(this._messageBoxView);
         this._messageBoxView.init();
         this._messageBoxView.state = ViewState.DEACTIVATED;
         this._messageBoxView.x = 3;
         this._messageBoxView.y = 26;
         this._partyCreatorView = new (getDefinitionByName("partyCreatorView") as Class)();
         this.addChild(this._partyCreatorView);
         this._partyCreatorView.init();
         this._partyCreatorView.state = ViewState.BACKGROUND;
         this._partyCreatorView.x = 15;
         this._partyCreatorView.y = 35;
         this._partyListView = new (getDefinitionByName("partyListView") as Class)();
         this.addChild(this._partyListView);
         this._partyListView.init();
         this._partyListView.state = ViewState.BACKGROUND;
         this._partyListView.x = 20;
         this._partyListView.y = 35;
         this._clanCreatorView = new (getDefinitionByName("clanCreatorView") as Class)();
         this._clanCreatorView.init();
         this._clanCreatorView.state = ViewState.BACKGROUND;
         this._clanCreatorView.x = 15;
         this._clanCreatorView.y = 35;
         this._clanListView = new (getDefinitionByName("clanListView") as Class)();
         this.addChild(this._clanListView);
         this._clanListView.init();
         this._clanListView.state = ViewState.BACKGROUND;
         this._clanListView.x = 20;
         this._clanListView.y = 30;
         this.myBgGrow.alpha = 0;
         this.x = POS_ORIGIN_X;
         this.y = POS_ORIGIN_Y;
         this.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.onStageKeyHandle);
         this.stage.addEventListener(KeyboardEvent.KEY_UP,this.onStageKeyHandle);
         this.myBtnParty.enable = false;
      }
      
      override protected function showView() : void
      {
         if(this._currentSelectedBtn != null)
         {
            this._expanded = true;
            this.broadcast(BCEvent.SOCIAL_EXPAND_CHANGE,{"expand":this._expanded});
            TweenLite.to(this,TweenParam.SPD_SOCIAL_MOVE,{
               "x":POS_SHOW_X,
               "ease":RoughEase.create(1,5,true,Elastic.easeOut,"out",false)
            });
            this.listen(BCEvent.EVT_PRESS_ESC_CLOSE_WINDOW,this.onEscPressHandle);
            this.listen(BCEvent.PARTY_LEFT,this.onPartyLeftHandle);
            this.listen(BCEvent.EVT_ON_CLAN_LEFT,this.onLeaveClanHandle);
            this.listen(BCEvent.EVT_PAGE_ID_CHANGED,this.onPageIDChangedHandle);
         }
      }
      
      override protected function hideView() : void
      {
         this._expanded = false;
         this.broadcast(BCEvent.SOCIAL_EXPAND_CHANGE,{"expand":this._expanded});
         TweenLite.to(this,TweenParam.SPD_SOCIAL_MOVE,{
            "x":POS_ORIGIN_X,
            "ease":RoughEase.create(1,5,true,Elastic.easeOut,"out",false)
         });
         if(this._currentSelectedBtn != null)
         {
            this._currentSelectedBtn.selected = false;
            this._currentSelectedBtn = null;
         }
         this.stopListen(BCEvent.EVT_PRESS_ESC_CLOSE_WINDOW,this.onEscPressHandle);
         this.stopListen(BCEvent.PARTY_LEFT,this.onPartyLeftHandle);
         this.stopListen(BCEvent.EVT_ON_CLAN_LEFT,this.onLeaveClanHandle);
         this.stopListen(BCEvent.EVT_PAGE_ID_CHANGED,this.onPageIDChangedHandle);
      }
      
      private function onPageIDChangedHandle(cEvt:ParaEvent) : void
      {
         if(this._expanded == true && cEvt.myPara.id == PageID.INROOM)
         {
            this.hideView();
         }
      }
      
      override protected function enableData() : void
      {
         this.bind(AsyncTaskID.MESSAGE_BOX,this.bind_messageBoxData);
         this.bind(AsyncTaskID.MESSAGE_PARTY,this.bind_partyBoxData);
         this.bind(AsyncTaskID.MESSAGE_CLAN,this.bind_clanBoxData);
      }
      
      override protected function disableData() : void
      {
         this.unbind(AsyncTaskID.MESSAGE_BOX,this.bind_messageBoxData);
         this.unbind(AsyncTaskID.MESSAGE_PARTY,this.bind_partyBoxData);
         this.unbind(AsyncTaskID.MESSAGE_CLAN,this.bind_clanBoxData);
      }
      
      override protected function onResolutionChange() : void
      {
         POS_ORIGIN_X = GameInfo.DEFAULT_WIDTH + (stage.stageWidth - GameInfo.DEFAULT_WIDTH) / 2 + POS_OFFSET_X;
         POS_SHOW_X = POS_ORIGIN_X - POS_SIZE_EXPAND - POS_OFFSET_X;
         this.x = POS_ORIGIN_X;
      }
      
      override protected function onInitListener() : void
      {
         this.addSyncDP(SyncDataID.IS_SOCIAL_PANEL_EXPANDED,this.getIsSocialPanelExpanded);
         this.myCloseBtn.addEventListener(BaseButton.EVT_CLICK,this.onCloseBtnClickHandle);
         this.myBtnMail.addEventListener(BaseButton.EVT_CLICK,this.onBtnClickHandle);
         this.myBtnFriend.addEventListener(BaseButton.EVT_CLICK,this.onBtnClickHandle);
         this.myBtnMessage.addEventListener(BaseButton.EVT_CLICK,this.onBtnClickHandle);
         this.myBtnParty.addEventListener(BaseButton.EVT_CLICK,this.onBtnClickHandle);
         this.myBtnClan.addEventListener(BaseButton.EVT_CLICK,this.onBtnClickHandle);
         this.myBtnQT.addEventListener(BaseButton.EVT_CLICK,this.onBtnClickHandle);
         this.listen(BCEvent.SHOW_MAIL,this.onShowSocialChildPanel);
         this.listen(BCEvent.SHOW_MESSAGEBOX,this.onShowSocialChildPanel);
         this.listen(BCEvent.SHOW_FRIENDLIST,this.onShowSocialChildPanel);
         this.listen(BCEvent.SHOW_PARTY_CREATOR,this.onShowSocialChildPanel);
         this.listen(BCEvent.SHOW_PARTY_LIST,this.onShowSocialChildPanel);
         this.listen(BCEvent.SHOW_CLAN_CREATOR,this.onShowSocialChildPanel);
         this.listen(BCEvent.SHOW_CLAN_LIST,this.onShowSocialChildPanel);
         this.addEventListener(MouseEvent.ROLL_OVER,this.onMouseOverHandle);
         this.addEventListener(MouseEvent.ROLL_OUT,this.onMouseOutHandle);
      }
      
      private function bind_messageBoxData(cRes:Object) : void
      {
         if(cRes.data.dp != null)
         {
            this.myBtnMessage.amount = cRes.data.dp.length;
         }
         else
         {
            this.myBtnMessage.amount = 0;
         }
      }
      
      private function bind_partyBoxData(cRes:Object) : void
      {
         if(cRes.data.dp != null)
         {
            this.myBtnParty.amount = cRes.data.dp.length;
         }
         else
         {
            this.myBtnParty.amount = 0;
         }
      }
      
      private function bind_clanBoxData(cRes:Object) : void
      {
         if(cRes.data.dp != null)
         {
            this.myBtnClan.amount = cRes.data.dp.length;
         }
         else
         {
            this.myBtnClan.amount = 0;
         }
      }
      
      private function onCloseBtnClickHandle(cEvt:ParaEvent) : void
      {
         this.hideView();
      }
      
      private function getIsSocialPanelExpanded() : Boolean
      {
         return this._expanded;
      }
      
      private function onShowSocialChildPanel(cEvt:ParaEvent) : void
      {
         if(cEvt.type == BCEvent.SHOW_PARTY_CREATOR || cEvt.type == BCEvent.SHOW_PARTY_LIST)
         {
            return;
         }
         if(!(cEvt.myPara && Boolean(cEvt.myPara.hasOwnProperty("refresh")) && cEvt.myPara.refresh == true))
         {
            if(this._currentSelectedBtn != null)
            {
               this._currentSelectedBtn.selected = false;
               this._currentSelectedBtn = null;
            }
         }
         if(this._currentSelectedView != null)
         {
            this._currentSelectedView.state = ViewState.DEACTIVATED;
            this.removeChild(this._currentSelectedView);
            this._currentSelectedView = null;
         }
         switch(cEvt.type)
         {
            case BCEvent.SHOW_MAIL:
               this._currentSelectedView = this._mailView;
               this._currentSelectedBtn = this.myBtnMail;
               this.broadcast(BCEvent.CLOSE_MAILPENDING);
               break;
            case BCEvent.SHOW_FRIENDLIST:
               this._currentSelectedView = this._friendListView;
               this._currentSelectedBtn = this.myBtnFriend;
               break;
            case BCEvent.SHOW_MESSAGEBOX:
               this.broadcast(BCEvent.MESSAGE_READ,{"asyncID":AsyncTaskID.MESSAGE_BOX});
               this._currentSelectedView = this._messageBoxView;
               this._currentSelectedBtn = this.myBtnMessage;
               break;
            case BCEvent.SHOW_PARTY_CREATOR:
               this.broadcast(BCEvent.MESSAGE_READ,{"asyncID":AsyncTaskID.MESSAGE_PARTY});
               this._currentSelectedView = this._partyCreatorView;
               this._currentSelectedBtn = this.myBtnParty;
               break;
            case BCEvent.SHOW_PARTY_LIST:
               this._currentSelectedView = this._partyListView;
               this._currentSelectedBtn = this.myBtnParty;
               this.broadcast(BCEvent.EVT_CHAT_CHANNEL_READ,{
                  "isParty":true,
                  "isClan":false
               });
               break;
            case BCEvent.SHOW_CLAN_CREATOR:
               this.broadcast(BCEvent.MESSAGE_READ,{"asyncID":AsyncTaskID.MESSAGE_CLAN});
               this._currentSelectedView = this._clanCreatorView;
               this._currentSelectedBtn = this.myBtnClan;
               this.broadcast(BCEvent.EVT_CHAT_CHANNEL_READ,{
                  "isParty":false,
                  "isClan":true
               });
               break;
            case BCEvent.SHOW_CLAN_LIST:
               this._currentSelectedView = this._clanListView;
               if(!(cEvt.myPara && Boolean(cEvt.myPara.hasOwnProperty("refresh")) && cEvt.myPara.refresh == true))
               {
                  this._currentSelectedBtn = this.myBtnClan;
               }
               break;
            case BCEvent.SHOW_QT:
               this._currentSelectedBtn = this.myBtnQT;
         }
         if(this._currentSelectedBtn != null)
         {
            if(!(cEvt.myPara && Boolean(cEvt.myPara.hasOwnProperty("refresh")) && cEvt.myPara.refresh == true))
            {
               this._currentSelectedBtn.selected = true;
            }
            this.addChild(this._currentSelectedBtn);
            this.myTitleCategoryIcon.gotoAndStop(TITLE_ICON_INDEX[this._currentSelectedBtn].frame);
            this.myTitleTF.text = TITLE_ICON_INDEX[this._currentSelectedBtn].title;
         }
         if(this._currentSelectedView != null)
         {
            this._currentSelectedView.state = ViewState.ACTIVATED;
            this.addChild(this._currentSelectedView);
         }
         if(!(cEvt.myPara && Boolean(cEvt.myPara.hasOwnProperty("refresh")) && cEvt.myPara.refresh == true))
         {
            this.showView();
         }
      }
      
      private function onBtnClickHandle(cEvt:ParaEvent) : void
      {
         this.changeSelectedChildPanelByBtn(cEvt.target as CodoSocialBtn);
      }
      
      private function changeSelectedChildPanelByBtn(cTargetBtn:CodoSocialBtn) : void
      {
         if(cTargetBtn == this.myBtnParty)
         {
            return;
         }
         if(this._currentSelectedBtn != cTargetBtn && cTargetBtn.selected == false)
         {
            switch(cTargetBtn)
            {
               case this.myBtnMail:
                  this.broadcast(BCEvent.TRY_OPEN_MAIL);
                  break;
               case this.myBtnFriend:
                  this.broadcast(BCEvent.TRY_OPEN_FRIENDLIST);
                  break;
               case this.myBtnMessage:
                  this.broadcast(BCEvent.TRY_OPEN_MESSAGEBOX);
                  break;
               case this.myBtnParty:
                  this.broadcast(BCEvent.TRY_OPEN_PARTY);
                  break;
               case this.myBtnClan:
                  this.broadcast(BCEvent.TRY_OPEN_CLAN);
                  break;
               case this.myBtnQT:
                  this.broadcast(BCEvent.TRY_OPEN_QT);
            }
         }
      }
      
      private function onEscPressHandle(cEvt:ParaEvent) : void
      {
         this.hideView();
      }
      
      private function onStageKeyHandle(cEvt:KeyboardEvent) : void
      {
         if(cEvt.keyCode == Keyboard.TAB)
         {
            if(cEvt.type == KeyboardEvent.KEY_DOWN && this._expanded == false && this.getSyncDP(SyncDataID.IS_WEAPON_ZOOMIN_OPEN) == false)
            {
               this.changeSelectedChildPanelByBtn(this.myBtnMail);
            }
            else if(cEvt.type == KeyboardEvent.KEY_UP && this._expanded == true)
            {
               this.hideView();
            }
         }
      }
      
      private function onPartyLeftHandle(cEvt:ParaEvent) : void
      {
         if(this._currentSelectedBtn == this.myBtnParty)
         {
            this.broadcast(BCEvent.SHOW_PARTY_CREATOR);
         }
      }
      
      private function onLeaveClanHandle(cEvt:ParaEvent) : void
      {
         if(this._currentSelectedBtn == this.myBtnClan)
         {
            this.broadcast(BCEvent.SHOW_CLAN_CREATOR);
         }
      }
      
      private function onMouseOverHandle(cEvt:MouseEvent) : void
      {
         TweenLite.to(this.myBgGrow,TweenParam.SPD_FADE,{"alpha":1});
      }
      
      private function onMouseOutHandle(cEvt:MouseEvent) : void
      {
         TweenLite.to(this.myBgGrow,TweenParam.SPD_FADE_OUT,{"alpha":0});
      }
   }
}

