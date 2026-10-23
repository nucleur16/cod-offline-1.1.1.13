package codo.controllers
{
   import codo.common.BaseController;
   import codo.common.CFunctionContainer;
   import codo.common.Logger;
   import codo.common.ParaEvent;
   import codo.constants.AsyncTaskID;
   import codo.constants.BCEvent;
   import codo.constants.BCEventCommon;
   import codo.constants.ChattingChannelType;
   import codo.constants.Common;
   import codo.constants.LocalizationAAS;
   import codo.constants.LocalizationParty;
   import codo.constants.LocalizationRoom;
   import codo.constants.LocalizationSystem;
   import codo.constants.MsgBtnCount;
   import codo.constants.PartyType;
   import codo.constants.RankedMatchStatus;
   import codo.constants.RoomListType;
   import codo.constants.SanitizedType;
   import codo.constants.SyncDataID;
   import codo.objects.ChatMessageObj;
   import codo.objects.FriendObj;
   import codo.objects.PartyMemberObj;
   import codo.objects.PartyObj;
   import codo.objects.ResultObj;
   import codo.services.MatchService;
   import codo.services.PartyService;
   import codo.utils.DataUtil;
   import codo.utils.LocalizeUtil;
   import codo.utils.ScaleformUtil;
   import codo.utils.SharedUtil;
   import flash.events.TimerEvent;
   import flash.utils.Timer;
   
   public class PartyController extends BaseController
   {
      public static const PARTY_TO_JOIN_GAME_TIMER:Number = 10;
      
      private var _partyInfo:PartyObj = new PartyObj();
      
      private var _partyInfoPlaceholder:PartyObj = null;
      
      private var _partyMembers:Array = [];
      
      private var _partySizeDP:Array = [];
      
      private var _partyTypeDP:Array = [];
      
      private var _partyToJoinGameTime:int = 0;
      
      private var _partyToJoinGameTimer:Timer = new Timer(1000,11);
      
      private var _matchType:String;
      
      private var _matchMap:String;
      
      private var _matchDiff:String;
      
      public function PartyController()
      {
         super();
      }
      
      override protected function onInitialized() : void
      {
         this.addSyncDP(SyncDataID.IS_IN_PARTY,this.isInParty);
         this.addSyncDP(SyncDataID.IS_PARTY_HOST,this.isPartyHost);
         this.addSyncDP(SyncDataID.IS_IN_MY_PARTY,this.isInMyParty);
         this.addSyncDP(SyncDataID.IS_PARTY_MAX_MEM,this.isPartyMembersMax);
         this.addSyncDP(SyncDataID.ARE_PARTY_MEMBERS_BOOT_CAMP,this.arePartyBootCamp);
         this.addSyncDP(SyncDataID.PARTY_SIZE,this.getPartySize);
         this.addSyncDP(SyncDataID.PARTY_TYPE,this.getPartyType);
         this.addSyncDP(SyncDataID.PARTY_INFO,this.getPartyInfo);
         this.addSyncDP(SyncDataID.PARTY_MEMBER,this.getPartyMembers);
         this.listen(BCEvent.EVT_KICK_PARTY_MEMBER,this.onKickPartyMemberHandle);
         this.listen(BCEvent.EVT_PROMOTE_PARTY_MEMBER,this.onPromotePartyMemberHandle);
         this.listen(BCEvent.EVT_PROPOSE_PARTY_MEMBERSHIP,this.onProposePartyMembershipHandle);
         this.listen(BCEvent.EVT_REJECT_PARTY_MEMBERSHIP,this.onRejectPartyMembershipHandle);
         this.listen(BCEvent.TRY_OPEN_INVITE_LIST,this.onTryShowInviteListHandle);
         this.listen(BCEvent.PARTY_CREATE,this.onCreatePartyHandle);
         this.listen(BCEvent.PARTY_MODIFY,this.onModifyPartyHandle);
         this.listen(BCEvent.PARTY_JOIN,this.onJoinPartyHandle);
         this.listen(BCEvent.PARTY_LEAVE,this.onLeavePartyHandle);
         this.listen(BCEvent.PARTY_SET_READY_STATE,this.onSetPartyReadyStateHandle);
         this.listen(BCEvent.TRY_OPEN_PARTY,this.onTryOpenPartyHandle);
         this.listen(BCEventCommon.EVT_ON_STRING_SANITIZED,this.onPartyInfoSanitizedHandle);
         this.listen(BCEvent.EVT_PLAYER_STATS_READY,this.onPlayerStatsReadyHandle);
         this.listen(BCEvent.EVT_INVITE_USERS_TO_PARTY,this.onInviteUsersToPartyHandle);
         this.bind(AsyncTaskID.PARTY_MEMBER_INFO,this.bind_partyMemberInfo);
         this.bind(AsyncTaskID.PARTY_INFO,this.bind_partyInfo);
         this.bind(AsyncTaskID.LEAVE_PARTY,this.bind_onPartyLeft);
         this.bind(AsyncTaskID.PARTY_START_TO_JOIN_GAME,this.bind_onPartyStartToJoinGame);
         this.bind(AsyncTaskID.PARTY_EVERYONE_IS_READY,this.bind_onPartyEveryoneIsReady);
         this._partyToJoinGameTimer.addEventListener(TimerEvent.TIMER,this.onPartyToJoinGameTimerHandle);
         this._partyToJoinGameTimer.addEventListener(TimerEvent.TIMER_COMPLETE,this.onPartyToJoinGameTimerCompleteHandle);
      }
      
      private function bind_partyMemberInfo(cRes:Object) : void
      {
         var i:int = 0;
         var friend:FriendObj = null;
         var lastHost:Object = null;
         var newHost:Object = null;
         var partyMember:PartyMemberObj = null;
         var partyMem:PartyMemberObj = null;
         if(cRes.data == null)
         {
            return;
         }
         var lastPartyMembers:Array = this._partyMembers.concat();
         var friendInfo:Object = new Object();
         this._partyMembers.splice(0);
         for(i = 0; i < cRes.data.length; i++)
         {
            partyMember = new PartyMemberObj();
            this.fillPartyMemberObj(partyMember,cRes.data[i]);
            partyMember.isMe = this.isMe(partyMember);
            this._partyMembers.push(partyMember);
         }
         if(this._partyMembers.length > 1)
         {
            this._partyMembers.sortOn("name",Array.CASEINSENSITIVE);
         }
         this.setBindData(AsyncTaskID.PARTY_MEMBER_INFO,this._partyMembers);
         this.onPartyStateChanged();
         for(i = 0; i < lastPartyMembers.length; i++)
         {
            partyMem = lastPartyMembers[i];
            if(!DataUtil.isInArray(partyMem.id,this._partyMembers))
            {
               friend = this.getSyncDP(SyncDataID.GET_FRIEND_OBJ_BY_ID,[partyMem.id]);
               if(friend != null)
               {
                  friendInfo.isInParty = false;
                  this.getSyncDP(SyncDataID.FRIEND_STATUS_CHANGE,[friend,friendInfo]);
               }
               this.sendSystemNotification(partyMem.isKicked ? LocalizationParty.NOTE_PARTY_MEMBER_KICKED : LocalizationParty.NOTE_PARTY_MEMBER_LEFT,[partyMem.name]);
            }
            if(partyMem.isHost)
            {
               lastHost = partyMem;
            }
         }
         for(i = 0; i < this._partyMembers.length; i++)
         {
            if(lastPartyMembers.length > 0 && !DataUtil.isInArray(this._partyMembers[i].id,lastPartyMembers))
            {
               friend = this.getSyncDP(SyncDataID.GET_FRIEND_OBJ_BY_ID,[partyMem.id]);
               if(friend != null)
               {
                  friendInfo.isInParty = true;
                  this.getSyncDP(SyncDataID.FRIEND_STATUS_CHANGE,[FriendObj,friendInfo]);
               }
               this.sendSystemNotification(LocalizationParty.NOTE_PARTY_NEW_MEMBER,[this._partyMembers[i].name]);
            }
            if(Boolean(this._partyMembers[i].isHost))
            {
               newHost = this._partyMembers[i];
            }
         }
         if(newHost != null && (lastHost == null || newHost.name != lastHost.name))
         {
            if(lastPartyMembers.length > 0)
            {
               this.sendSystemNotification(LocalizationParty.NOTE_PARTY_LEADER_CHANGED,[newHost.name]);
            }
         }
         lastPartyMembers = null;
      }
      
      private function bind_partyInfo(cRes:Object) : void
      {
         var type:String = null;
         if(cRes == null || cRes.data == null)
         {
            return;
         }
         if(this._partyInfo.id != "")
         {
            if(this._partyInfo.description != cRes.data.description)
            {
               this.sendSystemNotification(LocalizationParty.NOTE_PARTY_SETTING_CHANGED,[LocalizeUtil.localize(LocalizationParty.PARTY_DESCRIPTION),cRes.data.description]);
            }
            if(this._partyInfo.size != cRes.data.size)
            {
               this.sendSystemNotification(LocalizationParty.NOTE_PARTY_SETTING_CHANGED,[LocalizeUtil.localize(LocalizationParty.PARTY_SIZE),cRes.data.size.toString()]);
            }
            if(this._partyInfo.type != cRes.data.type)
            {
               type = cRes.data.type == PartyType.PRIVATE ? LocalizationParty.PARTY_TYPE_PRIVATE : LocalizationParty.PARTY_TYPE_PUBLIC;
               this.sendSystemNotification(LocalizationParty.NOTE_PARTY_SETTING_CHANGED,[LocalizeUtil.localize(LocalizationParty.PARTY_TYPE),LocalizeUtil.localize(type)]);
            }
         }
         this._partyInfo.id = cRes.data.id;
         this._partyInfo.description = cRes.data.description;
         this._partyInfo.size = cRes.data.size;
         this._partyInfo.type = cRes.data.type;
         this.setBindData(AsyncTaskID.PARTY_INFO,this._partyInfo);
      }
      
      private function bind_onPartyLeft(cRes:Object) : void
      {
         PartyService.getInstance().isInParty = false;
         this._partyMembers.splice(0);
         this._partyInfoPlaceholder = null;
         this.broadcast(BCEvent.PARTY_LEFT);
         this.onPartyStateChanged();
         if(Boolean(cRes.data))
         {
            this.showMsg(LocalizationParty.NOTE_YOU_ARE_KICKED_FROM_PARTY,MsgBtnCount.ONE);
         }
         if(cRes.resultMsg != null && cRes.resultMsg.length > 0)
         {
            this.showMsg(cRes.resultMsg,MsgBtnCount.ONE);
         }
      }
      
      private function bind_onPartyStartToJoinGame(cRes:Object) : void
      {
         if(cRes.data.type == RoomListType.RANKED)
         {
            var playlistObj:Object = this.getSyncDP(SyncDataID.PLAY_LIST_OBJ,[cRes.data.playListID]);
            if(playlistObj != null && playlistObj.name == "Quick Match" && playlistObj.type == 1 && playlistObj.minPartySize == 1 && playlistObj.maxPartySize == 6 && playlistObj.maxRoomSize == 12)
            {
               if(this.isPartyHost())
               {
                  return;
               }
               this.setReadyState({"ready":false});
            }
         }
         switch(cRes.data.type)
         {
            case RoomListType.MP:
               this._matchType = LocalizeUtil.localize(LocalizationRoom.LEAGUE_COMBAT_TRAINING);
               this._matchMap = MatchService.getInstance().getPlayListName(cRes.data.playListID);
               this._matchDiff = LocalizeUtil.localize(LocalizationRoom.LEAGUES[cRes.data.league]);
               break;
            case RoomListType.RANKED:
               this._matchType = LocalizeUtil.localize(LocalizationRoom.AUTO_MATCH_MP);
               this._matchMap = MatchService.getInstance().getPlayListName(cRes.data.playListID);
               this._matchDiff = LocalizeUtil.localize(LocalizationRoom.LEAGUES[cRes.data.league]);
               break;
            case RoomListType.COOP:
               this._matchType = LocalizeUtil.localize(LocalizationRoom.LEAGUE_COOP);
               this._matchMap = LocalizeUtil.localize(MatchService.getInstance().getMapName(cRes.data.map));
               this._matchDiff = LocalizeUtil.localize(LocalizationRoom.ROOM_LEVELS[cRes.data.league]);
         }
         this.startPartyWillJoinGameTimer();
      }
      
      private function bind_onPartyEveryoneIsReady(cRes:Object) : void
      {
         if(this.isPartyHost())
         {
            this.hideMsg();
            this.stopPartyWillJoinGameTimer();
            this.broadcast(BCEvent.PARTY_EVERYONE_IS_READY);
         }
      }
      
      private function onPartyToJoinGameTimerHandle(cEvt:TimerEvent = null) : void
      {
         var msg:String = null;
         var state:String = null;
         var isReady:Boolean = false;
         var label0:String = null;
         var label1:String = null;
         if(this._partyToJoinGameTime < PARTY_TO_JOIN_GAME_TIMER)
         {
            this.hideMsg();
         }
         var members:String = "\n";
         var timer:int = this._partyToJoinGameTime--;
         Logger.print("PartyController::onPartyToJoinGameTimerHandle timer: " + timer);
         for(var i:int = 0; i < this._partyMembers.length; i++)
         {
            isReady = Boolean(this._partyMembers[i].isReady);
            state = LocalizeUtil.localize(isReady ? LocalizationParty.PARTY_MEMBER_READY : LocalizationParty.PARTY_MEMBER_WAIT);
            state = (Boolean(this._partyMembers[i].isReady) ? "<FONT COLOR=\'#00FF00\'>" : "<FONT COLOR=\'#FF0000\'>") + state + "</FONT>";
            members += this._partyMembers[i].name + ": " + state + "\t";
         }
         if(this.isPartyHost())
         {
            msg = LocalizeUtil.param(LocalizationParty.NOTE_PARTY_WAITING_OTHERS,[timer.toString()]);
            this.showMsg(msg + members);
         }
         else
         {
            msg = LocalizeUtil.param(LocalizationParty.NOTE_PARTY_WILL_JOIN_GAME,[this._matchType,this._matchMap,this._matchDiff]);
            label0 = LocalizeUtil.localize(LocalizationSystem.ACCEPT) + "[" + timer + "]";
            label1 = LocalizationParty.LEAVE_PARTY;
            this.showMsg(msg + members,MsgBtnCount.TWO,0,this.acceptToJoinGame,label0,this.refuseToJoinGame,label1);
         }
      }
      
      private function onPartyToJoinGameTimerCompleteHandle(cEvt:TimerEvent) : void
      {
         this.hideMsg();
         if(!this.isPartyHost())
         {
            this.leaveParty();
         }
      }
      
      private function onKickPartyMemberHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainer.KickPartyMember(cEvt.myPara.id);
      }
      
      private function onPromotePartyMemberHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainer.PromotePartyMember(cEvt.myPara.id);
      }
      
      private function onProposePartyMembershipHandle(cEvt:ParaEvent) : void
      {
         if(this.isPartyMembersMax())
         {
            this.showMsg(LocalizationParty.NOTE_PARTY_MAX_MEMBERS,MsgBtnCount.ONE);
            return;
         }
         CFunctionContainer.ProposePartyMembership(cEvt.myPara.id);
         this.sendSystemNotification(LocalizationParty.NOTE_PROPOSE_PARTY_NEW_MEMBER,[cEvt.myPara.name]);
      }
      
      private function onRejectPartyMembershipHandle(cEvt:ParaEvent) : void
      {
         CFunctionContainer.RejectPartyMembership(cEvt.myPara.senderID);
      }
      
      private function onTryShowInviteListHandle(cEvt:ParaEvent) : void
      {
         this.broadcast(BCEvent.TRY_OPEN_FRIENDLIST,{"expand":true});
      }
      
      private function onCreatePartyHandle(cEvt:ParaEvent) : void
      {
         if(this.getSyncDP(SyncDataID.HAS_MP_CHARACTER))
         {
            this._partyInfoPlaceholder = new PartyObj(cEvt.myPara);
            if(this.isInParty())
            {
               return;
            }
            this.sanitizePartyInfo();
         }
         else
         {
            this.broadcast(BCEvent.EVT_PROMPT_CHARACTER_CREATION);
         }
      }
      
      private function onModifyPartyHandle(cEvt:ParaEvent) : void
      {
         if(this.isInParty())
         {
            this._partyInfoPlaceholder = new PartyObj(cEvt.myPara);
            this._partyInfoPlaceholder.description = this._partyInfo.description;
            if(this._partyInfoPlaceholder.size != this._partyInfo.size && this._partyInfoPlaceholder.size < this._partyMembers.length)
            {
               this.showMsg(LocalizeUtil.param(LocalizationParty.NOTE_PARTY_SMALLER_THAN_MEMBERS,[this._partyMembers.length,this._partyInfoPlaceholder.size]),MsgBtnCount.ONE);
               return;
            }
            if(this._partyInfoPlaceholder.size != this._partyInfo.size || this._partyInfoPlaceholder.type != this._partyInfo.type)
            {
               this.setPartyInfo();
            }
         }
      }
      
      private function onJoinPartyHandle(cEvt:ParaEvent) : void
      {
         if(this.getSyncDP(SyncDataID.IS_IN_ROOM))
         {
            this.showMsg(this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH) ? LocalizationRoom.NOTE_ALREADY_IN_RANKED_MATCH : LocalizationRoom.NOTE_ALREADY_IN_ROOM,MsgBtnCount.ONE);
            return;
         }
         this.request(AsyncTaskID.JOIN_PARTY,cEvt.myPara,this.onPartyJoined);
      }
      
      private function onLeavePartyHandle(cEvt:ParaEvent) : void
      {
         this.showMsg(LocalizationParty.LEAVE_PARTY_CONFIRM,MsgBtnCount.TWO,0,this.leaveParty,LocalizationSystem.CONFIRM,null,LocalizationSystem.CANCEL);
      }
      
      private function onSetPartyReadyStateHandle(cEvt:ParaEvent) : void
      {
         var localFilter:Object = cEvt.myPara.filter;
         if(localFilter != null && localFilter["_localQuickPartyReady"] == "cancelled" && cEvt.myPara.ready == false)
         {
            this.hideMsg();
            this.stopPartyWillJoinGameTimer();
            this.setReadyState({"ready":false});
            return;
         }
         var msg:String = null;
         if(this.isPartyHost())
         {
            msg = LocalizeUtil.localize(LocalizationParty.NOTE_PARTY_WAITING_OTHERS);
            msg = msg.substring(0,msg.length - 5);
            if(localFilter != null && localFilter["_localQuickPartyReady"] == "pending" && localFilter["_localQuickPartyCancel"] is Function)
            {
               this.showMsg(msg,MsgBtnCount.ONE,0,localFilter["_localQuickPartyCancel"],LocalizationSystem.CANCEL);
            }
            else
            {
               this.showMsg(msg);
            }
         }
         this.setReadyState(cEvt.myPara);
      }
      
      private function onPlayerStatsReadyHandle(cEvt:ParaEvent) : void
      {
         if(this.isInParty())
         {
            if(this._partyMembers.length <= 0)
            {
               CFunctionContainer.RefreshPartyMember();
            }
            if(this._partyInfo.size <= 0)
            {
               CFunctionContainer.RefreshPartyInfo();
            }
            this.broadcast(BCEvent.PARTY_JOINED);
         }
      }
      
      private function setPartyInfo() : void
      {
         CFunctionContainer.SetPartyInfo(this._partyInfoPlaceholder);
         this.broadcast(BCEvent.SHOW_PARTY_LIST,{"expand":true});
      }
      
      private function sanitizePartyInfo() : void
      {
         if(this._partyInfoPlaceholder.description.length > 0)
         {
            this.broadcast(BCEventCommon.EVT_SANITIZE_STRING,{
               "type":SanitizedType.PARTY_INFO,
               "content":this._partyInfoPlaceholder.description
            });
         }
         else
         {
            this.createParty();
         }
      }
      
      private function startPartyWillJoinGameTimer() : void
      {
         this._partyToJoinGameTime = PARTY_TO_JOIN_GAME_TIMER;
         this._partyToJoinGameTimer.reset();
         this._partyToJoinGameTimer.start();
      }
      
      private function stopPartyWillJoinGameTimer() : void
      {
         this._partyToJoinGameTime = 0;
         this._partyToJoinGameTimer.stop();
      }
      
      private function acceptToJoinGame() : void
      {
         this.stopPartyWillJoinGameTimer();
         this.setReadyState({"ready":true});
      }
      
      private function refuseToJoinGame() : void
      {
         this.stopPartyWillJoinGameTimer();
         this.leaveParty();
      }
      
      private function setReadyState(obj:Object) : void
      {
         CFunctionContainer.SetPartyReadyState(obj);
      }
      
      private function leaveParty() : void
      {
         if(this.getSyncDP(SyncDataID.CURRENT_RANKED_MATCH_STATUS) != RankedMatchStatus.IDLE)
         {
            this.broadcast(BCEvent.EVT_LEAVE_ROOM);
         }
         else
         {
            this.request(AsyncTaskID.LEAVE_PARTY);
         }
         CFunctionContainer.RemoveReservedChatChannel(ChattingChannelType.PARTY,ChattingChannelType.PARTY.toString(),"");
      }
      
      private function sendSystemNotification(msg:String, param:Array = null) : void
      {
         if(this.getSyncDP(SyncDataID.FETCH_RESERVED_MSG_FINISHED) == false)
         {
            return;
         }
         var msgObj:ChatMessageObj = new ChatMessageObj();
         msgObj.channelID = ChattingChannelType.PARTY;
         msgObj.senderName = LocalizeUtil.localize(LocalizationAAS.AAS_SYSTEM);
         msgObj.isNew = true;
         if(param != null && param.length != 0)
         {
            msgObj.message = LocalizeUtil.param(msg,param);
         }
         else
         {
            msgObj.message = msg;
         }
         msgObj.isSystem = true;
         this.broadcast(BCEvent.EVT_CHAT_MSG_FROM_SYSTEM,msgObj);
      }
      
      private function isInParty() : Boolean
      {
         return PartyService.getInstance().isInParty;
      }
      
      private function isPartyHost() : Boolean
      {
         return this.isInParty() && PartyService.getInstance().isPartyHost;
      }
      
      private function isInMyParty(cUser:Object) : Boolean
      {
         return DataUtil.isInArray(cUser.id,this._partyMembers);
      }
      
      private function isPartyMembersMax() : Boolean
      {
         return this._partyMembers.length >= this._partyInfo.size;
      }
      
      private function arePartyBootCamp() : Boolean
      {
         var right:Boolean = true;
         for(var i:int = 0; i < this._partyMembers.length; i++)
         {
            if(this._partyMembers[i].level > Common.BOOT_CAMP_THRESHOLD)
            {
               right = false;
               break;
            }
         }
         return right;
      }
      
      private function getPartySize() : Array
      {
         if(this._partySizeDP.length <= 0)
         {
            this._partySizeDP.push({
               "label":2,
               "id":2
            });
            this._partySizeDP.push({
               "label":4,
               "id":4
            });
            this._partySizeDP.push({
               "label":6,
               "id":6
            });
            this._partySizeDP.push({
               "label":8,
               "id":8
            });
            this._partySizeDP.push({
               "label":9,
               "id":9
            });
         }
         return this._partySizeDP;
      }
      
      private function getPartyType() : Array
      {
         if(this._partyTypeDP.length <= 0)
         {
            this._partyTypeDP.push({
               "label":LocalizationParty.PARTY_TYPE_PRIVATE,
               "id":PartyType.PRIVATE
            });
            this._partyTypeDP.push({
               "label":LocalizationParty.PARTY_TYPE_PUBLIC,
               "id":PartyType.PUBLIC
            });
         }
         return this._partyTypeDP;
      }
      
      private function getPartyInfo() : Object
      {
         if(!this.isInParty())
         {
            return null;
         }
         this._partyInfo.idxSize = DataUtil.indexInArray(this._partyInfo.size,this._partySizeDP);
         this._partyInfo.idxType = DataUtil.indexInArray(this._partyInfo.type,this._partyTypeDP);
         return this._partyInfo;
      }
      
      private function getPartyMembers() : Array
      {
         return this._partyMembers;
      }
      
      private function onPartyJoined(cRes:Object) : void
      {
         var partyInfo:Object = null;
         var success:Boolean = cRes.result == ResultObj.RESULT_SUCCESS;
         PartyService.getInstance().isInParty = success;
         if(success)
         {
            partyInfo = this.getPartyInfo();
            this.broadcast(BCEvent.PARTY_JOINED,cRes);
            this.sendSystemNotification(LocalizeUtil.localize(LocalizationParty.PARTY_DESCRIPTION) + " " + partyInfo.description);
            this.onPartyStateChanged();
            if(!ScaleformUtil.isScaleform)
            {
               this.bind_partyMemberInfo(cRes);
            }
            this.broadcast(BCEvent.SHOW_PARTY_LIST,{"expand":true});
            CFunctionContainer.ReserveChatChannel(ChattingChannelType.PARTY,ChattingChannelType.PARTY.toString().toString(),"");
         }
      }
      
      private function onPartyStateChanged() : void
      {
         this.broadcast(BCEvent.PARTY_STATE_CHANGED);
      }
      
      private function onTryOpenPartyHandle(cEvt:ParaEvent) : void
      {
         if(!this.isInParty())
         {
            if(!this.getSyncDP(SyncDataID.IS_IN_ROOM) && !this.getSyncDP(SyncDataID.IS_IN_RANKED_MATCH))
            {
               this.broadcast(BCEvent.SHOW_PARTY_CREATOR);
            }
         }
         else
         {
            this.broadcast(BCEvent.SHOW_PARTY_LIST);
         }
      }
      
      private function onPartyInfoSanitizedHandle(cEvt:ParaEvent) : void
      {
         if(cEvt.myPara.type != SanitizedType.PARTY_INFO)
         {
            return;
         }
         if(Boolean(cEvt.myPara.sanitized))
         {
            this.broadcast(BCEvent.PARTY_DESC_SANITIZED,cEvt.myPara.content);
            this.showMsg(LocalizationParty.NOTE_PARTY_DESC_SANITIZED,MsgBtnCount.ONE);
            return;
         }
         if(this.isInParty())
         {
            this.prevPage();
            this.setPartyInfo();
         }
         else
         {
            this.createParty();
         }
      }
      
      private function createParty() : void
      {
         if(!this.isInParty())
         {
            this.request(AsyncTaskID.CREATE_PARTY,this._partyInfoPlaceholder,this.onPartyJoined);
         }
      }
      
      private function fillPartyMemberObj(dest:PartyMemberObj, src:Object) : void
      {
         if(dest == null || src == null)
         {
            return;
         }
         dest.name = SharedUtil.FormatNameByTag(src.tag,src.name);
         dest.id = src.id || "";
         dest.isOnline = true;
         dest.isHost = Boolean(src.isHost);
         dest.isReady = Boolean(src.isReady);
         dest.level = int(src.level) || 0;
         dest.isKicked = Boolean(src.isKicked);
         if(src.badgeObj != null)
         {
            dest.badgeObj.id = uint(src.badgeObj.id) || 0;
            dest.badgeObj.name = src.badgeObj.name || "";
            dest.badgeObj.description = src.badgeObj.description || "";
            dest.badgeObj.imageURL = src.badgeObj.imageURL || "";
         }
      }
      
      private function isMe(cUser:Object) : Boolean
      {
         return this.getSyncDP(SyncDataID.IS_ME,[cUser]);
      }
      
      private function onInviteUsersToPartyHandle(cEvt:ParaEvent) : void
      {
         var tempUsersAry:Array = cEvt.myPara.users;
         for(var i:uint = 0; i < tempUsersAry.length; i++)
         {
            if(this.isInParty() && this.getSyncDP(SyncDataID.PARTY_CAN_INVITE_TO_PARTY_BY_USEROBJ,[tempUsersAry[i]]))
            {
               this.broadcast(BCEvent.EVT_PROPOSE_PARTY_MEMBERSHIP,tempUsersAry[i]);
            }
         }
      }
   }
}

