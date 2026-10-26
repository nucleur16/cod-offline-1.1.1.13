package codo.views
{
   import codo.common.BaseView;
   import codo.constants.GameInfo;
   import codo.constants.MouseSize;
   import codo.constants.TipType;
   import codo.constants.TweenParam;
   import codo.interfaces.ITipView;
   import codo.views.tip_sub.TipContentView;
   import com.greensock.TweenLite;
   import components.CodoUIComponent;
   import components.tip_part.BaseTip;
   import components.tip_part.ItemTip;
   import flash.display.DisplayObject;
   import flash.events.MouseEvent;
   import flash.geom.Point;
   
   public class TipView extends BaseView implements ITipView
   {
      public var myGeneralTip_alignTop:BaseTip;
      
      public var myGeneralTip_alignBottom:BaseTip;
      
      public var myItemTip:ItemTip;
      
      private var _currentTip:*;
      
      private var _currentData:Object;
      
      private var _tipContentView:TipContentView;
      
      public function TipView()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         CodoUIComponent.TIP_VIEW = this;
         this._tipContentView = new TipContentView();
         this.addChild(this._tipContentView);
         this.hideAllTip();
         this.mouseEnabled = false;
         this.mouseChildren = false;
         this._currentTip = this.myGeneralTip_alignTop;
      }
      
      public function displayTip(cTarget:CodoUIComponent) : void
      {
         var tempData:Object = null;
         if(cTarget.tipEnable == true)
         {
            TweenLite.killTweensOf(this);
            this.alpha = 0;
            this.z = 30;
            TweenLite.to(this,1,{
               "alpha":1,
               "z":0
            });
            tempData = this._tipContentView.getParameters(cTarget);
            if(tempData != null)
            {
               this._currentData = tempData;
               CodoUIComponent.REAL_STAGE.addEventListener(MouseEvent.MOUSE_MOVE,this.onTipMouseMoveHandle);
               this.onTipMouseOverHandle(cTarget);
            }
         }
      }
      
      public function removeTip(cTarget:CodoUIComponent) : void
      {
         CodoUIComponent.REAL_STAGE.removeEventListener(MouseEvent.MOUSE_MOVE,this.onTipMouseMoveHandle);
         this.onTipMouseOutHandle(cTarget);
         this._currentData = null;
      }
      
      private function onTipMouseOverHandle(cTarget:CodoUIComponent) : void
      {
         var targetPosX:Number = NaN;
         var targetPosY:Number = NaN;
         var globalPoint:Point = null;
         this.hideAllTip();
         this.chooseTipByID(this._currentData.type);
         if(this._currentData.data != null && this._currentData.data != "")
         {
            this._currentTip.data = this._currentData.data;
            if(this._currentData.mouseFollow == true)
            {
               this.tryTipMouseFollow(this._currentTip);
            }
            else if(this._currentData.isAbsolutePos == true)
            {
               this._currentTip.x = this._currentData.x;
               this._currentTip.y = this._currentData.y;
            }
            else
            {
               globalPoint = (this._currentData.target as DisplayObject).localToGlobal(new Point(0,0));
               this._currentTip.x = globalPoint.x + this._currentData.x;
               this._currentTip.y = globalPoint.y + this._currentData.y;
            }
            if(this._currentData.mouseFollow == false)
            {
               if(this._currentTip.x + this.getBounds(this._currentTip).x < 0)
               {
                  this._currentTip.x = -this.getBounds(this._currentTip).x;
               }
               else if(this._currentTip.x + this._currentTip.width + this._currentTip.getBounds(this._currentTip).x > CodoUIComponent.REAL_STAGE.stageWidth)
               {
                  this._currentTip.x = CodoUIComponent.REAL_STAGE.stageWidth - this._currentTip.width - this._currentTip.getBounds(this._currentTip).x;
               }
            }
            TweenLite.killTweensOf(this._currentTip);
            TweenLite.to(this._currentTip,TweenParam.SPD_FADE,{"autoAlpha":1});
         }
      }
      
      private function onTipMouseOutHandle(cTarget:CodoUIComponent) : void
      {
         TweenLite.killTweensOf(this._currentTip);
         TweenLite.to(this._currentTip,TweenParam.SPD_FADE,{"autoAlpha":0});
      }
      
      private function onTipMouseMoveHandle(cEvt:MouseEvent) : void
      {
         if(this._currentData != null && this._currentData.mouseFollow == true)
         {
            this.tryTipMouseFollow(this._currentTip);
         }
      }
      
      private function chooseTipByID(cTipID:uint) : void
      {
         switch(cTipID)
         {
            case TipType.GENERAL:
               this._currentTip = this.myGeneralTip_alignTop;
               break;
            case TipType.GENERAL_ALIGN_BOTTOM:
               this._currentTip = this.myGeneralTip_alignBottom;
               break;
            case TipType.ITEM_TIP:
               this._currentTip = this.myItemTip;
         }
      }
      
      private function hideAllTip() : void
      {
         for(var i:uint = 0; i < this.numChildren; i++)
         {
            TweenLite.killTweensOf(this.getChildAt(i));
            if(this.getChildAt(i) is BaseTip || this.getChildAt(i) is ItemTip)
            {
               this.getChildAt(i).visible = false;
               this.getChildAt(i).alpha = 0;
            }
         }
      }
      
      private function tryTipMouseFollow(cTarget:DisplayObject) : void
      {
         var targetX:int = CodoUIComponent.REAL_STAGE.mouseX + MouseSize.WIDTH;
         var targetY:int = CodoUIComponent.REAL_STAGE.mouseY + MouseSize.HEIGHT;
         if(targetX + cTarget.width + GameInfo.widthExtra > CodoUIComponent.REAL_STAGE.stageWidth && cTarget == this.myItemTip)
         {
            targetX = CodoUIComponent.REAL_STAGE.stageWidth - GameInfo.widthExtra - cTarget.width;
         }
         else if(targetX + cTarget.width / 2 + GameInfo.widthExtra > CodoUIComponent.REAL_STAGE.stageWidth && cTarget != this.myItemTip)
         {
            targetX = CodoUIComponent.REAL_STAGE.stageWidth - GameInfo.widthExtra - cTarget.width / 2;
         }
         if(targetY + cTarget.height + GameInfo.heightExtra > CodoUIComponent.REAL_STAGE.stageHeight)
         {
            targetY = CodoUIComponent.REAL_STAGE.stageHeight - cTarget.height - GameInfo.heightExtra;
         }
         cTarget.x = targetX;
         cTarget.y = targetY;
      }
   }
}

