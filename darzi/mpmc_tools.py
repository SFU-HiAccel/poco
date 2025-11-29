"""
Detect uses of
    mpmcbuffer<T [D], partition<cyclic<P>>, memcore<BRAM>, blocks<N>, pages<M>>
in a C/C++ source file.

* recognises direct uses of the full template
* recognises aliases (`using mybuf = ...`) and their later use
* recognises parameters of the form  mpmc<alias, K >

Tested with Clang = 18.
"""

import sys, os
import re
from pathlib import Path
from typing import Dict, List, Tuple
import json
import pdb
import itertools

import clang.cindex as cl

# -----------------------------------------------------------------------------
#  Helpers
# -----------------------------------------------------------------------------

MPMCBUFFER_RE = re.compile(r'\bmpmcbuffer\s*<', re.ASCII)
MPMC_RE       = re.compile(r'\bmpmc\s*<',       re.ASCII)
PARTITION_RE  = re.compile(
    r'(?:array_partition|partition)\s*<\s*(?:[\w:]+::)?'  # prefix
    r'(\w+)'                                              # strategy name
    r'(?:\s*<\s*([^>]+)\s*>)?'                            # optional '<P>'
    r'\s*>'
)

def canonical_spelling(tp: cl.Type) -> str:
    """Canonical spelling with all typedef sugar stripped."""
    return tp.get_canonical().spelling.replace("struct ", "").replace("class ", "")


def is_mpmcbuffer(tp: cl.Type) -> bool:
    """Light-weight check via the canonical spelling string."""
    return bool(MPMCBUFFER_RE.search(canonical_spelling(tp)))


def is_mpmc(tp: cl.Type) -> bool:
    return bool(MPMC_RE.search(canonical_spelling(tp)))


def first_template_arg_spelling(tp: cl.Type) -> str:
    """
    Extract the raw spelling of the first template argument.
    (Works even for aliased / elaborated types because we fall back to string parse.)
    """
    ct = tp.get_canonical()
    if ct.get_num_template_arguments() > 0:
        try:
            targ = ct.get_template_argument_type(0)
            if targ.kind != cl.TypeKind.INVALID:
                return canonical_spelling(targ)
        except cl.LibclangError:
            pass
    # fallback - crude but ... eh
    m = re.match(r'.*?<\s*([^,>]+)', ct.spelling)
    return m.group(1).strip() if m else ""

def get_int_arg(t):
    # fallback: regex  "mpmc<...,  42 >"
    m = re.search(r'[^,>]*?([0-9]+)\s*>$', t.spelling)
    return int(m.group(1)) if m else -1


# -----------------------------------------------------------------------------
#  AST scan
# -----------------------------------------------------------------------------

class BufferScanner:
    """Walks TU once, emits report describing required edits"""
    def __init__(self, tu: cl.TranslationUnit, original: bytes):
        self.tu = tu
        self.src = original
        self.aliases        : Dict[str, cl.Type] = {}   # alias name -> underlying mpmcbuffer<...>
        self.vardirects     : Dict[str, cl.Type] = {}
        self.func_param_pos : Dict[str, set[int]] = {}
        self.func_mpmc_args : Dict[str, List[str]] = {}
        self.func_args_count: Dict[str, Tuple(int,int)] = {}
        self.func_args      : List[Dict] = []
        self.invokes        : List[Dict] = []
        self.buffers_config : List[Dict] = []
        self.comp_errors    : List[Dict] = []
        self.ismpmc         = False
    
    def analyse(self):
        self._collect_aliases()
        self._scan_functions()
        self._scan_invokes()
        if(self.invokes):
            # print(self.invokes)
            self.ismpmc = True
        return {"aliases": self.aliases,
            "function_args": self.func_args,
            "invokes": self.invokes}

    # pass 1 - collect aliases  (typedef / using)
    def _collect_aliases(self):
        for cur in self.tu.cursor.walk_preorder():
            if cur.kind in (cl.CursorKind.TYPEDEF_DECL,
                            cl.CursorKind.TYPE_ALIAS_DECL):
                utype = cur.underlying_typedef_type
                # print(cur.spelling)
                if is_mpmcbuffer(utype):
                    self.aliases[cur.spelling] = utype

    # pass 2 - visit every function and record interesting nodes
    def _scan_functions(self):
        for fcur in self.tu.cursor.get_children():
            if fcur.kind != cl.CursorKind.FUNCTION_DECL or not fcur.is_definition():
                continue
            report : List[Tuple[str,str]] = []   # (kind, description)
            # --- parameters ------------------------------------------------
            for idx, p in enumerate(fcur.get_arguments()):
                # log all functions, with invoked parameters as -1, we set them later in _scan_invokes()
                self.func_args_count[fcur.spelling] = (idx+1,-1)
                if is_mpmcbuffer(p.type):
                    report.append(("param-direct", p.spelling))
                elif is_mpmc(p.type):
                    alias = first_template_arg_spelling(p.type)
                    if alias in self.aliases:
                        report.append(("param-mpmc-alias", f"{p.spelling}  (alias={alias})"))
                
                if is_mpmc(p.type):
                    repl = self._rewrite_mpmc_param(p, p.type)
                    if repl:
                        report.append(("param-direct", repl))
                        entry = self.func_mpmc_args.get(fcur.spelling)
                        print(repl)
                        if entry is None:
                            self.func_mpmc_args[fcur.spelling] = [p.spelling]
                        else:
                            entry.append(p.spelling)
                            self.func_mpmc_args[fcur.spelling] = entry
                        # remember position of mpmc --> (rx,tx) conversion for this `fcur`
                        self.func_args.append({
                            "file"  : p.location.file.name,
                            "start" : p.extent.start.offset,
                            "end"   : p.extent.end.offset,
                            "repl"  : repl[1],
                            "func"  : fcur.spelling,
                            "param" : p.spelling
                        })
                        self.func_param_pos.setdefault(fcur.spelling, set()).add(idx)

            for cur in fcur.get_children():
                if cur.kind == cl.CursorKind.COMPOUND_STMT:     # function body
                    report.extend(self._scan_body(cur))
            
            # output --------------------------------------------------------
            if report:
                print(f"\n=== Function '{fcur.spelling}' @ {fcur.location.file}:{fcur.location.line}")
                # for kind, desc in report:
                    # print(f"  {kind:<20} {desc}")

                for kind, desc in report:
                    if kind == "param-direct" and desc[0].startswith("mpmc"):
                        # print(f"  desc     : {desc}")
                        orig, new = desc
                        print(f"  original : {orig}")
                        print(f"  rewrite  : {new}")
                    else:
                        print(f"  {kind:<20} {desc}")

    def _scan_body(self, body_cursor) -> List[Tuple[str,str]]:
        out = []
        for cur in body_cursor.walk_preorder():
            if cur.kind == cl.CursorKind.VAR_DECL:
                vtype = cur.type
                if is_mpmcbuffer(vtype):
                    out.append(("var-direct", cur.spelling))
                    meta = self._decode_mpmcbuffer(vtype)
                    meta.update(
                        name = cur.spelling,
                        file = cur.location.file.name,
                        line = cur.location.line,
                        column = cur.location.column,
                    )
                    self.buffers_config.append(meta)
                elif is_mpmc(vtype):
                    alias = first_template_arg_spelling(vtype)
                    if alias in self.aliases:
                        out.append(("var-mpmc-alias", f"{cur.spelling}  (alias={alias})"))
        return out

    def _decode_mpmcbuffer(self, tp: cl.Type) -> Dict:
        """
        Return a dict with the template arguments of an mpmcbuffer<> type.
        Keys:
            datatype  : 'int', 'uint64_t', …
            length    : array length (int)
            partition : canonical spelling
            memcore   : canonical spelling
            blocks    : N  (int)          
            pages     : M  (int)           (optional)
        Anything missing is set to None.
        """
        canon = tp.get_canonical()
        nargs = canon.get_num_template_arguments()

        def typetemplate(i, kind=cl.TypeKind.INVALID):
            # print("type : " + str(canon.get_template_argument_type(i)))
            return canon.get_template_argument_type(i) if i < nargs else None

        def value(i):
            # print("value: " + str(canon.get_template_argument_type(i)))
            return get_int_arg(canon.get_template_argument_type(i)) if i < nargs else -1

        # first arg:  T [D]
        datatype = length = None
        if (arr := typetemplate(0)) and arr.kind == cl.TypeKind.CONSTANTARRAY:
            # print(arr.spelling)
            datatype = canonical_spelling(arr.get_array_element_type())
            length   = arr.get_array_size()

        #Also, `arr` is a `clang.cindex.Type` object, which is not callable, so the line 
        # 2nd and 3rd args are types
        # @TODO this needs to be handled for block as well, since then the partition won't produce a value
        partition = canonical_spelling(typetemplate(1)) if typetemplate(1) else None
        strategy = num_parts = None
        if partition:
            ps = PARTITION_RE.search(partition)
            strategy = ps.group(1)
            num_parts = int(ps.group(2)) if ps.group(2) is not None else None
        _memcore = canonical_spelling(typetemplate(2)) if typetemplate(2) else None
        # pdb.set_trace()
        memcore = typetemplate(2).get_template_argument_type(0).spelling

        # 4th & 5th args are non-type integral template params
        # pdb.set_trace()
        blocks = value(3) if value(3) >= 0 else None
        pages  = value(4) if value(4) >= 0 else None

        return dict(datatype=datatype, length=length,
                    partition=strategy, num_parts=num_parts,
                    memcore=memcore,
                    blocks=blocks, pages=pages)

    # build the replacement string for mpmc<buf,N> params
    def _rewrite_mpmc_param(self, param_cursor, mpmc_type):
        """
        Return (original_signature_fragment, replacement_string) or None.
        """
        canon = mpmc_type.get_canonical()
        if canon.get_num_template_arguments() < 2:
            return None  # malformed

        # 1st template argument: the buffer type or its alias
        buf_type = canon.get_template_argument_type(0)
        buf_name = canonical_spelling(buf_type)   # may be alias

        # 2nd template argument: the integer nports
        # use a regex "mpmc<...,  42 >"
        m = re.search(r',[^,>]*?([0-9]+)\s*>$', canon.spelling)
        nports = int(m.group(1)) if m else -1
        # nports   = get_int_arg(canon, 1)
        if nports < 0:
            return None  # second arg isn’t a non-negative integral

        # resolve alias to real mpmcbuffer<...>
        if buf_name in self.aliases:
            buf_type = self.aliases[buf_name]

        # extract element type T from the first template parameter of mpmcbuffer
        if not is_mpmcbuffer(buf_type):
            return None
        if buf_type.get_num_template_arguments() == 0:
            return None

        arr_tp = buf_type.get_template_argument_type(0)  # int [D]
        if arr_tp.kind != cl.TypeKind.CONSTANTARRAY:
            return None
        elem_tp = arr_tp.get_array_element_type()        # int
        T_str   = canonical_spelling(elem_tp)

        # build the two replacement parameters
        pname   = param_cursor.spelling            # original name (e.g. mb)
        orig    = f"{canonical_spelling(mpmc_type)} {pname}"
        new_sig = (
            f"tapa::istreams<{T_str}, {nports}>& {pname}_rx, "
            f"tapa::ostreams<{T_str}, {nports}>& {pname}_tx"
        )
        return orig, new_sig
    
    # pass 3 - scan `.invoke(...)` chains and note arg replacements
    def _scan_invokes(self):
        ''' scans the invokes literally and maintains a list of `args` for each invoke function.
            all depth-0 arguments (actual arguments of `.invoke`) are stored as one `arg` element,
            which in itself is a list. This helps club depth-1+ arguments as a list in one `arg` element.
            For example:
            `.invoke(func_name, var1, (var2_1, var2_2), var 3)`
            will populate `args` as `[[t_func_name], [t_var1], [t_var2_1, t_var2_2], t_var_3]`
            where `t_...` represents the corresponding token object.
        '''
        tokens = list(self.tu.get_tokens(extent=self.tu.cursor.extent))
        i = 1 # skip the first argument
        while i + 2 < len(tokens):
            # parse the parenthesised arg list
            # if tokens[i].spelling == "invoke" and tokens[i-1].spelling == "." and tokens[i+1].spelling == "(":
            if tokens[i].spelling == "invoke" and tokens[i-1].spelling == ".":
                afterinvoke_idx = i+1
                while tokens[afterinvoke_idx].spelling != "(":
                    afterinvoke_idx += 1
                depth = 0
                j = afterinvoke_idx                         # grab '('
                args, cur = [], []
                while j < len(tokens):
                    t = tokens[j]
                    # need to check the depth of the paranthesis for `invoke`. Consider only args at depth = 0
                    if t.spelling == "(":                   # starting brace for `invoke`
                        depth += 1
                        if depth == 1:
                            j += 1; continue
                    if t.spelling == ")":
                        depth -= 1
                        if depth == 0:                      # reached the `invoke`'s closing brace
                            args.append(cur); break
                    if t.spelling == "," and depth == 1:    # consider args only at depth = 1
                        args.append(cur)                    # store the current token (it's the function name)
                        cur = []                            # reset current list
                        j += 1; continue                    
                    cur.append(t); j += 1
                if args:                                    # if args get populated
                    func = args[0][0]
                    
                    # record number of parameters in function definition and in the invokes.
                    # don't break parsing here, emit error messages through `check_sanity`
                    try:
                        func_args_count_defntn = self.func_args_count.get(func.spelling)[0]
                    except TypeError:
                        self.comp_errors.append(f"Invoked function '{func.spelling}' in {func.location.file.name}:{func.location.line} was never defined by a forgetful POS")
                        i = j; continue

                    func_args_count_invoke = len(args[1:])
                    self.func_args_count[func.spelling] = (func_args_count_defntn, func_args_count_invoke)

                    pos_set = self.func_param_pos.get(func.spelling)
                    if (not pos_set):
                        i = j; continue                     # no mpmc params
                    for idx, arg in enumerate(args[1:], 0): # get all remaining args
                        if(idx) not in pos_set:
                            continue
                        if(len(arg)) != 1:                  # must be a single identifier
                            continue
                        token = arg[0]
                        # check if we already logged this function during the previous pass
                        base = token.spelling
                        self.invokes.append({
                            "file":  token.location.file.name,
                            "start": token.extent.start.offset,
                            "end":   token.extent.end.offset,
                            "repl":  f"{base}_tx, {base}_rx",
                        })
                i = j
            else:
                i += 1

    def check_sanity(self):
        returnflag = True

        # @TODO all invoke functions should have a corresponding definition in the source code
        for error in self.comp_errors:
            returnflag = False
            print(f"ERROR : {error}")

        # All functions should have the same number of arguments in the params and the invokes.
        for func in self.func_args_count:
            num_args_in_defntn = self.func_args_count[func][0]
            num_args_in_invoke = self.func_args_count[func][1]
            if(num_args_in_invoke != -1 and num_args_in_defntn != num_args_in_invoke):
                returnflag = False
                print(f"ERROR: His Highness Doofus Maximus has defined function '{func}' with {num_args_in_defntn} argument(s) but invoked it with {num_args_in_invoke}")        

        # @TODO No var-directs should be found anywhere in the code except the function cotnaining the invokes.

        # @TODO buffer configuration should have all valid fields except

        # @TODO total number of ports used in all param-directs combined should be <= total number of buffer ports
        return returnflag
                    

class Rewriter:
    def __init__(self, original: bytes):
        self.src = original
        self.func_arg_edits = []
        self.invoke_edits = []
        self.s2s_transform_edits = []
        self.brahma_stitches = []
        self.commited_edits = []  # all edits that have been committed so far

    def add_function_args(self, edits: List[Dict]):
        self.func_arg_edits = sorted(edits, key=lambda e: e["start"], reverse=True)
        for edit in self.func_arg_edits:
            self.commited_edits.append(edit)

    def add_invokes(self, edits:List[Dict]):
        self.invoke_edits = sorted(edits, key=lambda e: e["start"], reverse=True)
        for edit in self.invoke_edits:
            self.commited_edits.append(edit)

    def add_transforms(self, edits: List[Dict]):
        self.s2s_transform_edits = sorted(edits, key=lambda e: e["start"], reverse=True)
        for edit in edits:
            self.commited_edits.append(edit)

    def add_brahma_inserts(self, byte_index_to_insert: int, text_to_insert: str):
        brahma_tasks = {
            "start": byte_index_to_insert,
            "end": byte_index_to_insert,
            "repl": text_to_insert
        }
        self.brahma_stitches.append(brahma_tasks)
        self.commited_edits.append(brahma_tasks)

    def commit(self, update_src=True) -> bytes:
        """
        Commit all edits in reverse order to preserve indices while patching.
        """
        text = bytearray(self.src)
        for edit in sorted(self.commited_edits, key=lambda e: e["start"], reverse=True):
            text[edit["start"]:edit["end"]] = edit["repl"].encode()
        if(update_src):
            self.src = text
        return bytes(text)

class Optimizer:
    """
    Performs S2S transformations on the MPMC API calls
        * do_alloc -> blocking
        * do_free  -> blocking
        * do_read  -> optimized for non-blocking
        * do_write -> optimized for non-blocking
    """
    def __init__(self, scanner: BufferScanner):
        self.scanner = scanner
        self.tu = self.scanner.tu
        self.s2s_transforms: List[Dict] = []
        self.api_calls: List[Dict] = []
        self.target_funcs = set(self.scanner.func_param_pos)
        self._capi_id = itertools.count(0)
        self._dapi_id = itertools.count(0)
        print(f"Functions targeted for S2S transforms: {self.target_funcs}")
    
    def search_AST_recursively(self, cursor, kind):
        """
        helper function for searching the AST recursively
        """
        for node in cursor.get_children():
            if node.kind == kind:
                yield node
            yield from self.search_AST_recursively(node, kind)

    def errors(self):
        flag = True
        # @TODO check if the integer literal in the API call is less than the number of ports mentioned in params

    def perform_s2s_transforms(self):
        """
        Perform the S2S transforms on the API calls found in the source code.
        This is the main entry point for the optimizer.
        """
        for api_call in self.api_calls:
            callee_name = api_call["callee_name"]
            if callee_name == "do_alloc":
                self._get_transform_alloc(api_call)
            elif callee_name == "do_free":
                self._get_transform_free(api_call)
            elif callee_name == "do_read":
                self._get_transform_read(api_call)
            elif callee_name == "do_write":
                self._get_transform_write(api_call)

    def process_transforms(self):
        """
        public entry point
        """
        for func_cur in self.tu.cursor.get_children():
            if func_cur.kind != cl.CursorKind.FUNCTION_DECL or func_cur.spelling not in self.target_funcs:
                continue
            self._scan_body(func_cur)

        self.perform_s2s_transforms()
        return self.s2s_transforms
    
    def _is_api_call(self, func, cur, name_set=["do_alloc", "do_free", "do_read", "do_write"]):
        """
        Checks if a CALL_EXPR is a API call of the form mpmcbuf[port].do_<name_set>(...)
        """
        if cur.kind != cl.CursorKind.CALL_EXPR:
            return False, None

        api_node = None
        is_api_call = False
        api_spelling = None
        if cur.kind is cl.CursorKind.CALL_EXPR:
            tokens = list(self.tu.get_tokens(extent=cur.extent))
            api_node = cur

            target_var = tokens[0].spelling
            # print(func.spelling, target_var, self.scanner.func_mpmc_args.get(func.spelling))
            if(target_var not in self.scanner.func_mpmc_args.get(func.spelling)):
                # this API call does not belong to any MPMC variable
                return False, None

            # need '.', 'do_x', '('
            for i, tok in enumerate(tokens[:-2]):
                if tok.spelling == "." and tokens[i+2].spelling == "(":
                    api_spelling = tokens[i+1].spelling
                    if (tokens[i+1].spelling in name_set):
                        is_api_call = True
                        break
                    else:
                        print(f"FATAL: Malformed API call '{tokens[i+1].spelling}' at location {str(api_node.location)}")
                        sys.exit()                
                else:
                    continue   # not this API
        return is_api_call, api_spelling

    # walk one function's body
    def _scan_body(self, func_cursor):
        """
        Walk the function to check for all MPMC-API calls
        """
        # Which parameter positions were split in this function?
        split_pos = self.scanner.func_param_pos.get(func_cursor.spelling, set())
        if not split_pos:   # no MPMC params in this function
            return

        # Locate the API declarations first (DECL_STMT)
        for child in func_cursor.walk_preorder():
            if child.kind == cl.CursorKind.DECL_STMT:
                # potential API call. Confirm if it is.
                decl_stmt = child
                # Walk every descendent inside the DECL_STMT and decompose the source expression
                for cur in decl_stmt.walk_preorder():
                    # get the var_decl
                    if cur.kind == cl.CursorKind.VAR_DECL:
                        var_node = cur; continue
                    # get the type_ref
                    if cur.kind == cl.CursorKind.TYPE_REF:
                        typeref_node = cur; continue
                    # get the API call
                    is_api_call, callee_name = self._is_api_call(func_cursor, cur)
                    if is_api_call:
                        if(callee_name in ["do_alloc", "do_free"]):
                            api_entry = self._parse_control_api(callee_name, cur, var_node, typeref_node, decl_stmt, func_cursor)
                        elif(callee_name in ["do_read", "do_write"]):
                            api_entry = self._parse_dataio_api(callee_name, cur, var_node, typeref_node, decl_stmt, func_cursor)
                        self.api_calls.append(api_entry)

    def _parse_control_api(self, callee_name, cur, var_node, typeref_node, decl_stmt, func_cursor):
        text = bytearray(self.scanner.src)
        rhs_node = next(self.search_AST_recursively(var_node, cl.CursorKind.UNEXPOSED_EXPR))
        buffer_name = (text[rhs_node.extent.start.offset:rhs_node.extent.end.offset]).decode()
        int_node = next(self.search_AST_recursively(cur, cl.CursorKind.INTEGER_LITERAL))
        int_literal = int(text[int_node.extent.start.offset:int_node.extent.end.offset])
        # we have everything needed to perform the S2S transformation. Log this.
        print(f"Found control API {callee_name} on {buffer_name} through port {int_literal} in {func_cursor.spelling}")
        api_entry = {
            "func": func_cursor,
            "decl": decl_stmt,
            "call": cur,
            "var": var_node,
            "type": typeref_node,
            "callee_name": callee_name,
            "buffer_name": buffer_name,
            "port_idx": int_literal
        }
        return api_entry
    
    def _parse_dataio_api(self, callee_name, cur, var_node, typeref_node, decl_stmt, func_cursor):
        text = bytearray(self.scanner.src)
        rhs_node = next(self.search_AST_recursively(var_node, cl.CursorKind.UNEXPOSED_EXPR))
        buffer_name = (text[rhs_node.extent.start.offset:rhs_node.extent.end.offset]).decode()
        int_node = next(self.search_AST_recursively(cur, cl.CursorKind.INTEGER_LITERAL))
        int_literal = int(text[int_node.extent.start.offset:int_node.extent.end.offset])
        # we have everything needed to perform the S2S transformation. Log this.
        print(f"Found data API {callee_name} on {buffer_name} through port {int_literal} in {func_cursor.spelling}")
        api_entry = {
            "func": func_cursor,
            "decl":decl_stmt,
            "call": cur,
            "var": var_node,
            "type": typeref_node,
            "callee_name": callee_name,
            "buffer_name": buffer_name,
            "port_idx": int_literal
        }
        return api_entry

    # -----------------------------------------------------------------
    # helper that schedules a single do_alloc rewrite
    # -----------------------------------------------------------------
    def _get_transform_alloc(self, api_call):
        """
        Emits one replacement record turning:
            rsp = mb[1].do_alloc(args);
        into the 3-line prep, write, read sequence.
        """
        # argument text inside do_alloc(...)
        decl_start = api_call['decl'].extent.start.offset; decl_end = api_call['decl'].extent.end.offset
        arg_text = self._source_between(decl_start, decl_end)
        arg_text = arg_text[arg_text.find('(')+1 : arg_text.rfind(')')]
        indent_spaces = (api_call['decl'].location.column-1) * " "
        unique_id = next(self._capi_id)
        req_var   = f"_req_alloc_{unique_id}"
        rx_name   = f"{api_call['buffer_name']}_rx"
        tx_name   = f"{api_call['buffer_name']}_tx"
        new_code  = (
            f"sb_req_t {req_var} = sb_request_alloc({arg_text});\n{indent_spaces}"
            f"{tx_name}[{api_call['port_idx']}].write({req_var});\n{indent_spaces}"
            f"{api_call['type'].referenced.spelling} {api_call['var'].spelling} = {rx_name}[{api_call['port_idx']}].read();"
        )
        self.s2s_transforms.append({"start": decl_start, "end": decl_end, "repl": new_code})

    def _get_transform_free(self, api_call):
        """
        Emits one replacement record turning:
            rsp = mb[x].do_free(args);
        into the 3-line prep, write, read sequence.
        """
        # argument text inside do_alloc(...)
        decl_start = api_call['decl'].extent.start.offset; decl_end = api_call['decl'].extent.end.offset
        arg_text = self._source_between(decl_start, decl_end)
        arg_text = arg_text[arg_text.find('(')+1 : arg_text.rfind(')')]
        indent_spaces = (api_call['decl'].location.column-1) * " "
        # print(api_call['call'].location.column-1)
        unique_id = next(self._capi_id)
        req_var   = f"_req_free_{unique_id}"
        rx_name   = f"{api_call['buffer_name']}_rx"
        tx_name   = f"{api_call['buffer_name']}_tx"
        new_code  = (
            f"sb_req_t {req_var} = sb_request_free({arg_text});\n{indent_spaces}"
            f"{tx_name}[{api_call['port_idx']}].write({req_var});\n{indent_spaces}"
            f"{api_call['type'].referenced.spelling} {api_call['var'].spelling} = {rx_name}[{api_call['port_idx']}].read();"
        )
        self.s2s_transforms.append({"start": decl_start, "end": decl_end, "repl": new_code})

    def _get_transform_read(self, api_call):
        """
        Emits one replacement record turning rsp = mb[1].do_read(<args>) into the 3-line prep, write, read sequence
        """
        func        = api_call["func"]
        decl        = api_call['decl']
        call        = api_call["call"]
        var         = api_call["var"]
        type        = api_call["type"]
        callee_name = api_call["callee_name"]
        buffer_name = api_call["buffer_name"]
        port_idx    = api_call["port_idx"]
        decl_start = decl.extent.start.offset; decl_end = decl.extent.end.offset

        # Is the API inside a FOR_STMT?
        for_stmts = self.search_AST_recursively(func, cl.CursorKind.FOR_STMT)
        target_for_stmt = None
        for for_stmt in for_stmts:
            # skip those statements which exit before or start after our target declaration
            if(for_stmt.extent.end.offset <= decl_start or for_stmt.extent.start.offset >= decl_end):
                continue
            else:
                # API is within this `for`, record ...
                target_for_stmt = for_stmt
                # ... but keep walking to check nesting
                continue
        
        if target_for_stmt is not None:
            compound_start = next(self.search_AST_recursively(for_stmt, cl.CursorKind.COMPOUND_STMT)).extent.start.offset
            for_stmt_header_str = self._get_for_stmt_header(for_stmt)
            api_call_args_str = self._get_api_call_args(call)
            indent_level_decl = (decl.location.column-1) * " "

            # TX LOOP:
            #   - requires all expressions within the CursorKind.FOR_STMT
            #   - might need some analysis on how the API's arguments are determined so that they can be moved to the TX_LOOP
            unique_id = next(self._dapi_id)
            req_var   = f"_req_read_{unique_id}"
            tx_name   = f"{api_call['buffer_name']}_tx"
            tx_loop = (
                f"{{\n{indent_level_decl}"
                f"REQ_LOOP_R{unique_id}: for{for_stmt_header_str} {{\n{indent_level_decl}"
                f"{self._source_between(compound_start+1, decl_start)}\n{indent_level_decl}"
                f"  sb_req_t {req_var} = sb_request_read({api_call_args_str});\n{indent_level_decl}"
                f"  {tx_name}[{port_idx}].write({req_var});\n{indent_level_decl}}}\n"
            )
            # RX LOOP:
            #   - requires anything dependent on the read variable in the RX body
            #   - might need some analysis to make sure the API's arguments are independent of the read value.
            rx_name   = f"{api_call['buffer_name']}_rx"
            rx_loop = (
                f"{indent_level_decl}"
                f"RSP_LOOP_R{unique_id}: for{for_stmt_header_str} {{\n{indent_level_decl}"
                f"  {type.referenced.spelling} {var.spelling} = {rx_name}[{port_idx}].read();"
                f"\n{indent_level_decl}}}"
                f"{self._source_between(decl_end, for_stmt.extent.end.offset)}\n"
            )
            new_code = (f"{tx_loop}{rx_loop}")
            self.s2s_transforms.append({"start": for_stmt.extent.start.offset, "end": for_stmt.extent.end.offset, "repl": new_code})
        else:
            api_call_args_str = self._get_api_call_args(call)
            indent_level_decl = (decl.location.column-1) * " "
            # TX phase
            unique_id = next(self._dapi_id)
            req_var   = f"_req_read_{unique_id}"
            tx_name   = f"{api_call['buffer_name']}_tx"
            tx_loop = (
                f"sb_req_t {req_var} = sb_request_read({api_call_args_str});\n{indent_level_decl}"
                f"{tx_name}[{port_idx}].write({req_var});\n{indent_level_decl}}}\n"
            )
            # RX phase
            rx_name   = f"{api_call['buffer_name']}_rx"
            rx_loop = (
                f"{indent_level_decl}"
                f"{type.referenced.spelling} {var.spelling} = {rx_name}[{port_idx}].read();\n}}"
                f"{self._source_between(decl_end, for_stmt.extent.end.offset)}\n"
            )
            new_code = (f"{tx_loop}{rx_loop}")
            self.s2s_transforms.append({"start": for_stmt.extent.start.offset, "end": for_stmt.extent.end.offset, "repl": new_code})

    # Post review: Perhaps, there is a better way to perform this optimization. Similar for _get_transform_read.
    def _get_transform_write(self, api_call):
        """
        Emits one replacement record turning rsp = mb[1].do_write(<args>) into the 3-line prep, write, read sequence
        """
        func        = api_call["func"]
        decl        = api_call['decl']
        call        = api_call["call"]
        var         = api_call["var"]
        type        = api_call["type"]
        callee_name = api_call["callee_name"]
        buffer_name = api_call["buffer_name"]
        port_idx    = api_call["port_idx"]
        decl_start = decl.extent.start.offset; decl_end = decl.extent.end.offset

        # is the API inside a FOR_STMT? (TODO: check for nested FORs also)
        for_stmts = self.search_AST_recursively(func, cl.CursorKind.FOR_STMT)
        target_for_stmt = None
        for for_stmt in for_stmts:
            # skip those statements which exit before or start after our target declaration
            if(for_stmt.extent.end.offset <= decl_start or for_stmt.extent.start.offset >= decl_end):
                continue
            else:
                # API is within this `for`, record ...
                target_for_stmt = for_stmt
                # ... but keep walking to check nesting
                continue

        if target_for_stmt is not None:                 # perform changes within the targeted `for` block
            compound_start = next(self.search_AST_recursively(for_stmt, cl.CursorKind.COMPOUND_STMT)).extent.start.offset
            for_stmt_header_str = self._get_for_stmt_header(for_stmt)
            api_call_args_str = self._get_api_call_args(call)
            indent_level_for  = (for_stmt.location.column-1) * " "
            indent_level_decl = (decl.location.column-1) * " "
            # TX LOOP:
            #   - requires all expressions within the CursorKind.FOR_STMT
            #   - might need some analysis on how the API's arguments are determined so that they can be moved to the TX_LOOP
            unique_id = next(self._dapi_id)
            req_var   = f"_req_write_{unique_id}"
            tx_name   = f"{api_call['buffer_name']}_tx"
            tx_loop = (
                f"{{\n{indent_level_decl}"
                f"REQ_LOOP_W{unique_id}: for{for_stmt_header_str} {{\n{indent_level_decl}"
                f"{self._source_between(compound_start+1, decl_start)}\n{indent_level_decl}"
                f"  sb_req_t {req_var} = sb_request_write({api_call_args_str});\n{indent_level_decl}"
                f"  {tx_name}[{port_idx}].write({req_var});\n{indent_level_decl}}}\n"
            )
            # RX LOOP:
            #   - requires anything dependent on the read variable in the RX body
            #   - might need some analysis to make sure the API's arguments are independent of the read value.
            rx_name   = f"{api_call['buffer_name']}_rx"
            rx_loop = (
                f"{indent_level_decl}"
                f"RSP_LOOP_W{unique_id}: for{for_stmt_header_str} {{\n{indent_level_decl}"
                f"  {type.referenced.spelling} {var.spelling} = {rx_name}[{port_idx}].read();"
                f"\n{indent_level_decl}}}"
                f"{self._source_between(decl_end, for_stmt.extent.end.offset)}\n"
            )
            new_code = (f"{tx_loop}{rx_loop}")
            self.s2s_transforms.append({"start": for_stmt.extent.start.offset, "end": for_stmt.extent.end.offset, "repl": new_code})
        else:                                           # API is not inside a `for` block
            api_call_args_str = self._get_api_call_args(call)
            indent_level_decl = (decl.location.column-1) * " "
            # TX phase
            unique_id = next(self._dapi_id)
            req_var   = f"_req_write_{unique_id}"
            tx_name   = f"{api_call['buffer_name']}_tx"
            tx_loop = (
                f"sb_req_t {req_var} = sb_request_write({api_call_args_str});\n{indent_level_decl}"
                f"  {tx_name}[{port_idx}].write({req_var});\n{indent_level_decl}}}\n"
            )
            # RX phase
            rx_name   = f"{api_call['buffer_name']}_rx"
            rx_loop = (
                f"{indent_level_decl}"
                f"  {type.referenced.spelling} {var.spelling} = {rx_name}[{port_idx}].read();"
                f"{self._source_between(decl_end, for_stmt.extent.end.offset)}\n"
            )
            new_code = (f"{tx_loop}{rx_loop}")
            self.s2s_transforms.append({"start": for_stmt.extent.start.offset, "end": for_stmt.extent.end.offset, "repl": new_code})


    # -----------------------------------------------------------------
    # tiny helpers
    # -----------------------------------------------------------------

    def _get_for_stmt_header(self, for_stmt):
        for_stmt_header = []
        for_stmt_header_str = "("
        for child in for_stmt.get_children():
            if(child.kind == cl.CursorKind.COMPOUND_STMT):
                continue
            text = self._source_cursor(child)
            for_stmt_header.append(text)
            if(child.kind == cl.CursorKind.DECL_STMT):  # first declaration in loop header; comes with ';'
                for_stmt_header_str += f"{text[:-1]}"
            else:
                for_stmt_header_str += f"; {text}"
        for_stmt_header_str += ")"
        return for_stmt_header_str
    
    def _get_api_call_args(self, call):
        api_call_args = ""
        for child in call.get_children():
            if child.kind == cl.CursorKind.MEMBER_REF_EXPR:
                continue
            text = self._source_cursor(child)
            api_call_args += f"{text}, "
        api_call_args_str = api_call_args[:-2]
        return api_call_args_str

    def _parameter_index(self, func_cur, var_name):
        for i, p in enumerate(func_cur.get_arguments()):
            if p.spelling == var_name:
                return i
        return None

    def _source_between(self, start_loc, end_loc):
        return self.scanner.src[start_loc:end_loc].decode()

    def _source_cursor(self, cursor):
        return self._source_between(cursor.extent.start.offset, cursor.extent.end.offset)

    def _get_source_text(self, cursor):
        return self._source_between(cursor.extent.start, cursor.extent.end)

# NOT INTENDED TO BE RUN AS MAIN
if __name__ == "__main__":
    print("This module shouldn't be run as main")
    sys.exit(1)

