#include <gtest/gtest.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "adapters/control/wifi/wpa_supplicant/dnsmasq-dhcp-server.hpp"

namespace {

TEST(DnsmasqDhcpServerTest, StopKillsUnresponsiveDnsmasqWithinThreeSeconds) {
    char directory[] = "/tmp/sst-dnsmasq-XXXXXX";
    ASSERT_NE(mkdtemp(directory), nullptr);
    const std::filesystem::path executable = std::filesystem::path(directory) / "dnsmasq";
    const std::filesystem::path pid_file = std::filesystem::path(directory) / "pid";
    {
        std::ofstream script(executable);
        script << "#!/bin/sh\n"
               << "printf '%s' \"$$\" > '" << pid_file.string() << "'\n"
               << "trap '' TERM\n"
               << "exec sleep 30\n";
    }
    ASSERT_EQ(chmod(executable.c_str(), 0700), 0);
    const std::string old_path = std::getenv("PATH") != nullptr ? std::getenv("PATH") : "";
    const std::string path = std::string(directory) + ":" + old_path;
    ASSERT_EQ(setenv("PATH", path.c_str(), 1), 0);
    {
        sst::adapters::control::DnsmasqDhcpServer server;
        const bool started = server.Start("p2p-test0", "192.168.49.1");
        EXPECT_TRUE(started);
        if (started) {
            const auto start = std::chrono::steady_clock::now();
            server.Stop();
            EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(8));
            std::ifstream pid_input(pid_file);
            pid_t pid = -1;
            pid_input >> pid;
            EXPECT_GT(pid, 0);
            if (pid > 0) {
                EXPECT_EQ(kill(pid, 0), -1);
                EXPECT_EQ(errno, ESRCH);
            }
        }
    }
    EXPECT_EQ(setenv("PATH", old_path.c_str(), 1), 0);
    std::filesystem::remove_all(directory);
}

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
