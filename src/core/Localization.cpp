#include "core/Localization.h"

#include "i18n/LanguageManager.h"

#include <QRegularExpression>

namespace {
struct Replacement
{
    const char *from;
    const char *to;
};

bool isEnglish(const QString &languageCode)
{
    return languageCode == QLatin1String("en_US");
}

QString utf8(const char *text)
{
    return QString::fromUtf8(text);
}

bool containsCjk(const QString &value)
{
    for (const QChar ch : value) {
        const ushort u = ch.unicode();
        if ((u >= 0x3400 && u <= 0x9fff) || (u >= 0xf900 && u <= 0xfaff))
            return true;
    }
    return false;
}

QString replaceAll(QString value, const Replacement *items, int count)
{
    for (int i = 0; i < count; ++i)
        value.replace(utf8(items[i].from), QString::fromUtf8(items[i].to), Qt::CaseSensitive);
    return value;
}

QString englishDiagnostic(QString value)
{
    static const Replacement exact[] = {
        {"需求参数缺失或超出计算范围，无法完成采样校核", "Requirements are missing or out of range; sampling cannot be checked"},
        {"传输像素格式未确认，带宽和存储仅按位深估算", "Pixel format is unconfirmed; bandwidth and storage are estimated from bit depth"},
        {"相机标称最大帧率低于需求帧率", "Nominal camera frame rate is below the required frame rate"},
        {"相机最大帧率未填写，无法确认节拍", "Maximum camera frame rate is missing; cycle capability is unknown"},
        {"相机标称帧率满足需求，实际节拍仍取决于曝光和读出", "Nominal frame rate meets the requirement; exposure and readout still limit the actual cycle"},
        {"普通镜头实际物方像素粗于目标，当前方案采样不满足", "Actual fixed-lens sampling is coarser than the target; sampling fails"},
        {"普通镜头实际采样满足目标物方像素", "Actual fixed-lens sampling meets the target"},
        {"远心镜头方案", "Telecentric lens solution"},
        {"普通镜头方案", "Fixed-focal lens solution"},
        {"普通镜头：焦距无效，无法估算 FOV", "Fixed-focal lens: focal length is invalid, FOV cannot be estimated"},
        {"远心镜头：PMAG 无效，无法估算 FOV", "Telecentric lens: PMAG is invalid, FOV cannot be estimated"},
        {"普通镜头：当前 WD 必须大于焦距，薄透镜近似才有有效正倍率", "Fixed-focal lens: current WD must be greater than focal length for a valid thin-lens approximation"},
        {"远心镜头：FOV = SensorSize / PMAG，ObjectPixel = PixelSize / PMAG", "Telecentric lens: FOV = SensorSize / PMAG, ObjectPixel = PixelSize / PMAG"},
        {"远心：FOV = SensorSize / PMAG，ObjectPixel = PixelSize / PMAG", "Telecentric: FOV = SensorSize / PMAG, ObjectPixel = PixelSize / PMAG"},
        {"普通镜头：M = SensorSize / FOV，f ≈ WD × SensorSize / (FOV + SensorSize)", "Fixed-focal lens: M = SensorSize / FOV, f ~= WD x SensorSize / (FOV + SensorSize)"},
        {"FOV = SensorSize / PMAG，ObjectPixel = PixelSize / PMAG", "FOV = SensorSize / PMAG, ObjectPixel = PixelSize / PMAG"},
        {"M = SensorSize / FOV，f ≈ WD x SensorSize / (FOV + SensorSize)", "M = SensorSize / FOV, f ~= WD x SensorSize / (FOV + SensorSize)"}
    };
    value = replaceAll(value, exact, sizeof(exact) / sizeof(exact[0]));

    static const Replacement catalogPrefixes[] = {
        {"相机数据无效", "Camera data is invalid"},
        {"镜头数据无效", "Lens data is invalid"},
        {"光源数据无效", "Light data is invalid"}
    };
    value = replaceAll(value, catalogPrefixes, sizeof(catalogPrefixes) / sizeof(catalogPrefixes[0]));

    if (!containsCjk(value))
        return value;

    static const Replacement phrase[] = {
        {"黑白相机通常有更高灵敏度和边缘稳定性", "Monochrome cameras usually provide higher sensitivity and edge stability"},
        {"需求偏向黑白测量，但该相机为彩色型号", "The requirement prefers monochrome imaging, but this camera is a color model"},
        {"相机帧率满足节拍需求", "Camera frame rate meets the cycle requirement"},
        {"相机最大帧率低于需求节拍", "Camera maximum frame rate is below the required cycle rate"},
        {"相机最大帧率低于需求", "Camera maximum frame rate is below the requirement"},
        {"缺少接口带宽数据，需按相机手册确认吞吐余量", "Interface bandwidth is missing; confirm throughput margin from the camera manual"},
        {"接口带宽利用率约", "Interface bandwidth utilization is about"},
        {"连续采集余量充足", "sufficient margin for continuous acquisition"},
        {"建议保留触发和协议开销余量", "reserve margin for trigger and protocol overhead"},
        {"接近上限", "near the upper limit"},
        {"接口带宽不足", "Interface bandwidth is insufficient"},
        {"按分辨率/bit depth/fps 估算的带宽", "Estimated bandwidth from resolution / bit depth / fps"},
        {"超过接口余量", "exceeds interface capacity"},
        {"连续原始图像存储约", "Continuous raw image storage is about"},
        {"需确认硬盘、缓存和压缩策略", "confirm disk, cache, and compression strategy"},
        {"原始图像数据量约", "Raw image data volume is about"},
        {"高速运动目标优先全局快门", "High-speed motion target prefers a global shutter"},
        {"高速运动场景使用卷帘快门存在形变风险", "Rolling shutter has deformation risk in high-speed motion"},
        {"普通镜头焦距无效", "Fixed-focal lens focal length is invalid"},
        {"普通镜头焦距必须大于 0", "Fixed-focal lens focal length must be greater than 0"},
        {"普通镜头当前 WD 不大于焦距", "Current WD is not greater than the fixed-focal lens focal length"},
        {"普通镜头当前 WD 必须大于焦距", "Current WD must be greater than the fixed-focal lens focal length"},
        {"当前工作距离不大于镜头焦距", "Current working distance is not greater than the lens focal length"},
        {"无法按固定焦距镜头薄透镜模型得到有效 FOV", "a valid FOV cannot be calculated with the fixed-focal thin-lens model"},
        {"普通镜头视野覆盖工件和定位余量", "Fixed-focal lens FOV covers the part and positioning margin"},
        {"普通镜头 FOV 覆盖需求", "Fixed-focal lens FOV covers the requirement"},
        {"普通镜头 FOV 不覆盖需求", "Fixed-focal lens FOV does not cover the requirement"},
        {"普通镜头在当前工作距离下视野不足", "Fixed-focal lens has insufficient FOV at the current working distance"},
        {"普通镜头在当前 WD 下 FOV 不足", "Fixed-focal lens has insufficient FOV at the current WD"},
        {"物方像素精度", "Object-side pixel size"},
        {"物方像素", "Object-side pixel size"},
        {"满足目标", "meets target"},
        {"粗于目标", "is coarser than target"},
        {"镜头像圈小于相机传感器", "Lens image circle is smaller than the camera sensor"},
        {"镜头像圆", "Lens image circle"},
        {"小于相机传感器对角线", "is smaller than the camera sensor diagonal"},
        {"相机与镜头接口不匹配", "Camera and lens mounts do not match"},
        {"相机与远心镜头接口不匹配", "Camera and telecentric lens mounts do not match"},
        {"相机接口", "Camera mount"},
        {"与镜头接口", "and lens mount"},
        {"与远心镜头接口", "and telecentric lens mount"},
        {"不匹配", "do not match"},
        {"工作距离小于镜头最小工作距离", "Working distance is below the lens minimum working distance"},
        {"当前 WD 小于普通镜头最小工作距离", "Current WD is below the fixed-focal lens minimum working distance"},
        {"估算 DOF", "Estimated DOF"},
        {"覆盖高度波动", "covers height variation"},
        {"可能不足以覆盖高度波动", "may be insufficient to cover height variation"},
        {"可能不足", "may be insufficient"},
        {"缺少 F/# 或 DOF 数据，普通镜头景深需要确认", "F/# or DOF data is missing; fixed-focal depth of field needs confirmation"},
        {"缺少 F/# 或倍率数据，无法估算 DOF", "F/# or magnification data is missing; DOF cannot be estimated"},
        {"缺少 F/# 或 DOF 数据，景深需要确认", "F/# or DOF data is missing; depth of field needs confirmation"},
        {"按未标定 FOV 边缘粗估畸变误差约", "Rough uncalibrated FOV edge distortion error is about"},
        {"按 FOV 边缘估算畸变误差约", "Estimated FOV edge distortion error is about"},
        {"畸变边缘误差约", "Edge distortion error is about"},
        {"高于允许误差", "above the allowed tolerance"},
        {"最终测量需标定复核", "final measurement requires calibration review"},
        {"需按厂商畸变曲线和标定板复核", "review vendor distortion curves and calibration target"},
        {"镜头标称 MP 低于相机像素", "Lens rated MP is below the camera pixel count"},
        {"可能无法喂满传感器", "it may not fully support the sensor"},
        {"需确认远心镜头分辨率/MTF", "confirm telecentric lens resolution / MTF"},
        {"镜头 MP 标称低于相机像素", "Lens rated MP is below the camera pixel count"},
        {"需确认 MTF", "confirm MTF"},
        {"镜头 MP 余量较小", "Lens MP margin is low"},
        {"利用率约", "utilization is about"},
        {"镜头标称 MP 覆盖相机", "Lens rated MP covers the camera"},
        {"仍需按 MTF 曲线复核边缘对比度", "still review edge contrast against the MTF curve"},
        {"镜头缺少标称 MP 数据", "Lens rated MP data is missing"},
        {"需按厂家 MTF 曲线确认分辨率余量", "confirm resolution margin from the vendor MTF curve"},
        {"镜头推荐最小像元大于相机像元", "Lens recommended minimum pixel size is larger than the camera pixel size"},
        {"相机像元不小于镜头建议最小像元", "Camera pixel size is not smaller than the lens recommended minimum pixel size"},
        {"相机像元小于镜头推荐最小像元", "Camera pixel size is smaller than the lens recommended minimum pixel size"},
        {"高精度测量或高度波动场景，普通镜头存在透视误差", "High-precision measurement or height variation can introduce perspective error with fixed-focal lenses"},
        {"高精度/高度波动场景普通镜头存在透视误差", "High-precision / height-variation scenarios can introduce perspective error with fixed-focal lenses"},
        {"低/中精度或大视野任务使用普通镜头成本和安装更友好", "Fixed-focal lenses are lower cost and easier to install for low/medium precision or large-FOV tasks"},
        {"大视野任务优先普通工业镜头", "Large-FOV tasks prefer fixed-focal industrial lenses"},
        {"远心倍率 PMAG 无效", "Telecentric PMAG is invalid"},
        {"远心倍率 PMAG 必须大于 0", "Telecentric PMAG must be greater than 0"},
        {"远心 FOV", "Telecentric FOV"},
        {"远心镜头 FOV 不覆盖需求", "Telecentric lens FOV does not cover the requirement"},
        {"远心镜头固定倍率下 FOV 不足", "Telecentric lens FOV is insufficient at the fixed magnification"},
        {"需更低倍率或更大靶面", "use lower magnification or a larger sensor format"},
        {"远心物方像素", "Telecentric object-side pixel size"},
        {"远心物方像素满足目标", "Telecentric object-side pixel size meets the target"},
        {"远心物方像素粗于目标", "Telecentric object-side pixel size is coarser than the target"},
        {"远心镜头像圈/靶面小于相机传感器", "Telecentric lens image circle / sensor format is smaller than the camera sensor"},
        {"远心镜头像圆/最大靶面小于相机传感器", "Telecentric lens image circle / max sensor format is smaller than the camera sensor"},
        {"会产生暗角或边缘退化", "may cause vignetting or edge degradation"},
        {"远心镜头缺少标称 WD", "Telecentric lens nominal WD is missing"},
        {"远心镜头必须按厂商标称工作距离安装", "Telecentric lenses must be installed at the vendor nominal WD"},
        {"当前资料缺少 nominal WD", "current data is missing nominal WD"},
        {"工作距离落在远心镜头标称 WD 容差内", "Working distance is within the telecentric lens nominal WD tolerance"},
        {"工作距离偏离远心镜头标称 WD 容差", "Working distance is outside the telecentric lens nominal WD tolerance"},
        {"远心镜头通常只在标称 WD/远心范围内工作", "Telecentric lenses usually operate only within nominal WD / telecentric range"},
        {"当前 WD 偏差超过资料容差", "current WD deviation exceeds the documented tolerance"},
        {"远心镜头缺少 WD 容差数据", "Telecentric lens WD tolerance data is missing"},
        {"仅按接近标称 WD 粗略保留", "roughly retained because it is close to nominal WD"},
        {"需查 datasheet 复核安装距离", "check the datasheet for installation distance"},
        {"需查 datasheet 复核", "check the datasheet"},
        {"当前 WD 偏离远心镜头标称 WD，且镜头缺少 WD 容差数据", "Current WD differs from the telecentric lens nominal WD and the lens is missing WD tolerance data"},
        {"当前 WD 偏离远心镜头标称 WD，且缺少 WD 容差数据", "Current WD differs from the telecentric lens nominal WD and WD tolerance data is missing"},
        {"工作距离偏离远心镜头标称 WD 且缺少 WD 容差", "Working distance differs from the telecentric lens nominal WD and WD tolerance is missing"},
        {"当前 WD 又不接近标称 WD，不能直接判定可安装", "current WD is not close to nominal WD, so installation cannot be confirmed directly"},
        {"远心 DOF", "Telecentric DOF"},
        {"远心 DOF 可能不足", "Telecentric DOF may be insufficient"},
        {"需要确认光圈和景深", "confirm aperture and depth of field"},
        {"远心镜头缺少 DOF 数据", "Telecentric lens DOF data is missing"},
        {"无法确认是否覆盖高度波动", "cannot confirm height variation coverage"},
        {"残余远心误差约", "Residual telecentric error is about"},
        {"按远心度估算的残余视差约", "Residual parallax estimated from telecentricity is about"},
        {"远心度残余视差约", "Residual telecentricity parallax is about"},
        {"超过允许误差", "exceeds the allowed tolerance"},
        {"远心镜头缺少远心度数据", "Telecentricity data is missing"},
        {"无法估算高度波动带来的残余视差", "residual parallax from height variation cannot be estimated"},
        {"双远心结构有更好的倍率一致性", "Bi-telecentric structure provides better magnification consistency"},
        {"高精度测量/高度波动优先远心镜头以降低透视误差", "High-precision measurement / height variation prefers telecentric lenses to reduce perspective error"},
        {"当前精度/高度波动需求优先远心镜头", "Current precision / height variation prefers telecentric lenses"},
        {"大视野远心镜头通常体积、重量和成本较高", "Large-FOV telecentric lenses are usually larger, heavier, and more expensive"},
        {"视野覆盖当前需求", "FOV covers the current requirement"},
        {"视野不足", "FOV is insufficient"},
        {"需更短焦距/更低倍率或更大靶面", "use a shorter focal length, lower magnification, or larger sensor format"},
        {"镜头像圆/最大靶面不足以覆盖相机传感器", "Lens image circle / max sensor format is insufficient for the camera sensor"},
        {"相机分辨率或像元无效", "Camera resolution or pixel size is invalid"},
        {"无法计算传感器尺寸和镜头参数", "sensor size and lens parameters cannot be calculated"},
        {"相机分辨率无效", "Camera resolution is invalid"},
        {"无法计算物方像素", "object-side pixel size cannot be calculated"},
        {"按需求 FOV 估算", "Estimated from required FOV"},
        {"相机采样满足目标物方像素", "camera sampling meets the target object-side pixel size"},
        {"接口带宽未填写", "Interface bandwidth is empty"},
        {"无法判断吞吐余量", "throughput margin cannot be judged"},
        {"带宽利用率约", "Bandwidth utilization is about"},
        {"超过接口容量", "exceeds interface capacity"},
        {"接近接口上限", "near interface limit"},
        {"高速运动建议使用全局快门相机", "High-speed motion should use a global shutter camera"},
        {"高速运动场景已选择全局快门", "A global shutter is selected for the high-speed motion scene"},
        {"当前 WD 超出远心镜头标称 WD 容差", "Current WD is outside the telecentric lens nominal WD tolerance"},
        {"当前 WD 低于镜头最小工作距离", "Current WD is below the lens minimum working distance"},
        {"镜头像面小于传感器对角线", "Lens image plane is smaller than the sensor diagonal"},
        {"存在暗角风险", "vignetting risk exists"},
        {"建议有效照明面积不小于", "Recommended active illumination area is at least"},
        {"当前光源有效面积小于需求 FOV", "Current light active area is smaller than the required FOV"},
        {"当前光源覆盖余量低于 10%", "Current light coverage margin is below 10%"},
        {"当前光源覆盖余量约", "Current light coverage margin is about"},
        {"高速运动建议使用频闪/触发光源以压低曝光时间", "High-speed motion should use strobe / triggered lighting to reduce exposure time"},
        {"高速运动已选择频闪/触发光源", "Strobe / triggered lighting is selected for high-speed motion"},
        {"反光/玻璃表面建议优先同轴光或穹顶光", "Reflective / glass surfaces should prefer coaxial or dome lighting"},
        {"远心测量建议优先远心平行背光", "Telecentric measurement should prefer telecentric collimated backlight"},
        {"标称 WD", "Nominal WD"},
        {"与当前工作距离偏差较大", "differs significantly from current working distance"},
        {"焦距", "Focal length"},
        {"接近粗算目标", "is close to the rough target"},
        {"与粗算目标", "and the rough target"},
        {"差异较大", "differ significantly"},
        {"光源有效照明面积小于需求 FOV，需要确认安装与亮度", "Light active illumination area is smaller than the required FOV; confirm installation and brightness"},
        {"光源覆盖余量低于 10%，边缘亮度可能不足", "Light coverage margin is below 10%; edge brightness may be insufficient"},
        {"光源覆盖需求 FOV，覆盖余量约", "Light covers the required FOV; coverage margin is about"},
        {"高速运动优先频闪光源以压低曝光时间", "High-speed motion prefers strobe lighting to reduce exposure time"},
        {"高速运动场景建议确认频闪能力和曝光时间", "For high-speed motion, confirm strobe capability and exposure time"},
        {"反光/透明表面使用当前光型可能需要额外控反光验证", "Reflective / transparent surfaces may need additional glare-control validation with the current light type"},
        {"远心镜头搭配远心平行背光可提高轮廓测量一致性", "Telecentric lenses paired with telecentric collimated backlight can improve contour measurement consistency"},
        {"镜头支持同轴照明，适合低对比表面特征", "Lens supports coaxial illumination, suitable for low-contrast surface features"},
        {"反光表面优先同轴光或穹顶光以降低眩光", "Reflective surfaces should prefer coaxial or dome lighting to reduce glare"},
        {"尺寸/边缘测量优先背光形成稳定轮廓", "Dimension / edge measurement should prefer backlight for a stable silhouette"},
        {"低角度条形/暗场光适合划痕和细小缺陷", "Low-angle bar / dark-field light is suitable for scratches and small defects"},
        {"官方原始规格字段", "Official Raw Spec Fields"}
    };
    value = replaceAll(value, phrase, sizeof(phrase) / sizeof(phrase[0]));

    static const Replacement catalog[] = {
        {"无法创建产品库数据目录", "Unable to create product catalog data directory"},
        {"无法创建产品库迁移备份目录", "Unable to create product catalog migration backup directory"},
        {"无法备份本机参数库文件", "Unable to back up the local catalog file"},
        {"无法打开 SQLite 产品库", "Unable to open SQLite product catalog"},
        {"无法读取产品库版本", "Unable to read product catalog version"},
        {"无法创建目录", "Unable to create directory"},
        {"无法覆盖文件", "Unable to overwrite file"},
        {"无法初始化产品库文件", "Unable to initialize product catalog file"},
        {"无法打开内置产品库", "Unable to open built-in product catalog"},
        {"缺少合并键字段", "is missing merge key field"},
        {"字段数量不匹配", "field count mismatch"},
        {"期望", "expected"},
        {"实际", "actual"},
        {"无法更新本地产品库文件", "Unable to update local product catalog file"},
        {"无法写入相机产品", "Unable to write camera product"},
        {"无法写入镜头产品", "Unable to write lens product"},
        {"无法写入光源产品", "Unable to write light product"},
        {"无法开始相机产品导入事务", "Unable to start camera product import transaction"},
        {"无法提交相机产品导入事务", "Unable to commit camera product import transaction"},
        {"无法开始镜头产品导入事务", "Unable to start lens product import transaction"},
        {"无法提交镜头产品导入事务", "Unable to commit lens product import transaction"},
        {"无法开始光源产品导入事务", "Unable to start light product import transaction"},
        {"无法提交光源产品导入事务", "Unable to commit light product import transaction"},
        {"无法读取相机产品库", "Unable to read camera product catalog"},
        {"无法读取镜头产品库", "Unable to read lens product catalog"},
        {"无法读取光源产品库", "Unable to read light product catalog"},
        {"相机行号无效", "Camera row index is invalid"},
        {"镜头行号无效", "Lens row index is invalid"},
        {"光源行号无效", "Light row index is invalid"},
        {"相机", "camera"},
        {"镜头", "lens"},
        {"光源", "light"},
        {"款", "items"},
        {"无法查询相机数量", "Unable to query camera count"},
        {"无法查询镜头数量", "Unable to query lens count"},
        {"无法查询光源数量", "Unable to query light count"},
        {"无法查询相机产品", "Unable to query camera products"},
        {"无法查询镜头产品", "Unable to query lens products"},
        {"无法查询光源产品", "Unable to query light products"},
        {"无法查询筛选项", "Unable to query filter values"},
        {"无法查询产品数量", "Unable to query product count"},
        {"相机 ID 无效", "Camera ID is invalid"},
        {"镜头 ID 无效", "Lens ID is invalid"},
        {"光源 ID 无效", "Light ID is invalid"},
        {"无法更新相机产品", "Unable to update camera product"},
        {"无法更新镜头产品", "Unable to update lens product"},
        {"无法更新光源产品", "Unable to update light product"},
        {"无法删除相机产品", "Unable to delete camera product"},
        {"无法删除镜头产品", "Unable to delete lens product"},
        {"无法删除光源产品", "Unable to delete light product"},
        {"无法查询光源候选", "Unable to query light candidates"},
        {"无法查询相机候选", "Unable to query camera candidates"},
        {"无法查询镜头候选", "Unable to query lens candidates"},
        {"无法写入 CSV", "Unable to write CSV"},
        {"无法打开 CSV", "Unable to open CSV"},
        {"相机数据无效", "Camera data is invalid"},
        {"镜头数据无效", "Lens data is invalid"},
        {"光源数据无效", "Light data is invalid"},
        {"型号、分辨率、像元尺寸必须有效", "model, resolution, and pixel size must be valid"},
        {"型号和像圆必须有效", "model and image circle must be valid"},
        {"型号和有效照明尺寸必须有效", "model and active illumination size must be valid"},
        {"远心镜头缺少 PMAG 放大倍率", "Telecentric lens is missing PMAG magnification"},
        {"普通镜头缺少焦距", "Fixed-focal lens is missing focal length"},
        {"行 CSV 引号字段未闭合", "CSV row has an unclosed quoted field"},
        {"没有表头", "has no header row"},
        {"没有数据行", "has no data rows"},
        {"缺少必需字段", "missing required field"},
        {"内置相机库", "built-in camera catalog"},
        {"内置镜头库", "built-in lens catalog"},
        {"内置光源库", "built-in light catalog"},
        {"新增相机", "new camera"},
        {"新增镜头", "new lens"},
        {"新增光源", "new light"},
        {"、", ", "}
    };
    value = replaceAll(value, catalog, sizeof(catalog) / sizeof(catalog[0]));

    static const Replacement cleanup[] = {
        {" 中", " in "},
        {" 第 ", " row "},
        {" 行", " "},
        {"远心Object-side pixel size", "Telecentric object-side pixel size"},
        {"camera采样meets targetObject-side pixel size", "camera sampling meets the target object-side pixel size"},
        {"Telecentric object-side pixel sizemeets target", "Telecentric object-side pixel size meets target"},
        {"Lens image circle/最大靶面不足以覆盖camera传感器", "Lens image circle / max sensor format is insufficient for the camera sensor"},
        {"最大靶面不足以覆盖camera传感器", "max sensor format is insufficient for the camera sensor"},
        {"覆盖当前需求", "covers the current requirement"},
        {"覆盖需求", "covers the requirement"}
    };
    value = replaceAll(value, cleanup, sizeof(cleanup) / sizeof(cleanup[0]));

    value.replace(QString::fromUtf8("："), QStringLiteral(": "));
    value.replace(QString::fromUtf8("，"), QStringLiteral(", "));
    value.replace(QString::fromUtf8("；"), QStringLiteral("; "));
    value.replace(QString::fromUtf8("。"), QStringLiteral("."));
    value.replace(QString::fromUtf8("×"), QStringLiteral("x"));
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    value.replace(QStringLiteral(" :"), QStringLiteral(":"));
    value.replace(QStringLiteral(" ,"), QStringLiteral(","));
    value.replace(QStringLiteral(" ;"), QStringLiteral(";"));
    value = value.trimmed();

    return value;
}

QString chineseDiagnostic(QString value)
{
    static const Replacement exact[] = {
        {"Unable to read private key file.", "无法读取私钥文件。"},
        {"Private key XML is not well formed.", "私钥 XML 格式不正确。"},
        {"Private key XML has inconsistent RSA component sizes.", "私钥 XML 的 RSA 组件长度不一致。"},
        {"Private key has not been loaded.", "尚未加载私钥。"},
        {"Licensee is required.", "必须填写授权对象。"},
        {"Machine code is required.", "必须填写机器码。"},
        {"Serial is required.", "必须填写序列号。"},
        {"Expiration date must be today or later.", "到期日期必须是今天或之后。"},
        {"License signing is only supported on Windows.", "注册码签名仅支持 Windows。"}
    };
    value = replaceAll(value, exact, sizeof(exact) / sizeof(exact[0]));

    static const Replacement phrase[] = {
        {"Private key XML is missing", "私钥 XML 缺少"},
        {"Unable to open RSA provider.", "无法打开 RSA provider。"},
        {"Unable to import RSA private key.", "无法导入 RSA 私钥。"},
        {"Unable to sign license payload.", "无法签名授权载荷。"},
        {"Unable to check duplicate camera product", "无法检查相机产品重复项"},
        {"Unable to check duplicate lens product", "无法检查镜头产品重复项"},
        {"Unable to check duplicate light product", "无法检查光源产品重复项"},
        {"Duplicate camera product", "相机产品重复"},
        {"Duplicate lens product", "镜头产品重复"},
        {"Duplicate light product", "光源产品重复"},
        {"Unable to query camera candidates", "无法查询相机候选"},
        {"Unable to query lens candidates", "无法查询镜头候选"}
    };
    value = replaceAll(value, phrase, sizeof(phrase) / sizeof(phrase[0]));
    return value;
}
}

namespace CoreI18n {

QString localizedText(const char *zhUtf8, const char *enUtf8)
{
    return isEnglish(LanguageManager::instance().currentLanguage())
        ? QString::fromUtf8(enUtf8)
        : QString::fromUtf8(zhUtf8);
}

QString localizedDiagnosticForLanguage(const QString &value, const QString &languageCode)
{
    return isEnglish(languageCode) ? englishDiagnostic(value) : chineseDiagnostic(value);
}

QString localizedDiagnostic(const QString &value)
{
    return localizedDiagnosticForLanguage(value, LanguageManager::instance().currentLanguage());
}

QStringList localizedDiagnostics(const QStringList &values)
{
    return localizedDiagnosticsForLanguage(values, LanguageManager::instance().currentLanguage());
}

QStringList localizedDiagnosticsForLanguage(const QStringList &values, const QString &languageCode)
{
    QStringList localized;
    localized.reserve(values.size());
    for (const QString &value : values)
        localized.append(localizedDiagnosticForLanguage(value, languageCode));
    return localized;
}

}
