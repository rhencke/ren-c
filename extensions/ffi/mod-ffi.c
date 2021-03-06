//
//  File: %mod-ffi.c
//  Summary: "Foreign function interface main C file"
//  Section: Extension
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 Atronix Engineering
// Copyright 2012-2017 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
//=////////////////////////////////////////////////////////////////////////=//
//

#include "sys-core.h"

#include "tmp-mod-ffi.h"

#include "reb-struct.h"

REBTYP *EG_Struct_Type = nullptr;  // (E)xtension (G)lobal

// There is a platform-dependent list of legal ABIs which the MAKE-ROUTINE
// and MAKE-CALLBACK natives take as an option via refinement
//
static ffi_abi Abi_From_Word(const REBVAL *word) {
    switch (VAL_WORD_SYM(word)) {
      case SYM_DEFAULT:
        return FFI_DEFAULT_ABI;

    #ifdef X86_WIN64
      case SYM_WIN64:
        return FFI_WIN64;

    #elif defined(X86_WIN32) || defined(TO_LINUX_X86) || defined(TO_LINUX_X64)
      case SYM_SYSV:
        return FFI_SYSV;

      // !!! While these are defined on newer versions of LINUX X86 and X64
      // FFI, older versions (e.g. 3.0.13) only have STDCALL/THISCALL/FASTCALL
      // on Windows.  We could detect the FFI version, but since basically
      // no one uses anything but the default punt on it for now.
      //
      #ifdef X86_WIN32
        case SYM_STDCALL:
          return FFI_STDCALL;

        case SYM_THISCALL:
          return FFI_THISCALL;

        case SYM_FASTCALL:
          return FFI_FASTCALL;
      #endif

      #ifdef X86_WIN32
        case SYM_MS_CDECL:
          return FFI_MS_CDECL;
      #else
        case SYM_UNIX64:
          return FFI_UNIX64;
      #endif //X86_WIN32

    #elif defined (TO_LINUX_ARM)
      case SYM_VFP:
        return FFI_VFP;

      case SYM_SYSV:
        return FFI_SYSV;

    #elif defined (TO_LINUX_MIPS)
      case SYM_O32:
        return FFI_O32;

      case SYM_N32:
        return FFI_N32;

      case SYM_N64:
        return FFI_N64;

      case SYM_O32_SOFT_FLOAT:
        return FFI_O32_SOFT_FLOAT;

      case SYM_N32_SOFT_FLOAT:
        return FFI_N32_SOFT_FLOAT;

      case SYM_N64_SOFT_FLOAT:
        return FFI_N64_SOFT_FLOAT;
    #endif //X86_WIN64

      default:
        break;
    }

    fail (word);
}


//
//  register-struct-hooks: native [
//
//  {Make the STRUCT! datatype work with GENERIC actions, comparison ops, etc}
//
//      return: [void!]
//      generics "List for HELP of which generics are supported (unused)"
//          [block!]
//  ]
//
REBNATIVE(register_struct_hooks)
{
    FFI_INCLUDE_PARAMS_OF_REGISTER_STRUCT_HOOKS;

    Extend_Generics_Someday(ARG(generics));  // !!! vaporware, see comments

    // !!! See notes on Hook_Datatype for this poor-man's substitute for a
    // coherent design of an extensible object system (as per Lisp's CLOS)
    //
    EG_Struct_Type = Hook_Datatype(
        "http://datatypes.rebol.info/struct",
        "native structure definition",
        &T_Struct,
        &PD_Struct,
        &CT_Struct,
        &MAKE_Struct,
        &TO_Struct,
        &MF_Struct
    );

    return Init_Void(D_OUT);
}


//
//  unregister-struct-hooks: native [
//
//  {Remove behaviors for STRUCT! added by REGISTER-STRUCT-HOOKS}
//
//      return: [void!]
//  ]
//
REBNATIVE(unregister_struct_hooks)
{
    FFI_INCLUDE_PARAMS_OF_UNREGISTER_STRUCT_HOOKS;

    Unhook_Datatype(EG_Struct_Type);

    return Init_Void(D_OUT);
}


//
//  export make-routine: native [
//
//  {Create a bridge for interfacing with arbitrary C code in a DLL}
//
//      return: [action!]
//      lib [library!]
//          {Library DLL that C function lives in (get with MAKE LIBRARY!)}
//      name [text!]
//          {Linker name of the C function in the DLL}
//      ffi-spec [block!]
//          {Description of what C argument types the C function takes}
//      /abi [word!]
//          {Application Binary Interface ('CDECL, 'FASTCALL, 'STDCALL, etc.)}
//  ]
//
REBNATIVE(make_routine)
//
// !!! Would be nice if this could just take a filename and the lib management
// was automatic, e.g. no LIBRARY! type.
{
    FFI_INCLUDE_PARAMS_OF_MAKE_ROUTINE;

    ffi_abi abi;
    if (REF(abi))
        abi = Abi_From_Word(ARG(abi));
    else
        abi = FFI_DEFAULT_ABI;

    // Make sure library wasn't closed with CLOSE
    //
    REBLIB *lib = VAL_LIBRARY(ARG(lib));
    if (lib == nullptr)
        fail (PAR(lib));

    // Find_Function takes a char* on both Windows and Posix.
    //
    // !!! Should it error if any bytes aren't ASCII?
    //
    const REBYTE *utf8 = VAL_UTF8_AT(nullptr, ARG(name));

    CFUNC *cfunc = Find_Function(LIB_FD(lib), cast(char*, utf8));
    if (cfunc == nullptr)
        fail ("FFI: Couldn't find function in library");

    // Process the parameter types into a function, then fill it in

    REBACT *routine = Alloc_Ffi_Action_For_Spec(ARG(ffi_spec), abi);
    REBRIN *r = ACT_DETAILS(routine);

    Init_Handle_Cfunc(RIN_AT(r, IDX_ROUTINE_CFUNC), cfunc);
    Init_Blank(RIN_AT(r, IDX_ROUTINE_CLOSURE));
    Move_Value(RIN_AT(r, IDX_ROUTINE_ORIGIN), ARG(lib));

    return Init_Action_Unbound(D_OUT, routine);
}


//
//  export make-routine-raw: native [
//
//  {Create a bridge for interfacing with a C function, by pointer}
//
//      return: [action!]
//      pointer [integer!]
//          {Raw address of C function in memory}
//      ffi-spec [block!]
//          {Description of what C argument types the C function takes}
//      /abi [word!]
//          {Application Binary Interface ('CDECL, 'FASTCALL, 'STDCALL, etc.)}
//  ]
//
REBNATIVE(make_routine_raw)
//
// !!! Would be nice if this could just take a filename and the lib management
// was automatic, e.g. no LIBRARY! type.
{
    FFI_INCLUDE_PARAMS_OF_MAKE_ROUTINE_RAW;

    ffi_abi abi;
    if (REF(abi))
        abi = Abi_From_Word(ARG(abi));
    else
        abi = FFI_DEFAULT_ABI;

    // Cannot cast directly to a function pointer from a 64-bit value
    // on 32-bit systems.
    //
    CFUNC *cfunc = cast(CFUNC*, cast(uintptr_t, VAL_INT64(ARG(pointer))));
    if (cfunc == nullptr)
        fail ("FFI: nullptr pointer not allowed for raw MAKE-ROUTINE");

    REBACT *routine = Alloc_Ffi_Action_For_Spec(ARG(ffi_spec), abi);
    REBRIN *r = ACT_DETAILS(routine);

    Init_Handle_Cfunc(RIN_AT(r, IDX_ROUTINE_CFUNC), cfunc);
    Init_Blank(RIN_AT(r, IDX_ROUTINE_CLOSURE));
    Init_Blank(RIN_AT(r, IDX_ROUTINE_ORIGIN)); // no LIBRARY! in this case.

    return Init_Action_Unbound(D_OUT, routine);
}


//
//  export wrap-callback: native [
//
//  {Wrap an ACTION! so it can be called by raw C code via a memory address.}
//
//      return: [action!]
//      action [action!]
//          {The existing Rebol action whose behavior is being wrapped}
//      ffi-spec [block!]
//          {Description of what C types each Rebol argument should map to}
//      /abi [word!]
//          {Application Binary Interface ('CDECL, 'FASTCALL, 'STDCALL, etc.)}
//  ]
//
REBNATIVE(wrap_callback)
{
    FFI_INCLUDE_PARAMS_OF_WRAP_CALLBACK;

    ffi_abi abi;
    if (REF(abi))
        abi = Abi_From_Word(ARG(abi));
    else
        abi = FFI_DEFAULT_ABI;

    REBACT *callback = Alloc_Ffi_Action_For_Spec(ARG(ffi_spec), abi);
    REBRIN *r = ACT_DETAILS(callback);

    void *thunk; // actually CFUNC (FFI uses void*, may not be same size!)
    ffi_closure *closure = cast(ffi_closure*, ffi_closure_alloc(
        sizeof(ffi_closure), &thunk
    ));

    if (closure == nullptr)
        fail ("FFI: Couldn't allocate closure");

    ffi_status status = ffi_prep_closure_loc(
        closure,
        RIN_CIF(r),
        callback_dispatcher, // when thunk is called it calls this function...
        r, // ...and this piece of data is passed to callback_dispatcher
        thunk
    );

    if (status != FFI_OK)
        fail ("FFI: Couldn't prep closure");

    bool check = true; // avoid "conditional expression is constant"
    if (check && sizeof(void*) != sizeof(CFUNC*))
        fail ("FFI does not work when void* size differs from CFUNC* size");

    // It's the FFI's fault for using the wrong type for the thunk.  Use a
    // memcpy in order to get around strict checks that absolutely refuse to
    // let you do a cast here.
    //
    CFUNC *cfunc_thunk;
    memcpy(&cfunc_thunk, &thunk, sizeof(cfunc_thunk));

    Init_Handle_Cfunc(RIN_AT(r, IDX_ROUTINE_CFUNC), cfunc_thunk);
    Init_Handle_Cdata_Managed(
        RIN_AT(r, IDX_ROUTINE_CLOSURE),
        closure,
        sizeof(&closure),
        &cleanup_ffi_closure
    );
    Move_Value(RIN_AT(r, IDX_ROUTINE_ORIGIN), ARG(action));

    return Init_Action_Unbound(D_OUT, callback);
}


//
//  export addr-of: native [
//
//  {Get the memory address of an FFI STRUCT! or routine/callback}
//
//      return: [integer!]
//          {Memory address expressed as an up-to-64-bit integer}
//      value [action! struct!]
//          {Fixed address structure or routine to get the address of}
//  ]
//
REBNATIVE(addr_of) {
    FFI_INCLUDE_PARAMS_OF_ADDR_OF;

    REBVAL *v = ARG(value);

    if (IS_ACTION(v)) {
        if (not IS_ACTION_RIN(v))
            fail ("Can only take address of ACTION!s created though FFI");

        // The CFUNC is fabricated by the FFI if it's a callback, or
        // just the wrapped DLL function if it's an ordinary routine
        //
        REBRIN *rin = VAL_ACT_DETAILS(v);
        return Init_Integer(
            D_OUT, cast(intptr_t, RIN_CFUNC(rin))
        );
    }

    assert(IS_STRUCT(v));

    // !!! If a structure wasn't mapped onto "raw-memory" from the C,
    // then currently the data for that struct is a BINARY!, not a handle to
    // something which was malloc'd.  Much of the system is designed to be
    // able to handle memory relocations of a series data, but if a pointer is
    // given to code it may expect that address to be permanent.  Data
    // pointers currently do not move (e.g. no GC compaction) unless there is
    // a modification to the series, but this may change...in which case a
    // "do not move in memory" bit would be needed for the BINARY! or a
    // HANDLE! to a non-moving malloc would need to be used instead.
    //
    return Init_Integer(D_OUT, cast(intptr_t, VAL_STRUCT_DATA_AT(v)));
}


//
//  export make-similar-struct: native [
//
//  "Create a STRUCT! that reuses the underlying spec of another STRUCT!"
//
//      return: [struct!]
//      spec [struct!]
//          "Struct with interface to copy"
//      body [block! any-context! blank!]
//          "keys and values defining instance contents (bindings modified)"
//  ]
//
REBNATIVE(make_similar_struct)
//
// !!! Compatibility for `MAKE some-struct [...]` from Atronix R3.  There
// isn't any real "inheritance management" for structs, but it allows the
// re-use of the structure's field definitions, so it is a means of saving on
// memory (?)  Code retained for examination.
{
    FFI_INCLUDE_PARAMS_OF_MAKE_SIMILAR_STRUCT;

    REBVAL *spec = ARG(spec);
    REBVAL *body = ARG(body);

    Init_Struct(D_OUT, Copy_Struct_Managed(VAL_STRUCT(spec)));
    Init_Struct_Fields(D_OUT, body);
    return D_OUT;
}


//
//  destroy-struct-storage: native [
//
//  {Destroy the external memory associated the struct}
//
//      struct [struct!]
//      /free [action!]
//          {Specify the function to free the memory}
//  ]
//
REBNATIVE(destroy_struct_storage)
{
    FFI_INCLUDE_PARAMS_OF_DESTROY_STRUCT_STORAGE;

    if (IS_BINARY(VAL_STRUCT_DATA(ARG(struct))))
        fail (Error_No_External_Storage_Raw());

    RELVAL *handle = VAL_STRUCT_DATA(ARG(struct));

    DECLARE_LOCAL (pointer);
    Init_Integer(pointer, cast(intptr_t, VAL_HANDLE_POINTER(void, handle)));

    if (VAL_HANDLE_LEN(handle) == 0)
        fail (Error_Already_Destroyed_Raw(pointer));

    // TBD: assert handle length was correct for memory block size

    SET_HANDLE_LEN(handle, 0);

    if (REF(free)) {
        if (not IS_ACTION_RIN(ARG(free)))
            fail (Error_Free_Needs_Routine_Raw());

        rebElideQ(rebU1(ARG(free)), pointer, rebEND);
    }

    return nullptr;
}


//
//  export alloc-value-pointer: native [
//
//  {Persistently allocate a cell that can be referenced from FFI routines}
//
//      return: [integer!]
//      value [any-value!]
//          {Initial value for the cell}
//  ]
//
REBNATIVE(alloc_value_pointer)
//
// !!! Would it be better to not bother with the initial value parameter and
// just start the cell out blank?
{
    FFI_INCLUDE_PARAMS_OF_ALLOC_VALUE_POINTER;

    REBVAL *allocated = Move_Value(Alloc_Value(), ARG(value));
    rebUnmanage(allocated);

    return Init_Integer(D_OUT, cast(intptr_t, allocated));
}


//
//  export free-value-pointer: native [
//
//  {Free a cell that was allocated by ALLOC-VALUE-POINTER}
//
//      return: [<opt>]
//      pointer [integer!]
//  ]
//
REBNATIVE(free_value_pointer)
{
    FFI_INCLUDE_PARAMS_OF_FREE_VALUE_POINTER;

    REBVAL *cell = cast(REBVAL*, cast(intptr_t, VAL_INT64(ARG(pointer))));

    // Although currently unmanaged API handles are used, it would also be
    // possible to use a managed ones.
    //
    // Currently there's no way to make GC-visible references to the returned
    // pointer.  So the only value of using a managed strategy would be to
    // have the GC clean up leaks on exit instead of complaining in the
    // debug build.  For now, assume complaining is better.
    //
    rebFree(cell);

    return nullptr;
}


//
//  export get-at-pointer: native [
//
//  {Get the contents of a cell, e.g. one returned by ALLOC-VALUE-POINTER}
//
//      return: [<opt> any-value!]
//          {If the source looks up to a value, that value--else blank}
//      source [integer!]
//          {A pointer to a Rebol value}
//  ]
//
REBNATIVE(get_at_pointer)
//
// !!! In an ideal future, the FFI would probably add a user-defined-type
// for a POINTER!, and then GET could be overloaded to work with it.  No
// such mechanisms have been designed yet.  In the meantime, the interface
// for GET-AT-POINTER should not deviate too far from GET.
//
// !!! Alloc_Value() doesn't currently prohibit nulled cells mechanically,
// but libRebol doesn't allow them.  What should this API do?
{
    FFI_INCLUDE_PARAMS_OF_GET_AT_POINTER;

    REBVAL *cell = cast(REBVAL*, cast(intptr_t, VAL_INT64(ARG(source))));

    Move_Value(D_OUT, cell);
    return D_OUT; // don't return `cell` (would do a rebRelease())
}


//
//  export set-at-pointer: native [
//
//  {Set the contents of a cell, e.g. one returned by ALLOC-VALUE-POINTER}
//
//      return: [<opt> any-value!]
//          {Will be the value set to, or nullptr if the set value is nullptr}
//      target [integer!]
//          {A pointer to a Rebol value}
//      value [<opt> any-value!]
//          "Value to assign"
//      /opt
//          {Treat nulls as unsetting the target instead of an error}
//  ]
//
REBNATIVE(set_at_pointer)
//
// !!! See notes on GET-AT-POINTER about keeping interface roughly compatible
// with the SET native.
{
    FFI_INCLUDE_PARAMS_OF_SET_AT_POINTER;

    REBVAL *v = ARG(value);

    if (IS_NULLED(v) and not REF(opt))
        fail (Error_No_Value(v));

    REBVAL *cell = cast(REBVAL*, cast(intptr_t, VAL_INT64(ARG(target))));
    Move_Value(cell, v);

    RETURN (ARG(value)); // Returning cell would rebRelease()
}
