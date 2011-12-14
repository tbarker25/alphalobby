#pragma once

void PollServer(void);
void Connect(void (*onFinish)(void));
void Disconnect(void);
void SendToServer(const char *format, ...);
// char * GetLocalIP(void);
