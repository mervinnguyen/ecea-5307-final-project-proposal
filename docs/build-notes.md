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
git clone https://gitlab.com/buildroot.org/buildroot.git /workspaces/buildroot
cd /workspaces/buildroot
git switch -c project/rtc-monitoring 2026.08

make O=/workspaces/buildroot-output raspberrypi5_defconfig
```

From the final-project repository root, apply the checked-in configuration
and build:

```bash
cp config/buildroot.config /workspaces/buildroot-output/.config
make -C /workspaces/buildroot O=/workspaces/buildroot-output olddefconfig
make -C /workspaces/buildroot O=/workspaces/buildroot-output
```

The output directory is kept outside the Buildroot source tree.
Generated files in `buildroot-output` are not committed.

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

- Build outcome: **Success**; Buildroot completed and generated the Raspberry
  Pi 5 image outputs.
- Image: `images/sdcard.img` — 159,384,064 bytes (about 153 MiB).
- Build duration: the captured build log spans about 1 hour 54 minutes
  (2026-10-17 01:54–03:49 UTC); the full build may have started earlier.
- The generated image remains in `/workspaces/buildroot-output/images/` and is
  not committed to the repository.

## Flashing and boot test

_TODO — complete once the image is written to an SD card and booted._

Before flashing, confirm the target device name. Writing an image erases the
selected device.

- Flash method: _TODO_
- Boot result: _TODO_
- Kernel version (`uname -a`): _TODO_
- Userspace/Buildroot version (`cat /etc/os-release`): _TODO_
