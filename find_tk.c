#include <Python.h>
#include <tk.h>

// To ensure correct resolution of symbols, add Psapi.lib to TARGETLIBS
// and compile with -DPSAPI_VERSION=1

// Load TCL / Tk symbols from tkinter extension module at run-time.
// Typedefs, global vars for TCL / Tk library functions.
typedef Tcl_Command (*tcl_cc)(Tcl_Interp *interp,
        const char *cmdName, Tcl_CmdProc *proc,
        ClientData clientData,
        Tcl_CmdDeleteProc *deleteProc);
static tcl_cc TCL_CREATE_COMMAND;
typedef void (*tcl_app_res) (Tcl_Interp *interp, ...);
static tcl_app_res TCL_APPEND_RESULT;
typedef Tk_Window (*tk_mw) (Tcl_Interp *interp);
static tk_mw TK_MAIN_WINDOW;
typedef Tk_PhotoHandle (*tk_fp) (Tcl_Interp *interp, const char *imageName);
static tk_fp TK_FIND_PHOTO;
typedef void (*tk_ppb_nc) (Tk_PhotoHandle handle,
        Tk_PhotoImageBlock *blockPtr, int x, int y,
        int width, int height);
static tk_ppb_nc TK_PHOTO_PUTBLOCK;
typedef void (*tk_pb) (Tk_PhotoHandle handle);
static tk_pb TK_PHOTO_BLANK;

// Functions to fill global TCL / Tk function pointers from tkinter module.
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)

/*
 * On Windows, we can't load the tkinter module to get the TCL or Tk symbols,
 * because Windows does not load symbols into the library name-space of
 * importing modules. So, knowing that tkinter has already been imported by
 * Python, we scan all modules in the running process for the TCL and Tk
 * function names.
 */
#include <windows.h>
#define PSAPI_VERSION 1
#include <psapi.h>
// Must be linked with 'psapi' library

FARPROC _dfunc(HMODULE lib_handle, const char *func_name)
{
    // Load function `func_name` from `lib_handle`.
    // Set Python exception if we can't find `func_name` in `lib_handle`.
    // Returns function pointer or NULL if not present.

    char message[100];

    FARPROC func = GetProcAddress(lib_handle, func_name);
    if (func == NULL) {
        sprintf(message, "Cannot load function %s", func_name);
        PyErr_SetString(PyExc_RuntimeError, message);
    }
    return func;
}

int get_tcl(HMODULE hMod)
{
    // Try to fill TCL global vars with function pointers. Return 0 for no
    // functions found, 1 for all functions found, -1 for some but not all
    // functions found.
    TCL_CREATE_COMMAND = (tcl_cc) GetProcAddress(hMod, "Tcl_CreateCommand");
    if (TCL_CREATE_COMMAND == NULL) {  // Maybe not TCL module
        return 0;
    }
    TCL_APPEND_RESULT = (tcl_app_res) _dfunc(hMod, "Tcl_AppendResult");
    return (TCL_APPEND_RESULT == NULL) ? -1 : 1;
}

int get_tk(HMODULE hMod)
{
    // Try to fill Tk global vars with function pointers. Return 0 for no
    // functions found, 1 for all functions found, -1 for some but not all
    // functions found.
    TK_MAIN_WINDOW = (tk_mw) GetProcAddress(hMod, "Tk_MainWindow");
    if (TK_MAIN_WINDOW == NULL) {  // Maybe not Tk module
        return 0;
    }
    return ( // -1 if any are NULL
        ((TK_FIND_PHOTO = (tk_fp) _dfunc(hMod, "Tk_FindPhoto")) == NULL) ||
        ((TK_PHOTO_PUTBLOCK = (tk_ppb_nc)
          _dfunc(hMod, "Tk_PhotoPutBlock_NoComposite")) == NULL) ||
        ((TK_PHOTO_BLANK = (tk_pb) _dfunc(hMod, "Tk_PhotoBlank")) == NULL))
        ? -1 : 1;
}

int load_tkinter_funcs(void)
{
    // Load TCL and Tk functions by searching all modules in current process.
    // Return 0 for success, non-zero for failure.

    HMODULE hMods[1024];
    HANDLE hProcess;
    DWORD cbNeeded;
    unsigned int i;
    int found_tcl = 0;
    int found_tk = 0;

    // Returns pseudo-handle that does not need to be closed
    hProcess = GetCurrentProcess();

    // Iterate through modules in this process looking for TCL / Tk names
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            if (!found_tcl) {
                found_tcl = get_tcl(hMods[i]);
                if (found_tcl == -1) {
                    return 1;
                }
            }
            if (!found_tk) {
                found_tk = get_tk(hMods[i]);
                if (found_tk == -1) {
                    return 1;
                }
            }
            if (found_tcl && found_tk) {
                return 0;
            }
        }
    }

    if (found_tcl == 0) {
        PyErr_SetString(PyExc_RuntimeError, "Could not find TCL routines");
    } else {
        PyErr_SetString(PyExc_RuntimeError, "Could not find Tk routines");
    }
    return 1;
}

#else  // not Windows

/*
 * On Unix, we can get the TCL and Tk synbols from the tkinter module, because
 * tkinter uses these symbols, and the symbols are therefore visible in the
 * tkinter dynamic library (module).
 */
#if PY3K
#define TKINTER_PKG "tkinter"
#define TKINTER_MOD "_tkinter"
// From module __file__ attribute to char *string for dlopen.
char *fname2char(PyObject *fname)
{
    PyObject *bytes = PyUnicode_EncodeFSDefault(fname);
    if (bytes == NULL) {
        return NULL;
    }
    return PyBytes_AsString(bytes);
}
#else
#define TKINTER_PKG "Tkinter"
#define TKINTER_MOD "tkinter"
// From module __file__ attribute to char *string for dlopen
#define fname2char(s) (PyString_AsString(s))
#endif

#include <dlfcn.h>

void *_dfunc(void *lib_handle, const char *func_name)
{
    // Load function `func_name` from `lib_handle`.
    // Set Python exception if we can't find `func_name` in `lib_handle`.
    // Returns function pointer or NULL if not present.

    // Reset errors.
    dlerror();
    void *func = dlsym(lib_handle, func_name);
    if (func == NULL) {
        const char *error = dlerror();
        PyErr_SetString(PyExc_RuntimeError, error);
    }
    return func;
}

int _func_loader(void *lib)
{
    // Fill global function pointers from dynamic lib.
    // Return 1 if any pointer is NULL, 0 otherwise.
    return (
         ((TCL_CREATE_COMMAND = (tcl_cc)
           _dfunc(lib, "Tcl_CreateCommand")) == NULL) ||
         ((TCL_APPEND_RESULT = (tcl_app_res)
           _dfunc(lib, "Tcl_AppendResult")) == NULL) ||
         ((TK_MAIN_WINDOW = (tk_mw)
           _dfunc(lib, "Tk_MainWindow")) == NULL) ||
         ((TK_FIND_PHOTO = (tk_fp)
           _dfunc(lib, "Tk_FindPhoto")) == NULL) ||
         ((TK_PHOTO_PUTBLOCK = (tk_ppb_nc)
           _dfunc(lib, "Tk_PhotoPutBlock_NoComposite")) == NULL) ||
         ((TK_PHOTO_BLANK = (tk_pb)
           _dfunc(lib, "Tk_PhotoBlank")) == NULL));
}

int load_tkinter_funcs(void)
{
    // Load tkinter global funcs from tkinter compiled module.
    // Return 0 for success, non-zero for failure.
    int ret = -1;
    void *tkinter_lib;
    char *tkinter_libname;
    PyObject *pModule = NULL, *pSubmodule = NULL, *pString = NULL;

    pModule = PyImport_ImportModule(TKINTER_PKG);
    if (pModule == NULL) {
        goto exit;
    }
    pSubmodule = PyObject_GetAttrString(pModule, TKINTER_MOD);
    if (pSubmodule == NULL) {
        goto exit;
    }
    pString = PyObject_GetAttrString(pSubmodule, "__file__");
    if (pString == NULL) {
        goto exit;
    }
    tkinter_libname = fname2char(pString);
    if (tkinter_libname == NULL) {
        goto exit;
    }
    tkinter_lib = dlopen(tkinter_libname, RTLD_LAZY);
    if (tkinter_lib == NULL) {
        PyErr_SetString(PyExc_RuntimeError,
                "Cannot dlopen tkinter module file");
        goto exit;
    }
    ret = _func_loader(tkinter_lib);
    // dlclose probably safe because tkinter has been imported.
    dlclose(tkinter_lib);
exit:
    Py_XDECREF(pModule);
    Py_XDECREF(pSubmodule);
    Py_XDECREF(pString);
    return ret;
}
#endif // end not Windows

