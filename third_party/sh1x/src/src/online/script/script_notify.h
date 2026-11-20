#ifndef SCRIPT_NOTIFY_H
#define SCRIPT_NOTIFY_H
#ifdef __cplusplus
extern "C" {
#endif
void ClearPlayerCommandNotifications(unsigned client);
void ClearCommandNotifications(void);
void HandleActionNotify(unsigned client, const char *action_id);
int IsActionNotifyCommand(const char *command);
#ifdef __cplusplus
}
#endif
#endif
