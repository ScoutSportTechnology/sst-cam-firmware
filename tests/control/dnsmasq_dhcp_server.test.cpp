#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>

#include "adapters/control/wifi/wpa_supplicant/dnsmasq-dhcp-server.hpp"

namespace {

TEST(DnsmasqDhcpServerTest, ChildCanReceiveTerminationWhenFirmwareBlocksSignals) {
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGTERM);
    sigaddset(&signals, SIGINT);
    sigset_t previous;
    ASSERT_EQ(sigprocmask(SIG_BLOCK, &signals, &previous), 0);
    const pid_t child = fork();
    if (child == 0) {
        sst::adapters::control::ResetDnsmasqChildSignalMask();
        sigset_t current;
        if (sigprocmask(SIG_SETMASK, nullptr, &current) != 0) {
            _exit(2);
        }
        _exit(sigismember(&current, SIGTERM) == 0 && sigismember(&current, SIGINT) == 0 ? 0 : 1);
    }
    ASSERT_GT(child, 0);
    ASSERT_EQ(sigprocmask(SIG_SETMASK, &previous, nullptr), 0);
    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

}  // namespace
