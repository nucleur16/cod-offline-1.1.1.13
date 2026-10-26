package components.list_part
{
   import codo.common.ParaEvent;
   import codo.utils.LocalizeUtil;
   import codo.constants.InvitationType;
   import codo.constants.LocalizationSystem;
   import codo.constants.QuickMatchType;
   import components.BaseButton;
   import components.CodoGrowButtonNoTint;
   import flash.display.MovieClip;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;
   import flash.utils.Dictionary;
   
   public class InviteItemRenderer extends ListItemRendererBase
   {
      
      public static const EVT_ACCEPT_INVITE:String = "evt_accept_invite";
      
      public static const EVT_REFUSE_INVITE:String = "evt_refuse_invite";
      
      private static const TEXTCOLOUR_DES_MP:uint = 7648707;
      
      private static const TEXTCOLOUR_DES_CY:uint = 14908984;
      
      private static var CATEGORY_ICON_INDEX:Dictionary = new Dictionary();
      
      public var myTypeIcons:MovieClip;
      
      public var myCloseBtn:CodoGrowButtonNoTint;
      
      public var myAcceptBtn:CodoGrowButtonNoTint;
      
      public var myRefuseBtn:CodoGrowButtonNoTint;
      
      public var myTypeTF:TextField;
      
      public var myTimeTF:TextField;
      
      public var myNameTF:TextField;
      
      public var myBgFocus:MovieClip;
      
      public var myBg:Sprite;
      
      public function InviteItemRenderer()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         CATEGORY_ICON_INDEX[InvitationType.FRIEND_OUTGOING_PROPOSAL] = 1;
         CATEGORY_ICON_INDEX[InvitationType.FRIEND_OUTGOING_PROPOSAL_ACCEPTED] = 1;
         CATEGORY_ICON_INDEX[InvitationType.FRIEND_OUTGOING_PROPOSAL_REJECTED] = 1;
         CATEGORY_ICON_INDEX[InvitationType.GAME_OUTGOING_PROPOSAL] = 2;
         CATEGORY_ICON_INDEX[InvitationType.OTHERS] = 2;
         this.myAcceptBtn.label = LocalizationSystem.ACCEPT;
         this.myRefuseBtn.label = LocalizationSystem.REFUSE;
         this.myTypeIcons.stop();
         this.onStyleChangeForMatchType();
         this.setToNormal();
      }
      
      override protected function onInitListener() : void
      {
         this.myAcceptBtn.addEventListener(BaseButton.EVT_CLICK,this.onAcceptClickHandle);
         this.myRefuseBtn.addEventListener(BaseButton.EVT_CLICK,this.onRefuseClickHandle);
         this.myCloseBtn.addEventListener(BaseButton.EVT_CLICK,this.onRefuseClickHandle);
      }
      
      override protected function onGetData() : void
      {
         var dat:Object = this.getData();
         this.visible = true;
         if(dat.type == InvitationType.FRIEND_OUTGOING_PROPOSAL_ACCEPTED || dat.type == InvitationType.FRIEND_OUTGOING_PROPOSAL_REJECTED)
         {
            this.myTypeTF.text = LocalizeUtil.param(dat.title,[dat.senderName]);
         }
         else
         {
            this.myTypeTF.text = dat.title;
         }
         this.myNameTF.text = dat.senderName;
         this.myTimeTF.text = dat.receivedTime;
         this.myTypeIcons.gotoAndStop(CATEGORY_ICON_INDEX[dat.type]);
         switch(dat.type)
         {
            case InvitationType.FRIEND_OUTGOING_PROPOSAL:
               this.myCloseBtn.visible = true;
               this.myAcceptBtn.visible = false;
               this.myRefuseBtn.visible = false;
               break;
            case InvitationType.FRIEND_OUTGOING_PROPOSAL_ACCEPTED:
               this.myCloseBtn.visible = true;
               this.myAcceptBtn.visible = false;
               this.myRefuseBtn.visible = false;
               break;
            case InvitationType.FRIEND_OUTGOING_PROPOSAL_REJECTED:
               this.myCloseBtn.visible = true;
               this.myAcceptBtn.visible = false;
               this.myRefuseBtn.visible = false;
               break;
            case InvitationType.GAME_OUTGOING_PROPOSAL:
               this.myCloseBtn.visible = true;
               this.myAcceptBtn.visible = false;
               this.myRefuseBtn.visible = false;
               break;
            default:
               this.myCloseBtn.visible = false;
               this.myAcceptBtn.visible = true;
               this.myRefuseBtn.visible = true;
               this.myTypeIcons.gotoAndStop(CATEGORY_ICON_INDEX[InvitationType.OTHERS]);
         }
      }
      
      private function onAcceptClickHandle(cEvt:Event) : void
      {
         this.dispatchEvent(new ParaEvent(EVT_ACCEPT_INVITE,this.getData()));
      }
      
      private function onRefuseClickHandle(cEvt:Event) : void
      {
         this.dispatchEvent(new ParaEvent(EVT_REFUSE_INVITE,this.getData()));
      }
      
      override protected function setToNormal(cTween:Boolean = true) : void
      {
         this.myBgFocus.visible = false;
      }
      
      override protected function setToRollOver(cTween:Boolean = true) : void
      {
         this.myBgFocus.visible = true;
      }
      
      override public function onStyleChangeForMatchType() : void
      {
         this.myBgFocus.gotoAndStop(this.currentMatchType + 1);
         switch(this.currentMatchType)
         {
            case QuickMatchType.MP:
               this.myNameTF.textColor = TEXTCOLOUR_DES_MP;
               break;
            case QuickMatchType.CYBORG:
               this.myNameTF.textColor = TEXTCOLOUR_DES_CY;
         }
      }
   }
}

