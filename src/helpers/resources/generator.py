import os
import hashlib
import typing

os.chdir(os.path.dirname(os.path.realpath(__file__)))

def get_all_files(directory: str):
    for root, _, files in os.walk(directory):
        for file in files:
            yield os.path.join(root, file).replace("\\", "/")[len(directory):]

def generate(name: str, struct_name: str, ignorer: typing.Callable[[str], bool] = lambda _: False):
    cpp = open(f"./{name}.cpp", "w")
    files = {}
    
    for file in get_all_files(f"./{name}"):
        assert file.startswith("/")
        if ignorer(file):
            continue
        
        data = open(f"./{name}{file}", "rb").read()
        varname = f"f{hashlib.md5((file + name).encode()).hexdigest()}"
        arr = ",".join(map(str, data))
        cpp.write(f"static const unsigned char {varname}[] = {{{arr}}};\n")
        files[file] = (varname, len(data))
    
    cpp.write(f"""\
struct {struct_name} {{
    static Data get(const std::string& key) {{
        {"\n".join(map(lambda key, info: f"""\
            if (key == "{key}") return Data {{ .data = std::vector<ep_u8>({info[0]}, {info[0]} + {info[1]}) }};
        """, files.keys(), files.values()))}
        return {{}};
    }}
}};
""")

if __name__ == "__main__":
    generate("phigros", "PhiStaticResource")
    generate("milthm", "MilStaticResource", lambda x: (
        x.endswith((".ptt", ".pptx", ".py", ".html", ".js"))
        or x.startswith("/skin_analysis")
    ))
    generate("rizline", "RizStaticResource")
    
    print("done")
