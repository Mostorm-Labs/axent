# Third Party Lock

This file records the source of third-party assets vendored or referenced by Axent v1.

| Path | Source | Reference | Notes |
| --- | --- | --- | --- |
| `third_party/axdp` | vendor snapshot from existing local checkout | `c8a1068` | Source checkout was `https://gitee.com/auditoryworks_hamedal/axdp.git` before vendoring. |
| `third_party/axtp` | `https://github.com/Mostorm-Labs/axtp.git` vendor snapshot | `spec/v0.13.0` (`9357d8215164f4ca3df865a72d313d495ac8b682`) | Historical snapshot; the current runtime contract is locked by `third_party/axtp-cpp-runtime/AXTP_SPEC.lock.yaml`. |
| `third_party/axtp-cpp-runtime` | `https://github.com/Mostorm-Labs/axtp-cpp-runtime.git` | `v0.18.0.1` (`7a0e31c3c833ee0ac65d41221fc47e647cf4ae69`) | Bound to AXTP `spec/v0.18.0` (`2d10c51b27101d2873b5bf5495f39e4a7725bad4`). |
| `third_party/IXWebSocket` | `https://github.com/machinezone/IXWebSocket.git` | `2efe037c9cc96fd536774f17bdb5215161ee5087` | Axent-owned WebSocket concrete dependency. |
| `third_party/hidapi` | `https://github.com/libusb/hidapi.git` | `c3509c11174fe80ff59a47119433a7db5299af85` | Axent-owned HID concrete dependency. |
| `third_party/tea/macos/TeaSdkMacOS_1.0.0.36` | vendor binary SDK from existing local package | `TeaSdkMacOS_1.0.0.36` | TEA macOS SDK. |
