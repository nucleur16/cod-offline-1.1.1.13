package components.scoreboard_part
{
   import codo.constants.LocalizationDict;
   import codo.objects.ScoreboardObj;
   import components.CodoList;
   import components.CodoSprite;
   import components.list_part.HudScoreboardItemRenderer;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;
   
   public class ScoreboardTeamPanel extends CodoSprite
   {
      
      public var myScoreTF:TextField;
      
      public var myNameTF:TextField;
      
      public var myList:CodoList;
      
      public var myTeamBlue:Sprite;
      
      public var myTeamRed:Sprite;
      
      public var myBkg:Sprite;
      
      public var myCellPattern:Sprite;
      
      private var _style:uint;
      
      private static const MIN_TEAM_ROWS:int = 6;
      
      private static const MIN_EMBLEM_SCALE:Number = 0.3;
      
      public function ScoreboardTeamPanel()
      {
         super();
      }
      
      public function SetStyle(style:uint) : void
      {
         this._style = style;
         this.myList.addEventListener(Event.ADDED_TO_STAGE,this.InitList);
         if(!this.IsCyborgsAsset())
         {
            if(style == HudScoreboardItemRenderer.STYLE_BLUE)
            {
               this.myTeamBlue.visible = true;
               this.myTeamRed.visible = false;
               this.SetScoreDisplay(LocalizationDict.FORCE_TF,11725298);
            }
            else if(style == HudScoreboardItemRenderer.STYLE_RED)
            {
               this.myTeamBlue.visible = false;
               this.myTeamRed.visible = true;
               this.SetScoreDisplay(LocalizationDict.FORCE_SC,16435143);
            }
            else if(style == HudScoreboardItemRenderer.STYLE_NOTEAM)
            {
               this.myTeamBlue.visible = false;
               this.myTeamRed.visible = false;
               this.SetScoreDisplay("",11725298);
            }
         }
      }
      
      private function InitList(event:Event) : void
      {
         this.myList.removeEventListener(Event.ADDED_TO_STAGE,this.InitList);
         this.myList.setStyle(this._style);
      }
      
      public function AdjustHeight(delta:Number) : void
      {
         if(this.myBkg == null)
         {
            return;
         }
         if(Boolean(this.myBkg))
         {
            this.myBkg.height += delta;
         }
         if(Boolean(this.myCellPattern))
         {
            this.myCellPattern.height += delta;
         }
         if(delta > 0)
         {
            delta *= 0.5;
         }
         if(Boolean(this.myScoreTF))
         {
            this.myScoreTF.y += delta;
         }
         this.AdjustImg(this.myTeamBlue,delta);
         this.AdjustImg(this.myTeamRed,delta);
      }
      
      private function AdjustImg(sprite:Sprite, delta:Number) : void
      {
         var img:Sprite = sprite["myImg"];
         if(Boolean(img))
         {
            if(delta > 0)
            {
               img.y += delta;
            }
            else
            {
               sprite.visible = false;
            }
         }
      }
      
      public function SetScoreboardData(data:ScoreboardObj) : void
      {
         var score:int = 0;
         var players:Array = null;
         var mode:String = data.gametype;
         var layout:Object = null;
         var blueImg:Sprite = null;
         var redImg:Sprite = null;
         var rows:int = 0;
         var delta:Number = 0;
         var scoreSpace:Number = 0;
         var scoreFactor:Number = 0;
         var nameFactor:Number = 0;
         var panelRows:int = 0;
         if(this._style == HudScoreboardItemRenderer.STYLE_NOTEAM)
         {
            score = data.alliesScore;
            players = data.alliesPlayers != null ? data.alliesPlayers.concat() : [];
            if(data.axisPlayers != null)
            {
               players = players.concat(data.axisPlayers);
            }
            players.sortOn("rank",Array.NUMERIC);
         }
         else if(this._style == HudScoreboardItemRenderer.STYLE_BLUE)
         {
            score = data.alliesScore;
            players = data.alliesPlayers;
         }
         else if(this._style == HudScoreboardItemRenderer.STYLE_RED)
         {
            score = data.axisScore;
            players = data.axisPlayers;
         }
         if(Boolean(this.myScoreTF))
         {
            this.myScoreTF.text = String(score);
            this.myScoreTF.visible = mode != "coop" && mode != "zm" && mode != "dm" && mode != "gun" && mode != "oic" && mode != "shrp" && mode != "infect";
         }
         if(!this.IsCyborgsAsset() && players != null)
         {
            if(Boolean(this.myTeamBlue))
            {
               blueImg = this.myTeamBlue["myImg"] as Sprite;
            }
            if(Boolean(this.myTeamRed))
            {
               redImg = this.myTeamRed["myImg"] as Sprite;
            }
            layout = this.myList["codolPartyPanelLayout"];
            if(layout == null)
            {
               layout = {
                  "height":this.myBkg.height,
                  "patternHeight":(Boolean(this.myCellPattern) ? this.myCellPattern.height : 0),
                  "scoreY":(Boolean(this.myScoreTF) ? this.myScoreTF.y : 0),
                  "scoreX":(Boolean(this.myScoreTF) ? this.myScoreTF.x : 0),
                  "scoreWidth":(Boolean(this.myScoreTF) ? this.myScoreTF.width : 0),
                  "scoreHeight":(Boolean(this.myScoreTF) ? this.myScoreTF.height : 0),
                  "scoreScaleX":(Boolean(this.myScoreTF) ? this.myScoreTF.scaleX : 1),
                  "scoreScaleY":(Boolean(this.myScoreTF) ? this.myScoreTF.scaleY : 1),
                  "nameX":this.myNameTF.x,
                  "nameY":this.myNameTF.y,
                  "nameWidth":this.myNameTF.width,
                  "nameHeight":this.myNameTF.height,
                  "nameScaleX":this.myNameTF.scaleX,
                  "nameScaleY":this.myNameTF.scaleY,
                  "blueY":(Boolean(blueImg) ? blueImg.y : 0),
                  "redY":(Boolean(redImg) ? redImg.y : 0),
                  "imgX":(Boolean(blueImg) ? blueImg.x : (Boolean(redImg) ? redImg.x : 0)),
                  "imgWidth":(Boolean(blueImg) ? blueImg.width : (Boolean(redImg) ? redImg.width : 0)),
                  "imgHeight":(Boolean(blueImg) ? blueImg.height : (Boolean(redImg) ? redImg.height : 0))
               };
               this.myList["codolPartyPanelLayout"] = layout;
            }
            rows = Math.max(1,Math.min(18,players.length));
            panelRows = rows;
            if(this._style != HudScoreboardItemRenderer.STYLE_NOTEAM && Boolean(this.myScoreTF) && this.myScoreTF.visible)
            {
               panelRows = Math.max(rows,MIN_TEAM_ROWS);
            }
            delta = (this.myList.myItemRenderer["height"] + this.myList.lineSpace) * (panelRows - 9);
            this.myBkg.height = layout.height + delta;
            if(Boolean(this.myCellPattern))
            {
               this.myCellPattern.height = layout.patternHeight + delta;
            }
            if(Boolean(this.myScoreTF))
            {
               nameFactor = this.myScoreTF.visible ? Math.min(1,Math.max(0,this.myBkg.height - 2 * (layout.nameY - this.myBkg.y)) * 0.5 / layout.nameHeight) : 1;
               this.myNameTF.scaleX = layout.nameScaleX * nameFactor;
               this.myNameTF.scaleY = layout.nameScaleY * nameFactor;
               this.myNameTF.x = layout.nameX + (layout.nameWidth - this.myNameTF.width) * 0.5;
               this.myNameTF.y = layout.nameY;
               scoreSpace = this.myBkg.y + this.myBkg.height - this.myNameTF.y - this.myNameTF.height - (this.myNameTF.y - this.myBkg.y);
               scoreFactor = Math.min(1,Math.max(0,scoreSpace) / layout.scoreHeight);
               this.myScoreTF.scaleX = layout.scoreScaleX * scoreFactor;
               this.myScoreTF.scaleY = layout.scoreScaleY * scoreFactor;
               this.myScoreTF.x = layout.scoreX + (layout.scoreWidth - this.myScoreTF.width) * 0.5;
               this.myScoreTF.y = Math.max(this.myNameTF.y + this.myNameTF.height,Math.min(layout.scoreY + delta * 0.5,this.myBkg.y + this.myBkg.height - this.myScoreTF.height - (this.myNameTF.y - this.myBkg.y)));
            }
            this.FitEmblem(this.myTeamBlue,blueImg,layout,layout.blueY + delta * 0.5,HudScoreboardItemRenderer.STYLE_BLUE);
            this.FitEmblem(this.myTeamRed,redImg,layout,layout.redY + delta * 0.5,HudScoreboardItemRenderer.STYLE_RED);
            if(Boolean(this.myList.myHitarea))
            {
               this.myList.myHitarea.height = rows * (this.myList.myItemRenderer["height"] + this.myList.lineSpace) - this.myList.lineSpace;
            }
         }
         this.myList.setStyle(this._style);
         this.myList.dataProvider = players;
      }
      
      private function FitEmblem(team:Sprite, img:Sprite, layout:Object, wantY:Number, style:uint) : void
      {
         var top:Number = NaN;
         var bottom:Number = NaN;
         var scale:Number = NaN;
         var h:Number = NaN;
         if(!team || !img)
         {
            return;
         }
         team.visible = this._style == style;
         top = this.myNameTF.y + this.myNameTF.height;
         if(Boolean(this.myScoreTF) && this.myScoreTF.visible)
         {
            bottom = this.myScoreTF.y + this.myScoreTF.height * 0.1;
         }
         else
         {
            bottom = this.myBkg.y + this.myBkg.height - (this.myNameTF.y - this.myBkg.y);
         }
         scale = layout.imgHeight > 0 ? Math.min(1,Math.max(0,bottom - top) / layout.imgHeight) : 1;
         img.visible = scale >= MIN_EMBLEM_SCALE;
         img.scaleX = img.scaleY = scale;
         h = layout.imgHeight * scale;
         img.x = layout.imgX + (layout.imgWidth - layout.imgWidth * scale) * 0.5;
         img.y = Math.max(top,Math.min(wantY,bottom - h));
      }
      
      private function SetScoreDisplay(txt:String, color:uint) : void
      {
         if(!this.myNameTF)
         {
            return;
         }
         this.myNameTF.text = txt;
         this.myScoreTF.textColor = color;
      }
      
      protected function IsCyborgsAsset() : Boolean
      {
         return false;
      }
   }
}

