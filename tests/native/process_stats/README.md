# POSIX process statistics

Build this directory with Ninja and run CTest. The controlled test compiles the
actual private provider header with typed getrusage/clock seams and real platform
structures; it checks clock identity, units, integer boundaries, read failures and
atomic output. The separate real probe samples only this process's CPU usage and
the monotonic clock 100 times, with no waits or external resources. It does not
measure CPU-fraction accuracy or qualify the engine status command. Use
`CSE_STATS_SANITIZERS=ON` for ASan/UBSan. Windows retains its existing provider.
