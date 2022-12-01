#include <stdlib.h>
#include <i86.h>
#include <string.h>
#include <stdio.h>
#include <direct.h>
#include <process.h>

// strip executable file name from path
const char* fname_strip(const char *exepath, const char *cwd) {
    const char* p = exepath + strlen(exepath);
    while ((p >= exepath) && (*p != '\\')) p--;

    // alright, check if cwd equals exe dir
    const char *pp = p - 1, *execmp = exepath;
    while ((*execmp++ == *cwd++) && (execmp <= pp));

    return (execmp == p) ? ++p : exepath;
}

int main(int argc, char *argv[]) {
    // path argv[1]
    char **new_argv; char argv1buf[1024];
    memset(argv1buf, 0, sizeof(argv1buf));
    snprintf(argv1buf, sizeof(argv1buf), "%s %s", fname_strip(argv[0], getcwd(0,0)), (argc < 2) ? "" : argv[1]);

    char *temp_argv[2] = {argv[0], argv1buf};
    new_argv = (argc >= 2) ? argv : temp_argv;
    new_argv[1] = argv1buf;

    // pass to JLOAD.EXE
    int rtn = execv("JLOAD.EXE", new_argv);
    if (rtn == -1) {
        printf("error: unable to run or locate JLOAD.exe, aborting\n");
    }

    return 0;
}