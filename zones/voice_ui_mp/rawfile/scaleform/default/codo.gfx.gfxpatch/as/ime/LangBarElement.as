package ime
{
   import flash.display.MovieClip;
   import flash.display.Sprite;
   import flash.events.MouseEvent;
   import flash.geom.*;
   import flash.text.TextField;
   import flash.text.TextFormat;
   import scaleform.gfx.*;
   
   public class LangBarElement extends Sprite
   {
      
      // Decompilation skipped
      
      public var NumInputLanguages:Number = 5;
      
      public var InputLangNames:String = "English,Japanese,Chinese (Simplified),Chinese (Traditional),Korean";
      
      public var IMENames:String = "";
      
      public var Clips:Array;
      
      public var InputLangName:String;
      
      public var LangBar:MovieClip = null;
      
      public var InputLangTab_mc:MovieClip = null;
      
      public var IMENameTab_mc:MovieClip = null;
      
      public var LangBarVisibleState:Boolean = true;
      
      public var isLanguageBar:Boolean = true;
      
      public var TestVariable:Number = 0;
      
      public var NumRows:Number = 0;
      
      public var BGColorOnover:Number = 5619932;
      
      public var BGColor:Number = 14608366;
      
      public var TextColor:Number = 5592921;
      
      public var TextColorSelected:Number = 16777215;
      
      public var IMEListener:Object;
      
      public function LangBarElement()
      {
         // Decompilation skipped
      }
      
      public function SetBackgroundColor(color1:Number, color2:Number) : void
      {
         // Decompilation skipped
      }
      
      public function ChangeBackgroundColor(mc:MovieClip, frameNum:Number) : void
      {
         // Decompilation skipped
      }
      
      public function SetTextColor(color1:Number, color2:Number) : void
      {
         // Decompilation skipped
      }
      
      public function ChangeTextColor(tf:Object, frameNumber:Number) : void
      {
         // Decompilation skipped
      }
      
      public function ChangeTextColorSelected(tf:Object) : void
      {
         // Decompilation skipped
      }
      
      public function AddMouseDownToStage() : void
      {
         // Decompilation skipped
      }
      
      public function onSetCurrentInputLanguage(ev:IMEEventEx) : void
      {
         // Decompilation skipped
      }
      
      public function onSetSupportedLanguages(ev:IMEEventEx) : void
      {
         // Decompilation skipped
      }
      
      public function onSetSupportedIMENames(ev:IMEEventEx) : void
      {
         // Decompilation skipped
      }
      
      public function onSetIMEName(ev:IMEEventEx) : void
      {
         // Decompilation skipped
      }
      
      public function GetLanguageBarPositionY() : Number
      {
         // Decompilation skipped
      }
      
      public function GetLanguageBarPositionX() : Number
      {
         // Decompilation skipped
      }
      
      public function SetSupportedIMEs(imeNames:String) : void
      {
         // Decompilation skipped
      }
      
      public function SetInputLangName(inputLangName:String) : void
      {
         // Decompilation skipped
      }
      
      public function SetLastIME(lastImeName:String) : void
      {
         // Decompilation skipped
      }
      
      public function SetSupportedLanguages(inputLangNames:String) : void
      {
         // Decompilation skipped
      }
      
      public function LoadLanguageBar() : void
      {
         // Decompilation skipped
      }
      
      public function ShowLanguageBar(display:Boolean) : void
      {
         // Decompilation skipped
      }
      
      public function SelectItem(name:String) : void
      {
         // Decompilation skipped
      }
      
      public function MouseDownHandler(event:MouseEvent) : void
      {
         // Decompilation skipped
      }
      
      public function Resize(width:Number, mc:MovieClip) : void
      {
         // Decompilation skipped
      }
      
      public function Reposition(left_mc:MovieClip, right_mc:MovieClip) : void
      {
         // Decompilation skipped
      }
      
      public function CreateInputLangTab() : void
      {
         // Decompilation skipped
      }
      
      public function CloseList(numRows:Number) : void
      {
         // Decompilation skipped
      }
      
      public function GetListOrigin(listType:String) : Point
      {
         // Decompilation skipped
      }
      
      public function CheckListPosition(yPosTab:Number, tabHeight:Number, listHeight:Number, playerWindowH:Number) : Boolean
      {
         // Decompilation skipped
      }
      
      public function CreateList(listType:String, listData:String) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

