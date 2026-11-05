package codo.views
{
   import codo.common.BaseView;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEventCommon;
   import codo.constants.LocalizationServer;
   import codo.constants.ServerMessageDuration;
   import codo.constants.ServerMessageType;
   import codo.objects.ServerMessageObj;
   import codo.utils.LocalizeUtil;
   import codo.utils.ScaleformUtil;
   import com.greensock.TweenLite;
   import com.greensock.easing.Linear;
   import flash.events.TimerEvent;
   import flash.text.TextField;
   import flash.text.TextFieldAutoSize;
   import flash.utils.Timer;

   public class ServerMessageView extends BaseView
   {
      private static const POS_ORIGIN_X:Number = 880;

      private static const POS_ORIGIN_Y:Number = 80;

      private static var POS_X_START:int = 372;

      private static const POS_X_END:int = 4;

      private static const PIXELS_PER_SECOND:uint = 75;

      public var myMsgTF:TextField;

      private var _serverMsgToShow:Array = [];

      private var _currentServerMsgIndex:int = -1;

      private var _isInGame:Boolean = false;

      public function ServerMessageView()
      {
         super();
      }

      override protected function onInit() : void
      {
         this.myMsgTF.autoSize = TextFieldAutoSize.LEFT;
         this.myMsgTF.htmlText = "";
         this.x = POS_ORIGIN_X;
         this.y = POS_ORIGIN_Y;
      }

      override protected function onInitListener() : void
      {
         this.listen(BCEventCommon.EVT_NOTE_NOT_BUY_TRY_ON_ITEM,this.onNotBuyTryOnItemHandle);
         this.listen(BCEventCommon.EVT_SHOW_SERVER_MESSAGE,this.onShowServerMessageHandle);
         this.listen(BCEventCommon.EVT_HIDE_SERVER_MESSAGE,this.onHideServerMessageHandle);
         this.listen(BCEventCommon.EVT_IS_IN_GAME,this.onSetIsInGameHandle);
      }

      override protected function enableData() : void
      {
         this.bind(AsyncTaskID.SHOW_SERVER_MESSAGE,this.bind_showServerMessage);
         this.bind(AsyncTaskID.HIDE_SERVER_MESSAGE,this.bind_hideServerMessage);
         this.testServerMessage();
      }

      private function testServerMessage() : void
      {
         if(!ScaleformUtil.isScaleform)
         {
            this.bind_showServerMessage({"data":{
               "senderName":"[系统]:",
               "duration":(new Date().valueOf() + 60000) / 1000,
               "message":"[测试]请注意，您收到一条来自<FONT COLOR=\'#0000aa\'>腾讯</FONT>的消息。"
            }});
            this.bind_showServerMessage({"data":{
               "senderName":"[系统]:",
               "duration":60000,
               "message":"[测试]请注意，您收到一条来自<FONT COLOR=\'#00FF00\'>动视</FONT>的消息。"
            }});
         }
      }

      private function bind_showServerMessage(cRes:Object) : void
      {
         var now:Number = Number(new Date().valueOf());
         var timeStamp:Number = cRes.data.duration * 1000;
         if(timeStamp <= now)
         {
            Logger.print("ServerMessageView:::bind_showServerMessage the server message has expired!!! ");
            return;
         }
         var sender:String = "";
         var message:String = cRes.data.message.indexOf("@SF_UI_") >= 0 ? LocalizeUtil.localize(cRes.data.message) : cRes.data.message;
         var msg:String = sender + message;
         var type:uint = ServerMessageType.IDIP;
         if((msg.indexOf("Server queue ") == 0 || msg.indexOf("Match server starting.") == 0) && this.myMsgTF.text == msg)
         {
            for(var messageIndex:int = 0; messageIndex < this._serverMsgToShow.length; messageIndex++)
            {
               if(this._serverMsgToShow[messageIndex].type == type && this._serverMsgToShow[messageIndex].message == msg)
               {
                  this._serverMsgToShow[messageIndex].duration = timeStamp;
                  return;
               }
            }
         }
         var capacityWasShowing:Boolean = (this.myMsgTF.text.indexOf("Server queue ") == 0 || this.myMsgTF.text.indexOf("Match server starting.") == 0);
         this.removeExistingIDIPMessages();
         if(capacityWasShowing || msg.indexOf("Server queue ") == 0 || msg.indexOf("Match server starting.") == 0)
         {
            this.tryStopAnimation();
            this._currentServerMsgIndex = -1;
         }
         this.show({
            "message":msg,
            "type":type,
            "duration":timeStamp
         });
      }

      private function bind_hideServerMessage(cRes:Object) : void
      {
         this.hide();
      }

      private function onShowServerMessageCompleteHandle(event:TimerEvent) : void
      {
         for(var i:int = 0; i < this._serverMsgToShow.length; i++)
         {
            if(this._serverMsgToShow[i].timer == event.target)
            {
               this._serverMsgToShow[i].timer.stop();
               this._serverMsgToShow.splice(i,1);
               break;
            }
         }
      }

      private function onShowServerMessageHandle(cEvt:ParaEvent) : void
      {
         this.show(cEvt.myPara);
      }

      private function onHideServerMessageHandle(cEvt:ParaEvent) : void
      {
         this.hide();
      }

      private function onSetIsInGameHandle(cEvt:ParaEvent) : void
      {
         this._isInGame = cEvt.myPara;
         if(this._isInGame)
         {
            var capacityWasShowing:Boolean = (this.myMsgTF.text.indexOf("Server queue ") == 0 || this.myMsgTF.text.indexOf("Match server starting.") == 0);
            for(var queueIndex:int = 0; queueIndex < this._serverMsgToShow.length; )
            {
               if((this._serverMsgToShow[queueIndex].message.indexOf("Server queue ") == 0 || this._serverMsgToShow[queueIndex].message.indexOf("Match server starting.") == 0))
               {
                  this._serverMsgToShow.splice(queueIndex,1);
               }
               else
               {
                  queueIndex++;
               }
            }
            this.x = 282;
            this.y = 5;
            if(this._serverMsgToShow.length <= 0)
            {
               this.visible = false;
            }
            POS_X_START = 733;
            if(capacityWasShowing)
            {
               this.tryStopAnimation();
               this._currentServerMsgIndex = -1;
               this.tryStartAnimation();
            }
         }
         else
         {
            this.x = (this.myMsgTF.text.indexOf("Server queue ") == 0 || this.myMsgTF.text.indexOf("Match server starting.") == 0) ? 880 : POS_ORIGIN_X;
            this.y = (this.myMsgTF.text.indexOf("Server queue ") == 0 || this.myMsgTF.text.indexOf("Match server starting.") == 0) ? 80 : POS_ORIGIN_Y;
            POS_X_START = 372;
         }
      }

      private function onNotBuyTryOnItemHandle(cEvt:ParaEvent) : void
      {
         var msg:String = LocalizeUtil.localize(LocalizationServer.NOTIFY_HAVNT_BUY_TRYON_ITEM);
         var type:uint = ServerMessageType.TRY_ON;
         var duration:Number = ServerMessageDuration.TRY_ON;
         this.show({
            "message":msg,
            "type":type,
            "duration":duration
         });
      }

      private function isServerMsgRepetitive(serverMsg:ServerMessageObj) : Boolean
      {
         var temp:Object = null;
         for(var i:int = 0; i < this._serverMsgToShow.length; i++)
         {
            temp = this._serverMsgToShow[i];
            if(temp.type == serverMsg.type && temp.message == serverMsg.message && temp.duration == serverMsg.duration)
            {
               return true;
            }
         }
         return false;
      }

      private function removeExistingIDIPMessages() : void
      {
         for(var i:int = 0; i < this._serverMsgToShow.length; )
         {
            if(this._serverMsgToShow[i].type == ServerMessageType.IDIP)
            {
               this._serverMsgToShow.splice(i,1);
            }
            else
            {
               i++;
            }
         }
      }

      private function show(cServerMsg:Object) : void
      {
         var serverMsgObj:ServerMessageObj = new ServerMessageObj(cServerMsg);
         // A match hides the view. Queue/join messages must restore it after
         // returning to either multiplayer or Zombies without reloading codo.
         if(!this._isInGame)
         {
            this.visible = true;
         }
         if(this.isServerMsgRepetitive(serverMsgObj))
         {
            return;
         }
         if(this._isInGame)
         {
            this.visible = true;
         }
         if(serverMsgObj.duration != ServerMessageDuration.INFINITE && serverMsgObj.type != ServerMessageType.IDIP)
         {
            serverMsgObj.timer = new Timer(serverMsgObj.duration,1);
            serverMsgObj.timer.addEventListener(TimerEvent.TIMER_COMPLETE,this.onShowServerMessageCompleteHandle);
         }
         this._serverMsgToShow.push(serverMsgObj);
         if(this._currentServerMsgIndex == -1)
         {
            this.tryStartAnimation();
         }
      }

      private function hide() : void
      {
         this.tryStopAnimation();
      }

      private function tryStartAnimation() : void
      {
         var now:Number = NaN;
         if(this._serverMsgToShow.length <= 0)
         {
            this._currentServerMsgIndex = -1;
            this.myMsgTF.htmlText = "";
            if(this._isInGame)
            {
               this.visible = false;
            }
            return;
         }
         ++this._currentServerMsgIndex;
         if(this._currentServerMsgIndex >= this._serverMsgToShow.length)
         {
            this._currentServerMsgIndex = 0;
         }
         this.myMsgTF.htmlText = this._serverMsgToShow[this._currentServerMsgIndex].message;
         if(!this._isInGame)
         {
            this.x = (this._serverMsgToShow[this._currentServerMsgIndex].message.indexOf("Server queue ") == 0 || this._serverMsgToShow[this._currentServerMsgIndex].message.indexOf("Match server starting.") == 0) ? 880 : POS_ORIGIN_X;
            this.y = (this._serverMsgToShow[this._currentServerMsgIndex].message.indexOf("Server queue ") == 0 || this._serverMsgToShow[this._currentServerMsgIndex].message.indexOf("Match server starting.") == 0) ? 80 : POS_ORIGIN_Y;
            POS_X_START = 372;
         }
         var timer:Timer = this._serverMsgToShow[this._currentServerMsgIndex].timer;
         if(timer != null && !timer.running)
         {
            timer.start();
         }
         this.myMsgTF.x = POS_X_START;
         var tweenSpeed:Number = (this.myMsgTF.x - POS_X_END + this.myMsgTF.width) / PIXELS_PER_SECOND;
         TweenLite.to(this.myMsgTF,tweenSpeed,{
            "x":POS_X_END - this.myMsgTF.width,
            "ease":Linear.easeNone,
            "onComplete":this.tryStartAnimation
         });
         if(this._serverMsgToShow[this._currentServerMsgIndex].type == ServerMessageType.IDIP)
         {
            now = Number(new Date().valueOf());
            if(this._serverMsgToShow[this._currentServerMsgIndex].duration <= now)
            {
               this._serverMsgToShow.splice(this._currentServerMsgIndex,1);
            }
         }
      }

      private function tryStopAnimation() : void
      {
         TweenLite.killTweensOf(this.myMsgTF);
         this.myMsgTF.htmlText = "";
         if(this._isInGame)
         {
            this.visible = false;
         }
      }
   }
}
