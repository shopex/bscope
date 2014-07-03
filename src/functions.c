#include <sys/errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

void errExit(const char *format, ...) {
    va_list argList;
    va_start(argList, format);
    char buf[512];
    vsprintf(buf, format, argList);
    fputs(buf, 2);
    va_end(argList);
    exit(1);
}

static int
lockReg(int fd, int cmd, int type, int whence, int start, off_t len)
{
    struct flock fl;

    fl.l_type = type;
    fl.l_whence = whence;
    fl.l_start = start;
    fl.l_len = len;

    return fcntl(fd, cmd, &fl);
}

int                     /* Lock a file region using nonblocking F_SETLK */
lockRegion(int fd, int type, int whence, int start, int len)
{
    return lockReg(fd, F_SETLK, type, whence, start, len);
}

int createPidFile(const char *progName, const char *pidFile, int flags) {
    int fd;
    char buf[512];
    fd = open(pidFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
        errExit("Could not open PID file %s", pidFile);
    if (flags & 1) {
        /* Set the close-on-exec file descriptor flag */
        flags = fcntl(fd, F_GETFD);                     /* Fetch flags */
        if (flags == -1)
            errExit("Could not get flags for PID file %s", pidFile);
        flags |= FD_CLOEXEC;                            /* Turn on FD_CLOEXEC */
        if (fcntl(fd, F_SETFD, flags) == -1)            /* Update flags */
            errExit("Could not set flags for PID file %s", pidFile);
    }
    if (lockRegion(fd, F_WRLCK, 0, 0, 0) == -1) {
        if (errno  == EAGAIN || errno == EACCES){
            errExit("PID file '%s' is locked; probably "
                    "'%s' is already running", pidFile, progName);
        }else{
            errExit("Unable to lock PID file '%s'", pidFile);
        }
    }
    if (ftruncate(fd, 0) == -1)
        errExit("Could not truncate PID file '%s'", pidFile);
    snprintf(buf, 512, "%ld\n", (long) getpid());
    if (write(fd, buf, strlen(buf)) != strlen(buf))
        errExit("Writing to PID file '%s'", pidFile);
    return fd;
}
