#define BUILDING_NODE_EXTENSION
#define BUFSIZE 4096

#include <node.h>
#include <v8.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace v8;

void throwError(const char *errorMessage) {
	ThrowException(Exception::Error(String::New(errorMessage)));
}

void throwTypeError(const char *errorMessage) {
	ThrowException(Exception::TypeError(String::New(errorMessage)));
}

int parseArgs(const Arguments& args, int* pPort, char** pHost) {
	if (args.Length() < 1) {
		throwTypeError("port is not specified");
		return -1;
	}
	if (!args[0]->IsNumber()) {
		throwTypeError("port is not a number");
		return -1;
	}
	
	*pPort = args[0]->NumberValue();

	if (args.Length() >= 2) {
		if (!args[1]->IsString()) {
			throwTypeError("host is not a string");
			return -1;
		}
		Local<String> hostStr = args[1]->ToString();
		int hostLen = hostStr->Length();
		*pHost = (char*)malloc(hostLen + 1);
		hostStr->WriteAscii(*pHost, 0, hostLen + 1, 0);
	} else {
		// host is not specified, use default value.
		*pHost = "localhost";
	}

	return 0;
}

// Params: port, host[optional]
// Returns the socket for the connection.
Handle<Value> connectImpl(const Arguments& args) {
	HandleScope scope;

	int port;
	char* host;
	if (parseArgs(args, &port, &host) < 0) {
		return scope.Close(Undefined());
	}
	
	int sock, rv;
	struct addrinfo hints, *servinfo, *p;
	char portBuf[NI_MAXSERV];
	char errMsg[64];
	
	snprintf(portBuf, sizeof(portBuf), "%d", port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(host, portBuf, &hints, &servinfo)) != 0) {
		snprintf(errMsg, 64, "getaddrinfo: %s\n", gai_strerror(rv));
		throwError(errMsg);
		return scope.Close(Undefined());
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			continue;
		}
		if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
			close(sock);
			continue;
		}
		break;
	}
	if (p == NULL) {
		throwError("connect() failed");
		return scope.Close(Undefined());
	}

	freeaddrinfo(servinfo);
	
	return scope.Close(Number::New(sock));
}

// Params: socket
// Returns the received string.
Handle<Value> readImpl(const Arguments& args) {
	HandleScope scope;

	int sock, nBytes;
	char buf[BUFSIZE];
	
	if (args.Length() < 1) {
		throwTypeError("socket is not specified");
		return scope.Close(Undefined());
	}
	if (!args[0]->IsNumber()) {
		throwTypeError("socket is not a number");
		return scope.Close(Undefined());
	}

	sock = args[0]->NumberValue();
	nBytes = recv(sock, buf, BUFSIZE-1, 0);
	if (nBytes <= 0) {
		throwError("recv() failed");
		return scope.Close(Undefined());
	} else {
		buf[nBytes] = '\0';
		return scope.Close(String::New(buf));
	}
}

// Params: socket, data
// Returns the number of bytes sent.
Handle<Value> writeImpl(const Arguments& args) {
	HandleScope scope;

	int sock;
	Local<String> data;
	char buf[BUFSIZE];
	int start = 0, remaining, batch;
	int nBytes = 0, nBatch;
	
	if (args.Length() < 1) {
		throwTypeError("socket and data are not specified");
		return scope.Close(Undefined());
	} else if (args.Length() < 2) {
		throwTypeError("data is not specified");
		return scope.Close(Undefined());
	}
	if (!args[0]->IsNumber()) {
		throwTypeError("socket is not a number");
		return scope.Close(Undefined());
	}
	if (!args[1]->IsString()) {
		throwTypeError("data is not a string");
		return scope.Close(Undefined());
	}

	sock = args[0]->NumberValue();
	data = args[1]->ToString();
	remaining = data->Length();
	
	// In case the data is larger than our buffer, we write them in batches
	while (remaining > 0) {
		batch = remaining <= BUFSIZE - 1 ? remaining : BUFSIZE - 1;
		data->WriteAscii(buf, start, batch, 0);
		nBatch = send(sock, buf, batch, 0);
		if (nBatch == -1) {
			// An error occured. As we throw an error here we can't return
			// the number of bytes sent in this case, however this number
			// isn't accurate anyway, as a reportedly successful send()
			// right before a failure may not actually have succeeded.
			throwError("send() failed");
			return scope.Close(Undefined());
		} else if (nBatch < batch) {
			// We wasn't able to send the full batch, return the number
			// of bytes already sent
			nBytes += nBatch;
			break;
		}
		start += batch;
		remaining -= batch;
		nBytes += batch;
	}

	return scope.Close(Number::New(nBytes));
}

// Params: socket
// Returns 0 if the operation succeeds, -1 if fails.
Handle<Value> closeImpl(const Arguments& args) {
	HandleScope scope;

	int sock;
	
	if (args.Length() < 1) {
		throwTypeError("socket is not specified");
		return scope.Close(Undefined());
	}
	if (!args[0]->IsNumber()) {
		throwTypeError("socket is not a number");
		return scope.Close(Undefined());
	}
	
	sock = args[0]->NumberValue();
	return scope.Close(Number::New(close(sock)));
}

void init(Handle<Object> target) {
	NODE_SET_METHOD(target, "connect", connectImpl);
	NODE_SET_METHOD(target, "read", readImpl);
	NODE_SET_METHOD(target, "write", writeImpl);
	NODE_SET_METHOD(target, "close", closeImpl);
}

NODE_MODULE(syncnet, init);
