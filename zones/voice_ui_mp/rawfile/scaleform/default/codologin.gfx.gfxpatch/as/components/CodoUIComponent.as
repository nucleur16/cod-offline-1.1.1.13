package components
{
   import codo.common.Logger;
   import codo.k33.PadSnap;
   import codo.constants.GameInfo;
   import codo.constants.QuickMatchType;
   import codo.interfaces.ITipView;
   import components.interfaces.IUIComponent;
   import flash.display.DisplayObject;
   import flash.display.MovieClip;
   import flash.display.Stage;
   import flash.events.Event;
   import flash.events.MouseEvent;
   import flash.utils.getQualifiedClassName;
   
   public class CodoUIComponent extends MovieClip implements IUIComponent
   {
      
      public static var REAL_STAGE:Stage;
      
      // k33: starts the gamepad cursor snapping's target publisher (codo.k33.PadSnap) with the first control
      private static var k33PadSnap:Boolean = PadSnap.boot();
      
      public static var TIP_VIEW:ITipView;
      
      public static var CFunction_CMD_UI_Log:Function;
      
      public static var UI_LOG:Object;
      
      public var getChildNameTag:Function;
      
      protected var _useDefaultTip:Boolean = true;
      
      private var _addToStaged:Boolean = false;
      
      public function CodoUIComponent()
      {
         super();
         this.addEventListener(MouseEvent.ROLL_OVER,this.onDefaultMouseOverHandle);
         this.addEventListener(MouseEvent.ROLL_OUT,this.onDefaultMouseOutHandle);
         this.addEventListener(Event.ADDED_TO_STAGE,this.onAddToStageHandle);
      }
      
      private function onAddToStageHandle(cEvt:Event) : void
      {
         this._addToStaged = true;
         this.removeEventListener(Event.ADDED_TO_STAGE,this.onAddToStageHandle);
      }
      
      public function destroy() : void
      {
      }
      
      public function sendUILog(cEvtType:String) : void
      {
         if(CFunction_CMD_UI_Log != null)
         {
            CFunction_CMD_UI_Log(this.getMyFullName(),cEvtType);
         }
         if(UI_LOG != null)
         {
            UI_LOG.UILog_componentEvt(cEvtType,this.getMyFullName());
         }
      }
      
      protected function exportUIName() : void
      {
         Logger.exportUIName(this.getMyFullName());
      }
      
      protected function exportFullPath() : void
      {
         Logger.exportFullPath(this.getMyFullPath());
      }
      
      public function set tipEnable(cSwi:Boolean) : void
      {
         this._useDefaultTip = cSwi;
      }
      
      public function get tipEnable() : Boolean
      {
         return this._useDefaultTip;
      }
      
      protected function getMyFullName() : String
      {
         var tempFullName:String = getQualifiedClassName(this);
         var tempParent:DisplayObject = this.parent;
         while(tempParent != this.stage && tempParent != null && tempParent.name != "myContainer")
         {
            if(tempParent.name.indexOf("instance") >= 0 || tempParent.name.indexOf("myItemRenderer") >= 0)
            {
               tempFullName = getQualifiedClassName(tempParent) + "." + tempFullName;
            }
            else
            {
               tempFullName = tempParent.name + "." + tempFullName;
            }
            tempParent = tempParent.parent;
         }
         if(tempFullName.indexOf("flash.display::Sprite.") == 0)
         {
            tempFullName = tempFullName.substr(49,tempFullName.length - 49);
         }
         return tempFullName + ("@" + this.name + this.tryGetChildNameTag());
      }
      
      protected function getMyFullPath() : String
      {
         var tempFullPath:String = this.name;
         var tempParent:DisplayObject = this.parent;
         while(tempParent != this.stage && tempParent != null)
         {
            tempFullPath = tempParent.name + "." + tempFullPath;
            tempParent = tempParent.parent;
         }
         return tempFullPath;
      }
      
      protected function tryGetChildNameTag() : String
      {
         var tempTag:String = "";
         if(this.getChildNameTag != null)
         {
            tempTag = this.getChildNameTag();
            if(tempTag != "")
            {
               tempTag = "$" + tempTag;
            }
         }
         return tempTag;
      }
      
      private function onDefaultMouseOverHandle(cEvt:MouseEvent) : void
      {
         this.showTip();
      }
      
      private function onDefaultMouseOutHandle(cEvt:MouseEvent) : void
      {
         this.hideTip();
      }
      
      protected function showTip() : void
      {
         if(TIP_VIEW != null)
         {
            TIP_VIEW.displayTip(this);
         }
      }
      
      protected function hideTip() : void
      {
         if(TIP_VIEW != null)
         {
            TIP_VIEW.removeTip(this);
         }
      }
      
      override public function get stage() : Stage
      {
         if(REAL_STAGE != null)
         {
            return REAL_STAGE;
         }
         return super.stage;
      }
      
      protected function get currentMatchType() : uint
      {
         var tempResult:uint = QuickMatchType.MP;
         if(GameInfo.currentMatchType != null && GameInfo.currentMatchType() == true)
         {
            tempResult = QuickMatchType.CYBORG;
         }
         return tempResult;
      }
      
      public function onStyleChangeForMatchType() : void
      {
      }
   }
}

