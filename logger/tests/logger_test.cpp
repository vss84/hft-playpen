#include <gtest/gtest.h>
#include <regex>
#include <fstream>
#include "logger.h"

using namespace hft;
constexpr OverflowPolicy POLICY = OverflowPolicy::Drop;

static std::vector<std::string> ReadLines(const std::string &path)
{
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    
    while (std::getline(in, line)) 
        lines.push_back(line);
    
    return lines;
}

TEST(LoggerTest, BasicOrderingAndContent)
{
    const std::string path = "tmp_ordering.log";
    std::remove(path.c_str());
    {
        Logger logger(path, POLICY);
        const int N = 1000;
        for (int i = 0; i < 1000; ++i)
        {
            bool ok = logger.Log(LogLevel::INFO, "seq:" + std::to_string(i));
            EXPECT_TRUE(ok);
        }
        logger.Flush();
    }

    auto lines = ReadLines(path);
    EXPECT_GE(lines.size(), 1000u);
    std::regex seq_re("seq:(\\d+)");
    int last = -1;
    for (auto &line : lines)
    {
        std::smatch m;
        if (std::regex_search(line, m, seq_re))
        {
            int v = std::stoi(m[1].str());

            if (last >= 0) EXPECT_GE(v, last);
            last = v;
        }
    }
}

TEST(LoggerTest, FlushOnDestroy)
{
    const std::string path = "tmp_destroy.log";
    std::remove(path.c_str());
    {
        Logger logger(path, POLICY);
        const int N = 2048;
        for (int i = 0; i < N; ++i)
        {
            logger.Log(LogLevel::INFO, "x:" + std::to_string(1));
        }
    }

    auto lines = ReadLines(path);
    EXPECT_GE(lines.size(), 1u);
}

TEST(LoggerTest, PayloadBoundary)
{
    const std::string path = "tmp_boundary.log";
    std::remove(path.c_str());
    {
        Logger logger(path, POLICY);
        std::string big(300, 'A');
        ASSERT_GT(big.size(), 255u);
        bool ok = logger.Log(LogLevel::INFO, big);
        ASSERT_TRUE(ok);
    }

    auto lines = ReadLines(path);
    ASSERT_GE(lines.size(), 1u);
}