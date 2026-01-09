import argparse
import subprocess
import sys
import os


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("elf_file", type=str, help="The ELF file to check")
    parser.add_argument("iec_path", type=str, help="The path to the idf-extra-components directory")
    parser.add_argument("idf_path", type=str, help="The path to the esp-idf directory")
    parser.add_argument("strings_program", type=str, help="The program to use to extract strings from the ELF file (e.g. 'strings')")
    args = parser.parse_args()


    # look for 'arg_int.c' in 'strings app.elf'
    strings_output = subprocess.check_output([args.strings_program, args.elf_file], encoding="utf-8")
    lines_with_arg_int_c = [line for line in strings_output.splitlines() if "arg_int.c" in line]

    found_iec_path = False
    found_idf_path = False

    for line in lines_with_arg_int_c:
        if os.path.abspath(args.iec_path) in line:
            found_iec_path = True
        if os.path.abspath(args.idf_path) in line:
            found_idf_path = True

    print(f"Found arg_int.c in IDF: {found_idf_path}, in IEC: {found_iec_path}", file=sys.stderr)
    if found_idf_path or not found_iec_path:
        print("Error: arg_int.c was found in IDF or not found in IEC", file=sys.stderr)
        raise SystemExit(1)

if __name__ == "__main__":
    main()
