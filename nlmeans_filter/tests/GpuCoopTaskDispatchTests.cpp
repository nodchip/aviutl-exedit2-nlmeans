// 複数 GPU 協調タスクのディスパッチを GoogleTest で検証する。
#include <gtest/gtest.h>
#include "../exedit2/GpuCoopTaskDispatch.h"

TEST(GpuCoopTaskDispatchTests, ReturnsFalseWhenCallbackIsMissing)
{
	const std::vector<bool> results = execute_gpu_coop_tasks(3, nullptr, true);
	ASSERT_EQ(results.size(), 3u);
	EXPECT_FALSE(results[0]);
	EXPECT_FALSE(results[1]);
	EXPECT_FALSE(results[2]);
}

TEST(GpuCoopTaskDispatchTests, SerialDispatchRunsAllTasks)
{
	int callCount = 0;
	const std::vector<bool> results = execute_gpu_coop_tasks(
		4,
		[&callCount](size_t index) {
			++callCount;
			return (index % 2) == 0;
		},
		false);
	ASSERT_EQ(results.size(), 4u);
	EXPECT_EQ(callCount, 4);
	EXPECT_TRUE(results[0]);
	EXPECT_FALSE(results[1]);
	EXPECT_TRUE(results[2]);
	EXPECT_FALSE(results[3]);
}

TEST(GpuCoopTaskDispatchTests, AsyncDispatchRunsAllTasks)
{
	int callCount = 0;
	const std::vector<bool> results = execute_gpu_coop_tasks(
		3,
		[&callCount](size_t index) {
			++callCount;
			return index != 1;
		},
		true);
	ASSERT_EQ(results.size(), 3u);
	EXPECT_EQ(callCount, 3);
	EXPECT_TRUE(results[0]);
	EXPECT_FALSE(results[1]);
	EXPECT_TRUE(results[2]);
}
