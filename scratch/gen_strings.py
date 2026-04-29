import sys

def to_ucn(text):
    return "".join(f"\\u{ord(c):04x}" if ord(c) > 127 else c for c in text)

data = {
    "app_title": "RESEMBLE ENHANCE - プロフェッショナル・ダッシュボード",
    "status_proc": "処理中...",
    "status_ready": "システム待機中",
    "lang_btn_jp": "日本語",
    "sec_1": "1. 入力ソース",
    "browse": "参照...",
    "tip_dd": "(ヒント: .wavファイルをドラッグ＆ドロップできます)",
    "sec_2": "2. AIパラメータ設定",
    "denoise": "ノイズ除去強度",
    "steps": "推論ステップ数",
    "tau": "温度 (tau)",
    "load": "音声を読み込む",
    "enhance": "AI高音質化を実行",
    "sec_3": "3. 音声解析と出力",
    "wave_orig": "オリジナル波形",
    "wave_enh": "処理後波形",
    "save": "保存 (.WAV)",
    "play_enh": "処理後を再生",
    "play_orig": "オリジナルを再生",
    "stop": "停止",
    "sec_4": "4. システムログ",
    "copy": "ログをコピー",
    "log_loaded": "[System] 音声を読み込みました。",
    "log_err_load": "[Error] 音声ファイルを先に読み込んでください。",
    "log_saved": "[System] ファイルを保存しました。"
}

print("#pragma once")
print("#include <string>")
print("#include <windows.h>")
print("#include <vector>")
print("")
print("enum class Language { EN, JP };")
print("")
print("struct UIStrings {")
print("    static std::string toUTF8(const std::wstring& wstr) {")
print("        if (wstr.empty()) return \"\";")
print("        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);")
print("        std::vector<char> buffer(size);")
print("        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer.data(), size, NULL, NULL);")
print("        return std::string(buffer.data());")
print("    }")
print("")
print("    static std::string get(const std::string& id, Language lang) {")
print("        bool is_jp = (lang == Language::JP);")
print("")
for key, val in data.items():
    if key == "lang_btn_jp": continue
    en_val = val # Placeholder, I'll fix manually
    # Manual EN values for specific keys
    en_map = {
        "app_title": "RESEMBLE ENHANCE - PROFESSIONAL DASHBOARD",
        "status_proc": "PROCESSING...",
        "status_ready": "SYSTEM READY",
        "sec_1": "1. Input Audio Source",
        "browse": "BROWSE...",
        "tip_dd": "(Tip: You can drag and drop a .wav file here)",
        "sec_2": "2. AI Parameters & Configuration",
        "denoise": "Denoise Strength",
        "steps": "Solver Steps",
        "tau": "Temperature (tau)",
        "load": "LOAD AUDIO",
        "enhance": "ENHANCE AUDIO",
        "sec_3": "3. Audio Analysis & Output",
        "wave_orig": "Original Waveform",
        "wave_enh": "Enhanced Waveform",
        "save": "SAVE (.WAV)",
        "play_enh": "PLAY ENHANCED",
        "play_orig": "PLAY ORIGINAL",
        "stop": "STOP",
        "sec_4": "4. System Logs",
        "copy": "COPY LOGS",
        "log_loaded": "[System] Audio loaded.",
        "log_err_load": "[Error] Load audio first.",
        "log_saved": "[System] File saved."
    }
    en_val = en_map.get(key, val)
    jp_ucn = to_ucn(val)
    print(f"        if (id == \"{key}\") return is_jp ? toUTF8(L\"{jp_ucn}\") : \"{en_val}\";")

print("        if (id == \"lang_btn\") return is_jp ? \"English\" : toUTF8(L\"" + to_ucn("日本語") + "\");")
print("")
print("        return \"???\";")
print("    }")
print("};")
