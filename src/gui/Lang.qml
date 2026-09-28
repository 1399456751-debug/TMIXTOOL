pragma Singleton

import QtQuick

// Bilingual strings, kept as a plain lookup rather than Qt's .ts/.qm toolchain.
//
// This is a two-language utility with a few dozen labels; shipping the full
// translation pipeline would be more machinery than the problem deserves.
//
// Note: no string here contains an escape sequence. Multi-line copy is split
// across separate Text elements in the interface instead, which keeps this
// file free of the escaping that silently broke it before.
QtObject {
    id: lang

    property string code: "zh"

    readonly property var tables: ({
        "zh": {
            "appTitle":    "TMIXTOOL",
            "tagline":     "混音时间参考",
            "dropHint":    "拖入音频文件",
            "dropSub":     "或点击选择 · 支持 WAV / MP3",
            "analysing":   "正在分析",
            "bpm":         "速度",
            "key":         "调性",
            "keyNone":     "未检出调性",
            "tuning":      "调音",
            "tuningNone":  "未检出",
            "cents":       "音分",
            "confidence":  "把握",
            "uncertain":   "不确定",
            "folded":      "搜索得 %1，已折叠到偏好范围",
            "alternatives":"备选",
            "manualBpm":   "手动输入速度",
            "restore":     "恢复检测值",
            "delay":       "延迟",
            "delaySub":    "左右声道配对",
            "attack":      "压缩起始",
            "release":     "压缩释放",
            "releaseSub":  "实际会用的数值",
            "reverb":      "混响衰减",
            "preDelay":    "预延迟",
            "preDelaySub": "Haas 阈值 30 ms",
            "errorTitle":  "分析失败",
            "emptyHint":   "拖入音频文件，或直接输入速度",
            "emptySub":    "文件会给出速度、调性与调音；只输入速度也能立刻得到时值与效果器参考",
            "about":       "关于",
            "version":     "版本",
            "aboutBlurb":  "混音时间参考工具。分析音频文件，输出速度、调性与参考音高，并据此推导音符时值、延迟、压缩与混响的时间参考值。",
            "aboutNote":   "分析核心为独立的 C++ 库，不依赖界面框架，各项结果均可在命令行上独立复现与验证。"
        },
        "en": {
            "appTitle":    "TMIXTOOL",
            "tagline":     "Mixing time reference",
            "dropHint":    "Drop an audio file",
            "dropSub":     "or click to browse · WAV / MP3",
            "analysing":   "Analysing",
            "bpm":         "Tempo",
            "key":         "Key",
            "keyNone":     "No tonal centre",
            "tuning":      "Tuning",
            "tuningNone":  "not detected",
            "cents":       "cents",
            "confidence":  "confidence",
            "uncertain":   "UNCERTAIN",
            "folded":      "search found %1, folded into range",
            "alternatives":"Alternatives",
            "manualBpm":   "Set tempo by hand",
            "restore":     "Restore detected",
            "delay":       "Delay",
            "delaySub":    "stereo pairings",
            "attack":      "Compressor attack",
            "release":     "Compressor release",
            "releaseSub":  "values actually used",
            "reverb":      "Reverb decay",
            "preDelay":    "Pre-delay",
            "preDelaySub": "Haas threshold 30 ms",
            "errorTitle":  "Analysis failed",
            "emptyHint":   "Drop an audio file, or type a tempo",
            "emptySub":    "A file returns tempo, key and tuning; a typed tempo gives the note and effect timings straight away",
            "about":       "About",
            "version":     "Version",
            "aboutBlurb":  "A mixing time reference. Analyses an audio file for tempo, key and reference pitch, and derives note values, delay, compression and reverb timings from that tempo.",
            "aboutNote":   "The analysis core is a standalone C++ library with no GUI dependency, so every result can be reproduced and verified independently from the command line."
        }
    })

    // The use-case notes are produced by the analysis core, which stays free of
    // any GUI language. English is the canonical key; everything else is a
    // lookup, so switching the interface language does not require touching C++.
    readonly property var useZh: ({
        // attack
        "Brickwall - clip the peak, no movement":        "砖墙限幅 · 只削峰，不做动态",
        "Catch the very front of a transient":           "抓住瞬态最前端",
        "Kick and bass punch without losing the click":  "底鼓与贝斯有冲击力，又不丢 click",
        "Snare and drum bus - lets the hit through":     "军鼓与鼓组总线 · 让击打透过去",
        "Vocal control - softens peaks, stays open":     "人声控制 · 压峰但不发闷",
        "Bus glue - rides the level, not the note":      "总线胶水 · 控制电平而非音符",
        "Gentle levelling, transient passes through":    "温和拉平 · 瞬态原样通过",
        // release
        "Limiting - level returns before the next hit":  "限幅 · 下一次击打前电平已复位",
        "Drums - punchy, still reads as fast":           "鼓组 · 有冲击力，听感仍然快",
        "Drum bus - the usual starting point":           "鼓组总线 · 最常用的起点",
        "General purpose":                               "通用",
        "Vocals - smooths without pumping":              "人声 · 平滑而不抽气",
        "Program-dependent territory for most plugins":  "多数插件在此进入程序相关模式",
        "Bus glue - slow, lets transients breathe":      "总线胶水 · 慢，让瞬态呼吸",
        "Very slow levelling on a full mix":             "整混音上的极慢电平控制",
        // reverb
        "Very small room - air, no tail":                "极小房间 · 加空气感，无拖尾",
        "Tight plate - drums keep their punch":          "紧致板式 · 鼓组保留冲击力",
        "General purpose - vocals and drums":            "通用 · 人声与鼓组",
        "Hall - lets a phrase ring out":                 "大厅 · 让乐句延展",
        "Ambience - pads and sparse arrangements":       "氛围 · 铺底与留白编曲",
        // pre-delay
        "Glued - reverb starts with the source":         "紧贴 · 混响与音源同时起",
        "Barely separated, adds size only":              "刚分离 · 只增加体积感",
        "Tight - keeps the source upfront":              "紧凑 · 音源保持在前",
        "Small room":                                    "小房间",
        "Vocal clarity - the usual starting point":      "人声清晰度 · 最常用的起点",
        "Clear separation, still reads as one sound":    "明确分离，仍听成一个声音",
        "At the Haas limit - about to split in two":     "在 Haas 阈值上 · 即将裂成两个声音",
        "Slap begins - heard as a separate reflection":  "拍击开始 · 被听成独立反射声",
        "Wide - deliberate slapback":                    "宽阔 · 刻意的拍击延迟",
        "Large hall, source sits well forward":          "大厅 · 音源明显靠前",
        "Very wide - almost an echo":                    "极宽 · 接近回声",
        "Echo territory, no longer a pre-delay":         "回声范畴 · 已经不算预延迟"
    })

    function t (key) {
        const table = tables[code] || tables["zh"];
        return table[key] !== undefined ? table[key] : key;
    }

    // Translates a use-case note coming out of the analysis core. Falls back to
    // the English original so an untranslated string is visible rather than
    // blank.
    function use (english) {
        if (code !== "zh")
            return english;
        const translated = useZh[english];
        return translated !== undefined ? translated : english;
    }

    function toggle() {
        code = (code === "zh") ? "en" : "zh";
        codeChanged();
    }
}
