package codo.views
{
   import codo.common.BaseView;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ParaEvent;
   import codo.constants.BCEventCommon;
   import codo.constants.Common;
   import codo.constants.LocalizationDict;
   import codo.constants.LocalizationSummary;
   import codo.constants.SyncDataIDCommon;
   import codo.constants.TweenParam;
   import codo.constants.ViewState;
   import codo.objects.EXPLevelObj;
   import codo.utils.ImageLoader;
   import codo.utils.LocalizeUtil;
   import codo.utils.TimeUtil;
   import components.CodoGrowButtonNoTint;
   import components.XPBar;
   import flash.display.Bitmap;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.events.MouseEvent;
   import flash.text.TextField;
   
   public class SummaryView extends BaseView
   {
      private static var POS_X_ORIGIN:int = 640;
      
      private static var POS_Y_ORIGIN:int = 480;
      
      private static const MIN_LEVEL_UP_MINS:uint = 3;
      
      private static const NAME_ACHIEVEMENT_BAR:String = "myAchievementBar_";
      
      private static const NAME_ACHIEVEMENT_LABEL:String = "myAchievementLabel_";
      
      private static const NAME_ACHIEVEMENT_EXP:String = "myAchievementExp_";
      
      private static const NAME_ACHIEVEMENT_ICON:String = "myAchievementIcon_";
      
      private static const NAME_NEXT_UNLOCK_ITEM:String = "myNextUnlockItem_";
      
      private static const NUM_NEXT_UNLOCK_ITEM:uint = 4;
      
      public var myHeader:CodoGrowButtonNoTint;
      
      public var myCurrentTF:TextField;
      
      public var myTotelTimeTF:TextField;
      
      public var myTotalTimeLabelTF:TextField;
      
      public var myCurrentLevelTF:TextField;
      
      public var myNextLevelTF:TextField;
      
      public var myTotalXPGetLabelTF:TextField;
      
      public var myTotalXPGetTF:TextField;
      
      public var myXPBar:XPBar;
      
      public var myXPRemainTF:TextField;
      
      public var myTotalGPGetTF:TextField;
      
      public var myTotalGPGetLabelTF:TextField;
      
      public var myAchievementLabel_0:TextField;
      
      public var myAchievementLabel_1:TextField;
      
      public var myAchievementLabel_2:TextField;
      
      public var myAchievementExp_0:TextField;
      
      public var myAchievementExp_1:TextField;
      
      public var myAchievementExp_2:TextField;
      
      public var myAchievementBar_0:XPBar;
      
      public var myAchievementBar_1:XPBar;
      
      public var myAchievementBar_2:XPBar;
      
      public var myNextUnlockTitleTF:TextField;
      
      public var myUnlockWeaponIcon:Sprite;
      
      public var myUnlockWeaponNameTF:TextField;
      
      public var myIconNext:Sprite;
      
      public var myUnlockAttachment:Sprite;
      
      public var myCurrentAchievementLabelTF:TextField;
      
      public var myConfirmBtn:CodoGrowButtonNoTint;
      
      public var myCancelBtn:CodoGrowButtonNoTint;
      
      public var myBg:Sprite;
      
      private var _remainXPString:String;
      
      private var _summaryDat:Object;
      
      public function SummaryView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         this.myHeader.label = LocalizationSummary.SUMMARY_TITLE;
         this.myHeader.selected = true;
         this.myCurrentTF.text = LocalizationSummary.SUMMARY_CURRENT_INFO;
         this.myTotalTimeLabelTF.text = LocalizationSummary.SUMMARY_TOTAL_TIME;
         this.myTotalXPGetLabelTF.text = LocalizationSummary.SUMMARY_XP_GET;
         this.myTotalGPGetLabelTF.text = LocalizationSummary.SUMMARY_GP_GET;
         this.myXPRemainTF.text = LocalizationSummary.SUMMARY_REMAIN_X_XP;
         this.myCurrentAchievementLabelTF.text = LocalizationSummary.SUMMARY_ACHIEVEMENT_UNLOCKED;
         this.myConfirmBtn.label = LocalizationSummary.SUMMARY_BACK_TO_GAME;
         this.myCancelBtn.label = LocalizationSummary.SUMMARY_QUIT_GAME;
         this._remainXPString = this.myXPRemainTF.text;
         this.x = POS_X_ORIGIN;
         this.y = POS_Y_ORIGIN;
         this.tweenHide(0,{
            "autoAlpha":0,
            "scaleX":TweenParam.SCALE_POPUP,
            "scaleY":TweenParam.SCALE_POPUP
         });
         var fitFieldTime:Object = this.myTotalTimeLabelTF;
         if(fitFieldTime != null && fitFieldTime.text == "Time Online" && fitFieldTime.width > 4)
         {
            var fitFormatTime:Object = fitFieldTime.getTextFormat();
            var fitSizeTime:Number = Number(fitFieldTime.defaultTextFormat.size);
            if(fitSizeTime >= 10 && fitSizeTime <= 72)
            {
               fitFormatTime.size = fitSizeTime;
               fitFieldTime.setTextFormat(fitFormatTime);
               while(fitFieldTime.textWidth > fitFieldTime.width - 4 && fitSizeTime > 10)
               {
                  fitSizeTime = Math.max(10,fitSizeTime - 1);
                  fitFormatTime.size = fitSizeTime;
                  fitFieldTime.setTextFormat(fitFormatTime);
               }
            }
         }
         var fitFieldXP:Object = this.myTotalXPGetLabelTF;
         if(fitFieldXP != null && fitFieldXP.text == "XP Earned" && fitFieldXP.width > 4)
         {
            var fitFormatXP:Object = fitFieldXP.getTextFormat();
            var fitSizeXP:Number = Number(fitFieldXP.defaultTextFormat.size);
            if(fitSizeXP >= 10 && fitSizeXP <= 72)
            {
               fitFormatXP.size = fitSizeXP;
               fitFieldXP.setTextFormat(fitFormatXP);
               while(fitFieldXP.textWidth > fitFieldXP.width - 4 && fitSizeXP > 10)
               {
                  fitSizeXP = Math.max(10,fitSizeXP - 1);
                  fitFormatXP.size = fitSizeXP;
                  fitFieldXP.setTextFormat(fitFormatXP);
               }
            }
         }
         var fitFieldGP:Object = this.myTotalGPGetLabelTF;
         if(fitFieldGP != null && fitFieldGP.text == "GP Earned" && fitFieldGP.width > 4)
         {
            var fitFormatGP:Object = fitFieldGP.getTextFormat();
            var fitSizeGP:Number = Number(fitFieldGP.defaultTextFormat.size);
            if(fitSizeGP >= 10 && fitSizeGP <= 72)
            {
               fitFormatGP.size = fitSizeGP;
               fitFieldGP.setTextFormat(fitFormatGP);
               while(fitFieldGP.textWidth > fitFieldGP.width - 4 && fitSizeGP > 10)
               {
                  fitSizeGP = Math.max(10,fitSizeGP - 1);
                  fitFormatGP.size = fitSizeGP;
                  fitFieldGP.setTextFormat(fitFormatGP);
               }
            }
         }
      }
      
      override protected function onInitListener() : void
      {
         this.myConfirmBtn.addEventListener(MouseEvent.CLICK,this.onConfirmQuitGameHandle);
         this.myCancelBtn.addEventListener(MouseEvent.CLICK,this.onCancelQuitGameHandle);
         this.myXPBar.addEventListener(XPBar.EVT_COMPLETE,this.onXPBarCompleteHandle);
         this.myXPBar.addEventListener(XPBar.EVT_NODE_START,this.onXPBarNodeStartHandle);
         this.myAchievementBar_0.addEventListener(XPBar.EVT_NODE_START,this.onAchievementBarNodeStartHandle);
         this.myAchievementBar_1.addEventListener(XPBar.EVT_NODE_START,this.onAchievementBarNodeStartHandle);
         this.myAchievementBar_2.addEventListener(XPBar.EVT_NODE_START,this.onAchievementBarNodeStartHandle);
      }
      
      override protected function enableData() : void
      {
         this.summaryData = this.getSyncDP(SyncDataIDCommon.SUMMARY_DATA);
         for(var i:uint = 0; i < this._summaryDat.achievements.length; i++)
         {
            this[NAME_ACHIEVEMENT_BAR + i].play();
         }
         this.myXPRemainTF.text = "";
         this.myXPBar.play();
      }
      
      override protected function showView() : void
      {
         var levelUpMinutes:int = 0;
         var weaponUnlockObj:Object = null;
         this.tweenShow(TweenParam.SPD_SCALE,{
            "autoAlpha":1,
            "scaleX":1,
            "scaleY":1
         });
         this.hideAllUnlockInfo();
         if(CFunctionContainerCommon.AS_IsNotificationUnlocked("un_weapon_customization"))
         {
            levelUpMinutes = this.getSyncDP(SyncDataIDCommon.ESTIMATED_LEVELUP_MINUTES);
            if(levelUpMinutes != Common.RESULT_NOT_AVAILABLE)
            {
               this.showUserLevelUpInfo(levelUpMinutes);
            }
            else
            {
               weaponUnlockObj = this.getSyncDP(SyncDataIDCommon.CLOSEST_WEAPON_UNLOCK);
               if(weaponUnlockObj.levelUpMinutes != Common.RESULT_NOT_AVAILABLE)
               {
                  this.showWeaponLevelUpInfo(weaponUnlockObj.levelUpMinutes,weaponUnlockObj.weaponObj.name,weaponUnlockObj.weaponObj.imageURL,weaponUnlockObj.unlockObject.name,weaponUnlockObj.unlockObject.imageURL);
               }
            }
         }
      }
      
      override protected function hideView() : void
      {
         this.tweenHide(TweenParam.SPD_SCALE_OUT,{
            "autoAlpha":0,
            "scaleX":TweenParam.SCALE_POPUP,
            "scaleY":TweenParam.SCALE_POPUP
         });
      }
      
      private function hideAllUnlockInfo() : void
      {
         this.myNextUnlockTitleTF.visible = false;
         this.myUnlockWeaponIcon.visible = false;
         this.myUnlockWeaponNameTF.visible = false;
         this.myIconNext.visible = false;
         this.myUnlockAttachment.visible = false;
         for(var i:uint = 0; i < NUM_NEXT_UNLOCK_ITEM; i++)
         {
            this[NAME_NEXT_UNLOCK_ITEM + i].visible = false;
         }
      }
      
      private function showUserLevelUpInfo(remainingMinutes:int) : void
      {
         var tempCount:uint = 0;
         if(remainingMinutes < MIN_LEVEL_UP_MINS)
         {
            remainingMinutes = int(MIN_LEVEL_UP_MINS);
         }
         var i:uint = 0;
         var nextLevelDat:Object = this.getSyncDP(SyncDataIDCommon.CAREER_NEXT_LEVEL_UNLOCK_OBJ);
         if(nextLevelDat != null)
         {
            tempCount = 0;
            i = 0;
            while(i < nextLevelDat.items.length && tempCount < NUM_NEXT_UNLOCK_ITEM)
            {
               if(nextLevelDat.items[i] != null)
               {
                  this[NAME_NEXT_UNLOCK_ITEM + tempCount].visible = true;
                  ImageLoader.loadImg(nextLevelDat.items[i].imageURL,this[NAME_NEXT_UNLOCK_ITEM + tempCount].myImg,this.onUnlockItemImgComplete);
                  tempCount++;
               }
               i++;
            }
         }
         var bShowUnlockItem:Boolean = false;
         if(nextLevelDat != null && nextLevelDat.items.length > 0)
         {
            for(i = 0; i < nextLevelDat.items.length; i++)
            {
               if(nextLevelDat.items[i] != null)
               {
                  bShowUnlockItem = true;
                  break;
               }
            }
         }
         this.myNextUnlockTitleTF.visible = true;
         if(bShowUnlockItem)
         {
            this.myNextUnlockTitleTF.text = LocalizeUtil.param(LocalizationDict.PLAY_MORE_X_TIME_TO_LEVEL_UP_AND_UNLOCK,[remainingMinutes]);
         }
         else
         {
            this.myNextUnlockTitleTF.text = LocalizeUtil.param(LocalizationDict.PLAY_MORE_X_TIME_TO_LEVEL_UP,[remainingMinutes]);
         }
      }
      
      private function showWeaponLevelUpInfo(remainingMinutes:int, weaponName:String, weaponImage:String, attachmentName:String, attachmentImage:String) : void
      {
         if(remainingMinutes < MIN_LEVEL_UP_MINS)
         {
            remainingMinutes = int(MIN_LEVEL_UP_MINS);
         }
         this.myNextUnlockTitleTF.visible = true;
         this.myNextUnlockTitleTF.text = LocalizeUtil.param(LocalizationDict.PLAY_MORE_X_TIME_TO_LEVEL_UP,[remainingMinutes]);
         this.myUnlockAttachment.visible = true;
         this.myUnlockWeaponNameTF.visible = true;
         this.myIconNext.visible = true;
         this.myUnlockWeaponIcon.visible = true;
         ImageLoader.loadImg(weaponImage,this.myUnlockWeaponIcon,this.onUnlockWeaponImgComplete);
         this.myUnlockWeaponNameTF.text = weaponName;
         this.myUnlockAttachment["myNameTF"].text = attachmentName;
         ImageLoader.loadImg(attachmentImage + "_s",this.myUnlockAttachment["myImg"],this.onUnlockWeaponAttachmentImgComplete);
      }
      
      private function set summaryData(cData:Object) : void
      {
         this._summaryDat = cData;
         this.myTotelTimeTF.text = TimeUtil.timeToString(this._summaryDat.timePlayed);
         var RANK_DATA_INVALID_FOR_INCONSIST_VERSION:int = -1;
         if(this._summaryDat.xpGained != RANK_DATA_INVALID_FOR_INCONSIST_VERSION)
         {
            this.myTotalXPGetTF.text = String(this._summaryDat.xpGained);
            this.myCurrentLevelTF.visible = true;
            this.myNextLevelTF.visible = true;
            this.myXPBar.visible = true;
         }
         else
         {
            this.myTotalXPGetTF.text = LocalizationDict.RANK_TABLE_CHANGED;
            this.myCurrentLevelTF.visible = false;
            this.myNextLevelTF.visible = false;
            this.myXPBar.visible = false;
         }
         this.myTotalGPGetTF.text = String(this._summaryDat.gpGained);
         this.myXPBar.XPData = this._summaryDat.levelIncrease;
         this.hideAllAchievementInfo();
         for(var i:uint = 0; i < this._summaryDat.achievements.length; i++)
         {
            this[NAME_ACHIEVEMENT_BAR + i].visible = true;
            this[NAME_ACHIEVEMENT_BAR + i].XPData = this._summaryDat.achievements[i];
            this[NAME_ACHIEVEMENT_BAR + i].play();
         }
         if(this.state == ViewState.ACTIVATED)
         {
            for(i = 0; i < this._summaryDat.achievements.length; i++)
            {
               this[NAME_ACHIEVEMENT_BAR + i].play();
            }
            this.myXPRemainTF.text = "";
            this.myXPBar.play();
         }
      }
      
      private function onCancelQuitGameHandle(cEvt:Event) : void
      {
         this.broadcast(BCEventCommon.EVT_QUIT_GAME);
      }
      
      private function onConfirmQuitGameHandle(cEvt:Event) : void
      {
         this.broadcast(BCEventCommon.EVT_CLOSE_SUMMARY);
      }
      
      private function onXPBarCompleteHandle(cEvt:Event) : void
      {
         var levelIncreaseArr:Array = this._summaryDat.levelIncrease;
         var total:int = int(levelIncreaseArr[levelIncreaseArr.length - 1].total);
         var current:int = int(levelIncreaseArr[levelIncreaseArr.length - 1].current);
         this.myXPRemainTF.text = LocalizeUtil.replace(this._remainXPString,[total - current]);
      }
      
      private function onXPBarNodeStartHandle(cEvt:ParaEvent) : void
      {
         var tf:String = this._summaryDat.levelIncrease[cEvt.myPara.index].name;
         tf = tf.slice(2);
         var nextLvl:String = (Number(tf) + 1).toString();
         this.myCurrentLevelTF.text = tf;
         this.myNextLevelTF.text = nextLvl;
      }
      
      private function onAchievementBarNodeStartHandle(cEvt:ParaEvent) : void
      {
         var targetExpObj:EXPLevelObj = null;
         var barIndex:uint = uint(cEvt.target.name.split("_")[1]);
         if(this._summaryDat.achievements[barIndex].length > cEvt.myPara.index)
         {
            this._summaryDat.achievements[barIndex][cEvt.myPara.index];
            targetExpObj = new EXPLevelObj(this._summaryDat.achievements[barIndex][cEvt.myPara.index]);
            this[NAME_ACHIEVEMENT_LABEL + barIndex].text = targetExpObj.name;
            this[NAME_ACHIEVEMENT_EXP + barIndex].visible = targetExpObj.total > 1;
            this[NAME_ACHIEVEMENT_EXP + barIndex].text = targetExpObj.current + "/" + targetExpObj.total;
         }
      }
      
      private function hideAllAchievementInfo() : void
      {
         for(var i:uint = 0; i < 3; i++)
         {
            this[NAME_ACHIEVEMENT_BAR + i].visible = false;
            this[NAME_ACHIEVEMENT_LABEL + i].text = "";
            this[NAME_ACHIEVEMENT_EXP + i].text = "";
            this[NAME_ACHIEVEMENT_ICON + i].visible = false;
         }
      }
      
      private function onUnlockItemImgComplete(cBMP:Bitmap) : void
      {
         var scale:Number = 56 / cBMP.width;
         cBMP.width = 56;
         cBMP.height = scale * cBMP.height;
         cBMP.x = -cBMP.width / 2;
         cBMP.y = -cBMP.height / 2;
      }
      
      private function onUnlockWeaponImgComplete(cBMP:Bitmap) : void
      {
         cBMP.height = 56;
         cBMP.scaleX = cBMP.scaleY;
         cBMP.x = -cBMP.width / 2;
         cBMP.y = -cBMP.height / 2;
      }
      
      private function onUnlockWeaponAttachmentImgComplete(cBMP:Bitmap) : void
      {
         cBMP.height = 40;
         cBMP.scaleX = cBMP.scaleY;
         cBMP.x = -cBMP.width / 2;
         cBMP.y = -cBMP.height / 2;
      }
   }
}

