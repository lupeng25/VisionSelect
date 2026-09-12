#ifndef CANDIDATECHECKS_H
#define CANDIDATECHECKS_H

#include <QStringList>
#include <array>

enum class CandidateCheck { FieldOfView, Sampling, ImageCircle, Mount, WorkingDistance,
    DepthOfField, LightCoverage, FrameRate, Bandwidth, GlobalShutter, PixelFormat, Count };
enum class CandidateCheckState { NotApplicable, Passed, Unknown, Failed };

// 固定长度状态保留计算依据，不在批量评分热路径中构造诊断字符串。
struct CandidateChecks {
    std::array<CandidateCheckState, static_cast<size_t>(CandidateCheck::Count)> states{};
    CandidateCheckState &operator[](CandidateCheck check) { return states[static_cast<size_t>(check)]; }
    CandidateCheckState operator[](CandidateCheck check) const { return states[static_cast<size_t>(check)]; }
    int priority() const;
    bool failed() const { return priority() == static_cast<int>(CandidateCheckState::Failed); }
    bool unknown() const;
};

QString candidateCheckLabel(CandidateCheck check, const QString &language = {});
QString candidateCheckKey(CandidateCheck check);
QString candidateCheckStateLabel(CandidateCheckState state, const QString &language = {});
QStringList candidateCheckMessages(const CandidateChecks &checks, CandidateCheckState state,
                                   const QString &language = {});

#endif
