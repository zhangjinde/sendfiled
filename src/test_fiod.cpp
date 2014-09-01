#include <gtest/gtest.h>

#include "fiod.h"

TEST(Fiod, xxx)
{
    const int pid = fiod_spawn(".");

    EXPECT_GT(pid, 0);

    EXPECT_EQ(0, fiod_shutdown(pid));
}
