#include <gtest/gtest.h>
#include <regex>
#include <fstream>
#include "logger/logger.h"

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

static std::string ExtractPayload(const std::string &line)
{
    size_t space_count = 0;
    size_t pos = 0;

    for (size_t i = 0; i < line.length(); ++i)
    {
        if (line[i] == ' ')
        {
            space_count++;
            if (space_count == 3)
            {
                pos = i + 1;    
                break;
            }
        }
    }

    return line.substr(pos);
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
    std::remove(path.c_str());
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
    
    std::remove(path.c_str());
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
        logger.Flush();
    }

    auto lines = ReadLines(path);
    ASSERT_GE(lines.size(), 1u);

    std::string logged_payload = ExtractPayload(lines[0]);
    EXPECT_EQ(logged_payload.size(), 255u);
    std::string expected = std::string(255, 'A');
    EXPECT_EQ(logged_payload, expected);

    std::remove(path.c_str());
}

TEST(LoggerTest, LogLevels)
{
    const std::string path = "tmp_levels.log";
    std::remove(path.c_str());
    {
        Logger logger(path, POLICY);

        logger.Log(LogLevel::DEBUG, "debug_msg");
        logger.Log(LogLevel::INFO, "info_msg");
        logger.Log(LogLevel::WARNING, "warning_msg");
        logger.Log(LogLevel::ERROR, "error_msg");
        logger.Flush();
    }

    auto lines = ReadLines(path);
    ASSERT_EQ(lines.size(), 4u);

    EXPECT_TRUE(lines[0].find("DEBUG") != std::string::npos);
    EXPECT_TRUE(lines[0].find("debug_msg") != std::string::npos);

    EXPECT_TRUE(lines[1].find("INFO") != std::string::npos);
    EXPECT_TRUE(lines[1].find("info_msg") != std::string::npos);

    EXPECT_TRUE(lines[2].find("WARNING") != std::string::npos);
    EXPECT_TRUE(lines[2].find("warning_msg") != std::string::npos);

    EXPECT_TRUE(lines[3].find("ERROR") != std::string::npos);
    EXPECT_TRUE(lines[3].find("error_msg") != std::string::npos);

    std::remove(path.c_str());
}

TEST(LoggerTest, DropPolicy)
{
    const std::string path = "tmp_drop.log";
    std::remove(path.c_str());
    {
        Logger logger(path, OverflowPolicy::Drop);

        const int N = 10000;
        int successful_logs = 0;

        for (int i = 0; i < N; ++i)
        {
            if (logger.Log(LogLevel::INFO, "msg:" + std::to_string(i)))
            {
                successful_logs++;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        for (int i = N; i < N * 2; ++i)
        {
            if (logger.Log(LogLevel::INFO, "msg:" + std::to_string(i)))
            {
                successful_logs++;
            }
        }

        logger.Flush();

        uint64_t dropped = logger.dropped();
        uint64_t enqueued = logger.enqueued();

        EXPECT_GT(dropped + enqueued, 0u);  
        EXPECT_EQ(successful_logs, enqueued);  

        if (dropped > 0)
        {
            std::cout << "Successfully triggered " << dropped << " dropped messages\n";
        }
    }

    auto lines = ReadLines(path);
    EXPECT_LE(lines.size(), 20000u);

    std::remove(path.c_str());
}

TEST(LoggerTest, EmptyMessage)
{
    const std::string path = "tmp_empty.log";
    std::remove(path.c_str());
    {
        Logger logger(path, POLICY);

        bool ok = logger.Log(LogLevel::INFO, "");
        EXPECT_TRUE(ok);
        logger.Flush();
    }

    auto lines = ReadLines(path);
    ASSERT_EQ(lines.size(), 1u);

    std::string payload = ExtractPayload(lines[0]);
    EXPECT_TRUE(payload.empty());

    std::remove(path.c_str());
}