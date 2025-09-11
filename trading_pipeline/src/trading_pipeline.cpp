#include "trading_pipeline/trading_pipeline.h"

int main()
{
    hft::TradingPipeline pipeline(1);

    pipeline.Start();

    std::this_thread::sleep_for(std::chrono::seconds(10));

    pipeline.Stop();

    return 0;
}