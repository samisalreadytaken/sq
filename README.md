## Build

Clone all repositories and run the appropriate build script for your system with optional arguments.

```
git clone https://github.com/albertodemichelis/squirrel
git clone https://github.com/samisalreadytaken/sqdbg
git clone https://github.com/samisalreadytaken/sq
cd sq
build
```

For [libffi](https://github.com/libffi/libffi) support on Windows, put binaries in `libffi/win64/lib/` and headers in `libffi/win64/include/` (or `win32` for 32-bit). On other platforms, install `libffi-dev` or build it as needed.

```
sudo apt install libffi-dev
./build.sh ffi
```

## Usage

Use `sq -h` and `sq --help` to view help.

```
$ sq -x 0xAA,bb
10101010

$ sq -x 13 -x 37
1337

$ echo 97,cx | sq -
0x61 'a'

$ sq --exec=1000 --server=1212 --exec=2000 --repl
1000(sqdbg) Listening for connections on port 1212
2000
->

$ echo "return {a = 1, b = [2, {[3] = 4, c = 5}, 6], d = 7}" > t.nut
$ sq t.nut
{
    a = 1,
    b = [2, {[3] = 4, c = 5}, 6],
    d = 7
}

$ sq --json --file t.nut
{
    "a": 1,
    "b": [
        2,
        {
            3: 4,
            "c": 5
        },
        6
    ],
    "d": 7
}

$ sq --json --oneline --file t.nut
{"a": 1, "b": [2, {3: 4, "c": 5}, 6], "d": 7}
```

### FFI example

<details open><summary>Calling Windows <code>MessageBox</code></summary>

```cs
if ( _WIN32 )
{
    local user32_dll = ffi.LoadLibrary( "user32.dll" );
    MessageBox <- ffi.GetFunction( user32_dll,
        _charsize_ == 1 ?
            "int MessageBoxA( HANDLE, LPCSTR lpText, LPCSTR lpCaption, UINT uType )" :
            "int MessageBoxW( HANDLE, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType )" );
    local ret = MessageBox( null, "message", "title", 0 );
    printl( "ret:", ret );
}
```

</details>

<details><summary>Setting and calling native functions from script</summary>

```cs
ffi.typedef(@"
    struct functions_t
    {
        typedef int (*FN_INT_INT)(int, int);
        FN_INT_INT add;
        FN_INT_INT sub;
    };
");

local funcs = ffi.malloc( "struct functions_t" );
funcs.add = function( a, b ) { return a + b; }
funcs.sub = function( a, b ) { return a - b; }

assert( funcs.add( 1, 2 ) == 3 );
assert( funcs.sub( 1, 2 ) == -1 );
```

</details>

<details><summary>Multithreading</summary>

```cs
if ( _WIN32 )
{
    local kernel32_dll = ffi.LoadLibrary( "kernel32.dll" );
    ffi.GetFunctions( this, kernel32_dll, @"
        [[sq::GetLastError]]
        DWORD WaitForSingleObject( HANDLE hHandle, DWORD dwMilliseconds );

        [[sq::GetLastError]]
        BOOL CloseHandle( HANDLE hObject );

        typedef struct _SECURITY_ATTRIBUTES {
            DWORD  nLength;
            LPVOID lpSecurityDescriptor;
            BOOL   bInheritHandle;
        } SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

        typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)( LPVOID lpParameter );

        [[sq::GetLastError]]
        HANDLE CreateThread(
            LPSECURITY_ATTRIBUTES   lpThreadAttributes,
            SIZE_T                  dwStackSize,
            LPTHREAD_START_ROUTINE  lpStartAddress,
            LPVOID                  lpParameter,
            DWORD                   dwCreationFlags,
            LPDWORD                 lpThreadId
        );
    " );
}
else
{
    local libc_so = ffi.LoadLibrary( "libc.so.6" );
    ffi.GetFunctions( this, libc_so, @"
        // Define according to your system
        typedef void *pthread_t;

        typedef void (*THREADSTARTROUTINE)(void*);

        int pthread_create(
            pthread_t *thread,
            const pthread_attr_t *attr,
            THREADSTARTROUTINE start_routine,
            void *arg );
        int pthread_join( pthread_t thread, void **retval );
    " );
}

local ThreadMainLargeStack = function()
{
    local a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54, a55, a56, a57, a58, a59, a60, a61, a62, a63;
}

local threadcount = 16;

local closures = array( threadcount );
local pResults = ffi.malloc( format( "double[%d]", threadcount ) );
local pThreads = ffi.malloc( format( _WIN32 ? "HANDLE[%d]" : "pthread_t[%d]", threadcount ) );

for ( local i = 0; i < threadcount; ++i )
{
    // Random data for each thread to do work with
    local dataset = array( 1024 );
    foreach ( j, v in dataset )
        dataset[j] = rand().tofloat();

    local sin = ffi.Clone( sin );
    local ffi_set = ffi.Clone( ffi.set );
    local ffi_t_f64 = ffi.t_f64;

    // Thread functions accessing reference counted Squirrel objects would cause race conditions;
    // clone external functions and do not access any strings.
    // Use `sqdbg_disassemble` to confirm no string is used within the function.
    // Profiling requires compiling with SQDBG_DISABLE_PROFILER_AUTO and using `sqdbg_prof_begin/end`
    // outside of thread functions
    local ThreadFunc = function( pData )
    {
        // Random work
        local result = 0.0;
        local len = 1024;
        for ( local x = 100000; x--; )
            result += sin( dataset[x % len] ) * 3.14159265;

        // Set result to external address
        // Alternatively, thread unique Squirrel arrays can be used
        ffi_set( ffi_t_f64, pData, result );
        return 0;
    }

    local closure = closures[i] = ffi.MakeClosure(
        _WIN32 ? "LPTHREAD_START_ROUTINE" : "THREADSTARTROUTINE",
        ThreadFunc,
        // Squirrel threads should have enough stack size
        newthread( ThreadMainLargeStack ) );

    if ( _WIN32 )
    {
        local res = pThreads[i] = CreateThread(
            null,
            0,
            closure,
            pResults.ptr + i * pResults.align,
            0,
            0 );

        if ( !res )
            printl( "failed to create thread", i, "err:", ffi.GetLastError() );
    }
    else
    {
        local res = pthread_create(
                pThreads.ptr + i * pThreads.align,
                null,
                closure,
                pResults.ptr + i * pResults.align );

        if ( res )
            printl( "failed to create thread", i );
    }
}

for ( local i = 0; i < threadcount; ++i )
{
    printl( "wait", i );

    if ( _WIN32 )
    {
        const INFINITE = -1;
        WaitForSingleObject( pThreads[i], INFINITE );
        CloseHandle( pThreads[i] );
    }
    else
    {
        pthread_join( pThreads[i], null );
    }
}

printl( "results:" );
for ( local i = 0; i < threadcount; ++i )
    printl( "\t", i, "\t", pResults[i] );
```

</details>

<details><summary>Dump memory</summary>

```cs
function DumpMemory( ptr, size, width = 8 )
{
    try
    {
        if ( width <= 0 )
            width = 16;

        local border =
            "+--------------+-" + sqdbg_eval("\"---\"*width") + "-+-" + sqdbg_eval("\"-\"*width") + "+\n";
        print( border );
        ptr += 0;

        for ( local i = 0; i < size; )
        {
            local end = i + width;
            local pad = 0;

            if ( end > size )
            {
                pad = end - size;
                end = size;
            }

            printf( " 0x%X:  ", ptr + i );

            for ( local j = i; j < end; ++j )
                printf( "%02X ", ffi.get( ffi.t_u8, ptr + j ) );

            if ( pad )
                print( sqdbg_eval("\"   \"*pad") );

            print( "   " );

            for ( local j = i; j < end; ++j )
            {
                local v = ffi.get( ffi.t_u8, ptr + j );
                if ( v >= 0x20 && v <= 0x7E )
                {
                    print( v.tochar() );
                }
                else
                {
                    print( "." );
                }
            }

            print( "\n" );
            i = end;
        }

        return print( border );
        "---";"-";
    }
    catch ( exception )
    {
        printl( "\n", exception );
    }
}
```

</details>

## Licence

MIT, see [LICENSE](LICENSE).
