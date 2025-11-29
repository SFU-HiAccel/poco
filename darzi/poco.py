#!/usr/bin/env python3
import sys, os
from pathlib import Path
import json
import brahma
import pdb
import argparse

import logging
logging.basicConfig(filename='.log', filemode='w', level=logging.DEBUG)
logger = logging.getLogger(__name__)

import clang.cindex as cl
prefix = os.environ["CONDA_PREFIX"]          # raise fatal KeyError if not in Conda
libclang = os.path.join(prefix, "lib", "libclang.so")
cl.Config.set_library_file(libclang)

import mpmc_tools

def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog='poco',
        description='Frontend parser for the MPMC framework',
    )
    parser.add_argument(
        '-v',
        '--verbose',
        action='count',
        dest='verbose',
        help='Increase logging verbosity',
    )
    parser.add_argument(
        '--adir',
        type=str,
        dest='asset_directory',
        required=True,
        help='The directory that contains the CPP assets to stitch into the source kernel',
    )
    parser.add_argument(
        '--src',
        type=str,
        metavar='file',
        dest='src_kernel_file',
        required=True,
        help='The source file monolith to be parsed and transformed',
    )
    parser.add_argument(
        '--clang-args',
        type=str,
        metavar='clang_args',
        dest='clang_args',
        action='append',
        nargs='*',
        help='(optional) Extra arguments to pass to the Clang parser.',
    )
    parser.add_argument(
        '--clang-ipath',
        type=str,
        metavar='clang_ipath',
        dest='clang_ipaths',
        action='append',
        nargs='*',
        help='(optional) Include paths to pass to the Clang parser.',
    )
    parser.add_argument(
        '--dest',
        type=str,
        metavar='file',
        dest='dest_kernel_file',
        default='out/kernel_out.cpp',
        help='Destination file monolith',
    )
    parser.add_argument(
        '--top',
        type=str,
        metavar='top_func',
        dest='top_func',
        required=True,
        help='The top function or the kernel name to work for',
    )
    parser.add_argument(
        '--unit_sb_top',
        dest='unit_sb_top',
        action='store_true',
        help='Collapse all tasks inside the shared buffer to a single topwrap task.' \
        'This will force Autobridge to keep all of shared buffer in one slot.' \
        'Recommended only for small MPMC buffers'
    )
    parser.add_argument(
        '--buffer_config',
        dest='buffer_config',
        required=True,
        help='JSON structure that contains the configuration for the shared buffer'
    )
    parser.add_argument(
        '--dump_ast',
        dest='dump_ast',
        action='store_true',
        help='dump the JSON structure that contains the parsed AST'
    )
    return parser

def dump_ast(outfile, cursor, indent=0):
    outfile.write('  ' * indent + f'Kind: {cursor.kind}, Name: {cursor.spelling}, Location: {cursor.location}\n')
    for child in cursor.get_children():
        dump_ast(outfile, child, indent + 1)

def search_AST_recursively(cursor):
    for node in cursor.walk_preorder():
        if node.kind == cl.CursorKind.FUNCTION_DECL:
            print(node.spelling)

def step_parse(src, args) -> tuple[bool, Path, mpmc_tools.BufferScanner]:
    clang_args_std = args.clang_args
    _args_ipath = args.clang_ipaths
    content = src.read_bytes()
    idx = cl.Index.create()
    clang_args = []
    if(clang_args_std is not None):
        for arg in clang_args_std:
            try:
                clang_args.append(str(arg[0]))
            except:
                sys.exit("Please check Clang arguments supplied on command\n" + str(clang_args))
    if(_args_ipath is not None):
        for arg in _args_ipath:
            try:
                clang_args.append("-I"+str(arg[0]))
            except:
                sys.exit("Please check Clang arguments supplied on command\n" + str(clang_args))
    print(clang_args)
    tu  = idx.parse(str(src), args=clang_args, options=cl.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
    # for d in tu.diagnostics:
    #     print(d.severity, d.location, d.spelling)
    if(args.dump_ast):
        with open("poco_ast.json", "w") as f:
            dump_ast(f, tu.cursor)

    scanner = mpmc_tools.BufferScanner(tu, content)

    report = scanner.analyse()
    if(not scanner.check_sanity()):
        sys.exit()
    # json.dump(report, open("mpmc_report.json","w"), indent=2)
    print("========================================================")
    print(f"Found {len(scanner.aliases)} alias(es): {', '.join(scanner.aliases) or '-'}")
    json.dump(scanner.buffers_config, open("mpmc_report.json","w"), indent=2)

    return scanner

def step_optimize(src, scanner):
    optimizer = mpmc_tools.Optimizer(scanner)
    optimizer.process_transforms()
    # _post_opt = src.with_name(src.stem + "-optimized.cpp")
    return optimizer

def step_codegen(src, scanner, optimizer):
    # add all edits first one by one, then commit them all at once
    # all edits are made in reverse order to preserve indices while patching
    content = src.read_bytes()
    rw = mpmc_tools.Rewriter(content)
    if not scanner.ismpmc:
        return
    else:
        rw.add_invokes(scanner.invokes)

    if scanner.func_args:
        rw.add_function_args(scanner.func_args)
    if optimizer.s2s_transforms:
        rw.add_transforms(optimizer.s2s_transforms)

    return rw

def step_stitch(sb_obj, args, optimizer, src, rw):
    # MPMC buffer tasks before top-level task
    funcs = optimizer.search_AST_recursively(optimizer.scanner.tu.cursor, cl.CursorKind.FUNCTION_DECL)
    while(True):
        try:
            func = next(funcs)
            # if the function spelling matches and it is the actual definition, not a declaration.
            if (func.spelling == args.top_func and str(func.location.file) == str(src)):
                break
        except StopIteration:
            print(f"FATAL: No top-level function '{func.spelling}' found!"); sys.exit()
    with open(src, "r") as fp:
        fp.seek(0)
        for _ in range(func.location.line - 1):
            if not fp.readline():
                raise ValueError("This is programmatically unexpected and shouldn't have happened")
        byte_index_before_top_func = fp.tell()
        mpmc_tasks_str = (sb_obj.add_cpp_assets(args.asset_directory))
        rw.add_brahma_inserts(byte_index_before_top_func, f"{mpmc_tasks_str}\n\n")

    # declarations before tapa::task()
        indent_level = 0
        lastbyteindex = -1
        line = fp.readline()
        while(line != ''):
            if(line.find("tapa::task()") != -1):
                indent_level = len(line) - len(line.lstrip())
                byte_index_before_tapa_task = lastbytyeindex
                break
            lastbytyeindex = fp.tell()
            line = fp.readline()
        sb_decl = sb_obj.get_internal_decls(indent_level)
        sb_decl += sb_obj.get_backend_pages_decl(indent_level)
        rw.add_brahma_inserts(byte_index_before_tapa_task, sb_decl + "\n\n")
    
    # invokes after tapa::task()
        while(line != ''):
            if(line.find(".invoke") != -1):
                byte_index_after_tapa_task = fp.tell()
                break
            line = fp.readline()
        # for entry in optimizer.scanner.invokes:
        #     print(fp.tell(), entry['start'])
        sb_invokes = sb_obj.get_sb_task_invokes(indent_level)
        rw.add_brahma_inserts(byte_index_after_tapa_task, sb_invokes)


# MAIN
def main():
    parser = create_parser()
    args = parser.parse_args()
    print(args.clang_args, args.src_kernel_file, args.dest_kernel_file, args.top_func)
    src = Path(args.src_kernel_file).resolve()
    
    # syntax analysis and parsing
    scanner = step_parse(src, args)
    print(scanner.func_args)

    # Perform S2S transforms to optimize I/O
    if(scanner.ismpmc):
        optimizer = step_optimize(src, scanner)
        rw = step_codegen(src, scanner, optimizer)
    
        sb_obj = brahma.SharedBuffer()
        sb_obj.update_sb_config_from_json('mpmc_report.json')
        # stitch
        step_stitch(sb_obj, args, optimizer, src, rw)

        # commit now
        dest = rw.commit()
        # write out
        _pre_opt = Path(args.dest_kernel_file).resolve()
        _pre_opt.write_bytes(dest)
        print(f"Wrote {_pre_opt}")
        json.dump(dict(_pre_opt=str(dest)), open("poco_temp.json","w"), indent=2)
    else:
        print("Design does not require MPMC buffers")
    

if __name__ == "__main__":
    main()