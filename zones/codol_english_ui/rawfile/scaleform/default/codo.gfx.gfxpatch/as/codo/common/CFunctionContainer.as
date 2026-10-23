package codo.common
{
   import codo.objects.CareerStatsObj;
   import codo.objects.ChatMessageObj;
   import codo.objects.MailObj;
   import codo.utils.ScaleformUtil;
   
   public dynamic class CFunctionContainer
   {
      
      public static var IsPlayerStatsReady:Function;
      
      public static var SavePlayerStats:Function;
      
      public static var CL_ResetTryonList:Function;
      
      public static var CL_TryonItem:Function;
      
      public static var CL_TakeoffItem:Function;
      
      public static var CL_GetTryonItemID:Function;
      
      public static var CL_GetActiveCharacterLoadout:Function;
      
      public static var CL_SetDefaultCharacter:Function;
      
      public static var CL_SetCurrentLoadout:Function;
      
      public static var CL_LeaveCharacterPage:Function;
      
      public static var CL_EnterCharacterPage:Function;
      
      public static var CL_InitCharacterPage:Function;
      
      public static var CL_GetCharacters:Function;
      
      public static var CL_GetHeadFaction:Function;
      
      public static var CL_PreviewCharacter:Function;
      
      public static var AS_CheckAvatarRefreshInTeam:Function;
      
      public static var CL_IsCharacterCustomizable:Function;
      
      public static var Callsign_HandleTryonItems:Function;
      
      public static var Callsign_CheckExpire:Function;
      
      public static var Callsign_TryonTitle:Function;
      
      public static var Callsign_GetActiveCallsignTitle:Function;
      
      public static var Callsign_TryonBackground:Function;
      
      public static var Callsign_GetActiveCallsignBackground:Function;
      
      public static var Callsign_TryonEmblem:Function;
      
      public static var Callsign_GetActiveCallsignEmblem:Function;
      
      public static var Callsign_TryonBadge:Function;
      
      public static var Callsign_GetActiveCallsignBadge:Function;
      
      public static var GetCareerCallsign:Function;
      
      public static var GetCareerBadge:Function;
      
      public static var Callsign_UnequipBadge:Function;
      
      public static var UIViewer_SetGender:Function;
      
      public static var UIViewer_SetFaction:Function;
      
      public static var UIViewer_SetTerrain:Function;
      
      public static var AS_UIViewerSet3DSceneVisibility:Function;
      
      public static var UIViewer_GetTerrain:Function;
      
      public static var AS_GetTerrainForMap:Function;
      
      public static var WL_ResetTryonList:Function;
      
      public static var WL_TryonWeapon:Function;
      
      public static var WL_EquipWeaponAttachment:Function;
      
      public static var WL_TakeoffWeaponSlot:Function;
      
      public static var WL_ResetWeaponRotation:Function;
      
      public static var WL_GetUnlockedMPLoadoutNum:Function;
      
      public static var WL_SetCurrentWeaponLoadoutIndex:Function;
      
      public static var WL_GetCurrentWeaponLoadoutIndex:Function;
      
      public static var WL_SetIsWeaponZoomIn:Function;
      
      public static var WL_GetIsWeaponZoomIn:Function;
      
      public static var WL_SetCurrentSlot:Function;
      
      public static var WL_SetCurrentType:Function;
      
      public static var WL_IsWeaponSlotUnlocked:Function;
      
      public static var WL_GetCurrentWeaponID:Function;
      
      public static var WL_SetCurrentWeaponID:Function;
      
      public static var WL_GetWeaponCareerInfo:Function;
      
      public static var AS_GetSummaryParams:Function;
      
      public static var AS_GetWeaponSlotEnabled:Function;
      
      public static var AS_SavePlayerProfile:Function;
      
      public static var AS_GetItemList:Function;
      
      public static var AS_GetCloseToExpireItemList:Function;
      
      public static var AS_ScanForPromptingExpiration:Function;
      
      public static var WL_TryAbility:Function;
      
      public static var MKT_BuyItem:Function;
      
      public static var MKT_BuyRecentExpiredItems:Function;
      
      public static var MKT_AddToCart:Function;
      
      public static var MKT_RemoveFromCart:Function;
      
      public static var MKT_BuyCart:Function;
      
      public static var MKT_GetCartItemList:Function;
      
      public static var MKT_TryItem:Function;
      
      public static var MKT_SwapCartSKU:Function;
      
      public static var MKT_ClearShoppingCart:Function;
      
      public static var MKT_UpdateInventory:Function;
      
      public static var MKT_PurchaseGift:Function;
      
      public static var MKT_ClaimCoupon:Function;
      
      public static var MKT_TransferCoupon:Function;
      
      public static var MKT_RedeemCoupon:Function;
      
      public static var MKT_RefreshPendingCoupon:Function;
      
      public static var MKT_RefreshCoupon:Function;
      
      public static var MKT_RequestAccountInfo:Function;
      
      public static var MKT_RequestMarketFilters:Function;
      
      public static var MKT_SetMarketFilter:Function;
      
      public static var MKT_GetRenewAllObject:Function;
      
      public static var AS_SetHeadMarketFilter:Function;
      
      public static var AS_CreateUserName:Function;
      
      public static var AS_IsFirstLogin:Function;
      
      public static var AS_FirstTimeInCharacterCreation:Function;
      
      public static var AS_SetFactionInCharacterCreation:Function;
      
      public static var AS_SetChoiceInCharacterCreation:Function;
      
      public static var AS_ConfirmCharacterCreation:Function;
      
      public static var AS_GetCharacterOption:Function;
      
      public static var AS_ShowCharacterCreationSelection:Function;
      
      public static var AS_GetCharacterCreationStep:Function;
      
      public static var AS_SetCharacterCreationStep:Function;
      
      public static var AS_CancelCurrentCharacter:Function;
      
      public static var HasCharacter:Function;
      
      public static var HaveZombieCharacter:Function;
      
      public static var AS_NeedQuestionnaire:Function;
      
      public static var AS_GetQuestionnaire:Function;
      
      public static var AS_ParseQuestionnaire:Function;
      
      public static var AS_ReplyQuestionnaire:Function;
      
      public static var AS_OpenUserReportQuestionnaire:Function;
      
      public static var AS_CloseUserReportQuestionnaire:Function;
      
      public static var AS_QuestionnaireExists:Function;
      
      public static var AS_LoginSummaryViewed:Function;
      
      public static var AS_ShowLoginSummary:Function;
      
      public static var ListClan:Function;
      
      public static var CreateClan:Function;
      
      public static var SetClanTag:Function;
      
      public static var RefreshClan:Function;
      
      public static var RenameClan:Function;
      
      public static var LeaveClan:Function;
      
      public static var PromoteClanMember:Function;
      
      public static var DemoteClanMember:Function;
      
      public static var KickClanMember:Function;
      
      public static var ProposeClanMembership:Function;
      
      public static var WithdrawClanProposal:Function;
      
      public static var AcceptClanMembership:Function;
      
      public static var RejectClanMembership:Function;
      
      public static var SetClanProfile:Function;
      
      public static var SetClanMemberProfile:Function;
      
      public static var FetchClanInfo:Function;
      
      public static var GetClanInfo:Function;
      
      public static var ApplyToJoinClan:Function;
      
      public static var IsInClan:Function;
      
      public static var CreateParty:Function;
      
      public static var JoinParty:Function;
      
      public static var LeaveParty:Function;
      
      public static var KickPartyMember:Function;
      
      public static var PromotePartyMember:Function;
      
      public static var SetPartyInfo:Function;
      
      public static var SetPartyReadyState:Function;
      
      public static var ProposePartyMembership:Function;
      
      public static var RejectPartyMembership:Function;
      
      public static var IsPartyHost:Function;
      
      public static var IsInParty:Function;
      
      public static var GetNumOfPartyMembers:Function;
      
      public static var RefreshPartyMember:Function;
      
      public static var RefreshPartyInfo:Function;
      
      public static var TencentSanitizeString:Function;
      
      public static var TencentClaimRewards:Function;
      
      public static var TencentGetAASRecord:Function;
      
      public static var UILog:Function;
      
      public static var JoinChannel:Function;
      
      public static var JoinNewbieChannel:Function;
      
      public static var LeaveNewbieChannel:Function;
      
      public static var ListRoom:Function;
      
      public static var CreateRoom:Function;
      
      public static var JoinRoom:Function;
      
      public static var StartMatch:Function;
      
      public static var StartMatchCountDown:Function;
      
      public static var SetReadyState:Function;
      
      public static var GetReadyState:Function;
      
      public static var ChangeTeam:Function;
      
      public static var ChangeRoomInfo:Function;
      
      public static var GetRoomInfo:Function;
      
      public static var CancelList:Function;
      
      public static var RejectRoomMembership:Function;
      
      public static var QuickMatch:Function;
      
      public static var JumpGame:Function;
      
      public static var SetStatsForRoom:Function;
      
      public static var QueryRoomTeammateStats:Function;
      
      public static var GetGroupCounts:Function;
      
      public static var IsPilot:Function;
      
      public static var SetPilot:Function;
      
      public static var VoteMap:Function;
      
      public static var SetFinalVoting:Function;
      
      public static var IsLogOnLobby:Function;
      
      public static var IsRoomHost:Function;
      
      public static var EnableVoiceTalk:Function;
      
      public static var ListDedicatedServer:Function;
      
      public static var RefreshInRoom:Function;
      
      public static var IsRankedMatch:Function;
      
      public static var IsRoomFull:Function;
      
      public static var GetRankedMatchState:Function;
      
      public static var ReserveChatChannel:Function;
      
      public static var RemoveReservedChatChannel:Function;
      
      public static var FetchReservedChatChannel:Function;
      
      public static var FetchReservedChatMessage:Function;
      
      public static var SendChatMessage:Function;
      
      public static var ReserveChatMessage:Function;
      
      public static var SendTeamInstantMessage:Function;
      
      public static var SendGlobalInstantMessage:Function;
      
      public static var SendMail:Function;
      
      public static var FetchMailHeaders:Function;
      
      public static var FetchMailBodies:Function;
      
      public static var DeleteMails:Function;
      
      public static var FetchClanMailHeaders:Function;
      
      public static var FetchClanMailBodies:Function;
      
      public static var DeleteClanMails:Function;
      
      public static var GetAndClearMailPendingFlag:Function;
      
      public static var EnableRoomListEvent:Function;
      
      public static var EnableInRoomEvent:Function;
      
      public static var ListContact:Function;
      
      public static var ListFriendProposal:Function;
      
      public static var AddFriendByName:Function;
      
      public static var AddFriendByID:Function;
      
      public static var RemoveFriend:Function;
      
      public static var AcceptFriendProposal:Function;
      
      public static var RejectFriendProposal:Function;
      
      public static var WithdrawFriendshipProposal:Function;
      
      public static var ListBlackList:Function;
      
      public static var AddToBlackList:Function;
      
      public static var RemoveFromBlackList:Function;
      
      public static var IsInBlackList:Function;
      
      public static var GetPlayList:Function;
      
      public static var AS_PlayListUpdated:Function;
      
      public static var AS_EnablePublisherFilesUpdate:Function;
      
      public static var ListLeaderboard:Function;
      
      public static var AS_GetRecentMatchStats:Function;
      
      public static var AS_GetScoreBoard_Main:Function;
      
      public static var AS_GetScoreBoard_MapType_Main:Function;
      
      public static var AS_GetObjectives_Main:Function;
      
      public static var AS_GetWeaponLevelup:Function;
      
      public static var AS_GetRecentUnlockedItems:Function;
      
      public static var AS_CleanRecentUnlockItem:Function;
      
      public static var ListLevelUpUnlockObj:Function;
      
      public static var FetchIsPlayerLeveledUp:Function;
      
      public static var DismissChallenge:Function;
      
      public static var FetchRecentChallenge:Function;
      
      public static var SetRecommendedSetting:Function;
      
      public static var SetVoiceMicMute:Function;
      
      public static var IsSubtitleToggled:Function;
      
      public static var ToggleSubtitle:Function;
      
      public static var GetGammaRange:Function;
      
      public static var GetSensitivityRange:Function;
      
      public static var GetFOVRange:Function;
      
      public static var ConvertPosUIToScreen:Function;
      
      public static var IsMapUnlocked:Function;
      
      public static var IsCyborgTutorialComplete:Function;
      
      public static var FetchInvitation:Function;
      
      public static var RemoveInvitation:Function;
      
      public static var IsTCLSLogOn:Function;
      
      public static var GetCyborgCharacterLoadout:Function;
      
      public static var GetCyborgTokenCount:Function;
      
      public static var GetLockedMapsFromPublisher:Function;
      
      // Decompilation skipped
      
      public function CFunctionContainer()
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

