import os
import hashlib

def get_all_files(dir: str):
    for root, _, files in os.walk(dir):
        for file in files:
            yield os.path.join(root, file).replace("\\", "/")[len(dir):]

cpp = open("./resources/inlined_resources.cpp", "w")
files = {}

for file in get_all_files("./resources"):
    if file == "/inlined_resources.cpp": continue
    data = open("./resources" + file, "rb").read()
    varname = "f" + hashlib.md5(file.encode()).hexdigest()
    cpp.write(f"static const unsigned char {varname}[] = {{{",".join([str(byte) for byte in data])}}};\n")
    files[file] = (varname, len(data))

cpp.write(f"""\
struct StaticResource {{
    static easy_phi::Data get(const std::string& key) {{
        {"\n".join(map(lambda key, info: f"""\
            if (key == "{key}") return easy_phi::Data {{ .data = std::vector<easy_phi::ep_u8>({info[0]}, {info[0]} + {info[1]}) }};
        """, files.keys(), files.values()))}
        return {{}};
    }}
}};
""")

cpp.close()
