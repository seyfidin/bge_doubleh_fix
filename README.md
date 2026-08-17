# BGE Double H Fix — Native C++ / Win32

A tiny native Windows GUI for repairing the confirmed **Double H disappearing /
Lost Partner bug** in **Beyond Good & Evil - 20th Anniversary Edition (PC)**.

This version uses the Win32 API directly. There is:

- no Python dependency
- no .NET dependency
- no Qt/GTK dependency
- no external runtime bundled with the EXE

When compiled with MSVC `/MT`, the C/C++ runtime is statically linked as well.

## Confirmed repair

The affected save used to develop this tool contained:

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

The open-source `bgesav` project for the original game documents `0` as the
**Follow** state:

https://github.com/crumblingstatue/bgesav

## Safety design

The tool does not patch a hard-coded absolute offset.

It:

1. searches for `Perso_ID_status`;
2. requires exactly one match;
3. checks for the expected `0x8C` marker and 12 one-byte status values;
4. only enables the repair for the confirmed Double H state `4`;
5. creates a `.doubleh-backup` copy before writing;
6. changes only `Perso_ID_status[1]` from `4` to `0`;
7. reads the file back;
8. verifies that exactly one byte changed;
9. restores the backup if verification fails.

Any unfamiliar save structure is rejected instead of being modified.

## Build with Visual Studio / Build Tools

Install the Visual Studio 2022 Build Tools workload:

```text
Desktop development with C++
```

Then open:

```text
x64 Native Tools Command Prompt for VS 2022
```

and run:

```text
build_msvc.bat
```

The result is:

```text
build\BGE_DoubleH_Fix.exe
```

The build uses:

```text
/O2 /MT /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF
```

so it is a native, optimized, statically CRT-linked Windows executable.

## Usage

1. Close Beyond Good & Evil.
2. Run `BGE_DoubleH_Fix.exe`.
3. Choose a `slot*.sav`, drag it onto the window, or drag it directly onto the EXE.
4. If the confirmed Lost Partner state is detected, click **Repair Double H**.
5. The original save is backed up automatically.
6. Load the repaired save in the game.

Keep a separate backup of the complete save folder as an additional precaution.
