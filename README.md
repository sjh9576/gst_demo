
1. build

1) 환경 구성
cmake --preset debug

2) 프로젝트 빌드 
cmake --build --preset debug

3) 테스트 실행
ctest --preset test-debug

2. 개발 환경 세팅
Ctrl + Shift + P
"Preferences: Open Workspace Settings (JSON)"
settings.json

재빌드 후 build/debug/compile_commands.json 확인

Ctrl + Shift + P
C/C++: Edit Configurations (JSON)
c_cpp_properties.json
"compileCommands": "${workspaceFolder}/build/debug/compile_commands.json" 추가