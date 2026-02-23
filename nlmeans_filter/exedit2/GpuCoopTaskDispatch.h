// 複数 GPU 協調タスクのディスパッチ共通処理。
#ifndef EXEDIT2_GPU_COOP_TASK_DISPATCH_H
#define EXEDIT2_GPU_COOP_TASK_DISPATCH_H

#include <cstddef>
#include <functional>
#include <future>
#include <vector>

// タスク群を逐次または非同期で実行し、各タスクの成功可否を返す。
inline std::vector<bool> execute_gpu_coop_tasks(
	size_t taskCount,
	const std::function<bool(size_t)>& runTask,
	bool enableAsync)
{
	std::vector<bool> results(taskCount, false);
	if (!runTask) {
		return results;
	}
	if (!enableAsync || taskCount <= 1) {
		for (size_t i = 0; i < taskCount; ++i) {
			results[i] = runTask(i);
		}
		return results;
	}

	std::vector<std::future<bool>> futures;
	futures.reserve(taskCount);
	for (size_t i = 0; i < taskCount; ++i) {
		futures.push_back(std::async(std::launch::async, [runTask, i]() {
			return runTask(i);
		}));
	}
	for (size_t i = 0; i < taskCount; ++i) {
		results[i] = futures[i].get();
	}
	return results;
}

#endif
