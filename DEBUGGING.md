# Debugging

This project supports on-chip debugging of the STM32U595 over an ST-Link probe
using OpenOCD + GDB, integrated with VS Code via the
[Cortex-Debug](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug)
extension.

Building the firmware is only supported on Linux (see main [README](README.md)).
Debugging, however, can be done from three environments:

| Environment | Build | Debug | When to use |
|---|---|---|---|
| **Linux native** | Linux | Linux | Primary path, simplest |
| **Windows + WSL2** | WSL | WSL | Single-environment Windows setup |
| **Windows hybrid** | WSL or Docker | Native Windows | Avoids USB forwarding friction |

## Common VS Code setup

Install the [Cortex-Debug](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug)
extension. VS Code should prompt you automatically on first opening this repo;
otherwise search `cortex-debug` in the Extensions panel.

The committed [.vscode/launch.json](.vscode/launch.json) provides two
configurations:

- **Debug (OpenOCD + ST-Link)** — builds with `MODE=debug`, flashes the board,
  and halts at `main()`
- **Attach (OpenOCD + ST-Link)** — attaches to a board that's already running,
  without flashing

Cortex-Debug defaults to looking for `arm-none-eabi-gdb` on `PATH`. If you
install the ARM toolchain from xpack, Arm GNU, or Homebrew, this is the
binary you get. Linux apt users get `gdb-multiarch` instead and need the
symlink step in the Linux section below.

---

## Linux (Ubuntu 22.04+)

1. Install build prerequisites (if you haven't already):

   ```sh
   sudo apt-get update && sudo apt-get install --no-install-recommends -y $(cat ./packages.txt)
   ```

2. Install debug prerequisites:

   ```sh
   sudo apt-get install --no-install-recommends -y $(cat ./packages.debug.txt)
   ```

3. Verify OpenOCD is at least version **0.12.0** (STM32U5 support landed in
   0.12):

   ```sh
   openocd --version
   ```

   If apt gives you an older version, install from
   [xpack-openocd releases](https://github.com/xpack-dev-tools/openocd-xpack/releases)
   and put its `bin` directory on your `PATH`.

4. Symlink `gdb-multiarch` as `arm-none-eabi-gdb` so Cortex-Debug finds it:

   ```sh
   sudo ln -s "$(which gdb-multiarch)" /usr/local/bin/arm-none-eabi-gdb
   ```

   (If you installed the toolchain from xpack instead of apt, you already have
   `arm-none-eabi-gdb` and can skip this.)

5. Allow non-root access to the ST-Link (one-time):

   ```sh
   sudo cp /usr/share/openocd/contrib/60-openocd.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```

   Unplug and replug the ST-Link after this step.

6. Open the project in VS Code and press **F5**.

---

## Windows + WSL2 (single environment)

Build and debug both happen in WSL. The ST-Link USB device must be forwarded
from Windows into WSL using [usbipd-win](https://github.com/dorssel/usbipd-win).

1. Inside your WSL distro, follow the **Linux** steps above.

2. On the **Windows** side, install usbipd (PowerShell as Administrator):

   ```powershell
   winget install usbipd
   ```

3. With the ST-Link plugged in, bind the device once (PowerShell as
   Administrator):

   ```powershell
   usbipd list
   # find the ST-Link row and note its BUSID (e.g. 2-4)
   usbipd bind --busid <BUSID>
   ```

4. Attach it to WSL (does not require admin after the initial bind):

   ```powershell
   usbipd attach --wsl --busid <BUSID>
   ```

   **`attach` must be re-run every time** you unplug/replug the ST-Link or
   restart WSL. `bind` is persistent.

5. In WSL, confirm the device appeared:

   ```sh
   lsusb | grep -i st
   ```

   You should see something like `STMicroelectronics ST-LINK/V2` (or V3).

6. Open VS Code from WSL so extensions run in the same environment as your
   toolchain:

   ```sh
   cd /mnt/c/path/to/whale-tag-stm32
   code .
   ```

   You'll see `WSL: <distro>` in the bottom-left status bar. Install
   Cortex-Debug inside the WSL remote — extensions are per-remote.

7. Press **F5**.

---

## Windows hybrid (build in WSL, debug on native Windows)

This path avoids the USB forwarding dance entirely. The ST-Link is used by
native Windows tools, which see it without any configuration. The build still
runs in WSL (or Docker), but the ELF ends up on the shared filesystem at
`build/v3_1_1/debug/ceti-whale-tag.elf`, which both environments can see.

1. Build the firmware in WSL (or Docker):

   ```sh
   make MODE=debug
   ```

   Or use the VS Code **Build Debug** task from a WSL-remote window.

2. Install the ST-Link driver on Windows. The easiest source is
   [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) —
   installing it registers the ST-Link driver even if you don't use the GUI.

3. Install OpenOCD on Windows:
   - Download the latest from
     [xpack-openocd releases](https://github.com/xpack-dev-tools/openocd-xpack/releases)
     (pick `win32-x64`)
   - Extract somewhere (e.g. `C:\tools\openocd`)
   - Add the `bin` directory to your user `PATH`

4. Install the ARM GNU Toolchain on Windows:
   - Download from
     [Arm GNU Toolchain downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
     (pick the Windows `.exe` installer)
   - During install, check **"Add path to environment variable"**

5. Open a new PowerShell and verify:

   ```powershell
   openocd --version
   arm-none-eabi-gdb --version
   ```

6. Open the project in native Windows VS Code (not WSL-remote) and press
   **F5**. The `Debug (OpenOCD + ST-Link)` configuration will build in
   WSL via the `build-debug` task and then flash/debug natively.

   > **Note:** the `build-debug` task runs `make`, which on native Windows
   > needs to invoke WSL. If the task fails, set the VS Code default terminal
   > profile for this workspace to WSL, or manually build in WSL before
   > launching and use the **Attach** configuration instead.

---

## Running from the command line

Useful for when you don't want VS Code in the loop. From the project root:

```sh
# Terminal 1 — start OpenOCD GDB server
openocd -f whale-tag-stm32_stm32u595_debug.cfg

# Terminal 2 — connect with GDB
arm-none-eabi-gdb build/v3_1_1/debug/ceti-whale-tag.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) b main
(gdb) c
```

Linux apt users without the symlink from step 4 above can substitute
`gdb-multiarch` for `arm-none-eabi-gdb`.

## Troubleshooting

- **`Error: Can't find target/stm32u5x.cfg`** — Your OpenOCD is too old. Need
  ≥ 0.12.0. Install from xpack.
- **`Error: open failed`** / **`no ST-Link detected`** — Make sure no other
  process (STM32CubeProgrammer, a stray `openocd`, CubeIDE) is holding the
  probe. On WSL, re-run `usbipd attach`. On Linux native, check the udev rule
  step.
- **`Unable to find [arm-none-eabi-gdb]`** — Linux apt path only installs
  `gdb-multiarch`. Create the symlink from step 4 of the Linux section, or
  add `"gdbPath": "gdb-multiarch"` to your local launch config.
- **WSL can't see the ST-Link after a reboot** — `usbipd attach` has to be
  re-run every session.
- **Breakpoints silently don't fire** — Make sure you built with `MODE=debug`,
  not `release`. The VS Code launch config does this automatically via
  `preLaunchTask`.
