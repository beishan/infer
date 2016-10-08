/* Copyright (c) 2016 - present Facebook, Inc.
  * All rights reserved.
  *
  * This source code is licensed under the BSD style license found in the
  * LICENSE file in the root directory of this source tree. An additional grant
  * of patent rights can be found in the PATENTS file in the same directory.
 */
/** Given a clang command, normalize it via `clang -###` if needed to get a clear view of what work
    is being done and which source files are being compiled, if any, then replace compilation
    commands by our own clang with our plugin attached for each source file. */

open! Utils;


/** where we are */
let exec_dir = Filename.dirname @@ Sys.executable_name;

let xx_suffix =
  try (Sys.getenv "INFER_XX") {
  | Not_found => ""
  };


/** script to run our own clang */
let infer_clang_bin = exec_dir /\/ ("filter_args_and_run_fcp_clang" ^ xx_suffix);


/** script to attach the plugin to clang -cc1 commands and run InferClang */
let clang_cc1_capture = exec_dir /\/ "attach_plugin_and_run_clang_frontend.sh";


/** path to Apple's clang */
let apple_clang =
  try (Some (Sys.getenv "FCP_APPLE_CLANG" ^ xx_suffix)) {
  | Not_found => None
  };

let typed_clang_invocation args => {
  let is_assembly =
    /* whether language is set to "assembler" or "assembler-with-cpp" */
    {
      let assembly_language =
        Array.fold_left
          (
            fun (prev_arg, b) arg => {
              let b' = b || string_equal "-x" prev_arg && string_is_prefix "assembler" arg;
              (arg, b')
            }
          )
          ("", false)
          args |> snd;
      /* Detect -cc1as or assembly language commands. -cc1as is always the first argument if
         present. */
      string_equal args.(1) "-cc1as" || assembly_language
    };
  if is_assembly {
    `Assembly args
  } else if (args.(1) == "-cc1") {
    `CC1
      /* -cc1 is always the first argument if present. */
      args
  } else {
    `NonCCCommand args
  }
};

let commands_or_errors =
  /* commands generated by `clang -### ...` start with ' "/absolute/path/to/binary"' */
  Str.regexp " \"/\\|clang\\(\\|++\\): error:";


/** Given a list of arguments for clang [args], return a list of new commands to run according to
    the results of `clang -### [args]`. */
let normalize args =>
  switch (typed_clang_invocation args) {
  | `CC1 args =>
    Logging.out "InferClangWrapper got toplevel -cc1 command@\n";
    [`CC1 args]
  | `NonCCCommand args =>
    let clang_hashhashhash =
      String.concat
        " " (IList.map Filename.quote [infer_clang_bin, "-###", ...Array.to_list args |> IList.tl]);
    Logging.out "clang -### invocation: %s@\n" clang_hashhashhash;
    let normalized_commands = ref [];
    let one_line line =>
      if (string_is_prefix " \"" line) {
        /* massage line to remove edge-cases for splitting */
        "\"" ^ line ^ " \"" |>
          /* split by whitespace */
          Str.split (Str.regexp_string "\" \"") |>
          Array.of_list |>
          typed_clang_invocation
      } else {
        `ClangError line
      };
    let consume_input i =>
      try (
        while true {
          let line = input_line i;
          /* keep only commands and errors */
          if (Str.string_match commands_or_errors line 0) {
            normalized_commands := [one_line line, ...!normalized_commands]
          }
        }
      ) {
      | End_of_file => ()
      };
    /* collect stdout and stderr output together (in reverse order) */
    with_process_full clang_hashhashhash consume_input consume_input |> ignore;
    normalized_commands := IList.rev !normalized_commands;
    /* Discard assembly commands. This may make the list of commands empty, in which case we'll run
       the original clang command. We could be smarter about this and try to execute the assembly
       commands with our own clang. */
    IList.filter
      (
        fun
        | `Assembly asm_cmd => {
            Logging.out
              "Skipping assembly command %s@\n" (String.concat " " @@ Array.to_list asm_cmd);
            false
          }
        | _ => true
      )
      !normalized_commands
  | `Assembly _ =>
    /* discard assembly commands -- see above */
    Logging.out "InferClangWrapper got toplevel assembly command@\n";
    []
  };

let execute_clang_command clang_cmd =>
  switch clang_cmd {
  | `CC1 args =>
    /* this command compiles some code; replace the invocation of clang with our own clang and
       plugin */
    args.(0) = clang_cc1_capture;
    Logging.out "Executing -cc1 command: %s@\n" (String.concat " " @@ Array.to_list args);
    Process.create_process_and_wait args
  | `ClangError error =>
    /* An error in the output of `clang -### ...`. Outputs the error and fail. This is because
       `clang -###` pretty much never fails, but warns of failures on stderr instead. */
    Logging.err "%s" error;
    exit 1
  | `Assembly args =>
    /* We shouldn't get any assembly command at this point */
    (
      if Config.debug_mode {
        failwithf
      } else {
        Logging.err
      }
    )
      "WARNING: unexpected assembly command: %s@\n" (String.concat " " @@ Array.to_list args)
  | `NonCCCommand args =>
    /* Non-compilation (eg, linking) command. Run the command as-is. It will not get captured
       further since `clang -### ...` will only output commands that invoke binaries using their
       absolute paths. */
    if Config.debug_mode {
      Logging.out "Executing raw command: %s@\n" (String.concat " " @@ Array.to_list args)
    };
    Process.create_process_and_wait args
  };

let () = {
  let args = Sys.argv;
  let commands = normalize args;
  if (commands == []) {
    /* No command to execute after -###, let's execute the original command
       instead.

       In particular, this can happen when
       - there are only assembly commands to execute, which we skip, or
       - the user tries to run `infer -- clang -c file_that_does_not_exist.c`. In this case, this
         will fail with the appropriate error message from clang instead of silently analyzing 0
         files. */
    args.(0) = infer_clang_bin;
    Logging.out
      "WARNING: `clang -### <args>` returned\n  an empty set of commands to run and no error. Will run the original command directly:@\n\n       %s@\n"
      (String.concat " " @@ Array.to_list args);
    Process.create_process_and_wait args
  } else {
    IList.iter execute_clang_command commands
  };
  /* xcodebuild projects may require the object files to be generated by the Apple compiler, eg to
     generate precompiled headers compatible with Apple's clang. */
  switch apple_clang {
  | None => ()
  | Some bin =>
    args.(0) = bin;
    Process.create_process_and_wait args
  }
};