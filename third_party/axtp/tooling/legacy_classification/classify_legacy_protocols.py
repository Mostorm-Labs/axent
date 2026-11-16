#!/usr/bin/env python3
"""Classify legacy protocol entries into AXTP domain.feature buckets.

The output is an intake artifact, not registry truth. It lets reviewers see
each legacy command/method/event/config item beside its candidate AXTP
domain.feature, method/event name, and matching protocol document.
"""

from __future__ import annotations

import csv
import html
import json
import re
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
LEGACY_DIR = ROOT / "workspace" / "legacy-migration" / "evidence"
PROTOCOL_DIR = ROOT / "workspace" / "protocol"
GENERATED_PROTOCOL_JSON = ROOT / "contract" / "generated" / "protocol.json"
OUT_DIR = ROOT / "workspace" / "legacy-migration" / "classification"

METHOD_SPEC = "specs/30-registry.md"
EVENT_SPEC = "specs/30-registry.md"


def source_rel(path: Path) -> str:
    return str(path.relative_to(ROOT))


def discover_protocol_docs() -> dict[str, str]:
    docs: dict[str, str] = {}
    if not PROTOCOL_DIR.exists():
        return docs
    for path in sorted(PROTOCOL_DIR.glob("*/*.md")):
        if path.name == "README.md":
            continue
        capability = path.stem
        if "." not in capability:
            continue
        docs[capability] = source_rel(path)
    return docs


PROTOCOL_DOC_BY_CAPABILITY = discover_protocol_docs()


@dataclass
class Entry:
    source_protocol: str
    source_file: str
    source_line: int
    legacy_entry_type: str
    legacy_wire_name: str
    legacy_command_id: str
    legacy_command_name: str
    legacy_class: str
    legacy_method: str
    legacy_event: str
    legacy_config_name: str
    legacy_status: str
    legacy_payload_summary: str
    target_domain: str
    target_feature: str
    target_capability: str
    target_rpc_kind: str
    target_axtp_method: str
    target_axtp_event: str
    target_protocol_doc: str
    target_match_basis: str
    confidence: str
    rationale: str
    open_questions: str


def clean(text: object) -> str:
    if text is None:
        return ""
    s = str(text)
    s = html.unescape(s)
    s = s.replace("\\&\\#34;", '"').replace("&\\#34;", '"').replace("&#34;", '"')
    s = s.replace("<br>", " ").replace("<br/>", " ").replace("<br />", " ")
    s = re.sub(r"`([^`]*)`", r"\1", s)
    s = s.replace("\\-", "-").replace("\\.", ".").replace("\\_", "_")
    s = s.replace("\\|", "|")
    s = s.replace("~~", "").replace("*", "")
    s = re.sub(r"\s+", " ", s)
    return s.strip(" \t\r\n|")


def camel_to_words(name: str) -> str:
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", name)
    s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1 \2", s)
    return s.lower()


def lower_camel(words: str) -> str:
    parts = re.findall(r"[A-Za-z0-9]+", words)
    if not parts:
        return ""
    first = parts[0].lower()
    rest = [p[:1].upper() + p[1:].lower() for p in parts[1:]]
    return first + "".join(rest)


def pascal(feature: str) -> str:
    return feature[:1].upper() + feature[1:]


def split_md_row(line: str) -> list[str]:
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    cells: list[str] = []
    cur: list[str] = []
    escaped = False
    for ch in line:
        if escaped:
            cur.append(ch)
            escaped = False
            continue
        if ch == "\\":
            escaped = True
            cur.append(ch)
            continue
        if ch == "|":
            cells.append(clean("".join(cur)))
            cur = []
        else:
            cur.append(ch)
    cells.append(clean("".join(cur)))
    return cells


def load_symbol_sources() -> tuple[dict[str, set[str]], set[str], set[str]]:
    token_sources: dict[str, set[str]] = defaultdict(set)
    methods: set[str] = set()
    events: set[str] = set()

    token_pat = re.compile(r"\b[a-z][a-z0-9]*\.[A-Za-z][A-Za-z0-9]*\b")
    method_pat = re.compile(r'"method"\s*:\s*"([a-z][a-z0-9]*\.[A-Za-z][A-Za-z0-9]*)"')
    event_pat = re.compile(r'"event"\s*:\s*"([a-z][a-z0-9]*\.[A-Za-z][A-Za-z0-9]*)"')

    for path in sorted(PROTOCOL_DIR.rglob("*.md")):
        rel = source_rel(path)
        text = path.read_text(encoding="utf-8", errors="ignore")
        for token in token_pat.findall(text):
            token_sources[token].add(rel)
        for token in method_pat.findall(text):
            methods.add(token)
        for token in event_pat.findall(text):
            events.add(token)

    if GENERATED_PROTOCOL_JSON.exists():
        rel = source_rel(GENERATED_PROTOCOL_JSON)
        model = json.loads(GENERATED_PROTOCOL_JSON.read_text(encoding="utf-8"))
        for item in model.get("methods", []):
            token = item.get("name")
            if isinstance(token, str):
                token_sources[token].add(rel)
                methods.add(token)
        for item in model.get("events", []):
            token = item.get("name")
            if isinstance(token, str):
                token_sources[token].add(rel)
                events.add(token)
    return token_sources, methods, events


TOKEN_SOURCES, KNOWN_METHODS, KNOWN_EVENTS = load_symbol_sources()


def classify(raw_name: str, context: str = "", source_protocol: str = "") -> tuple[str, str, str, str, str, str, str]:
    """Return domain, feature, target_kind, action_template, confidence, rationale, open_questions."""

    name = clean(raw_name)
    name_lower = name.lower()
    words = camel_to_words(name)
    blob = f"{name} {words} {context}".lower()
    compact_blob = re.sub(r"[^a-z0-9]+", "", blob)
    word_tokens = set(re.findall(r"[a-z0-9]+", blob))

    def result(domain: str, feature: str, kind: str, template: str, confidence: str, why: str, questions: str = ""):
        return domain, feature, kind, template, confidence, why, questions

    if "config.multiset:video" in blob or "config.multiget:video" in blob:
        return result("video", "framing", "method", "config", "medium", "VM33 Video.mode=auto-framing，归 video.framing。")
    if "config.multiget:camera" in blob or "config.multiset:camera" in blob:
        return result("camera", "image", "method", "config", "low", "VM33 Camera 配置缺少字段，暂归 camera.image。", "补充 Camera 配置字段后确认是否拆到 exposure/focus/whiteBalance。")
    if "config." in blob and ":led" in blob:
        return result("device", "indicator", "method", "config", "high", "LED 配置归 device.indicator。")
    if "apinfo" in blob:
        return result("network", "ap", "method", "config_or_state", "high", "APInfo 配置和状态归 network.ap。")
    if any(k in name_lower for k in ["configjson", "defaultconfigjson", "debugjson", "config json"]):
        return result(
            "vendor",
            "genericConfig",
            "adapter_only",
            "needs_split",
            "medium",
            "老协议为泛 Config JSON；Naming/YAML mapping specs 要求按具体 payload 再拆 domain.feature。",
            "补充 JSON schema 或 Config Name 后再拆成正式能力。",
        )
    if ":" not in name and re.match(r"^config\.(set|setmulti|multiset|get|getmulti|multiget|restore|restoremulti|subscribe|unsubscribe)\b", name.lower()):
        return result(
            "vendor",
            "genericConfig",
            "adapter_only",
            "needs_split",
            "high",
            "VM33 泛 Config wire method 只作为适配入口；正式能力必须按 Param.Name 拆分。",
            "按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。",
        )

    if "firmware" in blob or "upgrade" in blob or "ota" in blob or "update progress" in blob or "remoteupgrade" in blob or "updateconfig" in blob or "update config" in blob:
        if "policy" in blob or "autoupdate" in blob or "channel" in blob or "updateconfig" in blob or "update config" in blob:
            return result("firmware", "updatePolicy", "method", "config", "high", "升级策略配置归 firmware.updatePolicy。")
        return result("firmware", "ota", "stream_or_method", "stream", "high", "升级/远程升级/升级进度归 firmware.ota，数据块通过业务域建流。")

    if "fbf" in blob or "streamfile" in blob or "ftpserver" in blob or "downloadstream" in blob or "uploadstream" in blob or "fileinfo" in blob or "trackfile" in blob or "savepath" in blob or "file transfer" in blob:
        return result("file", "transfer", "stream_or_method", "stream", "high", "文件上传、下载、校验归 file.transfer；stream 域不承载业务分类。")

    if "log" in blob:
        if "stream" in blob:
            return result("log", "stream", "stream_or_event", "stream", "high", "实时日志流归 log.stream。")
        if "file" in blob or "list" in blob or "delete" in blob:
            return result("log", "files", "method_or_event", "state", "medium", "日志文件列表和删除归 log.files。")
        return result("log", "export", "method_or_event", "export", "high", "日志打包、上传、导出结果归 log.export。")

    if "rtsp" in blob:
        return result("video", "rtsp", "method", "config", "high", "RTSP 服务配置归 video.rtsp，不归 network。")
    if "ndi" in blob:
        return result("video", "ndi", "method", "config", "high", "NDI 服务配置归 video.ndi，不归 network。")
    if "mjpeg" in blob:
        return result("video", "stream", "method", "config", "high", "codec 不作为 feature，MJPEG 归 video.stream 或 encoder 配置。")

    if "playlist item" in blob or "itemurl" in blob or "item url" in blob or ("media" in blob and "signage" in source_protocol):
        return result("signage", "media", "method", "state", "high", "播放项资源 URL 与媒体资源归 signage.media。")
    if "playlist" in blob:
        return result("signage", "playlist", "method", "config", "high", "播放列表配置归 signage.playlist。")
    if "appearance" in blob or "videoshowosd" in blob or "osd" in blob or "watermark" in blob:
        if "video" in blob and "show" in blob:
            return result("video", "osd", "method", "config", "high", "视频显示 OSD 归 video.osd；数字标牌外观才归 signage.osd。")
        if "signage" in source_protocol or "appearance" in blob:
            return result("signage", "osd", "method", "config", "medium", "数字标牌外观/面板配置归 signage.osd。")
        return result("video", "osd", "method", "config", "high", "OSD/水印/叠加显示归 video.osd。")
    if "schedule" in blob or "calendaring" in blob or "meeting" in blob:
        if "shutdown" in blob or "reboot" in blob or "scheduleconfig" in blob and "signage" not in source_protocol:
            return result("system", "lifecycle", "method", "config", "medium", "定时关机/重启属于系统生命周期策略，不是播放计划。")
        if "signage" in source_protocol:
            return result("signage", "schedule", "method", "config", "high", "数字标牌播放计划归 signage.schedule。")
        return result("room", "schedule", "method", "config", "high", "会议/日程归 room.schedule。")
    if "playback" in blob and "mode" not in blob:
        if "signage" in source_protocol:
            return result("signage", "playback", "method_or_event", "state", "high", "数字标牌播放状态归 signage.playback。")

    if any(k in blob for k in ["wifi", "wi-fi", "wlan", "tailwifi"]):
        if "test" in blob or "producttest" in blob:
            return result("diagnostic", "networkTest", "method", "action", "high", "Wi-Fi 产测/连通性测试归 diagnostic.networkTest。")
        if "apinfo" in blob or "apservice" in blob or "softap" in blob or re.search(r"\bap\b", blob):
            return result("network", "ap", "method", "config_or_action", "high", "AP/SoftAP 配置、开启、状态归 network.ap。")
        return result("network", "wifi", "method", "config_or_action", "high", "Wi-Fi 配置、扫描、连接和状态统一归 network.wifi。")
    if "bluetooth" in blob or "blue tooth" in blob:
        if "test" in blob or "bqb" in blob:
            return result("diagnostic", "networkTest", "method", "action", "medium", "蓝牙 BQB/产测归 diagnostic.networkTest。")
        return result("network", "bluetooth", "method", "config_or_state", "high", "蓝牙信息、名称、MAC 与恢复归 network.bluetooth。")
    if "devicediscovery" in blob or "device discovery" in blob:
        return result("network", "serviceEndpoint", "method", "state", "high", "设备发现返回地址和协议入口，归 network.serviceEndpoint；设备身份字段进入 response schema。")
    if any(k in blob for k in ["network", "netmask", "gateway", "dhcp", "ipaddress", "ip config", "macaddress", "mac addr", "setmac", "getmac"]):
        if "test" in blob or "producttest" in blob:
            return result("diagnostic", "networkTest", "method", "action", "high", "网络端口/MAC 产测归 diagnostic.networkTest 或 diagnostic.manufacturing。")
        if "mac" in blob:
            return result("network", "interface", "method", "config_or_state", "medium", "MAC 地址属于 network.interface 标识字段。")
        return result("network", "ip", "method", "config_or_state", "high", "IP/DHCP/netmask/gateway/default route 归 network.ip。")
    if "serviceendpoint" in blob or "visca" in blob or ("url" in blob and "live" not in blob and "playlist" not in blob):
        return result("network", "serviceEndpoint", "method", "config", "medium", "服务端口/服务入口信息归 network.serviceEndpoint；具体业务服务可再拆。")

    if any(k in blob for k in ["audio record", "audiorecord", "recording stream", "record data"]):
        return result("audio", "recording", "stream_or_method", "stream", "high", "音频录制与音频数据上报归 audio.recording。")
    if re.search(r"\b(eq|equalizer)\b", words) or "recordeq" in blob or "audioeq" in blob:
        return result("audio", "eq", "method", "config", "high", "EQ 参数、模式和配置归 audio.eq。")
    if any(k in blob for k in ["noise", "echo", "reverberation", "dereveration", "agc", "algorithm", "alg", "beam", "doa", "auto director", "speaker highlight"]):
        return result("audio", "algorithm", "method_or_event", "config_or_state", "medium", "音频算法、波束、DOA、降噪/回声/混响抑制归 audio.algorithm。")
    if "dante" in blob:
        return result("audio", "dante", "method", "config", "high", "Dante 设备授权、厂商信息归 audio.dante。")
    if any(k in blob for k in ["volume", "gain", "volup", "voldown", "linein", "line in", "line-out", "line out", "mute", "mic", "uac", "audio input", "audioin", "audioconfig", "audio work", "audio mix", "playback mode"]):
        if "mix" in blob or "mixer" in blob:
            return result("audio", "mixer", "method", "config", "high", "混音器与混音模式归 audio.mixer。")
        if "audio input" in blob or "linein" in blob or "line in" in blob or "mic" in blob or "audioin" in blob or "audioconfig" in blob:
            return result("audio", "input", "method", "config_or_state", "high", "音频输入、Line-in、Mic、音源选择归 audio.input。")
        if "playback" in blob:
            return result("audio", "playback", "method", "config_or_state", "high", "音频播放模式归 audio.playback。")
        if "uac" in blob:
            return result("audio", "uac", "method", "config_or_state", "high", "UAC 开关和状态归 audio.uac。")
        return result("audio", "volume", "method", "config_or_state", "high", "音量、增益、静音归 audio.volume。")

    if "zoom" in blob:
        return result("camera", "zoom", "method", "config_or_action", "high", "变焦归 camera.zoom。")
    if any(k in blob for k in ["focus", "manualfocus", "afcalibration", "lenscenter"]):
        if "calibration" in blob:
            return result("camera", "calibration", "method", "action", "high", "AF/镜头校准归 camera.calibration。")
        return result("camera", "focus", "method", "config_or_action", "high", "自动/手动聚焦归 camera.focus。")
    if "gyro" in blob or "slope angle" in blob:
        return result("sensor", "motion", "method_or_event", "state", "medium", "陀螺仪/倾角属于传感器状态，候选为 sensor.motion。")
    if any(k in blob for k in ["exposure", "backlight", "highlight", "shutter", "wdr", "power line", "powerline"]):
        return result("camera", "exposure", "method", "config", "high", "曝光、快门、WDR、补偿和工频归 camera.exposure。")
    if any(k in blob for k in ["image", "mirror", "flip", "sightangle", "sight angle", "watermark"]):
        if "mirror" in blob or "flip" in blob:
            return result("video", "outputTransform", "method", "config", "high", "镜像/翻转等输出变换归 video.outputTransform。")
        return result("camera", "image", "method", "config", "medium", "图像风格/视角等图像参数归 camera.image。")

    if any(k in blob for k in ["videomode", "track", "framing", "people", "allchannel", "startupposition", "notarget", "regiontracking", "frame rate", "framerate", "verticalscreen"]):
        return result("video", "framing", "method_or_event", "config_or_state", "medium", "追踪、构图、人数、帧率和智能画面状态归 video.framing。")
    if "positionnumberjson" in blob:
        return result("camera", "ptz", "method", "config", "medium", "机位/预置位编号 JSON 候选归 camera.ptz。", "确认 position number 是否为 PTZ preset 或房间座位编号。")
    if any(k in blob for k in ["pip", "split", "layout", "curtain", "scene out", "outplaymode", "sharemode", "byom", "hdmiout", "typecout", "video mixer"]):
        if "curtain" in blob or "scene out" in blob or "outplaymode" in blob or "sharemode" in blob or "hdmiout" in blob or "typecout" in blob or "byom" in blob:
            return result("output", "layout", "method", "config", "medium", "幕墙/输出布局/输出模式按 09 归 output.layout。")
        return result("video", "layout", "method", "config", "medium", "画中画、拼接和视频布局归 video.layout。")
    if any(k in blob for k in ["stream", "liveaddress", "pushstream", "livestream", "play stream", "video status", "videomgr"]):
        return result("video", "stream", "stream_or_method", "stream", "high", "视频流、直播推流、播放流状态归 video.stream。")
    if "record" in blob and "audio" not in blob:
        if "sd" in blob or "select" in blob or "remove" in blob or "delete" in blob or "list" in blob or "date" in blob:
            return result("storage", "recording", "method", "state_or_action", "medium", "录像文件查询/删除归 storage.recording。")
        return result("video", "recording", "method_or_event", "state_or_action", "high", "视频录像开始/停止/状态归 video.recording。")

    if any(k in blob for k in ["inputsource", "input source", "open net input", "close net input"]):
        if source_protocol.startswith("rooms") or "vm33" in source_protocol:
            return result("room", "source", "method", "config_or_action", "high", "会议室输入源管理归 room.source。")
        return result("input", "source", "method", "config_or_action", "high", "输入源归 input.source。")
    if any(k in blob for k in ["outputsource", "output source", "output interface"]):
        return result("output", "source", "method", "config_or_state", "high", "输出源和输出接口归 output.source。")
    if "scene" in blob:
        return result("room", "layout", "method", "config_or_action", "medium", "会议室场景和布局归 room.layout。")
    if any(k in blob for k in ["participant", "meeting", "amx100", "child device", "devicelist", "externaldevice"]):
        if "amx100" in blob:
            return result("device", "childDevice", "method_or_event", "config_or_state", "medium", "AMX100/子设备连接与信息归 device.childDevice，联动算法另归 audio.algorithm。")
        return result("room", "participant", "method_or_event", "state", "medium", "参与设备/会议成员关系归 room.participant。")

    if "dumpinfo" in blob:
        return result("diagnostic", "report", "method", "export", "medium", "DumpInfo 属于诊断信息或报告导出候选，归 diagnostic.report。")
    storage_hit = (
        "sd" in word_tokens
        or "sdcard" in compact_blob
        or "disk" in word_tokens
        or "ddr" in word_tokens
        or "storage" in word_tokens
        or "usbstorage" in compact_blob
        or "badflash" in compact_blob
        or "flash block" in blob
    )
    if storage_hit:
        if "test" in blob or "producttest" in blob or "badflash" in blob:
            return result("diagnostic", "storageTest", "method", "action", "high", "存储、U 盘、坏块产测归 diagnostic.storageTest。")
        if "sd" in word_tokens or "sdcard" in compact_blob:
            return result("storage", "sdCard", "method_or_event", "state_or_action", "high", "SD 卡状态、信息和格式化归 storage.sdCard。")
        return result("storage", "disk", "method", "state", "medium", "磁盘/容量信息归 storage.disk。")

    if any(k in blob for k in ["kvm", "hidcall", "usb", "combo key", "combokey", "key", "button", "gpio", "0x02 in"]):
        if "test" in blob or "producttest" in blob or "resetbutton" in blob:
            if "kvm" in blob:
                return result("diagnostic", "kvmTest", "method", "action", "high", "KVM 产测归 diagnostic.kvmTest。")
            return result("diagnostic", "inputTest", "method", "action", "high", "按键/USB/输入产测归 diagnostic.inputTest。")
        if "kvm" in blob:
            return result("input", "kvm", "method", "action", "high", "KVM 数据和控制归 input.kvm。")
        if "hid" in blob or "usb" in blob:
            return result("input", "hid", "method_or_event", "config_or_state", "medium", "HID/USB 控制和报告归 input.hid。")
        return result("input", "key", "method_or_event", "config_or_state", "high", "本地按键、组合键和按键状态归 input.key。")

    if "privacy" in blob:
        return result("privacy", "state", "method", "config_or_state", "high", "隐私开关/状态归 privacy.state。")
    if any(k in blob for k in ["bind", "passwd", "password", "auth", "token", "license", "lic"]):
        if "alg" in blob or "dante" in blob:
            return result("vendor", "license", "method", "config", "medium", "算法/Dante 授权属于模块授权，08 建议归 diagnostic.* 或 vendor.*。")
        if "passwd" in blob or "token" in blob:
            return result("auth", "token", "method", "config_or_state", "medium", "密码/token 属于 auth.token。")
        return result("auth", "session", "method_or_event", "config_or_state", "medium", "设备绑定/绑定码/绑定状态归 auth.session 候选。")

    if "system.subscribe" in blob:
        return result("device", "inventory", "event", "state", "medium", "System.Subscribe 在 VM33 中用于 GetDeviceList 变化，归 device.inventory。")
    if "menu" in blob and "language" in blob:
        return result("video", "osd", "method", "config", "medium", "菜单语言属于设备 UI/OSD 配置，候选归 video.osd。")
    if any(k in blob for k in ["deviceinfo", "devinfo", "device info", "devicetype", "devtype", "device name", "devname", "sn", "serial", "unique id", "uid", "hwid", "encryptedinfo"]):
        if "producttest" in blob or ("set" in words and any(k in blob for k in ["sn", "mac", "hwid", "encrypted"])):
            return result("diagnostic", "manufacturing", "method", "config_or_action", "high", "SN/MAC/HWID/加密信息写入归 diagnostic.manufacturing。")
        if "type" in blob or "identity" in blob or "sn" in blob or "unique" in blob or "hwid" in blob:
            return result("device", "identity", "method", "state_or_config", "high", "设备型号、SN、UID、HWID 等归 device.identity。")
        return result("device", "info", "method", "state", "high", "设备基础信息归 device.info。")
    if any(k in blob for k in ["bootdetect", "bootinfo", "booterase"]):
        return result("system", "initialization", "method", "state_or_action", "medium", "Boot detect/info/erase 属于启动或初始化流程，归 system.initialization。")
    if any(k in blob for k in ["time", "timezone", "sys time", "settime", "gettime", "synctime"]):
        return result("system", "time", "method", "config_or_state", "high", "校时、系统时间和时区归 system.time。")
    if any(k in blob for k in ["reboot", "reset", "restoreall", "initreset", "autopower", "shutdown", "live"]):
        if "resetconfig" in blob or "restore" in blob or "initreset" in blob or "reset" in blob:
            return result("system", "initialization", "method", "action", "medium", "恢复配置/初始化/复位归 system.initialization。")
        if "reboot" in blob or "shutdown" in blob:
            return result("system", "lifecycle", "method", "action_or_config", "high", "重启/关机/生命周期策略归 system.lifecycle。")
        return result("system", "lifecycle", "method", "state_or_config", "medium", "系统生命周期状态归 system.lifecycle。")
    if any(k in blob for k in ["state", "status", "telemetry", "power", "battery", "keepalive"]):
        if "power" in blob or "battery" in blob:
            return result("device", "power", "method_or_event", "state", "high", "电量/电源状态归 device.power。")
        if "keepalive" in blob:
            return result("system", "lifecycle", "method_or_event", "state", "medium", "保活与连接生命周期归 system.lifecycle 或协议会话层。")
        if "telemetry" in blob:
            return result("sensor", "telemetry", "event", "reported", "low", "遥测上报可候选为 sensor.telemetry，需确认是否拆为 device.power/sensor.*。", "确认遥测字段集合后再定 feature。")
        return result("device", "state", "method_or_event", "state", "medium", "泛设备状态查询归 device.state，后续可按字段拆细。")

    if "videoplay" in blob:
        return result("diagnostic", "videoTest", "method", "action", "high", "VideoPlay 出现在产测/解码测试上下文，归 diagnostic.videoTest。")
    if "audioplay" in blob:
        return result("diagnostic", "audioTest", "method", "action", "high", "AudioPlay 出现在产测/播放测试上下文，归 diagnostic.audioTest。")
    if "test" in blob or "producttest" in blob:
        if "audio" in blob or "line" in blob:
            return result("diagnostic", "audioTest", "method", "action", "high", "音频产测归 diagnostic.audioTest。")
        if "video" in blob:
            return result("diagnostic", "videoTest", "method", "action", "high", "视频产测归 diagnostic.videoTest。")
        return result("diagnostic", "selfTest", "method", "action", "medium", "未细分产测默认归 diagnostic.selfTest。")

    return result(
        "vendor",
        "uncategorized",
        "needs_review",
        "unknown",
        "low",
        "未命中 08 推荐 domain.feature；暂归 vendor.uncategorized，需人工确认。",
        "补充业务语义、payload 字段和调用场景。",
    )


def make_wire_name(entry_type: str, command_name: str, legacy_class: str, legacy_method: str, legacy_event: str, legacy_config_name: str) -> str:
    if entry_type == "config_name":
        return f"{legacy_class}.{legacy_method}:{legacy_config_name}"
    if legacy_class and legacy_method:
        return f"{legacy_class}.{legacy_method}"
    if legacy_method:
        return legacy_method
    if legacy_event:
        return legacy_event
    return command_name


def classify_entry(
    source_protocol: str,
    source_file: str,
    source_line: int,
    legacy_entry_type: str,
    legacy_command_id: str = "",
    legacy_command_name: str = "",
    legacy_class: str = "",
    legacy_method: str = "",
    legacy_event: str = "",
    legacy_config_name: str = "",
    legacy_status: str = "",
    legacy_payload_summary: str = "",
) -> Entry:
    legacy_wire_name = make_wire_name(legacy_entry_type, legacy_command_name, legacy_class, legacy_method, legacy_event, legacy_config_name)
    classify_name = legacy_wire_name or legacy_command_name or legacy_method or legacy_event or legacy_config_name
    context = " ".join(
        clean(x)
        for x in [
            legacy_entry_type,
            legacy_command_name,
            legacy_class,
            legacy_method,
            legacy_event,
            legacy_config_name,
            legacy_status,
            legacy_payload_summary,
        ]
        if x
    )
    domain, feature, target_kind, template, confidence, rationale, questions = classify(classify_name, context, source_protocol)
    capability = f"{domain}.{feature}"
    target_method, target_event, target_rpc_kind = suggest_target_rpc(
        domain=domain,
        feature=feature,
        target_kind=target_kind,
        template=template,
        legacy_entry_type=legacy_entry_type,
        legacy_wire_name=legacy_wire_name,
        legacy_command_name=legacy_command_name,
        legacy_class=legacy_class,
        legacy_method=legacy_method,
        legacy_event=legacy_event,
        legacy_config_name=legacy_config_name,
    )
    target_doc = protocol_doc_for(capability)
    match_basis = target_basis(target_method, target_event, target_doc, capability)
    return Entry(
        source_protocol=source_protocol,
        source_file=source_file,
        source_line=source_line,
        legacy_entry_type=legacy_entry_type,
        legacy_wire_name=legacy_wire_name,
        legacy_command_id=legacy_command_id,
        legacy_command_name=legacy_command_name,
        legacy_class=legacy_class,
        legacy_method=legacy_method,
        legacy_event=legacy_event,
        legacy_config_name=legacy_config_name,
        legacy_status=legacy_status,
        legacy_payload_summary=clean(legacy_payload_summary)[:260],
        target_domain=domain,
        target_feature=feature,
        target_capability=capability,
        target_rpc_kind=target_rpc_kind,
        target_axtp_method=target_method,
        target_axtp_event=target_event,
        target_protocol_doc=target_doc,
        target_match_basis=match_basis,
        confidence=confidence,
        rationale=rationale,
        open_questions=questions,
    )


def protocol_doc_for(capability: str) -> str:
    rel = PROTOCOL_DOC_BY_CAPABILITY.get(capability, "")
    if rel and (ROOT / rel).exists():
        return rel
    return ""


def target_basis(method: str, event: str, protocol_doc: str, capability: str) -> str:
    token = event or method
    if token and protocol_doc and token in TOKEN_SOURCES and protocol_doc in TOKEN_SOURCES[token]:
        return f"matched workspace/protocol ({protocol_doc})"
    if protocol_doc:
        return f"capability has workspace/protocol draft ({protocol_doc}); method/event is taxonomy/spec candidate"
    if method and method in KNOWN_METHODS:
        return f"matched method registry ({METHOD_SPEC})"
    if event and event in KNOWN_EVENTS:
        return f"matched event registry ({EVENT_SPEC})"
    if capability.startswith("vendor."):
        return "adapter-only / needs split before registry"
    return "Naming and Taxonomy spec candidate; workspace/protocol document not created yet"


def contains_any(blob: str, *parts: str) -> bool:
    return any(part in blob for part in parts)


def verb_flags(blob: str) -> dict[str, bool]:
    compact = re.sub(r"[^a-z0-9]+", "", blob)
    tokens = set(re.findall(r"[a-z0-9]+", blob))

    def has_token(*names: str) -> bool:
        return any(name in tokens for name in names)

    def starts_with(*prefixes: str) -> bool:
        return any(compact.startswith(prefix) for prefix in prefixes)

    return {
        "get": has_token("get", "query", "select", "list") or starts_with("get", "query", "select", "list"),
        "set": has_token("set", "create", "update") or starts_with("set", "create", "update"),
        "reset": has_token("reset", "restore", "default", "initreset") or starts_with("reset", "restore", "initreset"),
        "start": has_token("start", "begin", "open", "setup", "remoteupgrade") or starts_with("start", "begin", "open", "remoteupgrade", "setup"),
        "stop": has_token("stop", "close", "end") or starts_with("stop", "close", "end"),
        "delete": has_token("delete", "remove") or starts_with("delete", "remove"),
        "progress": has_token("progress") or "progress" in compact,
        "status": has_token("status", "state") or compact.endswith("status") or compact.endswith("state"),
        "scan": has_token("scan") or starts_with("scan"),
        "connect": (has_token("connect") or starts_with("connect")) and "disconnect" not in compact,
        "disconnect": has_token("disconnect", "unconnect") or starts_with("disconnect", "unconnect"),
    }


def is_read_request(flags: dict[str, bool]) -> bool:
    if flags["get"]:
        return True
    if not flags["status"]:
        return False
    return not any(flags[k] for k in ("set", "reset", "start", "stop", "delete", "connect", "disconnect"))


def suggest_target_rpc(
    domain: str,
    feature: str,
    target_kind: str,
    template: str,
    legacy_entry_type: str,
    legacy_wire_name: str,
    legacy_command_name: str,
    legacy_class: str,
    legacy_method: str,
    legacy_event: str,
    legacy_config_name: str,
) -> tuple[str, str, str]:
    if target_kind == "adapter_only" or feature in {"genericConfig", "uncategorized"}:
        return "", "", "adapter_only"

    blob = " ".join(
        [
            legacy_wire_name,
            legacy_command_name,
            legacy_class,
            legacy_method,
            legacy_event,
            legacy_config_name,
            camel_to_words(legacy_wire_name),
        ]
    ).lower()
    flags = verb_flags(blob)
    capability = f"{domain}.{feature}"

    if legacy_entry_type in {"event", "hid_report"} or legacy_event:
        event = event_for(capability, blob, template)
        return "", event, "event"

    if legacy_method.lower() in {"subscribe", "unsubscribe"}:
        event = event_for(capability, blob, template)
        return "", event, "event_subscription"

    compact_legacy = re.sub(r"[^a-z0-9]+", "", legacy_wire_name.lower())
    if compact_legacy.startswith("notify") or compact_legacy.startswith("on"):
        event = event_for(capability, blob, template)
        return "", event, "event_candidate"

    method = method_for(capability, blob, flags, template)
    if method:
        kind = "method"
        if "stream" in target_kind or template == "stream":
            kind = "stream_owner_method"
        return method, "", kind

    if target_kind == "event":
        return "", event_for(capability, blob, template), "event"

    fname = pascal(feature)
    if flags["reset"]:
        return f"{domain}.reset{fname}Config", "", "method"
    if flags["get"]:
        if template in {"state", "state_or_action"} or flags["status"]:
            return f"{domain}.get{fname}State", "", "method"
        return f"{domain}.get{fname}Config", "", "method"
    if flags["set"]:
        return f"{domain}.set{fname}Config", "", "method"
    return f"{domain}.{lower_camel(legacy_wire_name)}", "", "method_candidate"


def method_for(capability: str, blob: str, flags: dict[str, bool], template: str) -> str:
    read = is_read_request(flags)

    if capability == "network.wifi":
        if flags["scan"]:
            return "network.scanWifi"
        if flags["connect"]:
            return "network.connectWifi"
        if flags["disconnect"]:
            return "network.disconnectWifi"
        if "forget" in blob:
            return "network.forgetWifi"
        if "signal" in blob or (flags["status"] and not flags["get"]):
            return "network.getWifiState"
        if flags["reset"]:
            return "network.resetWifiConfig"
        return "network.getWifiConfig" if flags["get"] else "network.setWifiConfig"
    if capability == "network.ap":
        if flags["start"] or "openapservice" in blob:
            return "network.startAp"
        if flags["stop"]:
            return "network.stopAp"
        if "client" in blob:
            return "network.getApClients"
        if flags["status"] and not flags["get"]:
            return "network.getApState"
        if flags["reset"]:
            return "network.resetApConfig"
        return "network.getApConfig" if flags["get"] else "network.setApConfig"
    if capability == "network.ip":
        return "network.getIpConfig" if read else "network.setIpConfig"
    if capability == "network.interface":
        if "mac" in blob and not read:
            return "network.setInterfaceConfig"
        if "mac" in blob or "info" in blob:
            return "network.getInterfaceInfo"
        return "network.getInterfaces"
    if capability == "network.bluetooth":
        if read or "mac" in blob or "info" in blob:
            return "network.getBluetoothInfo"
        if flags["reset"]:
            return "network.resetBluetoothConfig"
        return "network.setBluetoothConfig"
    if capability == "network.serviceEndpoint":
        return "network.getServiceEndpointState" if read else "network.setServiceEndpointConfig"

    if capability == "video.ndi":
        if "cap" in blob:
            return "video.getNdiCapabilities"
        if flags["reset"]:
            return "video.resetNdiConfig"
        return "video.getNdiConfig" if read else "video.setNdiConfig"
    if capability == "video.rtsp":
        if "cap" in blob:
            return "video.getRtspCapabilities"
        if flags["reset"]:
            return "video.resetRtspConfig"
        return "video.getRtspConfig" if read else "video.setRtspConfig"
    if capability == "video.stream":
        if flags["stop"]:
            return "video.closeStream"
        if read:
            return "video.getStreamState"
        if "keyframe" in blob:
            return "video.requestKeyFrame"
        if "config" in blob and flags["set"]:
            return "video.setStreamConfig"
        return "video.openStream"
    if capability == "video.framing":
        if "cap" in blob:
            return "video.getFramingCapabilities"
        if flags["reset"]:
            return "video.resetFramingConfig"
        if "mode" in blob or "track" in blob or "verticalscreen" in blob:
            return "video.getFramingMode" if read else "video.setFramingMode"
        return "video.getFramingConfig" if read else "video.setFramingConfig"
    if capability == "video.outputTransform":
        if flags["reset"]:
            return "video.resetOutputTransform"
        return "video.getOutputTransform" if read else "video.setOutputTransform"
    if capability == "video.pip":
        if flags["reset"]:
            return "video.resetPipConfig"
        return "video.getPipConfig" if read else "video.setPipConfig"
    if capability == "video.layout":
        return "video.getLayoutConfig" if read else "video.setLayoutConfig"
    if capability == "video.scene":
        return "video.getSceneConfig" if read else "video.setSceneConfig"
    if capability == "video.osd":
        return "video.getOsdConfig" if read else "video.setOsdConfig"
    if capability == "video.recording":
        if flags["start"]:
            return "video.startRecording"
        if flags["stop"]:
            return "video.stopRecording"
        return "video.getRecordingState"

    if capability == "audio.algorithm":
        if "cap" in blob:
            return "audio.getAlgorithmCapabilities"
        if flags["reset"]:
            return "audio.resetAlgorithmConfig"
        return "audio.getAlgorithmConfig" if read else "audio.setAlgorithmConfig"
    if capability == "audio.eq":
        if "cap" in blob:
            return "audio.getEqCapabilities"
        if flags["reset"]:
            return "audio.resetEqConfig"
        return "audio.getEqConfig" if read else "audio.setEqConfig"
    if capability == "audio.recording":
        if flags["stop"]:
            return "audio.stopRecording"
        if "cancel" in blob:
            return "audio.cancelRecording"
        if read:
            return "audio.getRecordingState"
        return "audio.startRecording"
    if capability == "audio.volume":
        return "audio.getVolumeState" if read else "audio.setVolumeConfig"
    if capability in {"audio.input", "audio.routing"}:
        if read:
            return "audio.getInputConfig" if capability == "audio.input" else "audio.getRoutingConfig"
        return "audio.setInputConfig" if capability == "audio.input" else "audio.setRoutingConfig"
    if capability == "audio.mixer":
        return "audio.getMixerConfig" if read else "audio.setMixerConfig"
    if capability == "audio.playback":
        if flags["status"]:
            return "audio.getPlaybackState"
        return "audio.getPlaybackConfig" if flags["get"] else "audio.setPlaybackConfig"
    if capability == "audio.uac":
        return "audio.getUacState" if read else "audio.setUacConfig"
    if capability == "audio.dante":
        return "audio.getDanteConfig" if read else "audio.setDanteConfig"

    if capability == "camera.focus":
        if "autofocusstate" in re.sub(r"[^a-z0-9]+", "", blob):
            return "camera.getFocusMode" if read else "camera.setFocusMode"
        if "mode" in blob:
            return "camera.getFocusMode" if flags["get"] else "camera.setFocusMode"
        if "position" in blob or "manual" in blob:
            return "camera.getFocusPosition" if flags["get"] else "camera.setFocusPosition"
        if "auto" in blob and not flags["get"]:
            return "camera.triggerAutoFocus"
        if flags["stop"]:
            return "camera.stopFocus"
        return "camera.getFocusState" if read else "camera.triggerAutoFocus"
    if capability == "camera.zoom":
        return "camera.getZoomState" if read else "camera.setZoomConfig"
    if capability == "camera.calibration":
        return "camera.getCalibrationState" if read else "camera.startCalibration"
    if capability == "camera.exposure":
        return "camera.getExposureConfig" if read else "camera.setExposureConfig"
    if capability == "camera.image":
        return "camera.getImageConfig" if read else "camera.setImageConfig"
    if capability == "camera.ptz":
        return "camera.getPtzConfig" if read else "camera.setPtzConfig"

    if capability == "firmware.ota":
        compact = re.sub(r"[^a-z0-9]+", "", blob)
        if "upgradeinfo" in compact or "startupgrade" in compact or "remoteupgrade" in compact or "cloudupgrade" in compact:
            return "firmware.beginOta"
        if flags["progress"] or (read and "version" not in blob):
            return "firmware.getOtaState"
        if "version" in blob or "info" in blob:
            return "firmware.getInfo"
        if "data" in blob or "chunk" in blob:
            return "firmware.commitOtaBatch"
        if "verify" in blob:
            return "firmware.verifyOtaFiles"
        if "install" in blob or "apply" in blob:
            return "firmware.installOta"
        if "rollback" in blob:
            return "firmware.rollbackOta"
        if "cancel" in blob or flags["stop"]:
            return "firmware.cancelOta"
        return "firmware.beginOta"
    if capability == "firmware.updatePolicy":
        return "firmware.getUpdatePolicyConfig" if flags["get"] else "firmware.setUpdatePolicyConfig"

    if capability == "file.transfer":
        if read and "info" in blob:
            return "file.getTransferState"
        if flags["stop"] or "complete" in blob or "end" in blob:
            return "file.endTransfer"
        if "cancel" in blob:
            return "file.cancelTransfer"
        if "resume" in blob:
            return "file.resumeTransfer"
        return "file.beginTransfer"

    if capability == "log.stream":
        if flags["stop"]:
            return "log.closeStream"
        if read:
            return "log.getStreamState"
        return "log.openStream"
    if capability == "log.export":
        if flags["progress"] or read:
            return "log.getExportState"
        if "cancel" in blob:
            return "log.cancelExport"
        return "log.createExport"
    if capability == "log.files":
        if "delete" in blob:
            return "log.deleteFile"
        return "log.listFiles"

    if capability == "room.source":
        return "room.getSourceConfig" if read else "room.setSourceConfig"
    if capability == "room.layout":
        return "room.getLayoutConfig" if read else "room.setLayoutConfig"
    if capability == "room.schedule":
        return "room.getScheduleConfig" if read else "room.setScheduleConfig"
    if capability == "room.participant":
        return "room.getParticipantState" if read else "room.setParticipantConfig"
    if capability == "room.info":
        return "room.getInfo"

    if capability == "signage.playlist":
        return "signage.getPlaylistConfig" if read else "signage.setPlaylistConfig"
    if capability == "signage.media":
        return "signage.listMedia"
    if capability == "signage.schedule":
        return "signage.getScheduleConfig" if read else "signage.setScheduleConfig"
    if capability == "signage.playback":
        if flags["start"]:
            return "signage.startPlayback"
        if flags["stop"]:
            return "signage.stopPlayback"
        return "signage.getPlaybackState"
    if capability == "signage.osd":
        return "signage.getOsdConfig" if read else "signage.setOsdConfig"

    if capability == "storage.sdCard":
        if "format" in blob:
            return "storage.formatSdCard"
        return "storage.getSdCardState"
    if capability == "storage.disk":
        return "storage.getDiskState"
    if capability == "storage.recording":
        if flags["delete"]:
            return "storage.deleteRecording"
        return "storage.listRecordings"

    if capability == "device.info":
        return "device.getInfo" if read else "device.setInfoConfig"
    if capability == "device.identity":
        return "device.getIdentity" if read else "device.setIdentityConfig"
    if capability == "device.state":
        return "device.getState" if read else "device.setStateConfig"
    if capability == "device.power":
        return "device.getPowerState" if read else "device.setPowerConfig"
    if capability == "device.indicator":
        return "device.getIndicatorConfig" if flags["get"] else "device.setIndicatorConfig"
    if capability == "device.inventory":
        return "device.getInventory"
    if capability == "device.childDevice":
        return "device.getChildDeviceState" if read else "device.setChildDeviceConfig"

    if capability == "system.time":
        return "system.getTimeConfig" if flags["get"] else "system.setTimeConfig"
    if capability == "system.lifecycle":
        compact = re.sub(r"[^a-z0-9]+", "", blob)
        if "reboot" in blob:
            if "interval" in blob or "schedule" in blob:
                return "system.getRebootSchedule" if read else "system.setRebootSchedule"
            return "system.reboot"
        if "shutdown" in blob:
            if "auto" in blob or "schedule" in blob:
                return "system.getShutdownSchedule" if read else "system.setShutdownSchedule"
            return "system.shutdown"
        if "autopower" in compact or "keepalive" in compact or "setlive" in compact or "unconnect" in compact:
            return ""
        return "system.getLifecycleState" if read else ""
    if capability == "system.initialization":
        if read:
            return "system.getInitializationState"
        return "system.reset" if "reset" in blob else "system.startInitialization"
    if capability == "system.license":
        return "system.getLicenseState"

    if capability.startswith("diagnostic."):
        if capability == "diagnostic.report":
            return "diagnostic.getReportExportState" if read else "diagnostic.createReportExport"
        if capability == "diagnostic.networkTest":
            return "diagnostic.runNetworkTest"
        if capability == "diagnostic.audioTest":
            return "diagnostic.runAudioTest"
        if capability == "diagnostic.videoTest":
            return "diagnostic.runVideoTest"
        if capability == "diagnostic.storageTest":
            return "diagnostic.runStorageTest"
        if capability == "diagnostic.inputTest":
            return "diagnostic.runInputTest"
        if capability == "diagnostic.kvmTest":
            return "diagnostic.runKvmTest"
        if capability == "diagnostic.calibration":
            return "diagnostic.startCalibration"
        if capability == "diagnostic.manufacturing":
            return "diagnostic.setManufacturingData" if not flags["get"] else "diagnostic.getManufacturingData"
        return "diagnostic.runSelfTest"

    if capability == "input.hid":
        return "input.getHidConfig" if read else "input.setHidConfig"
    if capability == "input.key":
        return "input.getKeyConfig" if read else "input.setKeyConfig"
    if capability == "input.kvm":
        if flags["stop"]:
            return "input.closeKvm"
        return "input.openKvm"
    if capability == "input.source":
        return "input.getSourceState" if read else "input.setSourceConfig"

    if capability == "output.source":
        return "output.getSourceConfig" if read else "output.setSourceConfig"
    if capability == "output.layout":
        return "output.getLayoutConfig" if read else "output.setLayoutConfig"
    if capability == "output.routing":
        return "output.getRoutingConfig" if read else "output.setRoutingConfig"

    if capability == "auth.session":
        if flags["stop"]:
            return "auth.closeSession"
        return "auth.createSession" if not flags["get"] else "auth.getSessionState"
    if capability == "auth.token":
        return "auth.refreshToken" if not flags["get"] else "auth.getTokenState"
    if capability == "privacy.state":
        return "privacy.getState" if read else "privacy.setModeConfig"

    return ""


def event_for(capability: str, blob: str, template: str) -> str:
    if capability == "network.wifi":
        if "scan" in blob:
            return "network.wifiScanResultReported"
        if contains_any(blob, "status", "state", "connect", "signal"):
            return "network.wifiStateChanged"
        return "network.wifiConfigChanged"
    if capability == "network.ap":
        if "client" in blob:
            return "network.apClientChanged"
        if contains_any(blob, "status", "state", "open", "start", "stop"):
            return "network.apStateChanged"
        return "network.apConfigChanged"
    if capability == "network.ip":
        return "network.ipConfigChanged"
    if capability == "network.interface":
        return "network.interfaceStateChanged"
    if capability == "network.bluetooth":
        return "network.bluetoothStateChanged"
    if capability == "network.serviceEndpoint":
        return "network.serviceEndpointStateChanged"

    if capability == "video.ndi":
        return "video.ndiStateChanged" if contains_any(blob, "status", "state") else "video.ndiConfigChanged"
    if capability == "video.rtsp":
        return "video.rtspStateChanged" if contains_any(blob, "status", "state") else "video.rtspConfigChanged"
    if capability == "video.stream":
        return "video.streamStatsReported" if "stats" in blob else "video.streamStateChanged"
    if capability == "video.framing":
        if "mode" in blob:
            return "video.framingModeChanged"
        return "video.framingConfigChanged"
    if capability == "video.outputTransform":
        return "video.outputTransformChanged"
    if capability == "video.pip":
        return "video.pipConfigChanged"
    if capability == "video.layout":
        return "video.layoutConfigChanged"
    if capability == "video.scene":
        return "video.sceneConfigChanged"
    if capability == "video.recording":
        return "video.recordingStateChanged"

    if capability == "audio.algorithm":
        if "beam" in blob:
            return "audio.beamInfoReported"
        return "audio.algorithmConfigChanged"
    if capability == "audio.eq":
        return "audio.eqConfigChanged"
    if capability == "audio.recording":
        return "audio.recordingProgressReported" if "progress" in blob else "audio.recordingStateChanged"
    if capability == "audio.volume":
        return "audio.volumeStateChanged"
    if capability == "audio.playback":
        return "audio.playbackStateChanged"

    if capability == "firmware.ota":
        if "progress" in blob:
            return "firmware.otaProgressReported"
        if "result" in blob or "success" in blob or "failed" in blob:
            return "firmware.otaResultReported"
        return "firmware.otaStateChanged"
    if capability == "firmware.updatePolicy":
        return "firmware.updatePolicyChanged"

    if capability == "file.transfer":
        return "file.transferProgressReported" if "progress" in blob else "file.transferStateChanged"
    if capability == "log.stream":
        return "log.streamStateChanged"
    if capability == "log.export":
        return "log.exportProgressReported" if "progress" in blob else "log.exportStateChanged"
    if capability == "log.files":
        return "log.filesChanged"

    if capability == "room.source":
        return "room.sourceChanged"
    if capability == "room.layout":
        return "room.layoutChanged"
    if capability == "room.schedule":
        return "room.scheduleChanged"
    if capability == "room.participant":
        return "room.participantChanged"
    if capability == "room.info":
        return "room.infoChanged"

    if capability == "signage.media":
        return "signage.mediaChanged"
    if capability == "signage.playlist":
        return "signage.playlistChanged"
    if capability == "signage.schedule":
        return "signage.scheduleChanged"
    if capability == "signage.playback":
        return "signage.playbackStateChanged"
    if capability == "signage.osd":
        return "signage.osdChanged"

    if capability == "storage.sdCard":
        return "storage.sdCardStateChanged"
    if capability == "storage.disk":
        return "storage.diskStateChanged"
    if capability == "storage.recording":
        return "storage.recordingChanged"

    if capability == "device.info":
        return "device.infoChanged"
    if capability == "device.identity":
        return "device.identityChanged"
    if capability == "device.state":
        return "device.stateChanged"
    if capability == "device.power":
        return "device.powerStateChanged"
    if capability == "device.indicator":
        return "device.indicatorConfigChanged"
    if capability == "device.inventory":
        return "device.inventoryChanged"
    if capability == "device.childDevice":
        return "device.childDeviceStateChanged"

    if capability == "system.time":
        return "system.timeConfigChanged"
    if capability == "system.lifecycle":
        return "system.lifecycleStateChanged"
    if capability == "system.initialization":
        return "system.initializationStateChanged"

    if capability.startswith("diagnostic."):
        if capability == "diagnostic.report":
            return "diagnostic.reportExportProgressReported" if "progress" in blob else "diagnostic.reportExportStateChanged"
        return f"{capability}StateChanged"

    if capability == "input.hid":
        return "input.hidConfigChanged"
    if capability == "input.key":
        return "input.keyConfigChanged"
    if capability == "input.kvm":
        return "input.kvmStateChanged"
    if capability == "input.source":
        return "input.sourceStateChanged"

    if capability == "output.source":
        return "output.sourceChanged"
    if capability == "output.layout":
        return "output.layoutChanged"
    if capability == "output.routing":
        return "output.routingChanged"

    if capability == "auth.session":
        return "auth.sessionStateChanged"
    if capability == "auth.token":
        return "auth.tokenStateChanged"
    if capability == "privacy.state":
        return "privacy.stateChanged"

    fname = pascal(capability.split(".", 1)[1])
    domain = capability.split(".", 1)[0]
    if template == "reported" or "report" in blob:
        return f"{domain}.{fname[:1].lower() + fname[1:]}Reported"
    if "progress" in blob:
        return f"{domain}.{fname[:1].lower() + fname[1:]}ProgressReported"
    if "config" in blob:
        return f"{domain}.{fname[:1].lower() + fname[1:]}ConfigChanged"
    return f"{domain}.{fname[:1].lower() + fname[1:]}StateChanged"


def extract_axdp() -> list[Entry]:
    path = LEGACY_DIR / "AXDP源码协议扫描清单.md"
    text = path.read_text(encoding="utf-8")
    out: list[Entry] = []
    for lineno, line in enumerate(text.splitlines(), 1):
        if not re.match(r"^\|\s*\d+\s*\|", line):
            continue
        cells = split_md_row(line)
        if len(cells) < 6:
            continue
        _, name, id_text, status, in_payload, out_payload = cells[:6]
        payload = f"input={in_payload}; output={out_payload}"
        out.append(
            classify_entry(
                source_protocol="axdp_hid",
                source_file=source_rel(path),
                source_line=lineno,
                legacy_entry_type="command",
                legacy_command_id=id_text,
                legacy_command_name=name,
                legacy_method=name,
                legacy_status=status,
                legacy_payload_summary=payload,
            )
        )

    for lineno, line in enumerate(text.splitlines(), 1):
        if not re.match(r"^\|\s*`0x0[123]`", line):
            continue
        cells = split_md_row(line)
        if len(cells) < 4:
            continue
        report, direction, payload, event = cells[:4]
        out.append(
            classify_entry(
                source_protocol="axdp_hid",
                source_file=source_rel(path),
                source_line=lineno,
                legacy_entry_type="hid_report",
                legacy_command_id=report,
                legacy_command_name=report,
                legacy_event=event if direction.upper().startswith("IN") else "",
                legacy_status=direction,
                legacy_payload_summary=f"input={payload}; output/event={event}",
            )
        )
    return out


def extract_vm33() -> list[Entry]:
    path = LEGACY_DIR / "VM33协议文档.md"
    lines = path.read_text(encoding="utf-8").splitlines()
    out: list[Entry] = []

    class_pat = re.compile(r'"Class"\s*:\s*"([^"]+)"', re.I)
    method_pat = re.compile(r'"Method"\s*:\s*"([^"]+)"', re.I)
    name_pat = re.compile(r'"Name"\s*:\s*"([^"]+)"', re.I)

    seen_methods: dict[tuple[str, str], int] = {}
    seen_config: dict[tuple[str, str, str], int] = {}
    recent_class = ""
    recent_method = ""
    recent_line = 0

    for lineno, line in enumerate(lines, 1):
        cls = class_pat.search(line)
        if cls:
            recent_class = clean(cls.group(1))
            recent_line = lineno
        meth = method_pat.search(line)
        if meth:
            recent_method = clean(meth.group(1))
            recent_line = lineno
            if recent_class:
                seen_methods.setdefault((recent_class, recent_method), lineno)
        nm = name_pat.search(line)
        if nm and recent_class and lineno - recent_line <= 20:
            name = clean(nm.group(1))
            if recent_class.lower() == "config" and name.lower() not in {"othercfg", "this_wifi", "nearcast_admin", "eth0"}:
                seen_config.setdefault((recent_class, recent_method or "Set/Get", name), lineno)

    for (cls, meth), lineno in sorted(seen_methods.items(), key=lambda kv: kv[1]):
        ctx = "generic VM33 Config method; concrete Param.Name rows are classified separately" if cls.lower() == "config" else f"Class={cls}; Method={meth}"
        out.append(
            classify_entry(
                source_protocol="vm33_http_json",
                source_file=source_rel(path),
                source_line=lineno,
                legacy_entry_type="method",
                legacy_class=cls,
                legacy_method=meth,
                legacy_payload_summary=ctx,
            )
        )

    for (cls, meth, name), lineno in sorted(seen_config.items(), key=lambda kv: kv[1]):
        out.append(
            classify_entry(
                source_protocol="vm33_http_json",
                source_file=source_rel(path),
                source_line=lineno,
                legacy_entry_type="config_name",
                legacy_class=cls,
                legacy_method=meth,
                legacy_config_name=name,
                legacy_payload_summary=f"Config Param.Name={name}",
            )
        )

    return dedupe_entries(out)


def extract_rooms() -> list[Entry]:
    path = LEGACY_DIR / "Rooms协议文档.md"
    lines = path.read_text(encoding="utf-8").splitlines()
    out: list[Entry] = []
    method_pat = re.compile(r'"method"\s*:\s*"([^"]+)"', re.I)
    event_pat = re.compile(r'"event"\s*:\s*"([^"]+)"', re.I)
    sub_pat = re.compile(r'"subscribe"\s*:\s*"([^"]+)"', re.I)

    seen_methods: dict[str, int] = {}
    seen_events: dict[str, int] = {}

    for lineno, line in enumerate(lines, 1):
        for pat, store in [(method_pat, seen_methods), (event_pat, seen_events), (sub_pat, seen_events)]:
            m = pat.search(line)
            if not m:
                continue
            val = clean(m.group(1)).strip("*~ ")
            if val and val not in {"--"}:
                store.setdefault(val, lineno)

    for name, lineno in sorted(seen_methods.items(), key=lambda kv: kv[1]):
        out.append(
            classify_entry(
                source_protocol="rooms_ws_json",
                source_file=source_rel(path),
                source_line=lineno,
                legacy_entry_type="method",
                legacy_method=name,
                legacy_payload_summary="Rooms method field",
            )
        )
    for name, lineno in sorted(seen_events.items(), key=lambda kv: kv[1]):
        out.append(
            classify_entry(
                source_protocol="rooms_ws_json",
                source_file=source_rel(path),
                source_line=lineno,
                legacy_entry_type="event",
                legacy_event=name,
                legacy_payload_summary="Rooms event/subscribe field",
            )
        )
    return dedupe_entries(out)


def extract_signage() -> list[Entry]:
    paths = [
        LEGACY_DIR / "NearHub-Launcher设备管理命令.md",
        LEGACY_DIR / "NearHub-Launcher数字标牌设备管理通用管理命令.md",
    ]
    out: list[Entry] = []
    heading_pat = re.compile(r"^###\s+\[(指令|事件)\]\s+(.+?)\s*$")
    desc_pat = re.compile(r"^-\s+\*\*功能描述\*\*:\s*(.+)$")
    method_pat = re.compile(r"^-\s+\*\*method\*\*:\s*`([^`]+)`")
    for path in paths:
        lines = path.read_text(encoding="utf-8").splitlines()
        i = 0
        while i < len(lines):
            m = heading_pat.match(lines[i])
            if not m:
                i += 1
                continue
            kind_cn, heading_name = m.groups()
            entry_type = "method" if kind_cn == "指令" else "event"
            lineno = i + 1
            wire_name = clean(heading_name)
            desc = ""
            j = i + 1
            while j < min(len(lines), i + 16):
                dm = desc_pat.match(lines[j])
                mm = method_pat.match(lines[j])
                if dm:
                    desc = clean(dm.group(1))
                if mm:
                    wire_name = clean(mm.group(1))
                if heading_pat.match(lines[j]):
                    break
                j += 1
            out.append(
                classify_entry(
                    source_protocol="signage_sdk",
                    source_file=source_rel(path),
                    source_line=lineno,
                    legacy_entry_type=entry_type,
                    legacy_method=wire_name if entry_type == "method" else "",
                    legacy_event=wire_name if entry_type == "event" else "",
                    legacy_payload_summary=desc,
                )
            )
            i += 1
    return dedupe_entries(out)


def dedupe_entries(entries: list[Entry]) -> list[Entry]:
    seen: set[tuple[str, str, str, str, str]] = set()
    out: list[Entry] = []
    for e in entries:
        key = (e.source_protocol, e.legacy_entry_type, e.legacy_wire_name, e.legacy_command_id, e.legacy_config_name)
        if key in seen:
            continue
        seen.add(key)
        out.append(e)
    return out


def write_csv(entries: list[Entry]) -> Path:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUT_DIR / "legacy-protocol-classification.csv"
    fields = list(asdict(entries[0]).keys()) if entries else []
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for entry in entries:
            writer.writerow(asdict(entry))
    return path


def md_table(rows: list[list[str]]) -> str:
    if not rows:
        return ""
    header = rows[0]
    sep = ["---"] * len(header)
    body = rows[1:]

    def esc(s: object) -> str:
        return clean(s).replace("|", "\\|")

    lines = ["| " + " | ".join(esc(c) for c in header) + " |", "| " + " | ".join(sep) + " |"]
    for row in body:
        lines.append("| " + " | ".join(esc(c) for c in row) + " |")
    return "\n".join(lines)


def display_legacy(e: Entry) -> str:
    parts = []
    if e.legacy_command_id:
        parts.append(e.legacy_command_id)
    if e.legacy_wire_name:
        parts.append(e.legacy_wire_name)
    if e.legacy_config_name:
        parts.append(f"Name={e.legacy_config_name}")
    return " / ".join(parts)


def display_target(e: Entry) -> str:
    if e.target_axtp_method and e.target_axtp_event:
        return f"{e.target_axtp_method}; {e.target_axtp_event}"
    return e.target_axtp_method or e.target_axtp_event or "(split required)"


def write_markdown(entries: list[Entry], csv_path: Path) -> Path:
    path = OUT_DIR / "README.md"
    by_source = Counter(e.source_protocol for e in entries)
    by_capability = Counter(e.target_capability for e in entries)
    by_conf = Counter(e.confidence for e in entries)
    by_kind = Counter(e.target_rpc_kind for e in entries)

    source_rows = [["Source", "Entries"]]
    for source, count in sorted(by_source.items()):
        source_rows.append([source, str(count)])

    cap_rows = [["Capability", "Entries"]]
    for cap, count in by_capability.most_common():
        cap_rows.append([cap, str(count)])

    low_rows = [["Source", "File", "Line", "Legacy Entry", "Candidate", "Target", "Question"]]
    for e in entries:
        if e.confidence == "low" or e.target_feature in {"uncategorized", "genericConfig"}:
            low_rows.append([e.source_protocol, e.source_file, str(e.source_line), display_legacy(e), e.target_capability, display_target(e), e.open_questions or e.rationale])

    sample_rows = [["Source", "File", "Line", "Type", "Legacy Wire", "Class", "Method", "Event", "Config Name", "Capability", "AXTP Method", "AXTP Event", "Protocol Doc", "Confidence"]]
    for e in entries[:100]:
        sample_rows.append([
            e.source_protocol,
            e.source_file,
            str(e.source_line),
            e.legacy_entry_type,
            e.legacy_wire_name,
            e.legacy_class,
            e.legacy_method,
            e.legacy_event,
            e.legacy_config_name,
            e.target_capability,
            e.target_axtp_method,
            e.target_axtp_event,
            e.target_protocol_doc,
            e.confidence,
        ])

    source_links = [["Source", "Markdown"]]
    for source in sorted(by_source):
        source_links.append([source, f"by-source/{source}.md"])

    text = f"""# Legacy Protocol Domain-Feature Classification

本目录是 AXDP / VM33 / Rooms / Signage legacy intake 的逐条分类结果，不是 AXTP registry 事实源。分类依据为 `specs/30-registry.md`、`specs/50-tooling.md`，并对照 `workspace/protocol` 下已成型的业务协议文档。

生成脚本：`tooling/legacy_classification/classify_legacy_protocols.py`

CSV 明细：`{csv_path.relative_to(ROOT)}`

## 字段说明

- `legacy_command_name` / `legacy_class` / `legacy_method` / `legacy_event` / `legacy_config_name` 分开展示旧协议原始字段。
- `target_domain` / `target_feature` / `target_capability` 表示按 Naming and Taxonomy spec 归类后的能力块。
- `target_axtp_method` / `target_axtp_event` 表示建议落到的 AXTP method/event；没有正式协议文档的条目会标明 taxonomy/spec candidate。
- `target_protocol_doc` 优先指向 `workspace/protocol` 中已有设计文档；为空表示该 domain.feature 还需要补业务协议文档或 domain YAML。

## 分类原则

- 旧协议先归类到 `domain.feature`，再映射 method/event/schema adapter。
- `Config / State / Mode / Scan / Connection` 默认不是 feature；它们进入 method、event 或 schema 字段。
- `stream` 只承担公共流控和数据面；文件、固件、视频、音频、日志流归各自业务域。
- 泛 `ConfigJson` 或缺少具体 payload/Name 的条目标为 `adapter_only` 或 `needs_split`，不能直接进入正式 capability。
- 本清单不修改 `workspace/legacy-migration/plans/`、`contract/registry/`、`contract/protocol/axtp.protocol.yaml` 或 generated artifacts。

## 按 Source 查看

{md_table(source_links)}

## 覆盖统计

{md_table(source_rows)}

## 分类置信度

{md_table([["Confidence", "Entries"]] + [[k, str(v)] for k, v in sorted(by_conf.items())])}

## Target RPC Kind

{md_table([["Target RPC Kind", "Entries"]] + [[k, str(v)] for k, v in sorted(by_kind.items())])}

## Domain.Feature 汇总

{md_table(cap_rows)}

## 需要人工复核的条目

{md_table(low_rows)}

## 前 100 条样例

{md_table(sample_rows)}
"""
    path.write_text(text, encoding="utf-8")
    return path


def entry_rows(entries: list[Entry]) -> list[list[str]]:
    rows = [["Source", "File", "Line", "Type", "Legacy Wire", "Command ID", "Class", "Method", "Event", "Config Name", "Capability", "AXTP Method", "AXTP Event", "Protocol Doc", "Confidence", "Notes"]]
    for e in entries:
        rows.append([
            e.source_protocol,
            e.source_file,
            str(e.source_line),
            e.legacy_entry_type,
            e.legacy_wire_name,
            e.legacy_command_id,
            e.legacy_class,
            e.legacy_method,
            e.legacy_event,
            e.legacy_config_name,
            e.target_capability,
            e.target_axtp_method,
            e.target_axtp_event,
            e.target_protocol_doc,
            e.confidence,
            e.rationale if not e.open_questions else f"{e.rationale} Open: {e.open_questions}",
        ])
    return rows


def write_domain_indexes(entries: list[Entry]) -> list[Path]:
    paths: list[Path] = []
    by_domain: dict[str, list[Entry]] = defaultdict(list)
    for e in entries:
        by_domain[e.target_domain].append(e)

    for domain, domain_entries in sorted(by_domain.items()):
        sorted_entries = sorted(domain_entries, key=lambda x: (x.target_capability, x.source_protocol, x.source_line, x.legacy_wire_name))
        text = f"""# {domain} Legacy Classification

本文件从 `legacy-protocol-classification.csv` 按 domain 切分生成，用于后续撰写 `contract/registry/domains/{domain}/domain.yaml` 前的人工审查。

{md_table(entry_rows(sorted_entries))}
"""
        p = OUT_DIR / f"{domain}.md"
        p.write_text(text, encoding="utf-8")
        paths.append(p)
    return paths


def write_source_indexes(entries: list[Entry]) -> list[Path]:
    paths: list[Path] = []
    by_source: dict[str, list[Entry]] = defaultdict(list)
    source_dir = OUT_DIR / "by-source"
    source_dir.mkdir(parents=True, exist_ok=True)
    for e in entries:
        by_source[e.source_protocol].append(e)

    for source, source_entries in sorted(by_source.items()):
        sorted_entries = sorted(source_entries, key=lambda x: (x.source_file, x.source_line, x.legacy_entry_type, x.legacy_wire_name))
        text = f"""# {source} Legacy Classification

本文件从 `legacy-protocol-classification.csv` 按 legacy source 切分生成，逐条展示原协议字段、AXTP domain.feature、建议 method/event 和已匹配的 `workspace/protocol` 文档。

{md_table(entry_rows(sorted_entries))}
"""
        p = source_dir / f"{source}.md"
        p.write_text(text, encoding="utf-8")
        paths.append(p)
    return paths


def main() -> int:
    entries: list[Entry] = []
    entries.extend(extract_axdp())
    entries.extend(extract_vm33())
    entries.extend(extract_rooms())
    entries.extend(extract_signage())
    entries = sorted(entries, key=lambda e: (e.source_protocol, e.source_file, e.source_line, e.legacy_entry_type, e.legacy_wire_name))

    csv_path = write_csv(entries)
    readme_path = write_markdown(entries, csv_path)
    domain_paths = write_domain_indexes(entries)
    source_paths = write_source_indexes(entries)

    print(f"entries={len(entries)}")
    print(f"csv={csv_path.relative_to(ROOT)}")
    print(f"readme={readme_path.relative_to(ROOT)}")
    print(f"domain_indexes={len(domain_paths)}")
    print(f"source_indexes={len(source_paths)}")
    for source, count in sorted(Counter(e.source_protocol for e in entries).items()):
        print(f"{source}={count}")
    for cap, count in Counter(e.target_capability for e in entries).most_common(25):
        print(f"{cap}={count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
