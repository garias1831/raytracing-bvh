#ifndef SPLIT_HEURISTIC_H
#define SPLIT_HEURISTIC_H

enum class BvhSplitHeuristic {
    Median,
    Sah,
};

inline BvhSplitHeuristic g_bvh_split_heuristic = BvhSplitHeuristic::Median;

inline void set_bvh_split_heuristic(BvhSplitHeuristic heuristic) {
    g_bvh_split_heuristic = heuristic;
}

inline BvhSplitHeuristic get_bvh_split_heuristic() {
    return g_bvh_split_heuristic;
}

inline const char* bvh_split_heuristic_name(BvhSplitHeuristic heuristic) {
    switch (heuristic) {
        case BvhSplitHeuristic::Median:
            return "median";
        case BvhSplitHeuristic::Sah:
            return "sah";
    }

    return "unknown";
}

#endif
