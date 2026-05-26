from __future__ import annotations

import os
import typing
import dataclasses
import shutil

import tree_sitter, tree_sitter_cpp

def ensure_dirs(path: str):
    os.makedirs(path, exist_ok=True)

@dataclasses.dataclass
class BaseNode:
    docs: typing.Optional[str] = None
    space: str = ""
    name: str = ""
    
    def get_space_list(self):
        return list(filter(bool, self.space.split("::") + [self.name]))
    
@dataclasses.dataclass
class NamespaceDefine(BaseNode):
    nodes: list[BaseNode] = dataclasses.field(default_factory=list)
    
@dataclasses.dataclass
class FunctionDefine(BaseNode):
    code: str = ""

@dataclasses.dataclass
class VariableDefine(BaseNode):
    code: str = ""
    
@dataclasses.dataclass
class UsingDirective(BaseNode):
    code: str = ""

@dataclasses.dataclass
class StructDefine(BaseNode):
    code: str = ""
    nodes: list[BaseNode] = dataclasses.field(default_factory=list)

@dataclasses.dataclass
class EnumDefine(BaseNode):
    extends: typing.Optional[str] = None
    enums: list[tuple[str, typing.Optional[str]]] = dataclasses.field(default_factory=list)
    
    def get_declaration(self):
        res = f"enum class {self.name}"
        if self.extends is not None: res += f": {self.extends}"
        res += " {"
        
        for name, value in self.enums:
            enum = f"\n    {name}"
            if value is not None: enum += f" = {value}"
            enum += ","
            res += enum
        
        res += "\n};"
        return res

def _join_prefixes(*prefixes: str) -> str:
    return "::".join(filter(bool, prefixes))

def _clean_indent(text: str) -> str:
    result = "\n".join(map(str.strip, text.strip().splitlines()))
    while len(result) > 0 and result[0] == "\n": result = result[1:]
    return result
        
def _norm_code(code: str) -> str:
    if code[-1] == ";": code = code[:-1]
    code = _clean_indent(code).strip()
    if code[-1] != ";": code += ";"
    return "\n".join(map(lambda x: f"    {x}" if not x.endswith(";") else x, code.split("\n"))).strip()

class SourceAnalyzer:
    nodes: list[BaseNode]
    
    def __init__(self, source: bytes):
        self.nodes = []
        tree: tree_sitter.Tree = tree_sitter.Parser(tree_sitter.Language(tree_sitter_cpp.language())).parse(source)
        
        
        def get_block_docs(node: typing.Optional[tree_sitter.Node]):
            if node is None: return None
            
            if node.type not in ("declaration_list", "compound_statement", "field_declaration_list"):
                raise Exception("unhandled docs node type:", node.type)
            
            for child in node.children:
                if child.type == "comment":
                    text = child.text.decode("utf-8")
                    if not text.startswith("/*"): break
                    if not text.endswith("*/"): break
                    text = text[2:-2].strip()
                    
                    if text.startswith("!docs"):
                        return _clean_indent(text[5:])
                    else: break
                    
            return None

        def get_inline_docs(nodes: list[tree_sitter.Node], index: int):
            if len(nodes) <= index + 1: return None
            node = nodes[index + 1]
            
            if node.type == "comment":
                text = node.text.decode("utf-8")
                if not text.startswith("//"): return None
                text = text[2:].strip()
                
                if text.startswith("!inline-docs|"):
                    return _clean_indent(text[13:])
            
            return None

        def unwrap_identifier(node: typing.Optional[tree_sitter.Node]) -> typing.Optional[str]:
            if node is None: return None
            
            while node.type != "identifier":
                if node.type == "array_declarator": node = node.children[0]
                elif node.type == "pointer_declarator": node = node.children[1]
                elif node.type == "function_declarator": node = node.children[0]
                elif node.type == "qualified_identifier": break
                elif node.type == "operator_name": break
                elif node.type == "type_identifier": break
                elif node.type == "field_identifier": break
                elif node.type == "reference_declarator": node = node.children[1]
                elif node.type == "destructor_name": break
                elif node.type == "function_declarator": node = node.children[0]
                elif node.type == "parenthesized_declarator": node = node.children[1]
                elif node.type == "primitive_type": break
                elif node.type == "operator_cast": break
                elif node.type == "friend_declaration": break
                else:
                    raise Exception("unhandled declarator type:", node.type)
                
            return node.text.decode("utf-8")
        
        def walk(nodes: list[tree_sitter.Node], index: int, dst: list[BaseNode]):
            node = nodes[index]
            
            if node.type == "namespace_definition":
                define = NamespaceDefine()
                define.docs = get_block_docs(node.child_by_field_name("body"))
                define.name = node.child_by_field_name("name").text.decode("utf-8")
                dst.append(define)
                walk_nodes(node.children, define.nodes)
            elif node.type == "alias_declaration":
                directive = UsingDirective()
                directive.docs = get_inline_docs(nodes, index)
                directive.name = node.child_by_field_name("name").text.decode("utf-8")
                directive.code = node.text.decode("utf-8")
                directive.code = _norm_code(directive.code)
                dst.append(directive)
            elif node.type == "declaration":
                first_init_declarator_index = None
                
                for i, child in enumerate(node.children):
                    if child.type == "init_declarator":
                        if first_init_declarator_index is None: first_init_declarator_index = i
                        
                        define = VariableDefine()
                        define.docs = get_inline_docs(nodes, index)
                        define.code = " ".join(map(lambda x: x.text.decode("utf-8"), node.children[:first_init_declarator_index])) + " " + child.text.decode("utf-8")
                        define.code = _norm_code(define.code)
                        define.name = unwrap_identifier(child.children[0])
                        
                        dst.append(define)
            elif node.type == "function_definition":
                if node.children[-1].type == "delete_method_clause":
                    return
                
                define = FunctionDefine()
                define.name = unwrap_identifier(node.child_by_field_name("declarator"))
                define.code = " ".join(map(lambda x: x.text.decode("utf-8"), filter(lambda x: x.type != "compound_statement", node.children)))
                define.code = _norm_code(define.code)
                
                for child in node.children:
                    if child.type == "compound_statement":
                        define.docs = get_block_docs(child)
                        break
                    
                dst.append(define)
            elif node.type == "struct_specifier":
                body = node.child_by_field_name("body")
                define = StructDefine()
                define.code = " ".join(map(lambda x: x.text.decode("utf-8"), node.children[:-1]))
                define.code = _norm_code(define.code)
                define.docs = get_block_docs(body)
                define.name = unwrap_identifier(node.child_by_field_name("name"))
                if body is not None: walk_nodes(body.children, define.nodes)
                dst.append(define)
            elif node.type == "template_declaration":
                body = node.children[2]
                template = " ".join(map(lambda x: x.text.decode("utf-8"), node.children[:2]))
                
                if body.type not in ("function_definition", "struct_specifier", "alias_declaration"):
                    raise Exception("unhandled template declaration type:", body.type)
                
                raw = dst[-1] if dst else None
                walk([body], 0, dst)
                if dst and dst[-1] is not raw:
                    define = dst[-1]
                    define.code = template + "\n" + define.code
            elif node.type == "field_declaration":
                for i, child in enumerate(node.children):
                    if child.type in (*",=", "initializer_list") or (child.type == ";" and i > 1):
                        define_start_index = i - 1
                        break
                else:
                    if node.children[0].type in ("struct_specifier", "enum_specifier"):
                        return walk(node.children, 0, dst)
                    
                    raise Exception("no field declaration found")
                
                i = define_start_index
                while i < len(node.children) - 1:
                    child = node.children[i]
                    define = VariableDefine()
                    define.docs = get_inline_docs(nodes, index)
                    define.name = unwrap_identifier(child)
                    define.code = " ".join(map(lambda x: x.text.decode("utf-8"), node.children[:define_start_index])) + " " + " ".join(map(lambda x: x.text.decode("utf-8"), node.children[i:i + 3]))
                    define.code = _norm_code(define.code)
                    dst.append(define)
                    
                    if node.children[i + 1].type == "initializer_list": i += 1
                    
                    if node.children[i + 1].type == "=":
                        i += 3
                        if node.children[i].type == ",": i += 1
                    elif node.children[i + 1].type == ",": i += 2
                    elif node.children[i + 1].type == ";": break
                    else: raise Exception("unhandled field declaration type:", node.children[i + 1].type)
            elif node.type == "enum_specifier":
                define = EnumDefine()
                base = node.child_by_field_name("base")
                if base is not None: define.extends = unwrap_identifier(base)
                define.name = unwrap_identifier(node.child_by_field_name("name"))
                body = node.child_by_field_name("body")
                
                for child in body.children:
                    if child.type == "enumerator":
                        define.enums.append((
                            unwrap_identifier(child.child_by_field_name("name")),
                            value.text.decode("utf-8") if (value := child.child_by_field_name("value")) is not None else None
                        ))
                
                dst.append(define)
            elif node.type in (
                "translation_unit",
                "preproc_ifdef",
                "declaration_list"
            ):
                walk_nodes(node.children, dst)
            elif node.type in (
                "comment", "identifier", "preproc_def", "preproc_include",
                "namespace", "namespace_identifier", "preproc_else",
                "static_assert_declaration", "access_specifier",
                "preproc_function_def", "preproc_call", "expression_statement",
                *";{}"
            ) or node.type.startswith("#") or node.text.decode("utf-8") in ":": ...
            else:
                print("unhandled node type:", node.type)
        
        def walk_nodes(nodes: list[tree_sitter.Node], dst: list[BaseNode]):
            for i in range(len(nodes)):
                walk(nodes, i, dst)
        
        walk_nodes([tree.root_node], self.nodes)
    
    def resolve(self):
        for node in self.nodes:
            self._resolve_space("", node)
        
        self._merge_namespace(self.nodes)
    
    def _resolve_space(self, prefix: str, node: BaseNode):
        if "::" in node.name:
            raw = node.name
            splited = node.name.split("::")
            prefix += "::".join(splited[:-1])
            node.name = splited[-1]
            
            if isinstance(node, (FunctionDefine, StructDefine)):
                node.code = node.code.replace(raw, node.name, 1)
            
        node.space = prefix
        
        if isinstance(node, (NamespaceDefine, StructDefine)):
            for child in node.nodes:
                self._resolve_space(_join_prefixes(prefix, node.name), child)
    
    def _merge_namespace(self, nodes: list[BaseNode]):
        namespaces: dict[str, NamespaceDefine] = {}
        
        for node in nodes:
            if isinstance(node, NamespaceDefine):
                if node.name not in namespaces:
                    namespaces[node.name] = node
                else:
                    other = namespaces[node.name]
                    if other.docs is None: other.docs = node.docs
                    else: other.docs += "\n\n" + node.docs
                    other.nodes.extend(node.nodes)
                    
        nodes[:] = list(namespaces.values()) + list(map(lambda x: not isinstance(x, NamespaceDefine), namespaces.values()))

class DocsGenerator:
    def __init__(self, nodes: list[BaseNode]):
        self.nodes = nodes
    
    def generate_to(self, file_creator: typing.Callable[[str, str], typing.Any]):
        @dataclasses.dataclass
        class NavNode:
            name: str = ""
            index: str = ""
            nodes: dict[str, NavNode] = dataclasses.field(default_factory=dict)
            
            def to_markdown(self, space: list[str], indent: int = 0):
                prefix = " " * indent
                lines = []
                
                space = space.copy()
                if self.name: space.append(fix_filename(self.name))
                path = "/".join(space) + ".md"
                
                if self.name:
                    text = f"- [{self.name}]({path})" if space else f"- {self.name}"
                    lines.append(prefix + text)
                
                for node in self.nodes.values():
                    lines.extend(node.to_markdown(space, indent + (2 if self.name else 0)))
                
                return lines
        
        nav = NavNode()
            
        def norm_content(content: str) -> str:
            return content.replace("\r", "").strip() + "\n"
        
        added_paths = set()

        def from_lines(space: list[str], lines: list[str]):
            path = "/".join(map(fix_filename, space)) + ".md"
            content = norm_content("\n".join(lines))
            if path not in added_paths: added_paths.add(path)
            else: content = f"\n---\n\n{content}"
            file_creator(path, content)
            
            node = nav
            for it in space:
                if it not in node.nodes:
                    node.nodes[it] = NavNode(it)
                node = node.nodes[it]
            node.index = path
            
        def resolve_docs(docs: typing.Optional[str]):
            if docs is None: return ""
            
            while True:
                start = docs.find("`@")
                if start == -1: break
                end = docs.find("`", start + 2)
                ref = docs[start + 2:end]
                splited = ref.split("/")
                ref_dir, ref_file = "/".join(splited[:-1]), splited[-1]
                docs = docs[:start] + f"[`{ref}`](./{ref_dir}/{fix_filename(ref_file)}.md)" + docs[end + 1:]
            
            return docs

        def fix_filename(filename: str):
            unallows = "<>:\"/\\|?*."
            for char in unallows:
                filename = filename.replace(char, f"unallowed_{ord(char)}")

            if filename == "index": filename = "index_"
            return filename
        
        def walk(node: BaseNode):
            if isinstance(node, NamespaceDefine):
                lines = [
                    f"# Namespace: `{node.name}`", "",
                    f"space: `{node.space}`", "",
                    "## docs", "",
                    resolve_docs(node.docs), "",
                    "## defines", ""
                ]
                
                for child in node.nodes:
                    lines.append(f"- [{child.name}](./{fix_filename(node.name)}/{fix_filename(child.name)}.md)")
                
                from_lines(node.get_space_list(), lines)
                
                for child in node.nodes:
                    walk(child)
            elif isinstance(node, FunctionDefine):
                lines = [
                    f"# Function: `{node.name}`", "",
                    f"space: `{node.space}`", "",
                    "## prototype", "",
                    f"```cpp",
                    node.code,
                    "```", "",
                    "## docs", "",
                    resolve_docs(node.docs)
                ]
                
                from_lines(node.get_space_list(), lines)
            elif isinstance(node, VariableDefine):
                lines = [
                    f"# Variable: `{node.name}`", "",
                    f"space: `{node.space}`", "",
                    "## declaration", "",
                    f"```cpp",
                    node.code,
                    "```", "",
                    "## docs", "",
                    resolve_docs(node.docs)
                ]

                from_lines(node.get_space_list(), lines)
            elif isinstance(node, UsingDirective):
                lines = [
                    f"# Using directive: `{node.name}`", "",
                    f"space: `{node.space}`", "",
                    "## declaration", "",
                    f"```cpp",
                    node.code,
                    "```", "",
                    "## docs", "",
                    resolve_docs(node.docs)
                ]

                from_lines(node.get_space_list(), lines)
            elif isinstance(node, StructDefine):
                lines = [
                    f"# Struct: `{node.name}`", "",
                    f"space: `{node.space}`", "",
                    "## declaration", "",
                    f"```cpp",
                    node.code,
                    "```", "",
                    "## docs", "",
                    resolve_docs(node.docs), "",
                    "## members", ""
                ]
                
                for child in node.nodes:
                    lines.append(f"- [{child.name}](./{fix_filename(node.name)}/{fix_filename(child.name)}.md)")
                
                from_lines(node.get_space_list(), lines)
                
                for child in node.nodes:
                    walk(child)
            elif isinstance(node, EnumDefine):
                lines = [
                    f"# Enum: `{node.name}`", "",
                    f"space: `{node.space}`", "",
                    "## declaration", "",
                    f"```cpp",
                    node.get_declaration(),
                    "```", "",
                    "## docs", "",
                    resolve_docs(node.docs)
                ]
                
                from_lines(node.get_space_list(), lines)
                
        for node in self.nodes:
            walk(node)
        
        file_creator(".nojekyll", "")
        file_creator("README.md", open("./README.md", "r", encoding="utf-8").read())
        file_creator("index.html", rf"""
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <title> Easy Phi Documentation </title>
    <meta name = "viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="//cdn.jsdelivr.net/npm/docsify/lib/themes/vue.css">
</head>

<body>
    <div id="app"></div>
    <script>
        window.$docsify = {{
            name: "Easy Phi",
            repo: "https://github.com/qaqFei/easy-phi",
            loadSidebar: true,
            subMaxLevel: 3,
            
            search: {{
                maxAge: 4000,
                paths: "auto",
                placeholder: "Type to search",
                noData: "No Results!",
            }}
        }};
    </script>
    <script src="//cdn.jsdelivr.net/npm/docsify/lib/docsify.min.js"></script>
    <script src="//cdn.jsdelivr.net/npm/docsify/lib/plugins/search.min.js"></script>
    <script src="//cdn.jsdelivr.net/npm/docsify-copy-code"></script>
    <script src="//cdn.jsdelivr.net/npm/docsify/lib/plugins/zoom-image.min.js"></script>
</body>

</html>
""")
        file_creator("_sidebar.md", "\n".join(nav.to_markdown([])))
        
if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=str, default="./src/easy_phi.hpp")
    args = parser.parse_args()
    
    with open(args.source, "rb") as f:
        analyzer = SourceAnalyzer(f.read())
    
    analyzer.resolve()
    generator = DocsGenerator(analyzer.nodes)
    
    shutil.rmtree("./docs", ignore_errors=True)
    
    def file_creator(filename: str, content: str):
        print(f"Generating {filename} ...")
        
        path = os.path.join("./docs", filename)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        
        mode = "w" if not os.path.exists(path) else "a"
        with open(path, mode, encoding="utf-8") as f:
            f.write(content)

    generator.generate_to(file_creator)
