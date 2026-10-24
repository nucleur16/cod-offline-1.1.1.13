package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.Common;
   import codo.constants.LocalizationParty;
   import codo.constants.LocalizationRightClick;
   import codo.constants.MsgBtnCount;
   import codo.constants.RightClickMenuType;
   import codo.constants.SyncDataID;
   import codo.constants.SyncDataIDCommon;
   import codo.objects.FriendObj;
   import codo.objects.RightClickMenuItemObj;
   import codo.objects.UserObj;
   import codo.utils.LocalizeUtil;
   
   public class RightClickMenuController extends BaseController
   {
      private var _rightClickMenuItemsFriend:Vector.<RightClickMenuItemObj>;
      
      private var _rightClickMenuItemsRecent:Vector.<RightClickMenuItemObj>;
      
      private var _rightClickMenuItemsOnlineFriend:Vector.<RightClickMenuItemObj>;
      
      private var _rightClickMenuItemsParty:Vector.<RightClickMenuItemObj>;
      
      private var _rightClickMenuItemsInRoom:Vector.<RightClickMenuItemObj>;
      
      private var _rightClickMenuItemsLeaderboard:Vector.<RightClickMenuItemObj>;
      
      private var _userToInviteToParty:UserObj;
      
      public function RightClickMenuController()
      {
         super();
      }
      
      override protected function onInitialized() : void
      {
         this.addSyncDP(SyncDataID.RIGHT_CLICK_MENU,this.generateRightClickMenu);
         this.addSyncDP(SyncDataID.PARTY_CAN_KICK_MEMBER_BY_USEROBJ,this.canKickPartyMember);
         this.addSyncDP(SyncDataID.PARTY_CAN_PROMOTE_MEMBER_BY_USEROBJ,this.canPromotePartyMember);
         this.addSyncDP(SyncDataID.PARTY_CAN_INVITE_TO_PARTY_BY_USEROBJ,this.canInviteToParty);
         this.listen(BCEvent.QUICK_JOIN_FRIEND,this.onFriendQuickJoinHandle);
         this.listen(BCEvent.PARTY_JOINED,this.onPartyJoinedHandle);
         this.listen(BCEvent.JOIN_ROOM_BY_TICKET,this.onJoinRoomByTicketHandle);
      }
      
      private function generateRightClickMenu(cType:String, cUser:Object) : Vector.<RightClickMenuItemObj>
      {
         var rightClickMenuItems:Vector.<RightClickMenuItemObj> = null;
         switch(cType)
         {
            case RightClickMenuType.FRIENDS:
               rightClickMenuItems = this.generateFriend(cUser);
               break;
            case RightClickMenuType.RECENT_MATCH_USER:
               rightClickMenuItems = this.generateRecent(cUser);
               break;
            case RightClickMenuType.ONLINE_FRIENDS:
               rightClickMenuItems = this.generateOnlineFriend(cUser);
               break;
            case RightClickMenuType.IN_ROOM:
               rightClickMenuItems = this.generateInRoom(cUser);
               break;
            case RightClickMenuType.PARTY:
               rightClickMenuItems = this.generateParty(cUser);
               break;
            case RightClickMenuType.LEADERBOARD:
               rightClickMenuItems = this.generateLeaderboard(cUser);
               break;
            case RightClickMenuType.CLAN:
         }
         return rightClickMenuItems;
      }
      
      private function generateFriend(cUser:Object) : Vector.<RightClickMenuItemObj>
      {
         if(this._rightClickMenuItemsFriend == null)
         {
            this._rightClickMenuItemsFriend = new Vector.<RightClickMenuItemObj>();
            this._rightClickMenuItemsFriend[0] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.SEND_PRIVATE_MESSAGE,
               "handle":this.onInviteToPrivateChatHandle
            });
            this._rightClickMenuItemsFriend[1] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.BLOCK_USER,
               "handle":this.onBlockFriendHandle
            });
            this._rightClickMenuItemsFriend[2] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.INVITE_TO_PARTY,
               "handle":this.onInviteToPartyHandle
            });
            this._rightClickMenuItemsFriend[3] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.INVITE_TO_CLAN,
               "handle":this.onInviteToClanHandle
            });
            this._rightClickMenuItemsFriend[4] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.INVITE_TO_GAME,
               "handle":this.onInviteToGameHandle
            });
            this._rightClickMenuItemsFriend[5] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.JOIN_FRIEND_GAME,
               "handle":this.joinFriendGame
            });
            this._rightClickMenuItemsFriend[6] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.JOIN_FRIEND_PARTY,
               "handle":this.joinFriendParty
            });
            this._rightClickMenuItemsFriend[7] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.DELETE_FRIEND,
               "handle":this.onDeleteFriendHandle
            });
         }
         var userObj:UserObj = cUser as UserObj;
         this._rightClickMenuItemsFriend[0].enable = this.canInviteToPrivateChat(userObj);
         this._rightClickMenuItemsFriend[1].enable = false;
         this._rightClickMenuItemsFriend[2].enable = this.canInviteToParty(userObj);
         this._rightClickMenuItemsFriend[3].enable = this.canInviteToClan(userObj);
         this._rightClickMenuItemsFriend[4].enable = this.canInviteToGame(userObj);
         this._rightClickMenuItemsFriend[5].enable = this.canJoinFriendGame(userObj);
         this._rightClickMenuItemsFriend[6].enable = this.canJoinFriendParty(userObj);
         this._rightClickMenuItemsFriend[7].enable = this.canDeleteFriend(userObj);
         return this._rightClickMenuItemsFriend;
      }
      
      private function generateRecent(cUser:Object) : Vector.<RightClickMenuItemObj>
      {
         if(this._rightClickMenuItemsRecent == null)
         {
            this._rightClickMenuItemsRecent = new Vector.<RightClickMenuItemObj>();
            this._rightClickMenuItemsRecent[0] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.ADD_TO_FRIEND,
               "handle":this.onAddToFriendHandle
            });
         }
         var userObj:UserObj = new UserObj(cUser);
         this._rightClickMenuItemsRecent[0].enable = this.canInviteToFriend(userObj);
         return this._rightClickMenuItemsRecent;
      }
      
      private function generateOnlineFriend(cUser:Object) : Vector.<RightClickMenuItemObj>
      {
         if(this._rightClickMenuItemsOnlineFriend == null)
         {
            this._rightClickMenuItemsOnlineFriend = new Vector.<RightClickMenuItemObj>();
            this._rightClickMenuItemsOnlineFriend[0] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.SEND_PRIVATE_MESSAGE,
               "handle":this.onInviteToPrivateChatHandle
            });
            this._rightClickMenuItemsOnlineFriend[1] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.BLOCK_USER,
               "handle":this.onBlockFriendHandle
            });
            this._rightClickMenuItemsOnlineFriend[2] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.INVITE_TO_PARTY,
               "handle":this.onInviteToPartyHandle
            });
            this._rightClickMenuItemsOnlineFriend[3] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.INVITE_TO_CLAN,
               "handle":this.onInviteToClanHandle
            });
            this._rightClickMenuItemsOnlineFriend[4] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.INVITE_TO_GAME,
               "handle":this.onInviteToGameHandle
            });
            this._rightClickMenuItemsOnlineFriend[5] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.DELETE_FRIEND,
               "handle":this.onDeleteFriendHandle
            });
         }
         var userObj:UserObj = cUser as UserObj;
         this._rightClickMenuItemsOnlineFriend[0].enable = this.canInviteToPrivateChat(userObj);
         this._rightClickMenuItemsOnlineFriend[1].enable = false;
         this._rightClickMenuItemsOnlineFriend[2].enable = this.canInviteToParty(userObj);
         this._rightClickMenuItemsOnlineFriend[3].enable = this.canInviteToClan(userObj);
         this._rightClickMenuItemsOnlineFriend[4].enable = this.canInviteToGame(userObj);
         this._rightClickMenuItemsOnlineFriend[5].enable = this.canDeleteFriend(userObj);
         return this._rightClickMenuItemsOnlineFriend;
      }
      
      private function generateParty(cUser:Object) : Vector.<RightClickMenuItemObj>
      {
         if(this._rightClickMenuItemsParty == null)
         {
            this._rightClickMenuItemsParty = new Vector.<RightClickMenuItemObj>();
            this._rightClickMenuItemsParty[0] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.ADD_TO_FRIEND,
               "handle":this.onAddToFriendHandle
            });
            this._rightClickMenuItemsParty[1] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.SEND_PRIVATE_MESSAGE,
               "handle":this.onInviteToPrivateChatHandle
            });
            this._rightClickMenuItemsParty[2] = new RightClickMenuItemObj({
               "label":LocalizationParty.KICK_OUT_PARTY,
               "handle":this.onKickPartyMemberHandle
            });
            this._rightClickMenuItemsParty[3] = new RightClickMenuItemObj({
               "label":LocalizationParty.REASSIGN_PARTY_LEADER,
               "handle":this.onPromotePartyMemberHandle
            });
            this._rightClickMenuItemsParty[4] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.INVITE_TO_CLAN,
               "handle":this.onInviteToClanHandle
            });
         }
         var userObj:UserObj = cUser as UserObj;
         this._rightClickMenuItemsParty[0].enable = this.canInviteToFriend(userObj);
         this._rightClickMenuItemsParty[1].enable = this.canInviteToPrivateChat(userObj);
         this._rightClickMenuItemsParty[2].enable = this.canKickPartyMember(userObj);
         this._rightClickMenuItemsParty[3].enable = this.canPromotePartyMember(userObj);
         this._rightClickMenuItemsParty[4].enable = this.canInviteToClan(userObj);
         return this._rightClickMenuItemsParty;
      }
      
      private function generateInRoom(cUser:Object) : Vector.<RightClickMenuItemObj>
      {
         if(this._rightClickMenuItemsInRoom == null)
         {
            this._rightClickMenuItemsInRoom = new Vector.<RightClickMenuItemObj>();
            this._rightClickMenuItemsInRoom[0] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.ADD_TO_FRIEND,
               "handle":this.onAddToFriendHandle
            });
            this._rightClickMenuItemsInRoom[1] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.SEND_PRIVATE_MESSAGE,
               "handle":this.onInviteToPrivateChatHandle
            });
            this._rightClickMenuItemsInRoom[2] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.INVITE_TO_CLAN,
               "handle":this.onInviteToClanHandle
            });
         }
         var userObj:UserObj = cUser as UserObj;
         this._rightClickMenuItemsInRoom[0].enable = this.canInviteToFriend(userObj);
         this._rightClickMenuItemsInRoom[1].enable = this.canInviteToPrivateChat(userObj);
         this._rightClickMenuItemsInRoom[2].enable = this.canInviteToClan(userObj);
         return this._rightClickMenuItemsInRoom;
      }
      
      private function generateLeaderboard(cUser:Object) : Vector.<RightClickMenuItemObj>
      {
         if(this._rightClickMenuItemsLeaderboard == null)
         {
            this._rightClickMenuItemsLeaderboard = new Vector.<RightClickMenuItemObj>();
            this._rightClickMenuItemsLeaderboard[0] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.SEND_PRIVATE_MESSAGE,
               "handle":this.onInviteToPrivateChatHandle
            });
            this._rightClickMenuItemsLeaderboard[1] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.ADD_TO_FRIEND,
               "handle":this.onAddToFriendHandle
            });
            this._rightClickMenuItemsLeaderboard[2] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.INVITE_TO_CLAN,
               "handle":this.onInviteToClanHandle
            });
            this._rightClickMenuItemsLeaderboard[3] = new RightClickMenuItemObj({
               "label":LocalizationRightClick.INVITE_TO_GAME,
               "handle":this.onInviteToGameHandle
            });
         }
         var userObj:UserObj = new UserObj(cUser);
         this._rightClickMenuItemsLeaderboard[0].enable = this.canInviteToPrivateChat(userObj);
         this._rightClickMenuItemsLeaderboard[1].enable = this.canInviteToFriend(userObj);
         this._rightClickMenuItemsLeaderboard[2].enable = this.canInviteToClan(userObj);
         this._rightClickMenuItemsLeaderboard[3].enable = this.canInviteToGame(userObj);
         return this._rightClickMenuItemsLeaderboard;
      }
      
      private function onAddToFriendHandle(cUser:Object) : void
      {
         Logger.print("RightClickMenuController::onAddToFriendHandle cUser.name : " + cUser.name + " cUser.id:" + cUser.id);
         this.broadcast(BCEvent.EVT_ADD_FRIEND,{
            "isName":false,
            "value":cUser.id,
            "name":cUser.name
         });
      }
      
      private function onInviteToPrivateChatHandle(cUser:Object) : void
      {
         Logger.print("RightClickMenuController::onInviteToPrivateChatHandle cUser.name: " + cUser.name);
         this.broadcast(BCEvent.EVT_START_PRIVATE_CHAT,cUser);
      }
      
      private function onInviteToPartyHandle(cUser:Object) : void
      {
         return;
      }
      
      private function onInviteToGameHandle(cUser:Object) : void
      {
         return;
      }
      
      private function onFriendQuickJoinHandle(cEvt:ParaEvent) : void
      {
         var friendObj:FriendObj = cEvt.myPara;
         if(friendObj.isInRoom)
         {
            this.joinFriendGame(friendObj);
         }
         else if(friendObj.isInParty)
         {
            this.joinFriendParty(friendObj);
         }
      }
      
      private function onPartyJoinedHandle(cEvt:ParaEvent) : void
      {
         return;
      }
      
      private function onJoinRoomByTicketHandle(cEvt:ParaEvent) : void
      {
         this.joinRoomByTicket(cEvt.myPara.inviter,cEvt.myPara.session,cEvt.myPara.isBootCampRoom);
      }
      
      private function joinFriendParty(cUser:Object) : void
      {
         var friend:FriendObj = cUser as FriendObj;
         if(friend != null)
         {
            if(friend.isInRoom)
            {
               this.showMsg(LocalizationParty.NOTE_CANT_JOIN_FRIEND_PARTY_IN_ROOM,MsgBtnCount.ONE);
            }
            else
            {
               if(!friend.isInParty)
               {
                  this.showMsg(LocalizationParty.NOTE_CANT_JOIN_FRIEND_NONEXISTENT_PARTY,MsgBtnCount.ONE);
                  return;
               }
               if(friend.isPartyPrivate)
               {
                  this.showMsg(LocalizationParty.NOTE_CANT_JOIN_FRIEND_PRIVATE_PARTY,MsgBtnCount.ONE);
                  return;
               }
               this.broadcast(BCEvent.PARTY_JOIN,{
                  "senderID":friend.id,
                  "targetID":friend.sessionID
               });
            }
         }
      }
      
      private function joinFriendGame(cUser:Object) : void
      {
         var friend:FriendObj = cUser as FriendObj;
         if(friend != null)
         {
            if(friend.isInRoom)
            {
               this.joinRoomByTicket(friend.id,friend.sessionID,friend.playListObj != null && friend.playListObj.leagueID == 0);
            }
            else
            {
               this.showMsg(LocalizationParty.NOTE_CANT_JOIN_FRIEND_NONEXISTENT_ROOM,MsgBtnCount.ONE);
            }
         }
      }
      
      private function onInviteToClanHandle(cUser:Object) : void
      {
         this.request(AsyncTaskID.PROPOSE_CLAN_MEMBERSHIP,cUser);
      }
      
      private function onKickPartyMemberHandle(cUser:Object) : void
      {
         this.broadcast(BCEvent.EVT_KICK_PARTY_MEMBER,cUser);
      }
      
      private function onPromotePartyMemberHandle(cUser:Object) : void
      {
         if(this.getSyncDP(SyncDataID.IS_IN_MY_PARTY,[cUser]))
         {
            this.broadcast(BCEvent.EVT_PROMOTE_PARTY_MEMBER,cUser);
         }
      }
      
      private function onDeleteFriendHandle(cUser:Object) : void
      {
         this.broadcast(BCEvent.EVT_DELETE_FRIEND,cUser);
      }
      
      private function onBlockFriendHandle(cUser:Object) : void
      {
         this.broadcast(BCEvent.EVT_MOVE_FRIEND_TO_BLACKLIST,cUser);
      }
      
      private function canInviteToFriend(cUser:UserObj) : Boolean
      {
         return !this.isMe(cUser) && !this.isMyFriend(cUser) && cUser.name != this.getSyncDP(SyncDataID.PLAYER_NAME);
      }
      
      private function canInviteToParty(cUser:UserObj) : Boolean
      {
         return false;
      }
      
      private function canInviteToPrivateChat(cUser:UserObj) : Boolean
      {
         return !this.isMe(cUser) && cUser.isOnline;
      }
      
      private function canDeleteFriend(cUser:UserObj) : Boolean
      {
         return !this.isMe(cUser) && this.isMyFriend(cUser);
      }
      
      private function canKickPartyMember(cUser:UserObj) : Boolean
      {
         return !this.isMe(cUser) && this.isPartyHost() && !this.isInRoom() && !this.isRankedMatch() && this.isInMyParty(cUser);
      }
      
      private function canPromotePartyMember(cUser:UserObj) : Boolean
      {
         return !this.isMe(cUser) && this.isPartyHost() && !this.isInRoom() && !this.isRankedMatch() && this.isInMyParty(cUser);
      }
      
      private function canInviteToClan(cUser:UserObj) : Boolean
      {
         return !this.isMe(cUser) && this.getSyncDP(SyncDataID.CAN_PROPOSE_CLAN_MEMBERSHIP,[cUser]);
      }
      
      private function canInviteToGame(cUser:UserObj) : Boolean
      {
         return false;
      }
      
      private function canJoinFriendGame(cUser:UserObj) : Boolean
      {
         var isRoomValid:Boolean = false;
         var friend:FriendObj = cUser as FriendObj;
         if(friend != null)
         {
            isRoomValid = !friend.isRankedMatch && friend.isInRoom && !friend.isRoomFull && !friend.isRoomPrivate;
            if(friend.gameStatus == Common.CODO_PRESENCE_STORY_MISSION || friend.gameStatus == Common.CODO_PRESENCE_SPEC_OP)
            {
               isRoomValid &&= friend.friendStatus == Common.FRIEND_STATUS_LOBBY;
            }
            else if(friend.gameStatus == Common.CODO_PRESENCE_TRAINING)
            {
               isRoomValid &&= friend.friendStatus == Common.FRIEND_STATUS_LOBBY || friend.friendStatus == Common.FRIEND_STATUS_INGAME;
            }
            return friend.isOnline && isRoomValid && !this.isInRoom() && !this.isInParty();
         }
         return false;
      }
      
      private function canJoinFriendParty(cUser:UserObj) : Boolean
      {
         var friend:FriendObj = cUser as FriendObj;
         if(friend != null)
         {
            return friend.isOnline && friend.isInParty && !friend.isPartyPrivate && !this.isInRoom() && !this.isInParty();
         }
         return false;
      }
      
      private function isMyFriend(cUser:Object) : Boolean
      {
         return this.getSyncDP(SyncDataID.IS_FRIEND,[cUser]);
      }
      
      private function isRankedMatch() : Boolean
      {
         return this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH);
      }
      
      private function isRoomFull() : Boolean
      {
         return this.getSyncDP(SyncDataID.IS_ROOM_FULL);
      }
      
      private function isInRoom() : Boolean
      {
         return this.getSyncDP(SyncDataID.IS_IN_ROOM);
      }
      
      private function isInMyRoom(cUser:UserObj) : Boolean
      {
         return this.getSyncDP(SyncDataID.IS_IN_MY_ROOM,[cUser]);
      }
      
      private function isInParty() : Boolean
      {
         return this.getSyncDP(SyncDataID.IS_IN_PARTY);
      }
      
      private function isPartyHost() : Boolean
      {
         return this.getSyncDP(SyncDataID.IS_PARTY_HOST);
      }
      
      private function isInMyParty(cUser:UserObj) : Boolean
      {
         return this.getSyncDP(SyncDataID.IS_IN_MY_PARTY,[cUser]);
      }
      
      private function isMe(cUser:UserObj) : Boolean
      {
         return this.getSyncDP(SyncDataID.IS_ME,[cUser]);
      }
      
      private function joinRoomByTicket(inviterID:String, roomID:String, isBootCampRoom:Boolean = false) : void
      {
         var level:uint = 0;
         if(isBootCampRoom)
         {
            level = this.getSyncDP(SyncDataIDCommon.CAREER_CURRENT_LEVEL);
            if(level > Common.BOOT_CAMP_THRESHOLD)
            {
               this.showMsg(LocalizeUtil.param(LocalizationParty.NOTE_CANT_JOIN_FRIEND_BOOT_CAMP,[level]),MsgBtnCount.ONE);
               return;
            }
         }
         this.broadcast(BCEvent.EVT_JOIN_ROOM,{
            "invite":{
               "senderID":inviterID,
               "targetID":roomID
            },
            "ticket":true
         });
      }
      
      private function isRankedMatchReady(friend:FriendObj) : Boolean
      {
         return friend != null ? friend.rankedMatchState == 3 : false;
      }
   }
}

