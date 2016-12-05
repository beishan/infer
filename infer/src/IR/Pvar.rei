/*
 * vim: set ft=rust:
 * vim: set ft=reason:
 *
 * Copyright (c) 2009 - 2013 Monoidics ltd.
 * Copyright (c) 2013 - present Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
open! Utils;


/** Program variables. */
let module F = Format;


/** Type for program variables. There are 4 kinds of variables:
        1) local variables, used for local variables and formal parameters
        2) callee program variables, used to handle recursion ([x | callee] is distinguished from [x])
        3) global variables
        4) seed variables, used to store the initial value of formal parameters
    */
type t [@@deriving compare];


/** Equality for pvar's */
let equal: t => t => bool;


/** Compare two pvar's in alphabetical order */
let compare_alpha: t => t => int;


/** Dump a program variable. */
let d: t => unit;


/** Dump a list of program variables. */
let d_list: list t => unit;


/** Get the name component of a program variable. */
let get_name: t => Mangled.t;


/** [get_ret_pvar proc_name] retuns the return pvar associated with the procedure name */
let get_ret_pvar: Procname.t => t;


/** Get a simplified version of the name component of a program variable. */
let get_simplified_name: t => string;


/** Check if the pvar is an abduced return var or param passed by ref */
let is_abduced: t => bool;


/** Check if the pvar is a callee var */
let is_callee: t => bool;


/** Check if the pvar is a global var or a static local var */
let is_global: t => bool;


/** Check if the pvar is a static variable declared inside a function */
let is_static_local: t => bool;


/** Check if the pvar is a (non-static) local var */
let is_local: t => bool;


/** Check if the pvar is a seed var */
let is_seed: t => bool;


/** Check if the pvar is a return var */
let is_return: t => bool;


/** Check if a pvar is the special "this" var */
let is_this: t => bool;


/** return true if [pvar] is a temporary variable generated by the frontend */
let is_frontend_tmp: t => bool;


/** [mk name proc_name suffix] creates a program var with the given function name and suffix */
let mk: Mangled.t => Procname.t => t;


/** create an abduced variable for a parameter passed by reference */
let mk_abduced_ref_param: Procname.t => t => Location.t => t;


/** create an abduced return variable for a call to [proc_name] at [loc] */
let mk_abduced_ret: Procname.t => Location.t => t;


/** [mk_callee name proc_name] creates a program var
    for a callee function with the given function name */
let mk_callee: Mangled.t => Procname.t => t;


/** create a global variable with the given name */
let mk_global:
  is_constexpr::bool? =>
  is_pod::bool? =>
  is_static_local::bool? =>
  Mangled.t =>
  DB.source_file =>
  t;


/** create a fresh temporary variable local to procedure [pname]. for use in the frontends only! */
let mk_tmp: string => Procname.t => t;


/** Pretty print a program variable. */
let pp: printenv => F.formatter => t => unit;


/** Pretty print a list of program variables. */
let pp_list: printenv => F.formatter => list t => unit;


/** Pretty print a pvar which denotes a value, not an address */
let pp_value: printenv => F.formatter => t => unit;


/** Turn an ordinary program variable into a callee program variable */
let to_callee: Procname.t => t => t;


/** Turn a pvar into a seed pvar (which stores the initial value of a stack var) */
let to_seed: t => t;


/** Convert a pvar to string. */
let to_string: t => string;


/** Get the source file corresponding to a global, if known. Returns [None] if not a global. */
let get_source_file: t => option DB.source_file;


/** Is the variable's value a compile-time constant? Always (potentially incorrectly) returns
    [false] for non-globals. */
let is_compile_constant: t => bool;


/** Is the variable's type a "Plain Old Data" type (C++)? Always (potentially incorrectly) returns
    [true] for non-globals. */
let is_pod: t => bool;


/** Get the procname of the initializer function for the given global variable */
let get_initializer_pname: t => option Procname.t;

let module Set: PrettyPrintable.PPSet with type elt = t;
