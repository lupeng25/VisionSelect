#include "core/CandidateChecks.h"
#include "i18n/LanguageManager.h"
#include <algorithm>

namespace {
bool english(const QString &language)
{
    return (language.isEmpty() ? LanguageManager::instance().currentLanguage() : language).startsWith("en");
}
}

int CandidateChecks::priority() const
{
    return static_cast<int>(*std::max_element(states.begin(), states.end()));
}

bool CandidateChecks::unknown() const
{
    return std::find(states.begin(), states.end(), CandidateCheckState::Unknown) != states.end();
}

QString candidateCheckLabel(CandidateCheck check, const QString &language)
{
    static const char *zh[] = {"FOV 视野", "物方采样", "像圈/靶面", "相机/镜头接口", "WD 工作距离",
        "DOF 景深", "照明覆盖", "相机帧率", "接口带宽", "全局快门", "传输像素格式"};
    static const char *en[] = {"Field of view", "Object sampling", "Image circle / sensor", "Camera / lens mount",
        "Working distance", "Depth of field", "Illumination coverage", "Camera frame rate", "Interface bandwidth", "Global shutter", "Pixel format"};
    const size_t index = static_cast<size_t>(check);
    return index < static_cast<size_t>(CandidateCheck::Count)
        ? QString::fromUtf8(english(language) ? en[index] : zh[index]) : QString();
}

QString candidateCheckKey(CandidateCheck check)
{
    static const char *keys[] = {"fov", "sampling", "image_circle", "mount", "working_distance",
        "dof", "light_coverage", "fps", "bandwidth", "global_shutter", "pixel_format"};
    const auto index = static_cast<size_t>(check);
    return index < static_cast<size_t>(CandidateCheck::Count) ? QString::fromLatin1(keys[index]) : QString();
}

QString candidateCheckStateLabel(CandidateCheckState state, const QString &language)
{
    static const char *zh[] = {"不适用", "满足", "待确认", "不满足"};
    static const char *en[] = {"Not applicable", "Passed", "Needs confirmation", "Failed"};
    return QString::fromUtf8(english(language) ? en[static_cast<int>(state)] : zh[static_cast<int>(state)]);
}

QStringList candidateCheckMessages(const CandidateChecks &checks, CandidateCheckState state, const QString &language)
{
    QStringList messages;
    for (size_t i = 0; i < checks.states.size(); ++i)
        if (checks.states[i] == state)
            messages.append(candidateCheckLabel(static_cast<CandidateCheck>(i), language)
                + QStringLiteral(": ") + candidateCheckStateLabel(state, language));
    return messages;
}
