package codo.views
{
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.GameInfo;
   import codo.constants.GlobalBackBtnStatus;
   import codo.constants.LocalizationDict;
   import codo.constants.SE_ID;
   import codo.constants.SyncDataID;
   import codo.constants.TweenParam;
   import codo.objects.ResultObj;
   import codo.utils.SoundUtil;
   import com.greensock.TweenLite;
   import components.BaseButton;
   import components.BlinkButton;
   import components.clan_part.ClanQuickInfo;
   import components.mainmenu_part.MainMenuButton;
   import components.party_part.PartyQuickInfo;
   import flash.events.Event;
   
   public class QuickMatchChooseNewView extends BaseView
   {
      
      private static const POS_ORIGIN_X:Number;
      
      private static const POS_ORIGIN_Y:Number = -105;
      
      private static const POS_SHOW_Y:Number = 16;
      
      // Decompilation skipped
      
      public var myButtonMail:MainMenuButton;
      
      public var myButtonMessage:MainMenuButton;
      
      public var myButtonContact:MainMenuButton;
      
      public var myButtonParty:MainMenuButton;
      
      public var myPartyMemberInfo:PartyQuickInfo;
      
      public var myClanMemberInfo:ClanQuickInfo;
      
      public var myButtonClan:MainMenuButton;
      
      public var myButtonQt:MainMenuButton;
      
      public var myBackBtn:BlinkButton;
      
      private var _origin_partyInfo_x_icon:int;
      
      private var _origin_partyInfo_y_icon:int;
      
      private var _origin_clanInfo_x_icon:int;
      
      private var _origin_clanInfo_y_icon:int;
      
      private var _isCharacterCloseUp:Boolean = false;
      
      private var _isAddedToBlur:Boolean = false;
      
      public var theatreOnly:Boolean = false;
      
      public function QuickMatchChooseNewView()
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
      
      override public function addToBlur() : void
      {
         // Decompilation skipped
      }
      
      override public function removeFromBlur() : void
      {
         // Decompilation skipped
      }
      
      override protected function enableData() : void
      {
         // Decompilation skipped
      }
      
      private function bind_currentTopBackBtnStatus(cResult:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onShowTweenStart() : void
      {
         // Decompilation skipped
      }
      
      private function onShowTweenComplete() : void
      {
         // Decompilation skipped
      }
      
      private function onBackBtnClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onBtnMailClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onBtnClanClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onBtnQtClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onBtnMessageClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onBtnContactClickHandle(cEvt:Event) : void
      {
         // Decompilation skipped
      }
      
      private function onBtnPartyClickHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onContactExpandChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onMailExpandChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onMessageBoxExpandChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onPartyExpandChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onClanExpandChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onQtExpandChangedHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function isContactExpand() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isMailExpand() : Boolean
      {
         // Decompilation skipped
      }
      
      private function isMessageBoxExpand() : Boolean
      {
         // Decompilation skipped
      }
      
      private function bind_invitations(cRes:ResultObj) : void
      {
         // Decompilation skipped
      }
      
      private function onNewMailHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function bind_partyInvitations(cRes:ResultObj) : void
      {
         // Decompilation skipped
      }
      
      private function bind_partyMembers(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function bind_clanProposals(cRes:Object) : void
      {
         // Decompilation skipped
      }
      
      private function onPartyLeftHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onClanLeftHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
      
      private function onToggleInRoomElementsHandle(cEvt:ParaEvent) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

