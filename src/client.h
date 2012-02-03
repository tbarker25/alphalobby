#pragma once

void PollServer(void);
void Connect(void (*onFinish)(void));
void Disconnect(void);
void SendToServer(const char *format, ...);
// char * GetLocalIP(void);

enum ConnectionState {
	CONNECTION_OFFLINE,
	CONNECTION_CONNECTING,
	CONNECTION_ONLINE,
};
enum ConnectionState GetConnectionState(void);
