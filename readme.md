project structure: tree -I 'build|.git|bin|obj|.vscode'

Run at local: 
    Step 1: curl -sL https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -o src/httplib.h
    Step 2: brew install entr
    Step 3: find src/ | entr -r sh -c "clang++ -std=c++17 -Isrc src/main_desktop.cpp src/effects/aurora/AuroraEffect.cpp -o mac_emulator && ./mac_emulator"