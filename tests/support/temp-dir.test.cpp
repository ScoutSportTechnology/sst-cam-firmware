#include "temp-dir.hpp"

#include <gtest/gtest.h>

#include <filesystem>

TEST(TempDirTest, SamePrefixCreatesDistinctDirectoriesAndCleansUp) {
    std::filesystem::path first_path;
    {
        sst::test::TempDir first("parallel");
        sst::test::TempDir second("parallel");
        first_path = first.path();
        EXPECT_NE(first.path(), second.path());
        EXPECT_TRUE(std::filesystem::is_directory(first.path()));
        EXPECT_TRUE(std::filesystem::is_directory(second.path()));
    }
    EXPECT_FALSE(std::filesystem::exists(first_path));
}
