# BGE Double H Fix

A lightweight Windows utility to fix the confirmed **Double H Lost Partner bug** in **Beyond Good & Evil - 20th Anniversary Edition (PC)**.

**[Download the latest release](https://github.com/seyfidin/bge_doubleh_fix/releases/latest)**

## Features

- Detects the confirmed save state associated with Double H disappearing.
- Repairs Double H's status so he can follow Jade again.
- Creates a backup of the original save before making changes.
- Verifies the result and checks that only the intended byte changed.
- Native Windows executable with no separate runtime installation required.

## Usage

1. Close the game.
2. Download and run `BGE_DoubleH_Fix.exe`.
3. Select a `slot*.sav` file, drag it onto the app window, or drag it onto the EXE.
4. If the confirmed bug state is detected, click **Repair Double H** and confirm.
5. Load the repaired save in the game.

Keep a separate backup of your complete save folder as an additional precaution.

## Safety and limitations

The tool is designed for one confirmed repair case. It checks the save structure before enabling the repair and refuses to modify unrecognized formats.

The repair changes only:

```text
Perso_ID_status[1]: 4 -> 0
```

The value `0` is documented as the **Follow** state by the open-source [bgesav project](https://github.com/crumblingstatue/bgesav).

Before writing, the tool creates a `.doubleh-backup` copy. It then reads the save back and verifies that exactly the intended byte changed. If verification fails, it attempts to restore the backup.

This is not a general-purpose save editor. Keep your original save and backup files until you have confirmed the repaired save works in the game.

## Build from source

Requirements: Visual Studio 2022 Build Tools with the **Desktop development with C++** workload.

1. Open **x64 Native Tools Command Prompt for VS 2022**.
2. Run:

   ```bat
   build_msvc.bat
   ```

The executable is created at `build\\BGE_DoubleH_Fix.exe`.

## AI-assisted development

AI tools were used to assist with generating and refining parts of the source code. The project is maintained and tested by the author.

## Support, credits and license

- **Bug reports:** [Open an issue](https://github.com/seyfidin/bge_doubleh_fix/issues/new/choose). Please include the tool's status and output, but do not publicly upload save files containing information you do not want to share.
- **Reference:** The [bgesav project](https://github.com/crumblingstatue/bgesav) documents the original game's save-state values.
- **License:** [MIT](LICENSE).
