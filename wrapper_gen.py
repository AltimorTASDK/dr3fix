from textwrap import dedent

class Export():
    def __init__(self, name, ordinal):
        self.name = name
        self.ordinal = ordinal

def main():
    exports = []
    with open("exports.txt", "r") as export_file:
        for line in export_file:
            name, _, ordinal = line.split()
            exports.append(Export(name, ordinal))

    with open("dr3fix/steam_api64.def", "w") as def_file:
        def_file.write("LIBRARY STEAM_API64\n")
        def_file.write("EXPORTS\n")
        for export in exports:
            def_file.write(f"\t{export.name} @{export.ordinal}\n")

    with open("dr3fix/steam_api64_functions.cpp", "w") as cpp_file:
        cpp_file.write("#include <Windows.h>\n\n")
        cpp_file.write(dedent("""
            #include <Windows.h>

            extern "C" {
            """).strip() + "\n")

        for export in exports:
            cpp_file.write(f"\tFARPROC orig_{export.name};\n")

        cpp_file.write("}\n")
        cpp_file.write(dedent("""
            class functions {
            public:
            \tfunctions()
            \t{
            \t\tconst auto module = LoadLibrary("steam_api64_orig.dll");
            """))

        for export in exports:
            cpp_file.write(f'\t\torig_{export.name} = GetProcAddress(module, "{export.name}");\n')

        cpp_file.write(dedent("""
            \t}
            } functions;
            """))

    with open("dr3fix/steam_api64_wrapper.asm", "w") as asm_file:
        for export in exports:
            if export.name == "SteamAPI_Init":
                continue
            asm_file.write(f"extern orig_{export.name}:QWORD\n")
            asm_file.write(f"{export.name} proto\n\n")

        asm_file.write("\n.code\n")

        for export in exports:
            if export.name == "SteamAPI_Init":
                continue
            asm_file.write(dedent(f"""
                {export.name} proc
                        jmp     [orig_{export.name}]
                {export.name} endp
                """))

        asm_file.write("end")

if __name__ == "__main__":
    main()