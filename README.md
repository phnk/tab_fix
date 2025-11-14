# tab_fix
A win32 window switcher that uses two-letter combinations instead of endless Alt+Tab cycling.

## The Problem
Cycling through dozens of windows with Alt+Tab is slow and tedious. You shouldn't have to press Tab 47 times to get to the window you want. Furthermore, for some reason the window position tend to change for no reason.

## The Solution
Press `Ctrl+Space`, see all your windows, then type a two-letter combo to instantly switch to any window.

## Credit
This implementation is straight ripped from from [this YouTube video](https://www.youtube.com/watch?v=pAbf3jtoovA). Last I checked Adam Basis did not release the code. If they do use their implementation as it has way more features and is more efficient.

## How It Works
1. Press `Ctrl+Space` to activate the window switcher
2. A list of all open windows appears with two-letter codes
3. Type the two letters to instantly switch to that window

The letters are assigned based on:
- **First letter**: The first character of the program name (e.g., `c` for Chrome)
- **Second letter**: An enumerated letter for disambiguation (e.g., `a`, `b`, `c`, etc.)

### Example
If you have multiple Chrome windows open:
- `ca` → First Chrome window
- `cb` → Second Chrome window
- `cc` → Third Chrome window

If you have a VS Code window:
- `va` → First VS Code window

## Requirements
- Windows OS
- C++ compiler (MSVC, MinGW, etc.)

## Installation
### Clone and Build

```bash
git clone git@github.com:phnk/tab_fix.git
cd tab_fix
# Compile with your preferred C++ compiler
# Example with g++:
g++ -o tab_fix.exe main.cpp -lgdi32 -luser32
```

### Run
Double click your `.exe`

### Autostart (Optional)
To make tab_fix start automatically with Windows:

1. Press `Win+R` and type `shell:startup`
2. Create a shortcut to `tab_fix.exe` in the Startup folder

Or create a shortcut and place it in:
```
C:\Users\<YourUsername>\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup
```

## Usage
1. Press `Ctrl+Space` to activate
2. View the list of windows with their assigned letter combinations
3. Type the two-letter code for the window you want
4. Instantly switch to that window

## Configuration
Currently the hotkey is hardcoded to `Ctrl+Space`. Modify the source code to change the activation key.

## Contributing
Pull requests are welcome! Feel free to improve the code, add features, or fix bugs.

## Author
[phnk](https://github.com/phnk)
## Acknowledgments

Original idea from: https://www.youtube.com/watch?v=pAbf3jtoovA
