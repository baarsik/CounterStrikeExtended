#pragma once
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
// Test-only syscall seams around the actual executable owner body, never production callbacks.
int CseTestSocket(int, int, int);
int CseTestClose(int);
int CseTestFcntl(int, int, ...);
int CseTestDlclose(void*);
int CseTestAtfork(void (*)(), void (*)(), void (*)());
#define socket CseTestSocket
#define close CseTestClose
#define fcntl CseTestFcntl
#define dlclose CseTestDlclose
#define pthread_atfork CseTestAtfork
