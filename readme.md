project structure: tree -I 'build|.git|bin|obj|.vscode'

Run at local: 
    Step 1: curl -sL https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -o src/httplib.h
    Step 2: brew install entr
    Step 3: find src/ | entr -r sh -c "clang++ -std=c++17 -Isrc src/main_desktop.cpp src/effects/aurora/AuroraEffect.cpp -o mac_emulator && ./mac_emulator"


Blink Dots,Indicator Color,Subsystem Component,Severity Level,Description & Corrective Action
1 Dot,Yellow (Amber),Touch Pad 1,Warning,Touch Pad 1 capacitive baseline failure or impedance drift.
2 Dots,Yellow (Amber),Touch Pad 2,Warning,Touch Pad 2 capacitive baseline failure or impedance drift.
3 Dots,Yellow (Amber),Touch Pad 3,Warning,Touch Pad 3 capacitive baseline failure or impedance drift.
4 Dots,Yellow (Amber),Touch Pad 4,Warning,Touch Pad 4 capacitive baseline failure or impedance drift.
5 Dots,Yellow (Amber),Touch Pad 5,Warning,Touch Pad 5 capacitive baseline failure or impedance drift.
6 Dots,Red,Exhaust Fan / Thermal,Critical Alert,RP2040 core temp >65°C or exhaust fan failure. Interlock triggered.
7 Dots,Yellow (Amber),Ambient Sensor (BH1750),Warning,I2C communication lost or sensor unplugged. Check wiring.
8 Dots,Yellow (Amber),LED Matrix Panel,Warning (>10%),10%–20% panel pixels failing or data line degradation.
9 Dots,Red,LED Matrix Panel,Critical (>20%),>20% panel failure. Major matrix hardware fault.



1. Overview of Diagnostic Modes
Auto-Diagnosis Mode
When it runs: Automatically at midnight (2:00 AM) on the 20th of every month.

What it does: It performs a complete system-wide health scan and clears temporary runtime caches to keep your device running at peak performance while safely preserving your clock time (RTC).

The 21st Reminder Protocol: If a critical red error is found during the monthly auto-check, the system will gently pulse the full LED panel red for one minute at the top of every hour on the 21st. This reminds you to run a manual check and contact support.

Manual Diagnosis Mode
When it runs: Whenever you choose to initiate a check (such as responding to a reminder).

What it does: It immediately scans all connected hardware, clears active warning reminders once addressed, and displays your specific system status or error code through intuitive LED patterns.

2. Key Features & Benefits
Zero-Display Simplicity: No complicated screen menus are needed; all health updates and error statuses are communicated directly through your 32x32 or 64x64 LED matrix panel.

Smart Performance Care: Automatically wipes unnecessary cache files to ensure uninterrupted 24/7/365 reliability without ever resetting your time settings.

Proactive Protection: Isolates issues early—separating minor warnings from critical safety alerts—so hardware can be maintained before any failure impacts your experience.

3. User-Friendly Error Code Guide (Blink Codes)
If your system detects an issue during a health check, it will display a specific number of blinking indicator dots around the edges of the LED panel.

Blink Dots,Indicator Color,Subsystem Component,What It Means for You
Blink Dots,Indicator Color,Subsystem Component,Severity Level,Description & Corrective Action
1 Dot,Yellow (Amber),Touch Pad 1,Warning,Touch Pad 1 capacitive baseline failure or impedance drift.
2 Dots,Yellow (Amber),Touch Pad 2,Warning,Touch Pad 2 capacitive baseline failure or impedance drift.
3 Dots,Yellow (Amber),Touch Pad 3,Warning,Touch Pad 3 capacitive baseline failure or impedance drift.
4 Dots,Yellow (Amber),Touch Pad 4,Warning,Touch Pad 4 capacitive baseline failure or impedance drift.
5 Dots,Yellow (Amber),Touch Pad 5,Warning,Touch Pad 5 capacitive baseline failure or impedance drift.
6 Dots,Red,Exhaust Fan / Thermal,Critical Alert,RP2040 core temp >65°C or exhaust fan failure. Interlock triggered.
7 Dots,Yellow (Amber),Ambient Sensor (BH1750),Warning,I2C communication lost or sensor unplugged. Check wiring.
8 Dots,Yellow (Amber),LED Matrix Panel,Warning (>10%),10%–20% panel pixels failing or data line degradation.
9 Dots,Red,LED Matrix Panel,Critical (>20%),>20% panel failure. Major matrix hardware fault.