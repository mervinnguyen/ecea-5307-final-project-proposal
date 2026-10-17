# Buildroot Build Notes (Issue #1)

Reproducible Buildroot image for the Raspberry Pi 5B.

## Versions

| Item | Value |
|---|---|
| Buildroot release | 2026.08 (commit `d5180309b1`) |
| Defconfig | `raspberrypi5_defconfig` |
| Build host | GitHub Codespaces, Ubuntu 24.04, 2 CPUs, ~8 GB RAM |

## Host prerequisites

Buildroot reported these missing host packages, which were installed with apt:

```bash
sudo apt install bison cpio bc
```

## Build steps

```bash
cd /workspaces/final-project
git clone https://gitlab.com/buildroot.org/buildroot.git /workspaces/buildroot
cd /workspaces/buildroot
git switch -c project/rtc-monitoring 2026.08

make O=/workspaces/buildroot-output \
  BR2_EXTERNAL=/workspaces/final-project/buildroot-external \
  raspberrypi5_defconfig
```

From the final-project repository root, apply the checked-in configuration
and build with the external tree:

```bash
cp config/buildroot.config /workspaces/buildroot-output/.config
make -C /workspaces/buildroot O=/workspaces/buildroot-output \
  BR2_EXTERNAL=/workspaces/final-project/buildroot-external olddefconfig
make -C /workspaces/buildroot O=/workspaces/buildroot-output \
  BR2_EXTERNAL=/workspaces/final-project/buildroot-external
```

The output directory is kept outside the Buildroot source tree.
Generated files in `buildroot-output` are not committed.
The checked-in configuration records the external tree at
`/workspaces/final-project/buildroot-external`; if the repository is checked
out elsewhere, update `BR2_EXTERNAL` and regenerate the configuration with
`olddefconfig`.

The full build takes a long time on 2 CPUs. To keep it running if the terminal
disconnects, run it detached with a log file:

```bash
setsid nohup make O=/workspaces/buildroot-output > /workspaces/build.log 2>&1 < /dev/null &
```

An interrupted build can be resumed by rerunning the same `make` command.

## Saved configuration

The resulting Buildroot configuration is stored in
[`config/buildroot.config`](../config/buildroot.config).

## Build result

- Build outcome: **Success**; Buildroot completed with the health-monitor
  external package enabled and generated the Raspberry Pi 5 image outputs.
- Image: `images/sdcard.img` — about 153 MiB.
- The incremental Sprint 2 rebuild completed at 2026-10-17 04:24 UTC.
- The generated image remains in `/workspaces/buildroot-output/images/` and is
  not committed to the repository.

## Health monitor

`rtc-health-monitor` is built as an AArch64 target binary and installed at
`/usr/bin/rtc-health-monitor`. Its BusyBox init script is installed at
`/etc/init.d/S99rtc-health-monitor` and starts it at boot with a 60-second
interval, appending records to `/data/health.log`.

Each whitespace-separated `key=value` record contains:

- `system_time`: UTC system clock in ISO 8601 format.
- `rtc_time`: RTC time read from `/dev/rtc0`, in UTC/ISO 8601 format.
- `uptime_sec`: uptime in whole seconds from `/proc/uptime`.
- `memory_total_kb` and `memory_available_kb`: values from `/proc/meminfo`.
- `data_total_bytes` and `data_used_bytes`: filesystem capacity and used
  space from `statvfs("/data")`.
- `network`: `up` if a non-loopback interface is up, `down` if interfaces
  exist but none are up, or `unavailable` if no interface state can be read.

Unavailable RTC, system, memory, uptime, or filesystem metrics are written as
`unavailable`; missing network state does not stop collection. A failure to
open or append the log is reported to stderr and exits unsuccessfully.

Host validation: compiled with `-Wall -Wextra -Werror`; `--once` emitted a
record, two invocations appended two records, and an invalid zero-second
interval was rejected. The target binary and init script are present in the
Buildroot target tree. Pi boot, hardware RTC reading, and reboot-persistence
tests are pending access to the Raspberry Pi 5B.

## Flashing and boot test

**Pending:** The image has not yet been written to an SD card or booted on
the Raspberry Pi 5B because the hardware is not currently available. Sprint 1
issue #1 remains open until target boot and version details are recorded.

Before flashing, confirm the target device name. Writing an image erases the
selected device.

- Flash method: Pending
- Boot result: Pending
- Kernel version (`uname -a`): Pending
- Userspace/Buildroot version (`cat /etc/os-release`): Pending
