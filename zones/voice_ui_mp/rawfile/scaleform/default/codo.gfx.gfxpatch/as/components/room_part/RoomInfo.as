package components.room_part
{
   import codo.common.ParaEvent;
   import codo.constants.LocalizationRoom;
   import codo.constants.MatchType;
   import com.greensock.layout.ScaleMode;
   import components.LevelChooser;
   import flash.display.MovieClip;
   import flash.text.TextField;
   
   public class RoomInfo extends RoomInfoBase
   {
      public static const EVT_LEVEL_CHANGE:String = "level_change";
      
      public var myGameModeTF:TextField;
      
      public var myPlayersLabelTF:TextField;
      
      public var myPlayersTF:TextField;
      
      public var myRoomIDLabelTF:TextField;
      
      public var myRoomIDTF:TextField;
      
      public var myPlayListTF:TextField;
      
      public var myFakeMapImg:MovieClip;
      
      public var myPlayListDescriptionTF:TextField;
      
      public var myRoomNameLabelTF:TextField;
      
      public var myRoomNameTF:TextField;
      
      public var myLeagueLabelTF:TextField;
      
      public var myLeagueTF:TextField;
      
      public var myLevelChooser:LevelChooser;
      
      public function RoomInfo()
      {
         super();
      }
      
      override protected function onInit() : void
      {
         super.onInit();
         this.myRoomIDLabelTF.text = LocalizationRoom.LIST_LABEL_INDEX;
         this.myLeagueLabelTF.text = LocalizationRoom.LIST_LABEL_LEAGUE;
         this.myPlayListTF.visible = false;
         this.myMapAlbum.setImgSize(285,100);
         this.myMapAlbum.fitParams = {"scaleMode":ScaleMode.PROPORTIONAL_OUTSIDE};
         this.myLevelChooser.labels = LocalizationRoom.ROOM_LEVELS;
         this.myLevelChooser.addEventListener(LevelChooser.EVT_CHANGED,this.onChangeLevelHandle);
         this.myRoomNameLabelTF.visible = false;
         this.myRoomIDLabelTF.visible = false;
         this.myLeagueLabelTF.visible = false;
         this.myPlayersLabelTF.visible = false;
      }
      
      override public function set data(cDat:Object) : void
      {
         var diffNames:Array = null;
         var i:int = 0;
         super.data = cDat;
         if(cDat == null)
         {
            return;
         }
         this.myRoomIDTF.text = cDat.index.toString();
         this.myFakeMapImg.visible = false;
         this.myPlayListTF.visible = cDat.isPlayList;
         // A Survival room is a co-op room of a coop_survival_<level>: its mode is Survival, and it has no difficulty.
         var isSurvival:Boolean = !cDat.isPlayList && cDat.currentMap != null && String(cDat.currentMap.id).indexOf("coop_survival_") == 0;
         this.myLeagueTF.visible = cDat.matchType != MatchType.PVP && !isSurvival;
         this.myPlayListDescriptionTF.visible = false;
         this.myGameModeTF.visible = !cDat.isPlayList;
         this.myRoomNameTF.visible = !cDat.isPlayList;
         this.myRoomIDTF.visible = !cDat.isPlayList;
         // BEGIN room display owner name
         var displayRoomName:String = String(cDat.name);
         var displayHost:Object = null;
         var displayHostCount:int = 0;
         if(cDat.forces != null && cDat.forces.force0 != null && cDat.forces.force1 != null)
         {
            var displayMembers:Array = cDat.forces.force0.concat(cDat.forces.force1);
            for each(var displayMember in displayMembers)
            {
               if(displayMember != null && displayMember["_localQuickNativeRoleKnown"] == true && String(displayMember.id).length > 0 && displayMember["_localQuickNativeMemberID"] == String(displayMember.id) && (cDat.forces.isHost == true ? displayMember.isMe == true : displayMember["_localQuickNativeHost"] == true))
               {
                  displayHost = displayMember;
                  displayHostCount++;
               }
            }
         }
         if(displayHostCount == 1 && typeof displayHost.realName == "string" && displayHost.realName.length > 0)
         {
            displayRoomName = displayHost.realName;
         }
         // Internal launcher account aliases are identities, not room titles.
         // Automatic training keeps its playlist label through native commit.
         if(cDat["_managedTrainingPlaylist"] === 0 || cDat["_managedTrainingPlaylist"] === 2)
         {
            displayRoomName = cDat["_managedTrainingPlaylist"] === 0 ? "Boot Camp" : "Bot Training";
         }
         else if(/^p[0-9a-f]{12,16}$/i.test(displayRoomName))
         {
            var aliasOwner:Object = null;
            var aliasOwners:int = 0;
            if(cDat.forces != null && cDat.forces.force0 != null && cDat.forces.force1 != null)
            {
               for each(var aliasMember in cDat.forces.force0.concat(cDat.forces.force1))
               {
                  if(aliasMember != null && cDat.forces.isHost == true && aliasMember.isMe == true)
                  {
                     aliasOwner = aliasMember;
                     aliasOwners++;
                  }
               }
            }
            displayRoomName = aliasOwners == 1 && typeof aliasOwner.realName == "string" &&
               aliasOwner.realName.length > 0 && !/^p[0-9a-f]{12,16}$/i.test(aliasOwner.realName) ?
               aliasOwner.realName : "Matchmaking";
         }
         this.myRoomNameTF.text = displayRoomName;
         // END room display owner name
         this.myLeagueLabelTF.text = cDat.matchType == MatchType.PVP ? "" : LocalizationRoom.ROOM_LEVEL;
         this.myLeagueTF.text = LocalizationRoom.ROOM_LEVELS[cDat.currentLevel];
         this.myPlayersTF.text = cDat["_localQuickSharedLineage"] == true && cDat.currentMaxPlayers == 12 ? "2 ~ 12" : cDat.currentMinPlayers + " ~ " + cDat.currentMaxPlayers;
         if(Boolean(cDat.isPlayList))
         {
            this.myFakeMapImg.gotoAndPlay(1);
            this.myPlayListTF.text = cDat.playListName;
            this.myPlayListDescriptionTF.text = cDat.playListDescription;
         }
         else
         {
            this.myFakeMapImg.stop();
            this.myGameModeTF.text = isSurvival ? "@K33UI_MODE_SURVIVAL" : (cDat.matchType == MatchType.PVP ? cDat.currentMatchMode.name : LocalizationRoom.COOP_MAPS[cDat.mapType]);
         }
         if(!cDat.isPlayList && cDat.currentMatchMode.id == "combat")
         {
            this.myLeagueTF.text = LocalizationRoom.ROOM_LEVELS[cDat.currentLevel - 1];
         }
         this.myLevelChooser.visible = cDat.forces.showDifficulty;
         if(cDat.forces.difficulties.length > 0)
         {
            this.myLevelChooser.data = cDat.forces.difficulties;
            this.myLevelChooser.selectedIndex = cDat.forces.selectedDifficultyIndex;
            diffNames = new Array();
            i = 0;
            while(i < cDat.forces.difficulties.length)
            {
               diffNames.push(cDat.forces.difficulties[i].name);
               i++;
            }
            this.myLevelChooser.setEnableLevels(diffNames.length);
            this.myLevelChooser.labels = diffNames;
         }
      }
      
      private function onChangeLevelHandle(cEvt:ParaEvent) : void
      {
         this.dispatchEvent(new ParaEvent(EVT_LEVEL_CHANGE,{"data":this.myLevelChooser.selectedValue.id}));
      }
   }
}

