// Exercise the private directory owner directly, without exposing production test hooks.
#include "ServerBrowserMasterserverQuery.cpp"
#include "../server_browser_ui/check.h"
#include <future>
#include <iostream>

using namespace cse::server_browser;

void IncompletePublicationDoesNotInspectQueue() {
    Directory directory;
    std::unique_lock<std::mutex> publication(directory.mutex);
    auto snapshot = std::async(std::launch::async, [&directory] {
        return directory.CompleteAndDrained();
    });

    constexpr auto SnapshotWait = std::chrono::seconds(1);
    const bool rejectedBeforePublication = snapshot.wait_for(SnapshotWait) == std::future_status::ready;
    constexpr Address FinalAddress{0x7f000001, 27015, 27015};
    directory.pending.push_back(FinalAddress);
    directory.done.store(true, std::memory_order_release);
    publication.unlock();
    const bool completed = snapshot.get();

    Require(rejectedBeforePublication, "unfinished directory inspected its queue before acquiring completion");
    Require(!completed, "completion used a queue snapshot from before final address publication");
    Require(!directory.CompleteAndDrained(), "published final address was ignored");
    directory.pending.pop_front();
    Require(directory.CompleteAndDrained(), "drained completed directory did not finish");
}

int main() {
    return RunCase([] {
        IncompletePublicationDoesNotInspectQueue();
        std::cout << "directory completion publication passed\n";
    });
}
