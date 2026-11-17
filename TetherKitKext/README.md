# TetherKit.kext — kernel-extension build for OpenCore

OpenCore injects kernel extensions (kexts) at boot. It cannot load a DriverKit
system extension (`.dext`): those are installed and launched by macOS itself,
from a signed app, after boot. This folder therefore builds TetherKit as a
kext, `TetherKit.kext`, that OpenCore can inject like any other.

Because OpenCore injects it, the kext needs no Apple Developer account, no
entitlements, no code signature, and no change to SIP.

## What it is

`TetherKitRNDIS.cpp` is the IOKit driver from HoRNDIS (an `IOEthernetController`
on top of `IOUSBHostFamily`), which is the code the TetherKit DriverKit driver
was ported from. It carries the protocol fixes made in TetherKit:

- RNDIS framing and bounds checks come from `../TetherKitDriver/RNDISProtocolCore.cpp`,
  the same source file the DriverKit driver compiles (and the unit tests cover).
- Bulk OUT transfers that land on an exact packet-size multiple get one pad
  byte, so the phone does not sit waiting for the rest of the transfer.
- The device's `max_transfer_size` is clamped to a sane range.
- The MAC address query falls back to `OID_802_3_CURRENT_ADDRESS`, and a
  multicast MAC is repaired.

Not carried over: the interrupt-endpoint wait for `RESPONSE_AVAILABLE`. The
kext polls for the RNDIS response, as HoRNDIS always has (and as the DriverKit
driver falls back to).

Matching is the same as the DriverKit driver: interface classes 224/1/3
(stock Android), 239/4/1 and 2/2/255 (Linux gadget), plus device class 224/0/0.

## Build

```sh
cd TetherKitKext
make          # -> build/TetherKit.kext
make check    # also verifies symbols against the running kernel
```

Requires Xcode or the Command Line Tools. Options: `DEBUG=1` (verbose logging),
`MIN_MACOS=10.15` (oldest macOS to support), `ARCHS=x86_64`,
`KEXT_BUNDLE_ID=...`, `KEXT_VERSION=...`.

## Install with OpenCore

1. Mount your EFI partition and copy `build/TetherKit.kext` to `EFI/OC/Kexts/`.
2. Add it to `config.plist` under `Kernel` → `Add` (or use *OC Snapshot* in
   ProperTree, which writes the same entry):

   ```xml
   <dict>
       <key>Arch</key>
       <string>x86_64</string>
       <key>BundlePath</key>
       <string>TetherKit.kext</string>
       <key>Comment</key>
       <string>Android USB tethering (RNDIS)</string>
       <key>Enabled</key>
       <true/>
       <key>ExecutablePath</key>
       <string>Contents/MacOS/TetherKit</string>
       <key>MaxKernel</key>
       <string></string>
       <key>MinKernel</key>
       <string>19.0.0</string>
       <key>PlistPath</key>
       <string>Contents/Info.plist</string>
   </dict>
   ```

   `MinKernel` 19.0.0 is macOS 10.15, matching the default `MIN_MACOS`.
3. Reboot. Plug in the phone and turn on USB tethering; a new Ethernet
   interface appears in System Settings → Network.

To remove it, delete the entry (or set `Enabled` to false) and reboot.

Do not run this kext and the TetherKit DriverKit extension at the same time:
both match the same USB interfaces.

## Checking that it loaded

```sh
kmutil showloaded | grep -i tetherkit
```

```sh
log show --last 5m --predicate 'process == "kernel" AND eventMessage CONTAINS "TetherKit"'
```

## Status

The kext builds with the macOS 26 SDK, and `make check` resolves its symbols
against the macOS 26 (Darwin 25) boot kernel collection. It has not yet been
booted and exercised with a phone; treat the first boot as a test, and keep a
bootable USB copy of your EFI without the kext in case you need to back out.

## License

GPL, as HoRNDIS and TetherKit. See the headers of the source files.
