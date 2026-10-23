package components
{
   import codo.common.ParaEvent;
   import components.quickmatch_coop_part.DifficultiesSelector;
   import flash.display.Sprite;
   
   public class LevelChooser extends CodoUIComponent
   {
      public static const EVT_CHANGED:String = "evt_changed";
      
      private static const SRC_LEVEL_BTN:String = "myDifficulties_";
      
      private static const SRC_BAR:String = "myDifficultiesBar_";
      
      private static const MAX_LEVEL_BTN:uint = 3;
      
      public var myDifficulties_0:DifficultiesSelector;
      
      public var myDifficulties_1:DifficultiesSelector;
      
      public var myDifficulties_2:DifficultiesSelector;
      
      public var myDifficultiesBar_1:Sprite;
      
      public var myDifficultiesBar_2:Sprite;
      
      private var _data:Array;
      
      private var _diffData:Array;
      
      private var _labelsDat:Array;
      
      private var _currentSelectedBtn:DifficultiesSelector;
      
      private var _currentSelectedValue:Object;
      
      private var _currentSelectedIndex:uint;
      
      public function LevelChooser()
      {
         super();
         this.onInit();
         this.onInitListener();
      }
      
      private function onInit() : void
      {
         for(var i:int = 0; i < MAX_LEVEL_BTN; i++)
         {
            this[SRC_LEVEL_BTN + i].selected = false;
         }
      }
      
      private function onInitListener() : void
      {
         for(var i:int = 0; i < MAX_LEVEL_BTN; i++)
         {
            this[SRC_LEVEL_BTN + i].addEventListener(BaseButton.EVT_CLICK,this.onLevelBtnClickHandle);
         }
      }
      
      public function set labels(cDat:Array) : void
      {
         var previousLayout:Array = this._labelsDat == null ? null : this._labelsDat["endlessLayout"] as Array;
         this._labelsDat = cDat == null ? [] : cDat.concat();
         var extra:DifficultiesSelector = this.getChildByName("myDifficulties_3") as DifficultiesSelector;
         if(this._labelsDat.length > 3 && extra == null)
         {
            var factory:Class = Object(this.myDifficulties_2).constructor as Class;
            extra = new factory() as DifficultiesSelector;
            extra.name = "myDifficulties_3";
            extra.x = this.myDifficulties_2.x;
            extra.y = this.myDifficulties_2.y;
            extra.scaleX = this.myDifficulties_2.scaleX;
            extra.scaleY = this.myDifficulties_2.scaleY;
            extra.selected = false;
            extra.enable = false;
            extra.visible = false;
            extra.addEventListener(BaseButton.EVT_CLICK,this.onLevelBtnClickHandle);
            this.addChild(extra);
         }
         this._labelsDat["endlessLayout"] = previousLayout != null ? previousLayout : [this.myDifficulties_0.x,this.myDifficulties_1.x,this.myDifficulties_2.x,this.myDifficulties_0.scaleX,this.myDifficulties_1.scaleX,this.myDifficulties_2.scaleX];
         var i:int = 0;
         while(i < 4)
         {
            var button:DifficultiesSelector = this.getChildByName(SRC_LEVEL_BTN + i) as DifficultiesSelector;
            if(button != null && i < this._labelsDat.length)
            {
               button.label = this._labelsDat[i];
            }
            i++;
         }
      }
      
      public function set data(cDat:Array) : void
      {
         this._data = cDat;
      }
      
      public function get data() : Array
      {
         return this._data;
      }
      
      public function get selectedValue() : Object
      {
         var temObj:Object = null;
         var i:int = int(this._currentSelectedIndex);
         if(this._data == null)
         {
            temObj = [0];
         }
         else
         {
            temObj = this._data[i];
         }
         return temObj;
      }
      
      public function set selectedIndex(cIndex:uint) : void
      {
         this._currentSelectedIndex = cIndex;
         var slot:int = int(cIndex);
         if(this._diffData != null)
         {
            if(cIndex >= this._diffData.length || this._diffData[cIndex] == null)
            {
               this._currentSelectedIndex = 0;
               if(this._diffData.length == 0)
               {
                  return;
               }
            }
            slot = int(this._diffData[this._currentSelectedIndex].id);
         }
         if(slot < 0 || slot > 3)
         {
            return;
         }
         var button:DifficultiesSelector = this.getChildByName(SRC_LEVEL_BTN + slot) as DifficultiesSelector;
         if(button != null)
         {
            this.selectTargetBtn(button,false);
         }
      }
      
      public function get selectedIndex() : uint
      {
         return this._currentSelectedIndex;
      }
      
      public function set diffData(cDat:Array) : void
      {
         this._diffData = cDat == null ? [] : cDat;
         var endless:Boolean = false;
         var i:int = 0;
         var j:int = 0;
         j = 0;
         while(j < this._diffData.length)
         {
            if(this._diffData[j] != null && int(this._diffData[j].id) == 3)
            {
               endless = true;
            }
            j++;
         }
         var layout:Array = this._labelsDat == null ? null : this._labelsDat["endlessLayout"];
         i = 0;
         while(i < 4)
         {
            var button:DifficultiesSelector = this.getChildByName(SRC_LEVEL_BTN + i) as DifficultiesSelector;
            if(button != null)
            {
               var available:Boolean = false;
               j = 0;
               while(j < this._diffData.length)
               {
                  if(this._diffData[j] != null && int(this._diffData[j].id) == i)
                  {
                     available = true;
                  }
                  j++;
               }
               button.enable = available;
               button.visible = i < 3 || endless;
               if(layout != null && layout.length == 6)
               {
                  if(endless)
                  {
                     button.x = Number(layout[0]) + (Number(layout[1]) - Number(layout[0])) * i * 3 / 4;
                     button.scaleX = Number(layout[3]) * 3 / 4;
                  }
                  else if(i < 3)
                  {
                     button.x = Number(layout[i]);
                     button.scaleX = Number(layout[i + 3]);
                  }
               }
            }
            i++;
         }
         this.myDifficultiesBar_1.visible = !endless;
         this.myDifficultiesBar_2.visible = !endless;
      }
      
      public function get diffData() : Array
      {
         return this._diffData;
      }
      
      public function setEnableLevels(cAmount:uint) : void
      {
         var i:int = 0;
         while(i < 4)
         {
            var button:DifficultiesSelector = this.getChildByName(SRC_LEVEL_BTN + i) as DifficultiesSelector;
            if(button != null)
            {
               button.visible = i < cAmount && (i < 3 || button.enable);
            }
            var bar:Sprite = this.getChildByName(SRC_BAR + i) as Sprite;
            if(bar != null)
            {
               bar.visible = i < cAmount && cAmount < 4;
            }
            i++;
         }
      }
      
      private function selectTargetBtn(cTarget:DifficultiesSelector, cRefreshIndex:Boolean = true) : void
      {
         if(cTarget == null)
         {
            return;
         }
         if(this._currentSelectedBtn != null)
         {
            this._currentSelectedBtn.selected = false;
         }
         this._currentSelectedBtn = cTarget;
         if(cRefreshIndex)
         {
            var slot:int = int(cTarget.name.split(SRC_LEVEL_BTN)[1]);
            this._currentSelectedIndex = slot;
            if(this._diffData != null)
            {
               var i:int = 0;
               while(i < this._diffData.length)
               {
                  if(this._diffData[i] != null && int(this._diffData[i].id) == slot)
                  {
                     this._currentSelectedIndex = i;
                     break;
                  }
                  i++;
               }
            }
         }
         cTarget.selected = true;
      }
      
      private function onLevelBtnClickHandle(cEvt:ParaEvent) : void
      {
         this.selectTargetBtn(cEvt.target as DifficultiesSelector);
         this.dispatchEvent(new ParaEvent(EVT_CHANGED));
      }
   }
}

