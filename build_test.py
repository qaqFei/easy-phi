import subprocess
import os
import sys

def run(cmds: list[str]):
    cmds = list(filter(bool, cmds))
    print(cmds)
    subprocess.run(cmds, check=True)

os.makedirs("build", exist_ok=True)

debug = "--debug" in sys.argv
source = "test.cpp" if "--source" not in sys.argv else sys.argv[sys.argv.index("--source") + 1]
libraries = {
    "miniaudio": [
        "-I./test_files/externals/miniaudio/include",
        "./test_files/externals/miniaudio/lib/miniaudio.o",
    ],
    
    "skia": [
        "-I./test_files/externals/skia/include",
        "-L./test_files/externals/skia/lib",
        "-lskia",
    ],
    
    "glfw3": [
        "-I./test_files/externals/glfw3/include",
        "-L./test_files/externals/glfw3/lib",
        "-lglfw3",
    ],
    
    "glad": [
        "-I./test_files/externals/glad/include",
        "-L./test_files/externals/glad/lib",
        "-lglad",
    ],
    
    "ffmpeg": [
        "-I./test_files/externals/ffmpeg/include",
        "-L./test_files/externals/ffmpeg/lib",
        "-lavformat", "-lavcodec", "-lavutil",
        "-lx264", "-lfdk-aac", "-lmfx",
    ],
    
    "stb": [
        "-I./test_files/externals/stb/include",
    ]
}

build_cmds = [
    "g++", "-std=c++20",
    "-static",
    
    "-O3" if not debug else "-O0",
    "" if not debug else "-ggdb",
    
    f"./test_files/{source}",
    "-I./src",
    
    *sum(libraries.values(), []),
    
    "-lgdi32", "-lpthread", "-lopengl32", "-lole32",
    "-lshell32", "-luuid", "-lbcrypt",
    
    "-o", "./build/test"
]

run(build_cmds)

if "--run" in sys.argv:
    run_cmds = [
        "./build/test"
    ]
    
    if "--with-args" in sys.argv:
        run_cmds += sys.argv[sys.argv.index("--with-args") + 1:]
        
    run(run_cmds)
