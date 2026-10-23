package codo.objects
{
   public dynamic class ScoreboardObj extends CustomObject
   {
      public var axisScore:int;
      
      public var alliesScore:int;
      
      public var axisPlayers:Array;
      
      public var alliesPlayers:Array;
      
      public var gametype:String;
      
      public function ScoreboardObj(cObj:Object = null)
      {
         super(cObj);
         this.ForwardGametype(this.axisPlayers);
         this.ForwardGametype(this.alliesPlayers);
      }
      
      private function ForwardGametype(players:Array) : void
      {
         var i:int = 0;
         if(players != null)
         {
            i = 0;
            while(i < players.length)
            {
               if(this.gametype == "infect" && players[i] != null && players[i].hasOwnProperty("player") && players[i].player === "--" && !players[i].hasOwnProperty("rank") && !players[i].hasOwnProperty("score") && !players[i].hasOwnProperty("ping"))
               {
                  players.splice(i,1);
                  i--;
               }
               else
               {
                  players[i].gametype = this.gametype;
               }
               i++;
            }
         }
      }
      
      override protected function get objectDataKeys() : Array
      {
         return [{
            "id":"axisScore",
            "cls":int
         },{
            "id":"alliesScore",
            "cls":int
         },{
            "id":"axisPlayers",
            "cls":Array
         },{
            "id":"alliesPlayers",
            "cls":Array
         },{
            "id":"gametype",
            "cls":String
         }];
      }
   }
}

