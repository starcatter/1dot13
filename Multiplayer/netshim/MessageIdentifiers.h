// netshim: RakNet 3.401 message identifiers -- exact 3.401 numeric values for the
// IDs the wrapper switches on (both peers run the shim, but fidelity is cheap).
#pragma once
enum
{
	ID_CONNECTION_REQUEST_ACCEPTED = 14,
	ID_CONNECTION_ATTEMPT_FAILED = 15,
	ID_ALREADY_CONNECTED = 16,
	ID_NEW_INCOMING_CONNECTION = 17,
	ID_NO_FREE_INCOMING_CONNECTIONS = 18,
	ID_DISCONNECTION_NOTIFICATION = 19,
	ID_CONNECTION_LOST = 20,
	ID_MODIFIED_PACKET = 24,
	ID_TIMESTAMP = 25,             // never emitted by the shim (LP64 offset bug in the wrapper)
	ID_REMOTE_DISCONNECTION_NOTIFICATION = 28,
	ID_REMOTE_CONNECTION_LOST = 29,
	ID_REMOTE_NEW_INCOMING_CONNECTION = 30,
	ID_USER_PACKET_ENUM = 95
};
