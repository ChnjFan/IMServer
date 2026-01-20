@echo off

:: 生成gRPC代码的批处理脚本

:: 设置变量
set PROTO_FILE=gateway_routing.proto
set GEN_DIR=generated
set PROTOC=protoc
set GRPC_PLUGIN=grpc_cpp_plugin

:: 检查protoc是否存在
where %PROTOC% > nul 2> nul
if %ERRORLEVEL% neq 0 (
    :: 尝试常见的安装路径
    if exist "D:\Program Files\protoc\bin\protoc.exe" (
        set PROTOC="D:\Program Files\protoc\bin\protoc.exe"
        echo Found protoc at D:\Program Files\protoc\bin\protoc.exe
    ) else if exist "C:\Program Files\protoc\bin\protoc.exe" (
        set PROTOC="C:\Program Files\protoc\bin\protoc.exe"
        echo Found protoc at C:\Program Files\protoc\bin\protoc.exe
    ) else (
        echo Error: protoc not found in PATH
        echo Please install Protocol Buffers and add to PATH
        echo Or install to D:\Program Files\protoc\bin\protoc.exe
        pause
        exit /b 1
    )
)

:: 检查grpc_cpp_plugin是否存在
where %GRPC_PLUGIN% > nul 2> nul
if %ERRORLEVEL% neq 0 (
    :: 尝试常见的安装路径
    if exist "D:\Program Files\grpc\bin\grpc_cpp_plugin.exe" (
        set GRPC_PLUGIN="D:\Program Files\grpc\bin\grpc_cpp_plugin.exe"
        echo Found grpc_cpp_plugin at D:\Program Files\grpc\bin\grpc_cpp_plugin.exe
    ) else if exist "C:\Program Files\grpc\bin\grpc_cpp_plugin.exe" (
        set GRPC_PLUGIN="C:\Program Files\grpc\bin\grpc_cpp_plugin.exe"
        echo Found grpc_cpp_plugin at C:\Program Files\grpc\bin\grpc_cpp_plugin.exe
    ) else (
        echo Error: grpc_cpp_plugin not found in PATH
        echo Please install gRPC and add to PATH
        pause
        exit /b 1
    )
)

:: 创建生成目录
if not exist "%GEN_DIR%" (
    mkdir "%GEN_DIR%"
    echo Created directory: %GEN_DIR%
)

:: 生成protobuf代码
echo Generating protobuf code...
%PROTOC% -I=. --cpp_out=%GEN_DIR% %PROTO_FILE%
if %ERRORLEVEL% neq 0 (
    echo Error: Failed to generate protobuf code
    pause
    exit /b 1
)

:: 生成gRPC代码
echo Generating gRPC code...
%PROTOC% -I=. --grpc_out=%GEN_DIR% --plugin=protoc-gen-grpc=%GRPC_PLUGIN% %PROTO_FILE%
if %ERRORLEVEL% neq 0 (
    echo Error: Failed to generate gRPC code
    pause
    exit /b 1
)

echo Success: Generated gRPC code in %GEN_DIR%
dir %GEN_DIR%
pause
