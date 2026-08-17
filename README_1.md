# BGE Double H Fix

A tiny native Windows utility for repairing a confirmed **Double H disappearing /
Lost Partner bug** in **Beyond Good & Evil - 20th Anniversary Edition (PC)**.

No Python. No .NET. No installer.

## What it fixes

In the affected save used to develop this tool, Double H had disappeared
completely, including his HUD entry.

The save contained:

```text
Perso_ID_status = [4, 4, 3, 2, 2, 2, 0, 0, 0, 0, 0, 0]
                       ^
                       Double H
```

Changing only:

```text
Perso_ID_status[1]: 4 -> 0
```

made Double H immediately reappear and follow Jade again.

The original game's open-source `bgesav` project documents `0` as the
**Follow** state:

https://github.com/crumblingstatue/bgesav

## Safety

The tool does **not** patch a hard-coded absolute offset.

It:

1. searches for `Perso_ID_status`;
2. requires exactly one occurrence;
3. validates the expected serialized 12-value structure;
4. only enables automatic repair for the confirmed `4 -> 0` case;
5. creates a `.doubleh-backup` before writing;
6. changes exactly one byte;
7. reads the save back and verifies exactly one byte changed;
8. restores the original from the backup if verification fails.

Unknown save structures are rejected instead of modified.

## Usage

1. Close Beyond Good & Evil.
2. Start `BGE_DoubleH_Fix.exe`.
3. Select a `slot*.sav`, drag it into the window, or drag it directly onto the EXE.
4. If the confirmed Lost Partner state is detected, click **Repair Double H**.
5. Load the repaired save.

Keep a separate copy of the whole save folder as an additional precaution.

## Download

For normal users, download the latest Windows EXE from the **Releases** section.

Every tagged release also contains:

```text
BGE_DoubleH_Fix.exe.sha256
```

for integrity verification.

## GitHub Actions

Two workflows are included.

### Normal builds

`.github/workflows/build.yml`

Runs on pushes, pull requests, and manual dispatches. GitHub compiles the native
x64 EXE with MSVC and uploads it as a workflow artifact.

### Releases

`.github/workflows/release.yml`

When you push a tag such as:

```bash
git tag v1.0.0
git push origin v1.0.0
```

GitHub automatically:

- builds `BGE_DoubleH_Fix.exe`;
- generates its SHA-256 checksum;
- creates a GitHub Release;
- attaches both files to the release.

## Local builds

### MSVC

Open **x64 Native Tools Command Prompt for VS 2022** and run:

```text
build_msvc.bat
```

### MinGW-w64 / WinLibs

Extract WinLibs to `C:\winlibs` or put `g++.exe` on `PATH`, then run:

```text
build_mingw.bat
```

## License

MIT
