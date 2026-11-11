#pragma once
#include <cstddef>

extern "C" void ListenerAllocatorArm(void *pointer, std::size_t size);
extern "C" unsigned int ListenerAllocatorFailures();
extern "C" void *ListenerAllocatorCaller();
extern "C" int ListenerAllocatorFrames(void **frames, int capacity);
