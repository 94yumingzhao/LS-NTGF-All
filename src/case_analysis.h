// case_analysis.h - 案例分析工具
// 提供案例分析功能，包括需求紧张度计算、数据统计分析等

#ifndef CASE_ANALYSIS_H_
#define CASE_ANALYSIS_H_

#include "optimizer.h"
#include <string>

// 批量案例分析
void AnalyzeCase();

// 打印案例需求紧张度分析
void PrintCaseAnalysis(const AllValues& values, const AllLists& lists);

// 执行综合案例分析
void PerformComprehensiveAnalysis(const AllValues& values,
                                  const AllLists& lists,
                                  const string& data_file_path);

// 向后兼容别名
inline void analyze_case() { AnalyzeCase(); }
inline void print_case_analysis(const AllValues& v, const AllLists& l) { PrintCaseAnalysis(v, l); }
inline void perform_comprehensive_analysis(const AllValues& v, const AllLists& l, const string& p) {
    PerformComprehensiveAnalysis(v, l, p);
}

#endif  // CASE_ANALYSIS_H_
