#!/usr/bin/env python3
"""Generate AXTP legacy migration planning artifacts.

The generator intentionally writes workspace/legacy-migration/generated/* only. It treats
registry changes as candidate patches and keeps AXTP Core facts read-only.
"""

from __future__ import annotations

import json
import re
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import pandas as pd


ROOT = Path(__file__).resolve().parents[2]
LEGACY_DIR = ROOT / "docs" / "legacy-migration" / "evidence"
OUTPUT_DIR = ROOT / "docs" / "legacy-migration" / "generated"

EXISTING_METHODS = {
    "device.getInfo": {"id": 0x0101, "domain": "device", "bitOffset": 0},
    "capability.supportedMethods": {"id": 0x0201, "domain": "capability", "bitOffset": 0},
    "display.getBrightness": {"id": 0x0601, "domain": "display", "bitOffset": 0},
    "display.setBrightness": {"id": 0x0602, "domain": "display", "bitOffset": 1},
    "firmware.begin": {"id": 0x0402, "domain": "firmware", "bitOffset": 0},
    "firmware.end": {"id": 0x0403, "domain": "firmware", "bitOffset": 1},
    "firmware.verify": {"id": 0x0404, "domain": "firmware", "bitOffset": 2},
    "firmware.apply": {"id": 0x0405, "domain": "firmware", "bitOffset": 3},
    "stream.open": {"id": 0x0501, "domain": "stream", "bitOffset": 0},
    "network.getApInfo": {"id": 0x0E07, "domain": "network", "bitOffset": 0},
}

EXISTING_EVENTS = {
    "display.brightnessChanged": {"id": 0x0607, "domain": "display", "bitOffset": 0},
    "firmware.updateProgress": {"id": 0x0402, "domain": "firmware", "bitOffset": 0},
    "firmware.updateCompleted": {"id": 0x0403, "domain": "firmware", "bitOffset": 1},
    "firmware.updateFailed": {"id": 0x0404, "domain": "firmware", "bitOffset": 2},
    "stream.opened": {"id": 0x0501, "domain": "stream", "bitOffset": 0},
    "stream.error": {"id": 0x0503, "domain": "stream", "bitOffset": 1},
    "network.apInfoChanged": {"id": 0x0E01, "domain": "network", "bitOffset": 0},
}

EXISTING_CAPABILITIES = {
    "protocol.payload.control": {"id": 0x0001, "domain": "protocol"},
    "protocol.payload.rpc": {"id": 0x0002, "domain": "protocol"},
    "protocol.payload.stream": {"id": 0x0003, "domain": "protocol"},
    "device.info": {"id": 0x0101, "domain": "device"},
    "capability.supportedMethods": {"id": 0x0201, "domain": "capability"},
    "display.brightness": {"id": 0x0601, "domain": "display"},
    "firmware.ota": {"id": 0x0401, "domain": "firmware"},
    "stream.hidMedia": {"id": 0x050A, "domain": "stream"},
    "network.softAp": {"id": 0x0E06, "domain": "network"},
}

DOMAIN_ALIASES = {
    "system": "system",
    "device": "device",
    "network": "network",
    "storage": "storage",
    "audio": "audio",
    "maintenance": "firmware",
    "business": "binding",
    "digital signage": "signage",
    "appearance": "appearance",
    "update": "update",
    "schedule": "schedule",
    "log management": "log",
}

BULK_KEYWORDS = [
    "chunk",
    "data",
    "stream",
    "raw",
    "logdata",
    "file",
    "filetransfer",
    "uploaddata",
    "downloaddata",
    "ota",
    "kvm",
    "multipart",
    "filestream",
]

CORE_PAYLOAD_TYPES = ["CONTROL", "RPC", "STREAM"]

LEGACY_PROTOCOL_LABELS = {
    "axdp_hid": "AXDP HID",
    "rooms_ws_json": "Rooms WebSocket JSON",
    "signage_sdk": "signage SDK",
    "vm33_http_json": "VM33 HTTP JSON",
}


@dataclass
class Mapping:
    source: str
    legacy_protocol: str
    legacy_id: str
    legacy_name: str
    direction: str
    category: str
    description: str
    legacy_payload: str
    target_kind: str
    axtp_method: str | None = None
    axtp_event: str | None = None
    axtp_capability: str | None = None
    axtp_stream_profile: str | None = None
    status: str = "draft"
    confidence: str = "medium"
    promotion: str = "candidate_registry_patch"
    notes: list[str] = field(default_factory=list)


def clean(value: Any) -> str:
    if pd.isna(value):
        return ""
    return str(value).strip()


def lower_camel(name: str) -> str:
    words = re.findall(r"[A-Z]?[a-z0-9]+|[A-Z]+(?=[A-Z]|$)", name)
    if not words:
        return name[:1].lower() + name[1:]
    first, rest = words[0].lower(), words[1:]
    return first + "".join(w[:1].upper() + w[1:].lower() for w in rest)


def pascal(name: str) -> str:
    parts = re.split(r"[^A-Za-z0-9]+", name)
    return "".join(p[:1].upper() + p[1:] for p in parts if p)


def method_to_schema(name: str, suffix: str) -> str:
    return pascal(name) + suffix


def capability_for_method(method: str) -> str:
    domain, leaf = method.split(".", 1)
    if method == "device.getInfo":
        return "device.info"
    if method in ("display.getBrightness", "display.setBrightness"):
        return "display.brightness"
    if method.startswith("firmware."):
        return "firmware.ota"
    if method.startswith("stream."):
        return "stream.hidMedia"
    if method == "network.getApInfo":
        return "network.softAp"
    base = re.sub(r"^(get|set|request|notify|open|close|start|stop|begin|end)", "", leaf)
    base = base[:1].lower() + base[1:] if base else leaf
    return f"{domain}.{base or leaf}"


def stream_profile_for(text: str) -> str:
    hay = text.lower()
    if "firmware" in hay or "upgrade" in hay or "ota" in hay:
        return "firmware.ota"
    if "log" in hay:
        return "log.file"
    if "video" in hay or "camera" in hay or "rawstream" in hay or "kvm" in hay:
        return "media.video"
    if "audio" in hay:
        return "media.audio"
    if "file" in hay or "upload" in hay or "download" in hay or "playlist" in hay or "osd" in hay:
        return "file.transfer"
    if "state" in hay or "status" in hay:
        return "state.sync"
    return "legacy.binary"


def rpc_stream_hint(method: str, payload: str, desc: str) -> str | None:
    hay = f"{method} {payload} {desc}".lower()
    payload_norm = payload.lower().replace(" ", "")
    if payload_norm == "firmware" or any(k in hay for k in ["upgradebyurl", "cloudupgrade", "remoteupgrade"]):
        return "firmware.ota"
    if "log" in hay and any(k in hay for k in ["upload", "request", "collect", "打包"]):
        return "log.file"
    if payload_norm == "filetransfer" or any(k in hay for k in ["file", "multipart", "oss"]):
        return "file.transfer"
    if payload_norm == "rawstream" or "kvm" in hay:
        return "media.video"
    return None


def should_stream(name: str, payload: str, desc: str) -> bool:
    hay = f"{name} {payload} {desc}".lower()
    payload_norm = payload.lower().replace(" ", "")
    if payload_norm in {"rawstream", "logstream", "filetransfer"}:
        return True
    if payload_norm == "firmware" and any(k in hay for k in ["chunk", "data", "block", "image"]):
        return True
    if "get" in name.lower() and not any(k in hay for k in ["logdata", "file", "stream"]):
        return False
    return any(k in hay for k in BULK_KEYWORDS) and not any(
        small in hay for small in ["binaryrpc", "json_rpc", "json rpc"]
    )


def read_existing_legacy_mappings() -> list[Mapping]:
    return [
        Mapping(
            source="contract/registry/legacy/legacy_mapping.yaml",
            legacy_protocol="axdp_hid",
            legacy_id="0x000B0002",
            legacy_name="BetaDeviceInfo",
            direction="request_response",
            category="device",
            description="Existing AXTP legacy mapping for Beta device information.",
            legacy_payload="fixed_struct",
            target_kind="rpc_method",
            axtp_method="device.getInfo",
            status="compat",
            confidence="high",
            promotion="existing_registry_mapping",
            notes=["Authoritative existing mapping retained."],
        ),
        Mapping(
            source="contract/registry/legacy/legacy_mapping.yaml",
            legacy_protocol="axdp_hid",
            legacy_id="0x000B0042",
            legacy_name="BetaBrightnessSet",
            direction="request_response",
            category="display",
            description="Existing AXTP legacy mapping for Beta display brightness setting.",
            legacy_payload="fixed_struct",
            target_kind="rpc_method",
            axtp_method="display.setBrightness",
            status="compat",
            confidence="high",
            promotion="existing_registry_mapping",
            notes=["Authoritative existing mapping retained."],
        ),
    ]


def normalize_v2_method(domain: str, raw: str, fallback: str) -> str:
    raw = clean(raw)
    if raw and "." in raw:
        return raw
    if raw:
        return f"{domain}.{lower_camel(raw)}"
    return f"{domain}.{lower_camel(fallback)}"


def infer_legacy_domain(*values: str) -> str:
    hay = " ".join(v for v in values if v).lower()
    rules = [
        ("firmware", ["upgrade", "ota", "firmware", "升级"]),
        ("audio", ["audio", "mic", "speaker", "volume", "uac", "dante", "音频", "麦克风", "扬声器", "音量"]),
        ("video", ["video", "camera", "image", "osd", "pip", "ndi", "rtsp", "画面", "视频", "摄像", "图像"]),
        ("network", ["network", "wifi", "ap", "bluetooth", "ip", "网络", "蓝牙"]),
        ("storage", ["storage", "sd", "disk", "record", "存储", "录像", "录制"]),
        ("log", ["log", "日志"]),
        ("file", ["file", "upload", "download", "文件", "上传", "下载"]),
        ("system", ["reboot", "reset", "time", "keepalive", "restart", "重启", "恢复", "时间"]),
        ("device", ["device", "info", "name", "unique", "identity", "设备", "序列号"]),
        ("input", ["hid", "key", "kvm", "gpio", "按键"]),
        ("display", ["display", "brightness", "screen", "显示", "亮度"]),
        ("diagnostic", ["test", "diagnostic", "manufacturing", "测试", "诊断", "产测"]),
        ("auth", ["auth", "token", "permission", "license", "授权", "令牌"]),
        ("room", ["room", "rooms", "会议室"]),
    ]
    for domain, keywords in rules:
        if any(keyword in hay for keyword in keywords):
            return domain
    return "legacy"


def read_axdp_methods() -> list[Mapping]:
    path = LEGACY_DIR / "AXDP设备能力协议集.xlsx"
    if not path.exists():
        return []
    df = pd.read_excel(path, sheet_name="USB设备HID交互协议集", header=1)
    rows: list[Mapping] = []
    for _, row in df.iterrows():
        legacy_id = clean(row.get("宏参数（Cmd Value）"))
        legacy_name = clean(row.get("Unnamed: 1"))
        if not legacy_id or not legacy_name:
            continue
        interface = clean(row.get("接口（Interface）"))
        payload = clean(row.get("参数（Parameter）")) or "legacy_binary"
        desc = clean(row.get("说明（Explanation）")) or f"AXDP {legacy_name} command."
        domain = infer_legacy_domain(legacy_name, desc, interface, payload)
        raw_method = interface.splitlines()[0] if interface else legacy_name
        v2 = normalize_v2_method(domain, raw_method, legacy_name)
        stream = should_stream(v2, payload, desc)
        rows.append(
            Mapping(
                source="workspace/legacy-migration/evidence/AXDP设备能力协议集.xlsx:USB设备HID交互协议集",
                legacy_protocol="axdp_hid",
                legacy_id=legacy_id,
                legacy_name=legacy_name,
                direction="request_response",
                category=domain,
                description=desc,
                legacy_payload=payload or "legacy_binary",
                target_kind="stream" if stream else "rpc_method",
                axtp_method=None if stream and "chunk" in v2.lower() else v2,
                axtp_stream_profile=stream_profile_for(f"{v2} {payload} {desc}") if stream else rpc_stream_hint(v2, payload, desc),
                status="compat",
                confidence="high" if v2 in EXISTING_METHODS or "." in v2 else "medium",
                promotion="candidate_registry_patch",
                notes=["Source file: current AXDP capability protocol set"],
            )
        )
    return rows


def read_vm33_methods() -> tuple[list[Mapping], list[Mapping], list[Mapping], list[dict[str, Any]]]:
    path = LEGACY_DIR / "VM33_Protocol_V1_V2_Mapping.xlsx"
    if not path.exists():
        return [], [], [], []
    methods: list[Mapping] = []
    events: list[Mapping] = []
    caps: list[Mapping] = []
    issues: list[dict[str, Any]] = []

    df = pd.read_excel(path, sheet_name="V1_V2_Method_Mapping")
    for _, row in df.iterrows():
        legacy_id = clean(row.get("MethodId"))
        legacy_name = clean(row.get("V1 Identifier")) or ".".join(
            p for p in [clean(row.get("V1 Class")), clean(row.get("V1 Method"))] if p
        )
        if not legacy_id or not legacy_name:
            continue
        domain = clean(row.get("V2 Domain")) or "vm33"
        method = normalize_v2_method(domain, row.get("V2 MethodName"), clean(row.get("V1 Method")) or legacy_name)
        payload = clean(row.get("推荐PayloadType"))
        desc = clean(row.get("修改建议/备注")) or f"VM33 {legacy_name} compatibility method."
        stream = should_stream(method, payload, desc)
        methods.append(
            Mapping(
                source="workspace/legacy-migration/evidence/VM33_Protocol_V1_V2_Mapping.xlsx:V1_V2_Method_Mapping",
                legacy_protocol="vm33_http_json",
                legacy_id=legacy_id,
                legacy_name=legacy_name,
                direction="request_response",
                category=domain,
                description=desc,
                legacy_payload=payload or "JSON_RPC",
                target_kind="stream" if stream else "rpc_method",
                axtp_method=method,
                axtp_stream_profile=stream_profile_for(f"{method} {payload} {desc}") if stream else rpc_stream_hint(method, payload, desc),
                status="compat",
                confidence="high",
                promotion="candidate_registry_patch",
                notes=[clean(row.get("迁移策略"))],
            )
        )

    df = pd.read_excel(path, sheet_name="Event_Subscribe_Mapping")
    for _, row in df.iterrows():
        event = clean(row.get("V2事件名"))
        if not event:
            continue
        domain = event.split(".", 1)[0] if "." in event else "event"
        events.append(
            Mapping(
                source="workspace/legacy-migration/evidence/VM33_Protocol_V1_V2_Mapping.xlsx:Event_Subscribe_Mapping",
                legacy_protocol="vm33_http_json",
                legacy_id=clean(row.get("EventId")),
                legacy_name=clean(row.get("V1来源")),
                direction="event",
                category=domain,
                description=clean(row.get("事件含义")) or f"VM33 {event} event.",
                legacy_payload=clean(row.get("推荐PayloadType")) or "JSON_RPC",
                target_kind="event",
                axtp_event=event,
                status="compat",
                confidence="high",
                promotion="candidate_registry_patch",
                notes=[clean(row.get("修改建议"))],
            )
        )

    df = pd.read_excel(path, sheet_name="Config_Name_Mapping")
    for _, row in df.iterrows():
        key = clean(row.get("V2 Config Key"))
        if not key:
            continue
        domain = key.split(".", 1)[0] if "." in key else "config"
        caps.append(
            Mapping(
                source="workspace/legacy-migration/evidence/VM33_Protocol_V1_V2_Mapping.xlsx:Config_Name_Mapping",
                legacy_protocol="vm33_http_json",
                legacy_id=clean(row.get("ConfigId")),
                legacy_name=clean(row.get("V1 Name")),
                direction="capability",
                category=domain,
                description=clean(row.get("说明")) or f"VM33 {key} configuration capability.",
                legacy_payload=clean(row.get("推荐PayloadType")) or "JSON_RPC",
                target_kind="capability",
                axtp_capability=key,
                status="compat",
                confidence="medium",
                promotion="candidate_registry_patch",
                notes=[clean(row.get("修改建议/备注"))],
            )
        )

    df = pd.read_excel(path, sheet_name="Issues_Actions")
    for _, row in df.iterrows():
        issues.append(
            {
                "issue": clean(row.get("问题类别")),
                "current": clean(row.get("V1现状")),
                "recommendation": clean(row.get("V2修改建议")),
                "priority": clean(row.get("优先级")),
            }
        )
    return methods, events, caps, issues


def read_markdown_entries() -> list[Mapping]:
    rows: list[Mapping] = []
    for path in sorted(LEGACY_DIR.glob("*.md")):
        text = path.read_text(encoding="utf-8")
        headings = list(re.finditer(r"^### \[(指令|事件)\]\s+(.+?)\s*$", text, flags=re.M))
        for i, match in enumerate(headings):
            kind, name = match.group(1), match.group(2).strip()
            block = text[match.end() : headings[i + 1].start() if i + 1 < len(headings) else len(text)]
            prev_section = text[: match.start()].split("## ")[-1].splitlines()[0]
            section_name = re.sub(r"^\d+\.\s*", "", prev_section).split("(")[0].strip().lower()
            domain = DOMAIN_ALIASES.get(section_name, lower_camel(section_name) or "legacy")
            desc_match = re.search(r"- \*\*(?:功能描述|说明)\*\*:\s*(.+)", block)
            desc = desc_match.group(1).strip() if desc_match else f"Legacy Markdown {name} entry."
            status_match = re.search(r"- \*\*状态\*\*:\s*(.+)", block)
            source_status = status_match.group(1).strip() if status_match else "unknown"
            if kind == "事件":
                event_leaf = lower_camel(name[2:] if name.startswith("On") else f"{name}Received")
                event = f"{domain}.{event_leaf}"
                rows.append(
                    Mapping(
                        source=str(path.relative_to(ROOT)),
                        legacy_protocol="signage_sdk",
                        legacy_id=f"{path.stem}:{name}",
                        legacy_name=name,
                        direction="event",
                        category=domain,
                        description=desc,
                        legacy_payload="JSON_RPC",
                        target_kind="event",
                        axtp_event=event,
                        status="compat" if "已研发" in source_status or "研发中" in source_status else "draft",
                        confidence="medium",
                        promotion="candidate_registry_patch",
                        notes=[f"Source status: {source_status}"],
                    )
                )
            else:
                method = markdown_method(domain, name)
                stream = should_stream(method, "JSON_RPC", desc)
                rows.append(
                    Mapping(
                        source=str(path.relative_to(ROOT)),
                        legacy_protocol="signage_sdk",
                        legacy_id=f"{path.stem}:{name}",
                        legacy_name=name,
                        direction="request_response",
                        category=domain,
                        description=desc,
                        legacy_payload="JSON_RPC",
                        target_kind="stream" if stream else "rpc_method",
                        axtp_method=method,
                        axtp_stream_profile=stream_profile_for(f"{method} {desc}") if stream else rpc_stream_hint(method, "JSON_RPC", desc),
                        status="compat" if "已研发" in source_status or "研发中" in source_status else "draft",
                        confidence="medium" if method not in EXISTING_METHODS else "high",
                        promotion="candidate_registry_patch",
                        notes=[f"Source status: {source_status}"],
                    )
                )
    return rows


def markdown_method(domain: str, legacy_name: str) -> str:
    direct = {
        "KeepAlive": "system.keepAlive",
        "GetDeviceInfo": "device.getInfo",
        "SetDeviceName": "device.setName",
        "SetSysTime": "system.setTime",
        "ResetConfig": "system.resetConfig",
        "GetNetworkInfo": "network.getInfo",
        "GetSDInfo": "storage.getSdInfo",
        "FormatSd": "storage.formatSd",
        "SetLineOutVolume": "audio.setLineOutVolume",
        "GetLineOutVolume": "audio.getLineOutVolume",
        "SetLineInPreGain": "audio.setLineInPreGain",
        "GetLineInPreGain": "audio.getLineInPreGain",
        "RemoteUpgrade": "firmware.upgradeByUrl",
        "UpgradeProgress": "firmware.getUpdateProgress",
        "GetBindCode": "binding.getCode",
        "GetBindConfig": "binding.getConfig",
        "SetBindConfig": "binding.setConfig",
        "SetPlaylistConfig": "signage.setPlaylistConfig",
        "GetPlaylistConfig": "signage.getPlaylistConfig",
        "GetPlaylistItemUrl": "signage.getPlaylistItemUrl",
        "GetAppearanceConfig": "appearance.getConfig",
        "SetAppearanceConfig": "appearance.setConfig",
        "GetUpdateConfig": "update.getConfig",
        "SetUpdateConfig": "update.setConfig",
        "GetScheduleConfig": "schedule.getConfig",
        "SetScheduleConfig": "schedule.setConfig",
        "RequestLogUpload": "log.requestUpload",
        "NotifyLogUploadResult": "log.notifyUploadResult",
    }
    return direct.get(legacy_name, f"{domain}.{lower_camel(legacy_name)}")


def dedupe_mappings(rows: list[Mapping]) -> list[Mapping]:
    seen = set()
    out = []
    for row in rows:
        key = (row.legacy_protocol, row.legacy_id, row.legacy_name, row.target_kind, row.axtp_method, row.axtp_event)
        if key in seen:
            continue
        seen.add(key)
        row.notes = [n for n in row.notes if n]
        out.append(row)
    return sorted(out, key=lambda r: (r.legacy_protocol, r.legacy_id, r.legacy_name))


def allocate_ids(rows: list[Mapping]) -> tuple[dict[str, int], dict[str, int]]:
    existing_domain_ids = {v["domain"]: v["id"] >> 8 for v in EXISTING_METHODS.values()}
    candidate_domains = sorted(
        {
            (m.axtp_method or m.axtp_event or m.axtp_capability or m.category).split(".", 1)[0]
            for m in rows
            if m.target_kind in {"rpc_method", "event", "capability"} and (m.axtp_method or m.axtp_event or m.axtp_capability)
        }
    )
    next_high = 0x10
    domain_ids = dict(existing_domain_ids)
    for domain in candidate_domains:
        if domain in domain_ids:
            continue
        while next_high in domain_ids.values():
            next_high += 1
        domain_ids[domain] = next_high
        next_high += 1

    method_ord: dict[str, int] = defaultdict(int)
    for item in EXISTING_METHODS.values():
        method_ord[item["domain"]] = max(method_ord[item["domain"]], item["id"] & 0xFF, item["bitOffset"] + 1)
    event_ord: dict[str, int] = defaultdict(int)
    for item in EXISTING_EVENTS.values():
        event_ord[item["domain"]] = max(event_ord[item["domain"]], item["id"] & 0xFF, item["bitOffset"] + 1)
    cap_ord: dict[str, int] = defaultdict(int)
    for item in EXISTING_CAPABILITIES.values():
        cap_ord[item["domain"]] = max(cap_ord[item["domain"]], item["id"] & 0xFF)
    return domain_ids, {"method_ord": method_ord, "event_ord": event_ord, "cap_ord": cap_ord}


def build_registry_patch(rows: list[Mapping]) -> dict[str, Any]:
    domain_ids, ords = allocate_ids(rows)
    method_ord = ords["method_ord"]
    event_ord = ords["event_ord"]
    cap_ord = ords["cap_ord"]

    methods = []
    events = []
    capabilities = []
    schemas: dict[str, Any] = {}
    legacy_mappings = []
    seen_methods = set(EXISTING_METHODS)
    seen_events = set(EXISTING_EVENTS)
    seen_caps = set(EXISTING_CAPABILITIES)

    for row in sorted(rows, key=lambda r: (r.axtp_method or r.axtp_event or r.axtp_capability or "", r.legacy_name)):
        if row.target_kind == "rpc_method" and row.axtp_method:
            method = row.axtp_method
            cap = capability_for_method(method)
            if row.promotion == "adapter_only" and method not in EXISTING_METHODS:
                legacy_mappings.append(legacy_mapping_patch(row))
                continue
            if method not in seen_methods:
                domain = method.split(".", 1)[0]
                low = method_ord[domain] + 1
                method_ord[domain] = low
                req = method_to_schema(method, "Request")
                resp = method_to_schema(method, "Response")
                schemas[req] = legacy_request_schema(req, row)
                schemas[resp] = legacy_response_schema(resp, row)
                methods.append(
                    {
                        "id": f"0x{((domain_ids[domain] << 8) | low):04X}",
                        "name": method,
                        "domain": domain,
                        "status": row.status if row.status != "deprecated" else "draft",
                        "bitOffset": low - 1,
                        "since": "1.0.0",
                        "description": row.description,
                        "rpc_op": "request_response",
                        "request_schema": req,
                        "response_schema": resp,
                        "recommended_encoding": ["json", "binary_tlv"],
                        "capabilities": [cap],
                        "events": [],
                        "errors": ["SUCCESS", "RPC_PARAM_INVALID", "BUSY", "LEGACY_PAYLOAD_INVALID"],
                        "legacy": {
                            "protocol": row.legacy_protocol,
                            "id": row.legacy_id,
                            "name": row.legacy_name,
                            "payloadFormat": row.legacy_payload,
                            "adapter": adapter_name(row.legacy_name),
                        },
                    }
                )
                seen_methods.add(method)
            if cap not in seen_caps:
                domain = cap.split(".", 1)[0]
                low = cap_ord[domain] + 1
                cap_ord[domain] = low
                capabilities.append(
                    {
                        "id": f"0x{((domain_ids.get(domain, 0x10) << 8) | low):04X}",
                        "name": cap,
                        "domain": domain,
                        "status": row.status if row.status != "deprecated" else "draft",
                        "since": "1.0.0",
                        "type": "bool",
                        "description": f"Device supports {method}.",
                    }
                )
                seen_caps.add(cap)
            legacy_mappings.append(legacy_mapping_patch(row))
        elif row.target_kind == "event" and row.axtp_event:
            event = row.axtp_event
            cap = f"{event.split('.', 1)[0]}.events"
            if event not in seen_events:
                domain = event.split(".", 1)[0]
                low = event_ord[domain] + 1
                event_ord[domain] = low
                schema = method_to_schema(event, "Event")
                schemas[schema] = legacy_event_schema(schema, row)
                events.append(
                    {
                        "id": f"0x{(((0x80 | domain_ids[domain]) << 8) | low):04X}",
                        "name": event,
                        "domain": domain,
                        "status": row.status if row.status != "deprecated" else "draft",
                        "bitOffset": low - 1,
                        "since": "1.0.0",
                        "description": row.description,
                        "event_schema": schema,
                        "severity": "info",
                        "trigger": [row.legacy_name],
                        "capabilities": [cap],
                        "legacy": {"protocol": row.legacy_protocol, "id": row.legacy_id, "name": row.legacy_name},
                    }
                )
                seen_events.add(event)
            if cap not in seen_caps:
                domain = cap.split(".", 1)[0]
                low = cap_ord[domain] + 1
                cap_ord[domain] = low
                capabilities.append(
                    {
                        "id": f"0x{((domain_ids.get(domain, 0x10) << 8) | low):04X}",
                        "name": cap,
                        "domain": domain,
                        "status": row.status if row.status != "deprecated" else "draft",
                        "since": "1.0.0",
                        "type": "bool",
                        "description": f"Device can emit {domain} compatibility events.",
                    }
                )
                seen_caps.add(cap)
        elif row.target_kind == "capability" and row.axtp_capability:
            cap = row.axtp_capability
            if cap not in seen_caps:
                domain = cap.split(".", 1)[0]
                low = cap_ord[domain] + 1
                cap_ord[domain] = low
                capabilities.append(
                    {
                        "id": f"0x{((domain_ids.get(domain, 0x10) << 8) | low):04X}",
                        "name": cap,
                        "domain": domain,
                        "status": "draft",
                        "since": "1.0.0",
                        "type": "object",
                        "schema": method_to_schema(cap, "Capability"),
                        "description": row.description,
                        "legacy": {"protocol": row.legacy_protocol, "id": row.legacy_id, "name": row.legacy_name},
                    }
                )
                schemas[method_to_schema(cap, "Capability")] = legacy_capability_schema(method_to_schema(cap, "Capability"), row)
                seen_caps.add(cap)
        elif row.target_kind == "stream":
            legacy_mappings.append(legacy_mapping_patch(row))

    domain_files = []
    for domain in sorted({item["domain"] for item in methods + events + capabilities}):
        domain_files.append(
            {
                "domain": domain,
                "file": f"contract/registry/domains/{domain}/domain.yaml",
                "action": "create_or_merge_candidate",
            }
        )

    return {
        "generated_from": {
            "sources": [
                "contract/protocol/axtp.protocol.yaml",
                "contract/registry/**/*.yaml",
                "workspace/legacy-migration/evidence/*.xlsx",
                "workspace/legacy-migration/evidence/*.md",
                "workspace/legacy-migration/plans/*.md",
                "workspace/legacy-migration/plans/AXTP_Legacy_Migration_Matrix.xlsx",
            ],
            "policy": {
                "frame_header": "unchanged",
                "payload_types": CORE_PAYLOAD_TYPES,
                "compatibility_boundary": "legacy_adapter_only",
            },
        },
        "domain_id_allocation": {k: f"0x{v:02X}" for k, v in sorted(domain_ids.items())},
        "domain_files": domain_files,
        "methods": methods,
        "events": events,
        "errors": [],
        "capabilities": capabilities,
        "schemas": schemas,
        "legacy_mappings": legacy_mappings,
    }


def legacy_request_schema(name: str, row: Mapping) -> dict[str, Any]:
    return {
        "kind": "object",
        "description": f"Request schema for {row.axtp_method} adapted from {row.legacy_name}.",
        "fields": [
            {"id": "0x01", "name": "legacyParams", "type": "bytes", "required": False, "description": "Original legacy request payload retained for adapter round-trip diagnostics."},
            {"id": "0x02", "name": "legacyFormat", "type": "string", "required": False, "max_length": 32, "description": "Legacy payload format identifier."},
        ],
    }


def legacy_response_schema(name: str, row: Mapping) -> dict[str, Any]:
    return {
        "kind": "object",
        "description": f"Response schema for {row.axtp_method} adapted from {row.legacy_name}.",
        "fields": [
            {"id": "0x01", "name": "result", "type": "bytes", "required": False, "description": "Adapted response payload encoded by the selected RPC body encoding."},
            {"id": "0x02", "name": "legacyStatus", "type": "int32", "required": False, "description": "Original legacy status before AXTP ErrorCode mapping."},
        ],
    }


def legacy_event_schema(name: str, row: Mapping) -> dict[str, Any]:
    return {
        "kind": "object",
        "description": f"Event schema for {row.axtp_event} adapted from {row.legacy_name}.",
        "fields": [
            {"id": "0x01", "name": "payload", "type": "bytes", "required": False, "description": "Legacy event payload after adapter normalization."},
            {"id": "0x02", "name": "source", "type": "string", "required": False, "max_length": 64, "description": "Legacy event source identifier."},
        ],
    }


def legacy_capability_schema(name: str, row: Mapping) -> dict[str, Any]:
    return {
        "kind": "object",
        "description": f"Capability schema for {row.axtp_capability} derived from legacy configuration {row.legacy_name}.",
        "fields": [
            {"id": "0x01", "name": "supported", "type": "bool", "required": True, "description": "Whether the legacy capability is supported."},
            {"id": "0x02", "name": "legacyName", "type": "string", "required": False, "max_length": 64, "description": "Original legacy capability or configuration name."},
        ],
    }


def adapter_name(name: str) -> str:
    safe = re.sub(r"[^A-Za-z0-9]+", " ", name).title().replace(" ", "")
    return f"{safe}Adapter"


def legacy_mapping_patch(row: Mapping) -> dict[str, Any]:
    out = {
        "legacy_protocol": row.legacy_protocol,
        "legacy_id": row.legacy_id,
        "legacy_name": row.legacy_name,
        "direction": row.direction,
        "target_kind": row.target_kind,
        "status_mapping": {"0x00": "SUCCESS", "0x01": "RPC_PARAM_INVALID", "0x02": "BUSY", "default": "LEGACY_STATUS_UNMAPPED"},
        "request_adapter": f"{adapter_name(row.legacy_name)}Request",
        "response_adapter": f"{adapter_name(row.legacy_name)}Response",
    }
    if row.axtp_method:
        out["axtp_method_name"] = row.axtp_method
    if row.axtp_event:
        out["axtp_event_name"] = row.axtp_event
    if row.axtp_stream_profile:
        out["axtp_stream_profile"] = row.axtp_stream_profile
    return out


def mapping_to_dict(row: Mapping) -> dict[str, Any]:
    return {
        "source": row.source,
        "legacy_protocol": row.legacy_protocol,
        "legacy_id": row.legacy_id,
        "legacy_name": row.legacy_name,
        "direction": row.direction,
        "category": row.category,
        "description": row.description,
        "legacy_payload": row.legacy_payload,
        "target": {
            "kind": row.target_kind,
            **({"method": row.axtp_method} if row.axtp_method else {}),
            **({"event": row.axtp_event} if row.axtp_event else {}),
            **({"capability": row.axtp_capability} if row.axtp_capability else {}),
            **({"stream_profile": row.axtp_stream_profile} if row.axtp_stream_profile else {}),
        },
        "status": row.status,
        "confidence": row.confidence,
        "promotion": row.promotion,
        "notes": row.notes,
    }


def dump_yaml(value: Any) -> str:
    # JSON is valid YAML 1.2 and keeps generated output deterministic without
    # relying on PyYAML being available in the runtime.
    return json.dumps(value, ensure_ascii=False, indent=2) + "\n"


def covered_legacy_protocols(rows: list[Mapping]) -> list[str]:
    return sorted({row.legacy_protocol for row in rows})


def describe_legacy_protocols(rows: list[Mapping]) -> str:
    labels = [LEGACY_PROTOCOL_LABELS.get(protocol, protocol) for protocol in covered_legacy_protocols(rows)]
    if not labels:
        return "no legacy protocol"
    if len(labels) == 1:
        return labels[0]
    return ", ".join(labels[:-1]) + " and " + labels[-1]


def build_test_vectors(rows: list[Mapping]) -> dict[str, Any]:
    vectors = [
        {
            "id": "axdp-beta-device-info-existing",
            "source": "contract/registry/legacy/legacy_mapping.yaml",
            "legacy": {"protocol": "axdp_hid", "cmdValue": "0x000B0002", "name": "BetaDeviceInfo", "payloadHex": ""},
            "expect": {"targetKind": "rpc_method", "method": "device.getInfo", "requestSchema": "DeviceGetInfoRequest", "responseSchema": "DeviceGetInfoResponse", "error": "SUCCESS"},
        },
        {
            "id": "axdp-beta-brightness-existing",
            "source": "contract/registry/legacy/legacy_mapping.yaml",
            "legacy": {"protocol": "axdp_hid", "cmdValue": "0x000B0042", "name": "BetaBrightnessSet", "payloadHex": "32"},
            "expect": {"targetKind": "rpc_method", "method": "display.setBrightness", "request": {"value": 50}, "error": "SUCCESS"},
        },
        {
            "id": "legacy-unmapped-command",
            "legacy": {"protocol": "axdp_hid", "cmdValue": "0xDEADBEEF", "payloadHex": ""},
            "expect": {"reject": True, "error": "LEGACY_CMD_UNMAPPED"},
        },
        {
            "id": "legacy-payload-too-short",
            "legacy": {"protocol": "axdp_hid", "cmdValue": "0x000B0042", "payloadHex": ""},
            "expect": {"reject": True, "error": "LEGACY_PAYLOAD_TOO_SHORT"},
        },
        {
            "id": "legacy-status-busy",
            "legacy": {"protocol": "axdp_hid", "cmdValue": "0x000B0042", "status": "0x02"},
            "expect": {"mappedError": "BUSY", "retryable": True},
        },
        {
            "id": "firmware-bulk-uses-stream",
            "legacy": {"protocol": "axdp_hid", "cmdValue": "0xB0006", "name": "BetaUpgradeData", "payloadHex": "0011223344556677"},
            "expect": {"targetKind": "stream", "streamProfile": "firmware.ota", "payloadType": "STREAM", "rpcBodyCarriesChunk": False},
        },
        {
            "id": "log-upload-uses-stream",
            "legacy": {"protocol": "signage_sdk", "command": "RequestLogUpload", "params": {"url": "https://oss.example/logs.tgz"}},
            "expect": {"method": "log.requestUpload", "streamProfile": "log.file", "payloadType": "STREAM"},
        },
    ]
    if "vm33_http_json" in covered_legacy_protocols(rows):
        vectors += [
            {
                "id": "vm33-json-request",
                "legacy": {"protocol": "vm33_http_json", "body": {"Seq": 1, "Class": "Config", "Method": "Get", "Param": {"Name": "Wifi"}}},
                "expect": {"targetKind": "rpc_method", "method": "config.get", "rpc": {"op": "request", "requestId": 1}},
            },
            {
                "id": "vm33-notify-event",
                "legacy": {"protocol": "vm33_http_json", "body": {"notify": {"Class": "Config", "Method": "Subscribe", "Param": {"Name": "Wifi"}}}},
                "expect": {"targetKind": "event", "event": "config.changed"},
            },
        ]
    return {
        "metadata": {
            "name": "AXTP legacy adapter migration test vectors",
            "version": "generated",
            "frame_header_policy": "unchanged",
            "payload_types": CORE_PAYLOAD_TYPES,
        },
        "vectors": vectors,
        "coverage": {
            "total_generated_mappings": len(rows),
            "stream_mappings": sum(1 for row in rows if row.target_kind == "stream"),
            "event_mappings": sum(1 for row in rows if row.target_kind == "event"),
            "capability_mappings": sum(1 for row in rows if row.target_kind == "capability"),
        },
    }


def build_migration_plan(rows: list[Mapping], patch: dict[str, Any], issues: list[dict[str, Any]]) -> str:
    counts = defaultdict(int)
    for row in rows:
        counts[row.target_kind] += 1
    existing_reuse = sum(1 for row in rows if row.axtp_method in EXISTING_METHODS or row.axtp_event in EXISTING_EVENTS or row.axtp_capability in EXISTING_CAPABILITIES)
    lines = [
        "# AXTP Legacy Migration Plan (Generated)",
        "",
        "## Summary",
        "",
        f"This generated plan migrates {describe_legacy_protocols(rows)} protocol material into an AXTP v1 compatible compatibility layer. It does not modify AXTP Core. Registry changes are emitted as candidate patches only.",
        "",
        "## Source Coverage",
        "",
        f"- Source protocols: {', '.join(covered_legacy_protocols(rows)) or 'none'}",
        f"- Total legacy mappings: {len(rows)}",
        f"- RPC method mappings: {counts['rpc_method']}",
        f"- Event mappings: {counts['event']}",
        f"- Capability mappings: {counts['capability']}",
        f"- STREAM mappings: {counts['stream']}",
        f"- Existing AXTP registry entries reused: {existing_reuse}",
        "",
        "## Invariants",
        "",
        "- AXTP v1 Standard Frame Header remains unchanged.",
        "- PayloadType remains CONTROL=0x01, RPC=0x02, STREAM=0x03.",
        "- Legacy compatibility is implemented by adapter code and generated mapping tables, not by AXTP Core changes.",
        "- Bulk or continuous legacy data is mapped to STREAM. RPC is used only for setup, control and finalization.",
        "",
        "## Migration Matrix",
        "",
        "| Legacy source | Legacy object | Destination | Promotion | Notes |",
        "|---|---|---|---|---|",
    ]
    for row in rows[:260]:
        target = row.axtp_method or row.axtp_event or row.axtp_capability or row.axtp_stream_profile or "adapter_only"
        notes = "; ".join(row.notes) if row.notes else row.description
        lines.append(f"| `{row.legacy_protocol}` | `{row.legacy_name}` / `{row.legacy_id}` | `{row.target_kind}:{target}` | `{row.promotion}` | {notes[:120]} |")
    if len(rows) > 260:
        lines.append(f"| ... | ... | ... | ... | {len(rows) - 260} additional mappings are listed in `legacy-to-axtp-map.generated.yaml`. |")
    lines += [
        "",
        "## Candidate Registry Patch Summary",
        "",
        f"- Methods to add: {len(patch['methods'])}",
        f"- Events to add: {len(patch['events'])}",
        f"- Capabilities to add: {len(patch['capabilities'])}",
        f"- Schemas to add: {len(patch['schemas'])}",
        f"- Legacy mappings to add or merge: {len(patch['legacy_mappings'])}",
        "",
        "## Promotion Decisions",
        "",
        "- Existing mappings such as `BetaDeviceInfo -> device.getInfo` and `BetaBrightnessSet -> display.setBrightness` remain authoritative.",
        "- AXDP commands are retained as adapter mappings unless they target an already registered AXTP method.",
        "- Firmware/file/log/media/raw streams are represented as STREAM profiles and must not place large chunks into RPC bodies.",
        "",
        "## Unresolved Source Issues",
        "",
    ]
    for issue in issues:
        lines.append(f"- [{issue['priority'] or 'P?'}] {issue['issue']}: {issue['recommendation']}")
    lines += [
        "- `migration/legacy-sources/extracted` and `migration/legacy-sources/normalized` were absent; this run bootstrapped directly from current raw legacy files.",
        "- Several legacy rows lack detailed request/response field definitions. Generated schemas therefore preserve legacy payload bytes plus status metadata until normalized field-level schemas are available.",
        "",
        "## Implementation Order",
        "",
        "1. Review `legacy-to-axtp-map.generated.yaml` for mapping correctness.",
        "2. Review `registry-patches.generated.yaml` domain-by-domain before applying any patch to source registries.",
        "3. Implement the C++ legacy adapter using the generated mapping table as data, not hard-coded switch statements.",
        "4. Run generated test vectors against the adapter and AXTP runtime boundary.",
    ]
    return "\n".join(lines) + "\n"


def build_compatibility_layer(rows: list[Mapping]) -> str:
    return f"""# AXTP Legacy Compatibility Layer (Generated)

## Boundary

The compatibility layer sits outside AXTP Core. It accepts the legacy protocols covered by the generated map ({describe_legacy_protocols(rows)}), translates them into AXTP RPC/Event/STREAM operations, and translates responses back to the legacy caller.

The layer must not change the AXTP v1 Frame Header, STREAM header, CONTROL opcodes, RPC operation values or PayloadType registry.

## Request Translation

- Decode the legacy envelope and identify `legacy_protocol`, `legacy_id` or Class/Method alias.
- Resolve the mapping from `legacy-to-axtp-map.generated.yaml`.
- For `rpc_method`, build an AXTP RPC request with the mapped method name and schema.
- For `stream`, call the mapped setup/finalization RPC when present, then move chunk or continuous data over `PayloadType=STREAM`.
- Preserve raw legacy payload bytes in adapter diagnostics when a field-level schema is not yet available.

## Response Translation

- Map AXTP `SUCCESS` to the legacy success status for the source protocol.
- Map `RPC_PARAM_INVALID`, `BUSY` and adapter validation failures to the source protocol status vocabulary.
- For unmapped legacy status values, return `LEGACY_STATUS_UNMAPPED` internally and the nearest legacy failure code externally.
- Preserve the original request correlation field, such as an AXDP command value or JSON `Seq`.

## Event Translation

- Legacy notify/subscribe events are normalized into AXTP Event Registry names.
- AXTP event payloads include normalized fields when known and raw legacy payload bytes when only an alias is available.
- Event fan-out and subscription filtering remain adapter behavior.

## Capability Projection

- Legacy support matrices and configuration-name rows project into candidate AXTP capabilities.
- `capability.supportedMethods` is generated from mapped methods exposed by the adapter for the current device model.
- Conflicts between legacy feature bits and AXTP capability declarations produce `LEGACY_CAPABILITY_CONFLICT`.

## STREAM Bridging

- Firmware data, file transfer, log upload, media, KVM, raw audio/video and state-sync bursts use STREAM.
- RPC bodies may carry only setup metadata, small control parameters, result summaries and task IDs.
- STREAM uses the AXTP v1 16-byte header: `streamId:uint32`, `seqId:uint32`, `cursor:uint64`.
- WebSocket Unframed JSON compatibility may expose setup/control RPCs but must not carry STREAM bytes.

## Failure Behavior

- Unknown legacy command: `LEGACY_CMD_UNMAPPED`.
- Malformed payload: `LEGACY_PAYLOAD_INVALID`.
- Payload shorter than required fixed layout: `LEGACY_PAYLOAD_TOO_SHORT`.
- Payload longer than supported fixed layout: `LEGACY_PAYLOAD_TOO_LONG`.
- Unsupported legacy field: `LEGACY_FIELD_UNSUPPORTED`.
- Adapter timeout while waiting for a legacy response: `LEGACY_RESPONSE_TIMEOUT`.

## Generated Coverage

- Total mappings: {len(rows)}
- STREAM mappings: {sum(1 for row in rows if row.target_kind == 'stream')}
- Event mappings: {sum(1 for row in rows if row.target_kind == 'event')}
- Capability mappings: {sum(1 for row in rows if row.target_kind == 'capability')}
"""


def build_cpp_plan(rows: list[Mapping]) -> str:
    return """# C++ Legacy Adapter Plan (Generated)

## Goal

Provide a C++ legacy adapter skeleton that consumes `workspace/legacy-migration/generated/legacy-to-axtp-map.generated.yaml` and exposes LEGACY_PROTOCOLS traffic as AXTP v1 RPC/Event/STREAM operations.

## Components

- `LegacyProtocolDetector`: inspects incoming bytes or JSON and returns one of the generated `legacy_protocol` values.
- `LegacyCommandDecoder`: parses protocol-specific command value, sequence/correlation fields, legacy status and raw payload bytes.
- `LegacyToAxtpMapper`: resolves generated mapping entries and builds AXTP RPC requests, event emissions or stream-open instructions.
- `AxtpToLegacyMapper`: translates AXTP responses/events back into legacy response envelopes.
- `LegacyStatusMapper`: owns status mapping tables such as `0x00 -> SUCCESS`, `0x01 -> RPC_PARAM_INVALID`, `0x02 -> BUSY`.
- `LegacyStreamBridge`: moves firmware/file/log/media chunks over AXTP STREAM while keeping setup/control in RPC.
- `LegacyAdapter`: orchestrates detection, decode, mapping, AXTP dispatch and legacy response encoding.

## Header Sketch

```cpp
namespace axtp::legacy {

enum class LegacyProtocol {
  kAxdpHid,
  kVm33HttpJson,
  kSignageSdk,
  kUnknown,
};

enum class LegacyTargetKind {
  kRpcMethod,
  kEvent,
  kCapability,
  kStream,
};

struct LegacyEnvelope {
  LegacyProtocol protocol;
  std::string legacyId;
  std::string legacyName;
  std::uint32_t sequence = 0;
  std::vector<std::uint8_t> payload;
};

struct AxtpTarget {
  LegacyTargetKind kind;
  std::string methodName;
  std::string eventName;
  std::string capabilityName;
  std::string streamProfile;
};

class LegacyProtocolDetector {
 public:
  LegacyProtocol Detect(std::span<const std::uint8_t> bytes) const;
};

class LegacyCommandDecoder {
 public:
  Result<LegacyEnvelope> Decode(LegacyProtocol protocol, std::span<const std::uint8_t> bytes) const;
};

class LegacyToAxtpMapper {
 public:
  Result<AxtpTarget> Resolve(const LegacyEnvelope& envelope) const;
};

class AxtpToLegacyMapper {
 public:
  Result<std::vector<std::uint8_t>> EncodeResponse(const LegacyEnvelope& request, const Message& response) const;
  Result<std::vector<std::uint8_t>> EncodeEvent(const Message& event) const;
};

class LegacyStatusMapper {
 public:
  ErrorCode ToAxtp(LegacyProtocol protocol, std::int32_t legacyStatus) const;
  std::int32_t FromAxtp(LegacyProtocol protocol, ErrorCode error) const;
};

class LegacyStreamBridge {
 public:
  Result<std::uint32_t> Open(const LegacyEnvelope& envelope, const AxtpTarget& target);
  Result<void> PushChunk(std::uint32_t streamId, std::span<const std::uint8_t> chunk);
  Result<void> Close(std::uint32_t streamId);
};

class LegacyAdapter {
 public:
  Result<std::vector<std::uint8_t>> Handle(std::span<const std::uint8_t> incoming);
};

}  // namespace axtp::legacy
```

## Data Generation

- Generate compact lookup tables from `legacy-to-axtp-map.generated.yaml`.
- Keep generated tables separate from AXTP Core generated headers.
- Do not introduce new PayloadType constants.
- STREAM mappings must expose stream profile, setup method if any, and status mapping.

## Tests

- Run `test-vectors.generated.json` through `LegacyAdapter::Handle`.
- Assert existing Beta mappings target current generated AXTP method IDs.
- Assert firmware/file/log/media bulk vectors use STREAM.
- Assert unknown command and invalid payload vectors produce legacy adapter errors.
""".replace("LEGACY_PROTOCOLS", describe_legacy_protocols(rows))


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    axdp = read_axdp_methods()
    vm33_methods, vm33_events, vm33_caps, issues = read_vm33_methods()
    md = read_markdown_entries()
    rows = dedupe_mappings(read_existing_legacy_mappings() + axdp + vm33_methods + vm33_events + vm33_caps + md)
    patch = build_registry_patch(rows)

    (OUTPUT_DIR / "legacy-to-axtp-map.generated.yaml").write_text(
        dump_yaml({"metadata": {"name": "AXTP legacy to AXTP migration map", "generated": True}, "mappings": [mapping_to_dict(r) for r in rows]}),
        encoding="utf-8",
    )
    (OUTPUT_DIR / "registry-patches.generated.yaml").write_text(dump_yaml(patch), encoding="utf-8")
    (OUTPUT_DIR / "migration-plan.generated.md").write_text(build_migration_plan(rows, patch, issues), encoding="utf-8")
    (OUTPUT_DIR / "compatibility-layer.generated.md").write_text(build_compatibility_layer(rows), encoding="utf-8")
    (OUTPUT_DIR / "cpp-legacy-adapter-plan.generated.md").write_text(build_cpp_plan(rows), encoding="utf-8")
    (OUTPUT_DIR / "test-vectors.generated.json").write_text(
        json.dumps(build_test_vectors(rows), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"generated {len(rows)} mappings")
    print(f"methods={len(patch['methods'])} events={len(patch['events'])} capabilities={len(patch['capabilities'])} schemas={len(patch['schemas'])}")


if __name__ == "__main__":
    main()
