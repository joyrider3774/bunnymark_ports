#!/usr/bin/env python3
"""Build BunnyMark for every device and put the files to flash or run in releases/.

Every file is named <device>_BunnyMark<variant>.<ext>, for example PicoSystem_BunnyMark.uf2:

  CHGame         .bin   plug it in, pick the port and upload, its bootloader takes it over USB
  Arduboy        .hex   upload it, or open it in an emulator (Ardens); .bin the same program as a
                        plain image from address 0
  ESPboy         .bin   _LovyanGFX and _TFT_eSPI, one per display library; flash at 0x0
  PyBadge        .uf2   double press reset and copy it onto the drive that appears
  PyGamer        .uf2   same as the PyBadge
  PicoSystem     .uf2   hold X while switching on and copy it onto the drive that appears
  Explorer       .uf2   hold BOOT while pressing RESET and copy it onto the drive that appears
  Tufty          .uf2   hold HOME while pressing RESET and copy it onto the drive that appears
  ThumbyColor    .uf2   _ARM and _RISCV, the same sketch for the RP2350's Cortex-M33 and its Hazard3
                        RISC-V cores; hold DOWN while switching on and copy it onto the drive
  Aka            .zip   the folder for the AKA launcher's SD card: firmware.bin, meta.json, screen.bmp
  Windows        .exe   _SDL2 and _SDL3, linked statically, they run on their own
  Vircon32       .v32   the cartridge, for the Vircon32 emulator (desktop or web); _Lua the same
                        program in Lua, _TIC80 and _PICO8 the TIC-80 and PICO-8 carts, all three
                        made into Vircon32 cartridges by v32lua; _CPP the same program in C++,
                        through v32c++. Each also as _Opt (_TIC80_Opt, ...): its assembly put
                        through the v32opt optimizer (-O3) before it is assembled
  Web            .zip   _SDL2 and _SDL3: index.html, .js and .wasm, ready for a web server or an
                        itch.io HTML game
  TIC80          .tic   the TIC-80 cart (made here from tic80/bunnymark.lua), for TIC-80 itself
  PICO8          .p8    the PICO-8 cart, for PICO-8 itself

The settings of a build are passed to the compiler as defines; the sources are not touched. What
is built for each device is listed in TARGETS below, how in DEVICES.

Needs the Arduino IDE 1.8 folder with the board packages (arduino-builder) for the ESPboy, the
PyBadge, the PyGamer and the Pimoroni devices, the Arduino IDE 2's arduino-cli for the CHGame
(whose board package is only published for IDE 2) and the Arduboy, bateske/CHGame's libraries
(CHGfx, CHGame, CHSd) for the CHGame, ESP-IDF and the Gamebuino AKA library for the AKA, and MSYS2
with the mingw64 cmake, ninja, gcc, SDL2 and SDL3 for the Windows exes, and the Vircon32 DevTools
(compile, assemble, png2vircon, packrom) for the Vircon32 cartridge, and v32lua
(github.com/wedge1020/v32lua) with them for the Vircon32 cartridges made from the TIC-80 and PICO-8
carts. The TIC-80 and PICO-8 carts themselves need nothing.

Usage:
  python tools/build_releases.py                 build everything
  python tools/build_releases.py --only Tufty ESPboy
  python tools/build_releases.py --list          show what would be built

  --arduino DIR      the Arduino IDE 1.8 folder (default C:/arduino, or ARDUINO_DIR)
  --arduino2 DIR     the Arduino IDE 2 folder, whose arduino-cli builds the CHGame and the Arduboy
                     (default C:/arduino2, or ARDUINO2_DIR), with its own settings file
                     (~/.arduinoIDE/arduino-cli.yaml) so it finds the IDE's libraries
  --arduino-cli PATH build every Arduino device with this arduino-cli instead, with its own
                     settings (default ARDUINO_CLI). This is what the CI workflow uses
  --chgame-libs DIR  the libraries folder of bateske/CHGame's board package, as installed
                     (default the installed CHGame 0.3.0 package's, or CHGAME_LIBS)
  --idf DIR          ESP-IDF (default IDF_PATH, or C:/github/esp-idf)
  --idf-tools DIR    where ESP-IDF installed its tools (default IDF_TOOLS_PATH, or C:/Espressif)
  --aka-lib DIR      the Gamebuino AKA library (default AKA_LIB_DIR, or C:/github/Gamebuino_AKA_lib)
  --msys2 DIR        MSYS2's mingw64 bin folder (default C:/msys64/mingw64/bin, or MSYS2_BIN);
                     where it is not there, cmake, ninja and gcc come from the PATH
  --cross-windows    build the Windows exes on Linux with mingw-w64 (tools/mingw-w64.cmake),
                     downloading SDL2 and SDL3 and linking them statically
  --vircon32 DIR     the Vircon32 DevTools folder (default C:/utils/vircon32/DevTools, or
                     VIRCON32_DEVTOOLS; the Linux package puts them in /usr/local/Vircon32/DevTools)
  --emsdk DIR        the Emscripten SDK for the browser build (default EMSDK, or C:/github/emsdk)
  --v32lua PATH      the v32lua compiler (default V32LUA, or C:/github/v32lua/bin/v32lua)
  --v32cxx PATH      the v32c++ transpiler (default V32CXX, or C:/github/v32cxx/bin/v32c++)
  --v32opt PATH      the v32opt assembly optimizer for the _Opt cartridges (default V32OPT, or
                     C:/github/v32opt/v32opt)
"""
import argparse
import glob
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

GAME = "BunnyMark"
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RELEASES = os.path.join(ROOT, "releases")
WORK = os.path.join(tempfile.gettempdir(), "bunnymark_releases")

# TFT_eSPI takes its display setup from its own User_Setup.h, which a fresh install of the
# library has set for some other display. USER_SETUP_LOADED makes it take these instead: the
# ESPboy's ST7735, with its chip select on the I/O expander (no CS pin) and DC on GPIO16
TFT_ESPI_ESPBOY = {
    "USER_SETUP_LOADED": 1, "ST7735_DRIVER": 1, "ST7735_GREENTAB3": 1,
    "TFT_WIDTH": 128, "TFT_HEIGHT": 128, "TFT_RGB_ORDER": "TFT_BGR",
    "TFT_CS": -1, "TFT_DC": 16, "TFT_RST": -1, "LOAD_GLCD": 1,
    "SPI_FREQUENCY": 27000000, "SPI_READ_FREQUENCY": 20000000, "SPI_TOUCH_FREQUENCY": 2500000,
}

# (device, variant added to the file name, defines)
TARGETS = [
    ("CHGame", "", {}),
    ("Arduboy", "", {}),
    ("ESPboy", "_LovyanGFX", {"LOVYANGFX": 1}),
    ("ESPboy", "_TFT_eSPI", dict(LOVYANGFX=0, **TFT_ESPI_ESPBOY)),
    ("PyBadge", "", {}),
    ("PyGamer", "", {}),
    ("PicoSystem", "", {}),
    ("Explorer", "", {}),
    ("Tufty", "", {}),
    # the same sketch for each of the RP2350's two kinds of core, see DEVICES
    ("ThumbyColor", "_ARM", {}),
    ("ThumbyColor", "_RISCV", {}),
    ("Aka", "", {}),
    ("Windows", "_SDL2", {"BUNNYMARK_SDL": 2}),
    ("Windows", "_SDL3", {"BUNNYMARK_SDL": 3}),
    ("Vircon32", "", {}),
    ("Web", "_SDL2", {"BUNNYMARK_SDL": 2}),
    ("Web", "_SDL3", {"BUNNYMARK_SDL": 3}),
    ("TIC80", "", {}),
    ("PICO8", "", {}),
    # the TIC-80 and PICO-8 carts made into Vircon32 cartridges by v32lua
    ("Vircon32", "_TIC80", {}),
    ("Vircon32", "_PICO8", {}),
    # and the C version written in Lua, also compiled by v32lua, and in C++, through v32c++
    ("Vircon32", "_Lua", {}),
    ("Vircon32", "_CPP", {}),
    # every Vircon32 cartridge again, its assembly put through the v32opt optimizer (-O3) first
    ("Vircon32", "_Opt", {}),
    ("Vircon32", "_TIC80_Opt", {}),
    ("Vircon32", "_PICO8_Opt", {}),
    ("Vircon32", "_Lua_Opt", {}),
    ("Vircon32", "_CPP_Opt", {}),
]

# sketch: the sketch folder. fqbn: board and options. cli: built with arduino-cli even next to an Arduino IDE 1.8. libraries: extra library
# folders, relative to --chgame-libs. toolchain: the compiler a core asks for, pinned (see
# toolchain_pref). outputs: what is released, "uf2 from bin" made here from the .bin at the given
# base address and UF2 family. arch: a variant's CPU Architecture, put into the fqbn's {arch}
DEVICES = {
    "CHGame": {
        "sketch": "chgame/BunnyMark",
        "fqbn": "CHGame:ch32v:rev0:opt=o2std,periph=game,usb=uploadonly",
        "cli": True,
        "libraries": ["CHGfx", "CHGame", "CHSd"],
        "outputs": ["bin"],
    },
    "Arduboy": {
        "sketch": "arduboy/BunnyMark",
        "fqbn": "arduino:avr:leonardo",
        "cli": True,
        # the .bin is the program alone, from address 0, made here from the .hex (the build's own
        # .bin has the bootloader in it as well)
        "outputs": ["hex", "bin from hex"],
    },
    "ESPboy": {
        "sketch": "espboy/BunnyMark",
        "fqbn": "esp8266:esp8266:d1_mini:xtal=160,vt=flash,exception=disabled,stacksmash=disabled,ssl=basic,"
                "mmu=3232,non32xfer=fast,eesz=4M2M,ip=lm2f,dbg=Disabled,lvl=None____,wipe=none,baud=921600",
        "outputs": ["bin"],
    },
    "PyBadge": {
        "sketch": "pybadge/BunnyMark",
        "fqbn": "adafruit:samd:adafruit_pybadge_m4",
        "toolchain": ("arm-none-eabi-gcc", "9-2019q4"),
        "outputs": ["uf2 from bin"],
        "uf2": (0x4000, 0x55114460),
    },
    "PyGamer": {
        "sketch": "pybadge/BunnyMark",
        "fqbn": "adafruit:samd:adafruit_pygamer_m4",
        "toolchain": ("arm-none-eabi-gcc", "9-2019q4"),
        "outputs": ["uf2 from bin"],
        "uf2": (0x4000, 0x55114460),
    },
    "PicoSystem": {
        "sketch": "picosystem/BunnyMark",
        "fqbn": "rp2040:rp2040:generic:flash=16777216_0,boot2=boot2_w25q080_2_padded_checksum,freq=125,"
                "usbstack=picosdk,opt=Small",
        "outputs": ["uf2"],
    },
    "Explorer": {
        "sketch": "pimoroni2350/BunnyMark",
        "fqbn": "rp2040:rp2040:pimoroni_explorer:flash=16777216_0,arch=arm,freq=150,usbstack=picosdk,opt=Small",
        "outputs": ["uf2"],
    },
    "Tufty": {
        "sketch": "pimoroni2350/BunnyMark",
        "fqbn": "rp2040:rp2040:generic_rp2350:variantchip=RP2530B,psramcs=GPIO8,psram=8mb,flash=16777216_0,"
                "arch=arm,freq=150,usbstack=picosdk,opt=Small",
        "outputs": ["uf2"],
    },
    # arduino-pico has no Thumby Color board: the Generic RP2350 with the RP2350A chip, as in the
    # *_embedded games. Built for the Cortex-M33 (arm) and for the Hazard3 cores (riscv)
    "ThumbyColor": {
        "sketch": "thumbycolor/BunnyMark",
        "fqbn": "rp2040:rp2040:generic_rp2350:variantchip=RP2350A,flash=16777216_0,arch={arch},freq=150,"
                "usbstack=picosdk,opt=Small",
        "arch": {"_ARM": "arm", "_RISCV": "riscv"},
        "outputs": ["uf2"],
    },
    "Aka": {
        "idf": "aka",
        "card": "aka/card/bunnymark",
        "outputs": ["zip"],
    },
    "Windows": {
        "cmake": "sdl",
        "outputs": ["exe"],
    },
    "Vircon32": {
        "vircon32": "vircon32",
        # the variants _TIC80 and _PICO8 are those carts compiled by v32lua instead of the C program,
        # _Lua the C program's Lua twin (vircon32/lua, v32lua's native Vircon32 API); _CPP its C++
        # twin (vircon32/cpp), turned into Vircon32 C by v32c++
        "carts": {"_TIC80": "TIC80", "_PICO8": "PICO8", "_Lua": "Lua"},
        "lua": "vircon32/lua/bunnymark.lua",
        "cpp": "vircon32/cpp/bunnymark.cpp",
        "outputs": ["v32"],
    },
    "TIC80": {
        # the cart as a TIC-80 .lua project (code, then its -- <TILES> and -- <PALETTE> sections),
        # released as a binary .tic made from it here
        "cart": "tic80/bunnymark.lua",
        "outputs": ["tic"],
    },
    "PICO8": {
        "cart": "pico8/bunnymark.p8",
        "outputs": ["p8"],
    },
    "Web": {
        "cmake": "sdl",
        "web": True,
        # index.html with its .js and .wasm, zipped: ready for a web server or an itch.io HTML game
        "outputs": ["web zip"],
    },
}


def file_name(device, variant, ext):
    return "%s_%s%s.%s" % (device, GAME, variant, ext)


def define_flags(defines):
    return " ".join("-D%s=%s" % (name, value) for name, value in sorted(defines.items()))


def bin_to_uf2(data, base, family):
    """The .bin as UF2 blocks of 256 bytes each, for a bootloader that takes the given family"""
    count = (len(data) + 255) // 256
    out = bytearray()
    for i in range(count):
        chunk = data[i * 256:(i + 1) * 256]
        out += struct.pack("<8I", 0x0A324655, 0x9E5D5157, 0x00002000, base + i * 256, 256, i, count, family)
        out += chunk + bytes(476 - len(chunk))
        out += struct.pack("<I", 0x0AB16F30)
    return bytes(out)


def hex_to_bin(text):
    """An Intel HEX file's data as one image from address 0, gaps filled with 0xFF (erased flash)"""
    image = bytearray()
    upper = 0
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith(":"):
            continue
        record = bytes.fromhex(line[1:])
        count, address, kind = record[0], (record[1] << 8) | record[2], record[3]
        data = record[4:4 + count]
        if kind == 0:
            start = upper + address
            if len(image) < start + count:
                image.extend(b"\xff" * (start + count - len(image)))
            image[start:start + count] = data
        elif kind == 2:
            upper = ((data[0] << 8) | data[1]) << 4
        elif kind == 4:
            upper = ((data[0] << 8) | data[1]) << 16
        elif kind == 1:
            break
    return bytes(image)


def toolchain_pref(device, packages, log):
    """The runtime.tools pref that pins a device's compiler, or "" when it pins none. A core asks for
    {runtime.tools.arm-none-eabi-gcc.path} without a version, and with several cores installed the
    builder takes the newest compiler rather than the one the core wants. None when it is missing"""
    pin = DEVICES[device].get("toolchain")
    if not pin:
        return ""
    name, version = pin
    found = sorted(glob.glob(os.path.join(packages, "*", "tools", name, version)))
    if not found:
        with open(log, "w") as f:
            f.write("%s pins %s %s, which is not installed under %s\n" % (device, name, version, packages))
        return None
    return "runtime.tools.%s.path=%s" % (name, found[0].replace(os.sep, "/"))


def cli_packages(cli, config):
    """Where an arduino-cli keeps its board packages"""
    command = [cli] + (["--config-file", config] if config else []) + ["config", "dump", "--format", "json"]
    try:
        import json
        dump = json.loads(subprocess.run(command, capture_output=True, text=True).stdout or "{}")
        data = dump.get("config", dump).get("directories", {}).get("data", "")
        if data:
            return os.path.join(data, "packages")
    except (OSError, ValueError, AttributeError):
        pass
    return os.path.join(os.path.expanduser("~"), ".arduino15", "packages")


# ---------------------------------------------------------------------------------- the builders

def build_arduino(device, variant, defines, build_dir, args, log):
    """An Arduino sketch, with arduino-cli or the IDE 1.8's arduino-builder. Returns the path of the
    build's files without extension, or None"""
    spec = DEVICES[device]
    sketch = os.path.join(ROOT, spec["sketch"])
    name = os.path.basename(sketch)
    fqbn = spec["fqbn"].format(arch=spec["arch"][variant]) if "arch" in spec else spec["fqbn"]
    flags = define_flags(defines)
    cache = build_dir + "_cache"
    os.makedirs(build_dir, exist_ok=True)
    os.makedirs(cache, exist_ok=True)
    libraries = [os.path.join(args.chgame_libs, l) for l in spec.get("libraries", [])]
    for lib in libraries:
        if not os.path.isdir(lib):
            with open(log, "w") as f:
                f.write("%s was not found: pass --chgame-libs <bateske/CHGame's libraries folder>\n" % lib)
            return None

    if args.arduino_cli or spec.get("cli"):
        cli = args.arduino_cli or os.path.join(args.arduino2, "resources", "app", "lib", "backend",
                                               "resources", "arduino-cli" + (".exe" if os.name == "nt" else ""))
        config = None if args.arduino_cli else os.path.join(os.path.expanduser("~"), ".arduinoIDE",
                                                             "arduino-cli.yaml")
        if config and not os.path.isfile(config):
            config = None
        toolchain = toolchain_pref(device, cli_packages(cli, config), log)
        if toolchain is None:
            return None
        command = [cli] + (["--config-file", config] if config else []) + [
            "compile", "--fqbn", fqbn, "--build-path", build_dir, "--build-cache-path", cache,
            "--build-property", "compiler.c.extra_flags=" + flags,
            "--build-property", "compiler.cpp.extra_flags=" + flags]
        for lib in libraries:
            command += ["--library", lib]
        if toolchain:
            command += ["--build-property", toolchain]
        command.append(sketch)
    else:
        portable = os.path.join(args.arduino, "portable")
        toolchain = toolchain_pref(device, os.path.join(portable, "packages"), log)
        if toolchain is None:
            return None
        command = [
            os.path.join(args.arduino, "arduino-builder.exe" if os.name == "nt" else "arduino-builder"),
            "-compile", "-logger=human",
            "-hardware", os.path.join(args.arduino, "hardware"),
            "-hardware", os.path.join(portable, "packages"),
            "-tools", os.path.join(args.arduino, "tools-builder"),
            "-tools", os.path.join(args.arduino, "hardware", "tools", "avr"),
            "-tools", os.path.join(portable, "packages"),
            "-built-in-libraries", os.path.join(args.arduino, "libraries"),
            "-libraries", os.path.join(portable, "sketchbook", "libraries"),
            "-fqbn", fqbn, "-ide-version=10819",
            "-build-path", build_dir, "-build-cache", cache,
            "-prefs", "compiler.c.extra_flags=" + flags,
            "-prefs", "compiler.cpp.extra_flags=" + flags,
        ]
        for lib in libraries:
            command += ["-libraries", lib]
        if toolchain:
            command += ["-prefs", toolchain]
        command.append(os.path.join(sketch, name + ".ino"))
    with open(log, "w") as f:
        f.write(" ".join(command) + "\n\n")
        f.flush()
        result = subprocess.run(command, stdout=f, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        return None
    return os.path.join(build_dir, name + ".ino")


def idf_python(idf_tools):
    """idf.py runs in the virtual environment ESP-IDF made for itself"""
    roots = [idf_tools] if idf_tools else []
    roots.append(os.path.join(os.path.expanduser("~"), ".espressif"))
    for root in roots:
        for where in ("Scripts", "bin"):
            found = sorted(glob.glob(os.path.join(root, "python_env", "*", where, "python*")))
            found = [f for f in found if os.path.isfile(f) and not f.endswith(("w.exe", "-config"))]
            if found:
                return found[-1]
    return sys.executable


def build_aka(defines, build_dir, args, log):
    """The AKA app with ESP-IDF. Returns the .bin, or None"""
    env = dict(os.environ)
    env["IDF_PATH"] = args.idf
    if args.idf_tools:
        env["IDF_TOOLS_PATH"] = args.idf_tools
    env["AKA_LIB_DIR"] = args.aka_lib
    # idf.py refuses to run inside MSYS2 or Git Bash; nothing it needs is in these
    for shell_var in ("MSYSTEM", "MSYSTEM_PREFIX", "MSYSCON", "MINGW_PREFIX"):
        env.pop(shell_var, None)
    if not os.path.isfile(os.path.join(args.idf, "tools", "idf.py")):
        with open(log, "w") as f:
            f.write("ESP-IDF was not found in %s: pass --idf or set IDF_PATH\n" % args.idf)
        return None
    python = idf_python(args.idf_tools)
    export = subprocess.run([python, os.path.join(args.idf, "tools", "idf_tools.py"), "export",
                             "--format", "key-value"], capture_output=True, text=True, env=env)
    if export.returncode != 0:
        with open(log, "w") as f:
            f.write("ESP-IDF could not say where its tools are:\n\n" + (export.stderr or export.stdout))
        return None
    for line in export.stdout.splitlines():
        if "=" in line and line.split("=", 1)[0].isidentifier():
            name, value = line.split("=", 1)
            env[name] = value.replace("%PATH%", env.get("PATH", "")).replace("$PATH", env.get("PATH", ""))
    project = os.path.join(ROOT, DEVICES["Aka"]["idf"])
    command = [python, os.path.join(args.idf, "tools", "idf.py"), "-C", project, "-B", build_dir]
    command += ["-D%s=%s" % kv for kv in sorted(defines.items())]
    command += ["-DAKA_LIB_DIR=" + args.aka_lib.replace(os.sep, "/"), "build"]
    with open(log, "w") as f:
        result = subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env)
    if result.returncode != 0:
        return None
    return os.path.join(build_dir, "bunnymark.bin")


def build_windows(defines, build_dir, args, log):
    """One SDL exe with CMake and ninja: MSYS2's on Windows, or cross compiled with mingw-w64 and a
    downloaded SDL on Linux. Returns the exe, or None"""
    env = dict(os.environ)
    if args.msys2 and os.path.isdir(args.msys2):
        env["PATH"] = args.msys2 + os.pathsep + env.get("PATH", "")
    sdl = str(defines["BUNNYMARK_SDL"])
    configure = ["cmake", "-S", os.path.join(ROOT, DEVICES["Windows"]["cmake"]), "-B", build_dir,
                 "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DBUNNYMARK_SDL=" + sdl]
    if args.cross_windows:
        configure += ["-DCMAKE_TOOLCHAIN_FILE=" + os.path.join(HERE, "mingw-w64.cmake").replace(os.sep, "/"),
                      "-DUSE_VENDORED_SDL=ON"]
    with open(log, "w") as f:
        for command in (configure, ["cmake", "--build", build_dir]):
            f.write(" ".join(command) + "\n")
            f.flush()
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    return os.path.join(build_dir, "bunnymark_sdl%s.exe" % sdl)


def build_web(defines, build_dir, args, log):
    """The SDL port for the browser with Emscripten (SDL from its own ports). Returns the page's path
    without extension (the .html, .js and .wasm beside each other), or None"""
    em = os.path.join(args.emsdk, "upstream", "emscripten")
    if not os.path.isfile(os.path.join(em, "emcmake.py")):
        with open(log, "w") as f:
            f.write("the Emscripten SDK was not found in %s: pass --emsdk or set EMSDK\n" % args.emsdk)
        return None
    env = dict(os.environ)
    env["EMSDK"] = args.emsdk
    node = sorted(glob.glob(os.path.join(args.emsdk, "node", "*", "bin")))
    paths = [em] + node[-1:] + ([args.msys2] if args.msys2 and os.path.isdir(args.msys2) else [])
    env["PATH"] = os.pathsep.join(paths + [env.get("PATH", "")])
    python = sorted(glob.glob(os.path.join(args.emsdk, "python", "*", "python*")))
    python = python[-1] if python else sys.executable
    sdl = str(defines["BUNNYMARK_SDL"])
    configure = [python, os.path.join(em, "emcmake.py"), "cmake", "-S",
                 os.path.join(ROOT, DEVICES["Web"]["cmake"]), "-B", build_dir,
                 "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DBUNNYMARK_SDL=" + sdl]
    with open(log, "w") as f:
        for command in (configure, ["cmake", "--build", build_dir]):
            f.write(" ".join(command) + "\n")
            f.flush()
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env).returncode != 0:
                return None
    return os.path.join(build_dir, "bunnymark_sdl%s" % sdl)


def run_vircon32_steps(commands, build_dir, args, log, optimize):
    """Runs the commands of a Vircon32 build in the build folder, writing them and their output to
    the log. Two steps are not commands: "move" puts the ROM definition written into obj/ beside
    the build, where packrom looks for the paths in it, and "optimize" (only when optimize is set,
    for the _Opt variants) runs v32opt -O3 over obj/BunnyMark.asm before it is assembled. True when
    every step worked"""
    with open(log, "w") as f:
        for command in commands:
            if command == "move":
                os.replace(os.path.join(build_dir, "obj", GAME + ".xml"), os.path.join(build_dir, GAME + ".xml"))
                continue
            if command == "optimize":
                if not optimize:
                    continue
                # the compiler's assembly kept as BunnyMark.raw.asm, the optimized one takes its name
                asm = os.path.join(build_dir, "obj", GAME + ".asm")
                os.replace(asm, os.path.join(build_dir, "obj", GAME + ".raw.asm"))
                command = [args.v32opt, "-O3", "-v", "obj/%s.raw.asm" % GAME, "-o", "obj/%s.asm" % GAME]
            f.write(" ".join(command) + "\n")
            f.flush()
            if subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, cwd=build_dir).returncode != 0:
                return False
    return True


def missing_v32opt(args, log, optimize):
    """True (with the reason in the log) when an _Opt variant is asked for without v32opt"""
    if optimize and not os.path.isfile(args.v32opt):
        with open(log, "w") as f:
            f.write("v32opt was not found at %s: pass --v32opt\n" % args.v32opt)
        return True
    return False


def build_vircon32(build_dir, args, log, optimize=False):
    """The Vircon32 cartridge with its DevTools: the C program compiled and assembled, the texture
    converted, and both packed by the ROM definition. Run in the build folder, where the ROM
    definition's obj/ paths point. Returns the .v32's path without extension, or None"""
    source = os.path.join(ROOT, DEVICES["Vircon32"]["vircon32"])
    exe = ".exe" if os.name == "nt" else ""
    tool = lambda name: os.path.join(args.vircon32, name + exe)
    if not os.path.isfile(tool("compile")):
        with open(log, "w") as f:
            f.write("the Vircon32 DevTools were not found in %s: pass --vircon32\n" % args.vircon32)
        return None
    if missing_v32opt(args, log, optimize):
        return None
    os.makedirs(os.path.join(build_dir, "obj"), exist_ok=True)
    shutil.copyfile(os.path.join(source, "BunnyMark.xml"), os.path.join(build_dir, "BunnyMark.xml"))
    commands = [
        [tool("compile"), os.path.join(source, "BunnyMark.c"), "-o", "obj/BunnyMark.asm"],
        "optimize",
        [tool("assemble"), "obj/BunnyMark.asm", "-o", "obj/BunnyMark.vbin"],
        [tool("png2vircon"), os.path.join(source, "assets", "bunny.png"), "-o", "obj/bunny.vtex"],
        [tool("packrom"), "BunnyMark.xml", "-o", "BunnyMark.v32"],
    ]
    if not run_vircon32_steps(commands, build_dir, args, log, optimize):
        return None
    return os.path.join(build_dir, "BunnyMark")


def lua_to_tic(text):
    """A binary TIC-80 cart (.tic) from a .lua project: its code, tiles and palette as chunks of
    a 4 byte header (type in bits 0-4 and bank in bits 5-7 of the first byte, the size in the next
    two, little endian, then a reserved byte) and the data without its trailing zeros (TIC-80's
    src/cart.c). The project's sections are one hex digit per pixel; in the cart a byte holds two
    pixels, the left one in its low nibble"""
    CODE, TILES, PALETTE = 5, 1, 12
    start = text.find("\n-- <")
    code = (text if start < 0 else text[:start]).rstrip() + "\n"
    sections = {}
    for name, body in re.findall(r"^-- <(\w+)>\n(.*?)^-- </\1>", text, re.M | re.S):
        sections[name] = {int(n): row for n, row in re.findall(r"^-- (\d{3}):([0-9a-fA-F]+)$", body, re.M)}
    tiles = bytearray(256 * 32)
    for index, row in sections.get("TILES", {}).items():
        for i in range(32):
            tiles[index * 32 + i] = int(row[2 * i], 16) | int(row[2 * i + 1], 16) << 4
    palette = bytes.fromhex(sections.get("PALETTE", {}).get(0, ""))
    out = bytearray()
    for kind, data in ((TILES, bytes(tiles)), (PALETTE, palette), (CODE, code.encode("utf-8"))):
        data = data.rstrip(b"\0")
        if data:
            out += struct.pack("<BHB", kind, len(data), 0) + data
    return bytes(out)


def build_cart(device, build_dir, log):
    """The TIC-80 cart made into a .tic, or the PICO-8 cart copied, into the build folder as
    BunnyMark.tic or .p8. Returns its path without extension"""
    source = os.path.join(ROOT, DEVICES[device]["cart"])
    os.makedirs(build_dir, exist_ok=True)
    target = os.path.join(build_dir, GAME)
    with open(log, "w") as f:
        f.write(source + "\n")
    if device == "TIC80":
        with open(source, encoding="utf-8") as f:
            data = lua_to_tic(f.read())
        with open(target + ".tic", "wb") as f:
            f.write(data)
    else:
        shutil.copyfile(source, target + ".p8")
    return target


def build_v32lua(device, build_dir, args, log, optimize=False):
    """A TIC-80 or PICO-8 cart (the .tic made from the TIC-80 project, so the released cart is the
    one converted), or ("Lua") the native Lua version with the C version's texture, compiled by
    v32lua into assembly and a ROM definition, assembled and packed with the Vircon32 DevTools.
    Returns the .v32's path without extension, or None"""
    exe = ".exe" if os.name == "nt" else ""
    tool = lambda name: os.path.join(args.vircon32, name + exe)
    if not os.path.isfile(args.v32lua):
        with open(log, "w") as f:
            f.write("v32lua was not found at %s: pass --v32lua\n" % args.v32lua)
        return None
    if not os.path.isfile(tool("assemble")):
        with open(log, "w") as f:
            f.write("the Vircon32 DevTools were not found in %s: pass --vircon32\n" % args.vircon32)
        return None
    if missing_v32opt(args, log, optimize):
        return None
    os.makedirs(os.path.join(build_dir, "obj"), exist_ok=True)
    textures = []
    if device == "Lua":
        # the source and the texture its --#texture names (assets/bunny.png), converted to the
        # .vtex the ROM definition lists
        cart, ext = os.path.join(build_dir, GAME), ".lua"
        shutil.copyfile(os.path.join(ROOT, DEVICES["Vircon32"]["lua"]), cart + ext)
        os.makedirs(os.path.join(build_dir, "assets"), exist_ok=True)
        shutil.copyfile(os.path.join(ROOT, "vircon32", "assets", "bunny.png"),
                        os.path.join(build_dir, "assets", "bunny.png"))
        textures = [[tool("png2vircon"), "assets/bunny.png", "-o", "assets/bunny.vtex"]]
    else:
        cart = build_cart(device, os.path.join(build_dir, "cart"), log + ".cart")
        ext = ".tic" if device == "TIC80" else ".p8"
    # v32lua writes the ROM definition (and the textures and sounds it lists) beside the assembly,
    # with paths from the build folder (obj/...), where packrom looks from the definition's own
    # folder: it is moved up first, as v32lua's demos do
    commands = [
        [args.v32lua, "--title", "[%s] %s" % (device, GAME), "-o", "obj/%s.asm" % GAME, cart + ext],
        "optimize",
        [tool("assemble"), "obj/%s.asm" % GAME, "-o", "obj/%s.vbin" % GAME],
    ] + textures + [
        "move",
        [tool("packrom"), GAME + ".xml", "-o", GAME + ".v32"],
    ]
    if not run_vircon32_steps(commands, build_dir, args, log, optimize):
        return None
    return os.path.join(build_dir, GAME)


def build_v32cxx(build_dir, args, log, optimize=False):
    """The C++ version: v32c++ turns it into Vircon32 C and writes its ROM definition (the texture
    its #texture names, as a .vtex), then the Vircon32 DevTools compile, assemble and pack it, as
    the C version. Returns the .v32's path without extension, or None"""
    exe = ".exe" if os.name == "nt" else ""
    tool = lambda name: os.path.join(args.vircon32, name + exe)
    if not os.path.isfile(args.v32cxx):
        with open(log, "w") as f:
            f.write("v32c++ was not found at %s: pass --v32cxx\n" % args.v32cxx)
        return None
    if not os.path.isfile(tool("compile")):
        with open(log, "w") as f:
            f.write("the Vircon32 DevTools were not found in %s: pass --vircon32\n" % args.vircon32)
        return None
    if missing_v32opt(args, log, optimize):
        return None
    for folder in ("obj", "assets"):
        os.makedirs(os.path.join(build_dir, folder), exist_ok=True)
    shutil.copyfile(os.path.join(ROOT, DEVICES["Vircon32"]["cpp"]), os.path.join(build_dir, GAME + ".cpp"))
    shutil.copyfile(os.path.join(ROOT, "vircon32", "assets", "bunny.png"),
                    os.path.join(build_dir, "assets", "bunny.png"))
    # v32c++ writes the ROM definition beside the C (obj/), with paths from the build folder,
    # where packrom looks from the definition's own folder: it is moved up first
    commands = [
        [args.v32cxx, "-o", "obj/%s.c" % GAME, GAME + ".cpp"],
        [tool("compile"), "obj/%s.c" % GAME, "-o", "obj/%s.asm" % GAME],
        "optimize",
        [tool("assemble"), "obj/%s.asm" % GAME, "-o", "obj/%s.vbin" % GAME],
        [tool("png2vircon"), "assets/bunny.png", "-o", "assets/bunny.vtex"],
        "move",
        [tool("packrom"), GAME + ".xml", "-o", GAME + ".v32"],
    ]
    if not run_vircon32_steps(commands, build_dir, args, log, optimize):
        return None
    return os.path.join(build_dir, GAME)


# ---------------------------------------------------------------------------------- the releases

def release(device, variant, built):
    """Copies or makes the release files out of what a build left, returns their paths"""
    return [release_one(device, variant, built, out) for out in DEVICES[device]["outputs"]]


def release_one(device, variant, built, out):
    spec = DEVICES[device]
    if out == "bin from hex":
        target = os.path.join(RELEASES, file_name(device, variant, "bin"))
        with open(built + ".hex") as f:
            data = hex_to_bin(f.read())
        with open(target, "wb") as f:
            f.write(data)
    elif out == "uf2 from bin":
        base, family = spec["uf2"]
        target = os.path.join(RELEASES, file_name(device, variant, "uf2"))
        with open(built + ".bin", "rb") as f:
            data = bin_to_uf2(f.read(), base, family)
        with open(target, "wb") as f:
            f.write(data)
    elif out == "zip":
        # the launcher's folder on the card: the app as firmware.bin beside its meta.json and screen.bmp
        target = os.path.join(RELEASES, file_name(device, variant, "zip"))
        card = os.path.join(ROOT, spec["card"])
        folder = os.path.basename(card)
        with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as z:
            z.write(built, folder + "/firmware.bin")
            for name in sorted(os.listdir(card)):
                if name != "firmware.bin":
                    z.write(os.path.join(card, name), folder + "/" + name)
    elif out == "web zip":
        # the page as index.html, so the zip's folder is the site
        target = os.path.join(RELEASES, file_name(device, variant, "zip"))
        name = os.path.basename(built)
        with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as z:
            z.write(built + ".html", "index.html")
            for ext in (".js", ".wasm"):
                z.write(built + ext, name + ext)
    elif out == "exe":
        target = os.path.join(RELEASES, file_name(device, variant, "exe"))
        shutil.copyfile(built, target)
    else:
        target = os.path.join(RELEASES, file_name(device, variant, out))
        shutil.copyfile(built + "." + out, target)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--only", nargs="+", metavar="DEVICE")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--arduino", default=os.environ.get("ARDUINO_DIR", "C:/arduino"))
    parser.add_argument("--arduino2", default=os.environ.get("ARDUINO2_DIR", "C:/arduino2"))
    parser.add_argument("--arduino-cli", default=os.environ.get("ARDUINO_CLI", ""))
    parser.add_argument("--chgame-libs", default=os.environ.get(
        "CHGAME_LIBS", os.path.join(os.environ.get("LOCALAPPDATA", ""),
                                    "Arduino15/packages/CHGame/hardware/ch32v/0.3.0/libraries")))
    parser.add_argument("--idf", default=os.environ.get("IDF_PATH", "C:/github/esp-idf"))
    parser.add_argument("--idf-tools", default=os.environ.get("IDF_TOOLS_PATH", "C:/Espressif"))
    parser.add_argument("--aka-lib", default=os.environ.get("AKA_LIB_DIR", "C:/github/Gamebuino_AKA_lib"))
    parser.add_argument("--msys2", default=os.environ.get("MSYS2_BIN", "C:/msys64/mingw64/bin"))
    parser.add_argument("--cross-windows", action="store_true")
    parser.add_argument("--vircon32", default=os.environ.get("VIRCON32_DEVTOOLS", "C:/utils/vircon32/DevTools"))
    parser.add_argument("--emsdk", default=os.environ.get("EMSDK", "C:/github/emsdk"))
    parser.add_argument("--v32opt", default=os.environ.get(
        "V32OPT", "C:/github/v32opt/v32opt" + (".exe" if os.name == "nt" else "")))
    parser.add_argument("--v32cxx", default=os.environ.get(
        "V32CXX", "C:/github/v32cxx/bin/v32c++" + (".exe" if os.name == "nt" else "")))
    parser.add_argument("--v32lua", default=os.environ.get(
        "V32LUA", "C:/github/v32lua/bin/v32lua" + (".exe" if os.name == "nt" else "")))
    args = parser.parse_args()

    targets = [t for t in TARGETS if not args.only or t[0] in args.only]
    if args.only:
        unknown = set(args.only) - set(DEVICES)
        if unknown:
            sys.exit("unknown device: %s (one of %s)" % (" ".join(sorted(unknown)), " ".join(DEVICES)))
    if args.list:
        for device, variant, defines in targets:
            names = [file_name(device, variant, out.split()[0]) for out in DEVICES[device]["outputs"]]
            print("%-32s %s" % (" ".join(names), define_flags(defines)))
        return

    os.makedirs(RELEASES, exist_ok=True)
    os.makedirs(WORK, exist_ok=True)
    failed = 0
    for device, variant, defines in targets:
        defines = dict(defines)
        build_dir = os.path.join(WORK, device + variant)
        log = build_dir + ".log"
        if "idf" in DEVICES[device]:
            built = build_aka(defines, build_dir, args, log)
        elif DEVICES[device].get("web"):
            built = build_web(defines, build_dir, args, log)
        elif "cmake" in DEVICES[device]:
            built = build_windows(defines, build_dir, args, log)
        elif "vircon32" in DEVICES[device]:
            # an _Opt variant is its plain one with the assembly optimized
            optimize = variant.endswith("_Opt")
            base = variant[:-len("_Opt")] if optimize else variant
            if base == "_CPP":
                built = build_v32cxx(build_dir, args, log, optimize)
            elif base in DEVICES[device]["carts"]:
                built = build_v32lua(DEVICES[device]["carts"][base], build_dir, args, log, optimize)
            else:
                built = build_vircon32(build_dir, args, log, optimize)
        elif "cart" in DEVICES[device]:
            built = build_cart(device, build_dir, log)
        else:
            built = build_arduino(device, variant, defines, build_dir, args, log)
        if built is None:
            failed += 1
            lines = open(log, errors="replace").read().splitlines() if os.path.exists(log) else []
            errors = [l.strip() for l in lines if "error" in l.lower()][:3] or lines[-3:]
            print("FAILED  %s%s, see %s" % (device, variant, log))
            for line in errors:
                print("        " + line[:160])
            continue
        for target in release(device, variant, built):
            print("ok      %-36s %8d bytes" % (os.path.basename(target), os.path.getsize(target)), flush=True)
    print("\n%d of %d built, in %s" % (len(targets) - failed, len(targets), RELEASES))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
