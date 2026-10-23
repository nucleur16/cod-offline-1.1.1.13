package components.list_part
{
   import codo.common.ParaEvent;
   import codo.interfaces.IList;
   import components.BaseButton;
   import components.interfaces.IListItemRendererBase;
   import flash.events.MouseEvent;
   import flash.text.TextField;
   
   public class ListItemRendererBase8 extends BaseButton implements IListItemRendererBase
   {
      protected var _container:IList;
      
      public var myLabelTF:TextField;
      
      private var _dataIndex:int = -1;
      
      protected var _data:Object = {};
      
      protected var _selected:Boolean;
      
      private var _styleID:uint = 0;
      
      public function ListItemRendererBase8()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         if(this.myLabelTF != null)
         {
            this.myLabelTF.mouseEnabled = false;
         }
         this.setToNormal();
      }
      
      public function set container(cList:IList) : void
      {
         this._container = cList;
      }
      
      public function set dataIndex(cIndex:int) : void
      {
         var tempNewDat:Object = null;
         this._dataIndex = cIndex;
         if(this._container != null)
         {
            tempNewDat = this._container.getDataByIndex(this._dataIndex);
            this.setData(tempNewDat);
            this.selected = this._container.selectedDataIndex == this.dataIndex;
         }
      }
      
      public function get dataIndex() : int
      {
         return this._dataIndex;
      }
      
      public function set index(cIndex:int) : void
      {
         this.dataIndex = cIndex;
      }
      
      public function get index() : int
      {
         return this._dataIndex;
      }
      
      public function setData(cDat:Object) : void
      {
         this._data = cDat;
         if(this._data != null)
         {
            this.enable = true;
            this.onGetData();
         }
         else
         {
            this.enable = false;
            this.onNoData();
         }
      }
      
      public function getData() : Object
      {
         return this._data;
      }
      
      public function set selected(cSwi:Boolean) : void
      {
         if(this._selected != cSwi)
         {
            this._selected = cSwi;
            if(this._selected == true)
            {
               this.onSelected();
            }
            else
            {
               this.onUnselected();
            }
         }
      }
      
      public function get selected() : Boolean
      {
         return this._selected;
      }
      
      public function set style(cStyleID:uint) : void
      {
         this._styleID = cStyleID;
         this.onStyleChanged();
      }
      
      public function get style() : uint
      {
         return this._styleID;
      }
      
      protected function onStyleChanged() : void
      {
      }
      
      protected function onSelected() : void
      {
         this.setToRollOver();
      }
      
      protected function onUnselected() : void
      {
         this.setToNormal();
      }
      
      protected function onGetData() : void
      {
         if(this.myLabelTF != null)
         {
            this.myLabelTF.text = this._data.label || this._data.name || "";
         }
         this.visible = true;
         var fitFieldItem:Object = this.myLabelTF;
         if(fitFieldItem != null && fitFieldItem.text == "Team Deathmatch" && fitFieldItem.width > 4)
         {
            var fitFormatItem:Object = fitFieldItem.getTextFormat();
            var fitSizeItem:Number = Number(fitFieldItem.defaultTextFormat.size);
            if(fitSizeItem >= 10 && fitSizeItem <= 72)
            {
               fitFormatItem.size = fitSizeItem;
               fitFieldItem.setTextFormat(fitFormatItem);
               while(fitFieldItem.textWidth > fitFieldItem.width - 4 && fitSizeItem > 10)
               {
                  fitSizeItem = Math.max(10,fitSizeItem - 1);
                  fitFormatItem.size = fitSizeItem;
                  fitFieldItem.setTextFormat(fitFormatItem);
               }
            }
         }
      }
      
      protected function onNoData() : void
      {
         this.visible = false;
      }
      
      override protected function onMouseOutHandle(cEvt:MouseEvent) : void
      {
         this.sendUILog(cEvt.type);
         if(this._enable && this._selected == false)
         {
            this._isFocusIn = false;
            this.setToNormal();
            this.dispatchEvent(new ParaEvent(EVT_MOUSE_OUT));
         }
      }
   }
}

