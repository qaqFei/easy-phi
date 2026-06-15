import subprocess
import os
import sys
import time
import struct

def run(cmds: list[str]):
    cmds = list(filter(bool, cmds))
    print(cmds)
    subprocess.run(cmds, check=True)

def change_pe_to_gui_subsystem(path: str):
    with open(path, "r+b") as f:
        f.seek(0x3C)
        pe_offset = struct.unpack("<I", f.read(4))[0]
        
        f.seek(pe_offset)
        if f.read(4) != b"PE\x00\x00":
            return
        
        subsystem_offset = pe_offset + 4 + 20 + 0x44
        
        f.seek(subsystem_offset)
        f.write(struct.pack("<H", 2))
    
os.makedirs("build", exist_ok=True)

with open("./dev.flag", "w"):
    ...

debug = "--debug" in sys.argv
source = "test_phi.cpp" if "--source" not in sys.argv else sys.argv[sys.argv.index("--source") + 1]
libraries = {
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
        "-lx264", "-lfdk-aac", "-lmfx"
    ]
}

short_commit_hash = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode("utf-8").strip()
repo_github = "https://github.com/qaqFei/easy-phi"
exec_ext = ".exe" if os.name == "nt" else ""

build_cmds = [
    "g++", "-std=c++20",
    "-static",
    
    "-O3" if not debug else "-O0",
    "" if not debug else "-ggdb",
    
    "-Wsign-compare",
    "-Wa,-mbig-obj",
    
    f"-DBUILD_SHORT_COMMIT_HASH=\"{short_commit_hash}\"",
    f"-DBUILD_TIME={time.time()}",
    f"-DBUILD_IS_DEBUG={1 if debug else 0}",
    f"-DBUILD_REPO_GITHUB=\"{repo_github}\"",
    "-DEASY_PHI_IS_RELEASE" if not debug else "",
    
    f"./test_files/{source}",
    "-I./src",
    
    *sum(libraries.values(), []),
    
    "-lgdi32", "-lopengl32", "-lole32",
    "-lshell32", "-luuid", "-lbcrypt",
    "-lws2_32", "-lcrypt32",
    "-lcomctl32",
    
    "-o", f"./build/test{exec_ext}"
]

run(build_cmds)

if "--no-console" in sys.argv:
    if os.name == "nt":
        change_pe_to_gui_subsystem(f"./build/test{exec_ext}")
    else:
        print("WARNING: --no-console is not supported on non-Windows systems")

if "--run" in sys.argv:
    run_cmds = [
        "./build/test"
    ]
    
    if "--with-args" in sys.argv:
        run_cmds += sys.argv[sys.argv.index("--with-args") + 1:]
        
    run(run_cmds)
