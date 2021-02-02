g++ -save-temps=obj -o simulate.exe -std=c++11 simulate.cpp

:: use it on cmd to ensure colorful output
:: reference from https://www.codeproject.com/Tips/5255355/How-to-Put-Color-on-Windows-Console
reg add HKEY_CURRENT_USER\Console /v VirtualTerminalLevel /t REG_DWORD /d 0x00000001 /f