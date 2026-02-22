// Processor テスト用の最小ハーネス。
#ifndef PROCESSOR_HARNESS_H
#define PROCESSOR_HARNESS_H

#include <algorithm>
#include <vector>
#include "../ProcessorCpu.h"
#include "../ProcessorAvx2.h"

struct ProcessorRunConfig
{
	int width;
	int height;
	int totalFrames;
	int currentFrame;
	int searchRadius;
	int timeRadius;
	int h;
	int seed;
};

struct ProcessorRunResult
{
	std::vector<PIXEL_YC> output;
};

struct MockEditContext
{
	int width;
	int height;
	int totalFrames;
	int currentFrame;
	int cacheDepth;
	std::vector<std::vector<PIXEL_YC>> frames;
};

inline int mock_get_frame(void* editp)
{
	MockEditContext* ctx = reinterpret_cast<MockEditContext*>(editp);
	return ctx->currentFrame;
}

inline int mock_get_frame_n(void* editp)
{
	MockEditContext* ctx = reinterpret_cast<MockEditContext*>(editp);
	return ctx->totalFrames;
}

inline BOOL mock_set_ycp_filtering_cache_size(void* fp, int w, int h, int d, int flag)
{
	(void)fp;
	(void)w;
	(void)h;
	(void)flag;
	(void)d;
	return TRUE;
}

inline PIXEL_YC* mock_get_ycp_filtering_cache_ex(void* fp, void* editp, int n, int* w, int* h)
{
	(void)fp;
	MockEditContext* ctx = reinterpret_cast<MockEditContext*>(editp);
	const int clamped = (std::max)(0, (std::min)(ctx->totalFrames - 1, n));
	if (w != NULL) {
		*w = ctx->width;
	}
	if (h != NULL) {
		*h = ctx->height;
	}
	return ctx->frames[static_cast<size_t>(clamped)].data();
}

inline std::vector<PIXEL_YC> generate_frame(int width, int height, int frameIndex, int seed)
{
	std::vector<PIXEL_YC> frame(static_cast<size_t>(width) * static_cast<size_t>(height));
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const int v = (x * 31 + y * 17 + frameIndex * 13 + seed * 7) % 1024;
			PIXEL_YC p = {};
			p.y = static_cast<short>(v);
			p.cb = static_cast<short>((v - 256) / 2);
			p.cr = static_cast<short>((512 - v) / 2);
			frame[static_cast<size_t>(y * width + x)] = p;
		}
	}
	return frame;
}

template<class TProcessor>
inline ProcessorRunResult run_processor_impl(const ProcessorRunConfig& cfg)
{
	MockEditContext ctx = {};
	ctx.width = cfg.width;
	ctx.height = cfg.height;
	ctx.totalFrames = cfg.totalFrames;
	ctx.currentFrame = cfg.currentFrame;
	ctx.cacheDepth = cfg.timeRadius * 2 + 1;
	ctx.frames.resize(static_cast<size_t>(cfg.totalFrames));
	for (int i = 0; i < cfg.totalFrames; ++i) {
		ctx.frames[static_cast<size_t>(i)] = generate_frame(cfg.width, cfg.height, i, cfg.seed);
	}

	EXFUNC exfunc = {};
	exfunc.get_frame = mock_get_frame;
	exfunc.get_frame_n = mock_get_frame_n;
	exfunc.set_ycp_filtering_cache_size = mock_set_ycp_filtering_cache_size;
	exfunc.get_ycp_filtering_cache_ex = mock_get_ycp_filtering_cache_ex;

	int track[4] = { cfg.searchRadius, cfg.timeRadius, cfg.h, 0 };
	FILTER fp = {};
	fp.track = track;
	fp.exfunc = &exfunc;

	std::vector<PIXEL_YC> ycpEdit = ctx.frames[static_cast<size_t>(cfg.currentFrame)];
	std::vector<PIXEL_YC> ycpTemp(static_cast<size_t>(cfg.width) * static_cast<size_t>(cfg.height));

	FILTER_PROC_INFO fpip = {};
	fpip.w = cfg.width;
	fpip.h = cfg.height;
	fpip.max_w = cfg.width;
	fpip.max_h = cfg.height;
	fpip.ycp_edit = ycpEdit.data();
	fpip.ycp_temp = ycpTemp.data();
	fpip.editp = &ctx;

	TProcessor processor;
	const BOOL ok = processor.proc(fp, fpip);
	ProcessorRunResult result = {};
	if (!ok) {
		return result;
	}
	result.output.assign(fpip.ycp_edit, fpip.ycp_edit + static_cast<size_t>(cfg.width) * static_cast<size_t>(cfg.height));
	return result;
}

inline bool is_avx2_processor_available()
{
	ProcessorAvx2 p;
	return p.isPrepared();
}

inline ProcessorRunResult run_processor_cpu(const ProcessorRunConfig& cfg)
{
	return run_processor_impl<ProcessorCpu>(cfg);
}

inline ProcessorRunResult run_processor_avx2(const ProcessorRunConfig& cfg)
{
	return run_processor_impl<ProcessorAvx2>(cfg);
}

inline bool run_processor_parity_case(const ProcessorRunConfig& cfg)
{
	ProcessorRunResult cpu = run_processor_cpu(cfg);
	ProcessorRunResult avx2 = run_processor_avx2(cfg);
	if (cpu.output.size() == 0 || avx2.output.size() == 0) {
		return false;
	}
	if (cpu.output.size() != avx2.output.size()) {
		return false;
	}
	for (size_t i = 0; i < cpu.output.size(); ++i) {
		if (cpu.output[i].y != avx2.output[i].y ||
			cpu.output[i].cb != avx2.output[i].cb ||
			cpu.output[i].cr != avx2.output[i].cr) {
			return false;
		}
	}
	return true;
}

#endif
