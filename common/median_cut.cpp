
#define NOMINMAX

#include <vector>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <unordered_map>

#include "median_cut.h"

//--------------------------------
// 基本構造体
//--------------------------------

struct Box {
	std::vector<int> indices; // hist への参照
	uint8_t rmin, rmax;
	uint8_t gmin, gmax;
	uint8_t bmin, bmax;
};

//--------------------------------
// Box処理
//--------------------------------
static void compute_bounds(Box& box, const std::vector<HistColor>& hist) {
	box.rmin = box.gmin = box.bmin = 255;
	box.rmax = box.gmax = box.bmax = 0;

	for (int idx : box.indices) {
		const auto& c = hist[idx];
		box.rmin = std::min(box.rmin, c.r);
		box.rmax = std::max(box.rmax, c.r);
		box.gmin = std::min(box.gmin, c.g);
		box.gmax = std::max(box.gmax, c.g);
		box.bmin = std::min(box.bmin, c.b);
		box.bmax = std::max(box.bmax, c.b);
	}
}

static int longest_axis(const Box& box) {
	int r = box.rmax - box.rmin;
	int g = box.gmax - box.gmin;
	int b = box.bmax - box.bmin;

	if (r >= g && r >= b) return 0;
	if (g >= r && g >= b) return 1;
	return 2;
}

//--------------------------------
// 重みベース分割
//--------------------------------
static size_t split_by_weight(const std::vector<int>& sorted, const std::vector<HistColor>& hist) {

	uint64_t total = 0;
	for (int idx : sorted) total += hist[idx].count;

	uint64_t half = total / 2;

	uint64_t acc = 0;
	for (size_t i = 0; i < sorted.size(); i++) {
		acc += hist[sorted[i]].count;
		if (acc >= half) return i + 1;
	}

	return sorted.size() / 2; // fallback
}

//--------------------------------
// Box分割
//--------------------------------
static void split_box(const Box& box, Box& box1, Box& box2, const std::vector<HistColor>& hist) {

	int axis = longest_axis(box);

	std::vector<int> sorted = box.indices;

	std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
		if (axis == 0) return hist[a].r < hist[b].r;
		if (axis == 1) return hist[a].g < hist[b].g;
		return hist[a].b < hist[b].b;
	});

	size_t mid = split_by_weight(sorted, hist);

	box1.indices.assign(sorted.begin(), sorted.begin() + mid);
	box2.indices.assign(sorted.begin() + mid, sorted.end());

	compute_bounds(box1, hist);
	compute_bounds(box2, hist);
}

//--------------------------------
// 平均色（重み付き）
//--------------------------------
static Color average_color(const Box& box, const std::vector<HistColor>& hist) {

	uint64_t r = 0, g = 0, b = 0;
	uint64_t total = 0;

	for (int idx : box.indices) {
		const auto& c = hist[idx];
		r += c.r * c.count;
		g += c.g * c.count;
		b += c.b * c.count;
		total += c.count;
	}

	if (total == 0) return { 0,0,0 };

	return {
			(uint8_t)(r / total),
			(uint8_t)(g / total),
			(uint8_t)(b / total)
	};
}

// ---------------------------------------------------------
// kmeans
// ---------------------------------------------------------

inline int dist2(const Color& a, const HistColor& b) {
	int dr = a.r - b.r;
	int dg = a.g - b.g;
	int db = a.b - b.b;
	return dr * dr + dg * dg + db * db;
}

void kmeans_refine(
	std::vector<Color>& palette,
	const std::vector<HistColor>& hist,
	int iterations = 5) {
	int K = (int)palette.size();

	for (int it = 0; it < iterations; it++) {

		std::vector<uint64_t> sum_r(K, 0), sum_g(K, 0), sum_b(K, 0);
		std::vector<uint64_t> sum_w(K, 0);

		// 割り当て
		for (const auto& h : hist) {

			int best = 0;
			int best_d = INT_MAX;

			for (int i = 0; i < K; i++) {
				int d = dist2(palette[i], h);
				if (d < best_d) {
					best_d = d;
					best = i;
				}
			}

			sum_r[best] += h.r * h.count;
			sum_g[best] += h.g * h.count;
			sum_b[best] += h.b * h.count;
			sum_w[best] += h.count;
		}

		// 更新
		for (int i = 0; i < K; i++) {
			if (sum_w[i] == 0) continue;

			palette[i].r = (uint8_t)(sum_r[i] / sum_w[i]);
			palette[i].g = (uint8_t)(sum_g[i] / sum_w[i]);
			palette[i].b = (uint8_t)(sum_b[i] / sum_w[i]);
		}
	}
}

//

//--------------------------------
// メイン：ヒストグラムMedian Cut
//--------------------------------
std::vector<Color> median_cut_histogram(const std::vector<HistColor>& hist, int target_colors) {

	std::vector<Box> boxes;

	// ② 初期ボックス
	Box root;
	root.indices.resize(hist.size());
	std::iota(root.indices.begin(), root.indices.end(), 0);
	compute_bounds(root, hist);

	boxes.push_back(std::move(root));

	// ③ 分割ループ
	while ((int)boxes.size() < target_colors) {

		auto it = std::max_element(boxes.begin(), boxes.end(),
															 [](const Box& a, const Box& b) {
			int ra = std::max({ a.rmax - a.rmin, a.gmax - a.gmin, a.bmax - a.bmin });
			int rb = std::max({ b.rmax - b.rmin, b.gmax - b.gmin, b.bmax - b.bmin });
			return ra < rb;
		});

		if (it == boxes.end() || it->indices.size() <= 1) break;

		Box b1, b2;
		split_box(*it, b1, b2, hist);

		*it = boxes.back();
		boxes.pop_back();

		boxes.push_back(std::move(b1));
		boxes.push_back(std::move(b2));
	}

	// ④ パレット生成
	std::vector<Color> palette;
	palette.reserve(boxes.size());

	for (auto& box : boxes) {
		palette.push_back(average_color(box, hist));
	}

	// kmeans する前に上の Median Cut のクオリティがいまいちっぽい...
	// kmeans_refine(palette, hist, 5);

	return palette;
}

