/* RETRO --------------------------------------------------------------
  A personal, minimalistic forth
  Copyright (c) 2016 - 2018 Charles Childers

  This is `rre`, short for `run retro and exit`. It's the basic
  interface layer for Retro on FreeBSD, Linux and macOS.

  rre embeds the image file into the binary, so the compiled version
  of this is all you need to have a functional system.

  I'll include commentary throughout the source, so read on.
  ---------------------------------------------------------------------*/


/*---------------------------------------------------------------------
  RRE provides numerous extensions to RETRO. Support for these can be
  enabled/disabled here. (Note that this won't remove the words, just
  support for them. You should also edit `rre.forth` to remove anything
  you don't want/need after making changes here).
  ---------------------------------------------------------------------*/

#define ENABLE_FILES
#define ENABLE_FLOATING_POINT
#define ENABLE_UNIX
#define ENABLE_GOPHER


/*---------------------------------------------------------------------
  Begin by including the various C headers needed.
  ---------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>


/*---------------------------------------------------------------------
  First, a few constants relating to the image format and memory
  layout. If you modify the kernel (Rx.md), these will need to be
  altered to match your memory layout.
  ---------------------------------------------------------------------*/

#define TIB            1025
#define D_OFFSET_LINK     0
#define D_OFFSET_XT       1
#define D_OFFSET_CLASS    2
#define D_OFFSET_NAME     3



/*---------------------------------------------------------------------
  Next we get into some things that relate to the Nga virtual machine
  that RETRO runs on.
  ---------------------------------------------------------------------*/

#define CELL         int32_t      /* Cell size (32 bit, signed integer */
#define IMAGE_SIZE   524288 * 16  /* Amount of RAM. 8MiB by default.   */
#define ADDRESSES    2048         /* Depth of address stack            */
#define STACK_DEPTH  512          /* Depth of data stack               */

CELL sp, rp, ip;                  /* Data, address, instruction pointers */
CELL data[STACK_DEPTH];           /* The data stack                    */
CELL address[ADDRESSES];          /* The address stack                 */
CELL memory[IMAGE_SIZE + 1];      /* The memory for the image          */

#define TOS  data[sp]             /* Shortcut for top item on stack    */
#define NOS  data[sp-1]           /* Shortcut for second item on stack */
#define TORS address[rp]          /* Shortcut for top item on address stack */


/*---------------------------------------------------------------------
  Moving forward, a few variables. These are updated to point to the
  latest values in the image.
  ---------------------------------------------------------------------*/

CELL Dictionary;
CELL NotFound;
CELL interpret;


/*---------------------------------------------------------------------
  Function prototypes.
  ---------------------------------------------------------------------*/

CELL stack_pop();
void stack_push(CELL value);
int string_inject(char *str, int buffer);
char *string_extract(int at);
int d_link(CELL dt);
int d_xt(CELL dt);
int d_class(CELL dt);
int d_name(CELL dt);
int d_lookup(CELL Dictionary, char *name);
CELL d_xt_for(char *Name, CELL Dictionary);
CELL ioGetFileHandle();
CELL ioOpenFile();
CELL ioReadFile();
CELL ioWriteFile();
CELL ioCloseFile();
CELL ioGetFilePosition();
CELL ioSetFilePosition();
CELL ioGetFileSize();
CELL ioDeleteFile();
void ioFlushFile();
void update_rx();
void execute(int cell);
void evaluate(char *s);
int not_eol(int ch);
void read_token(FILE *file, char *token_buffer, int echo);
char *read_token_str(char *s, char *token_buffer, int echo);
void include_file(char *fname);
void ngaGopherUnit();
void ngaFloatingPointUnit();
CELL ngaLoadImage(char *imageFile);
void ngaPrepare();
void ngaProcessOpcode(CELL opcode);
void ngaProcessPackedOpcodes(int opcode);
int ngaValidatePackedOpcodes(CELL opcode);


char **sys_argv;
int sys_argc;

/* Some I/O Parameters */

#define MAX_OPEN_FILES   128
#define IO_TTY_PUTC  1000
#define IO_TTY_GETC  1001
#define IO_FS_OPEN    118
#define IO_FS_CLOSE   119
#define IO_FS_READ    120
#define IO_FS_WRITE   121
#define IO_FS_TELL    122
#define IO_FS_SEEK    123
#define IO_FS_SIZE    124
#define IO_FS_DELETE  125
#define IO_FS_FLUSH   126


/*---------------------------------------------------------------------
  Now to the fun stuff: interfacing with the virtual machine. There are
  a things I like to have here:

  - push a value to the stack
  - pop a value off the stack
  - extract a string from the image
  - inject a string into the image.
  - lookup dictionary headers and access dictionary fields
  ---------------------------------------------------------------------*/


/*---------------------------------------------------------------------
  Stack push/pop is easy. I could avoid these, but it aids in keeping
  the code readable, so it's worth the slight overhead.
  ---------------------------------------------------------------------*/

CELL stack_pop() {
  sp--;
  return data[sp + 1];
}

void stack_push(CELL value) {
  sp++;
  data[sp] = value;
}


/*---------------------------------------------------------------------
  Strings are next. RETRO uses C-style NULL terminated strings. So I
  can easily inject or extract a string. Injection iterates over the
  string, copying it into the image. This also takes care to ensure
  that the NULL terminator is added.
  ---------------------------------------------------------------------*/

int string_inject(char *str, int buffer) {
  if (!str) {
    memory[buffer] = 0;
    return 0;
  }
  int m = strlen(str);
  int i = 0;
  while (m > 0) {
    memory[buffer + i] = (CELL)str[i];
    memory[buffer + i + 1] = 0;
    m--; i++;
  }
  return buffer;
}


/*---------------------------------------------------------------------
  Extracting a string is similar, but I have to iterate over the VM
  memory instead of a C string and copy the charaters into a buffer.
  This uses a static buffer (`string_data`) as I prefer to avoid using
  `malloc()`.
  ---------------------------------------------------------------------*/

char string_data[8192];
char *string_extract(int at) {
  CELL starting = at;
  CELL i = 0;
  while(memory[starting] && i < 8192)
    string_data[i++] = (char)memory[starting++];
  string_data[i] = 0;
  return (char *)string_data;
}


/*---------------------------------------------------------------------
  Continuing along, I now define functions to access the dictionary.

  RETRO's dictionary is a linked list. Each entry is setup like:

  0000  Link to previous entry (NULL if this is the root entry)
  0001  Pointer to definition start
  0002  Pointer to class handler
  0003  Start of a NULL terminated string with the word name

  First, functions to access each field. The offsets were defineed at
  the start of the file.
  ---------------------------------------------------------------------*/

int d_link(CELL dt) {
  return dt + D_OFFSET_LINK;
}

int d_xt(CELL dt) {
  return dt + D_OFFSET_XT;
}

int d_class(CELL dt) {
  return dt + D_OFFSET_CLASS;
}

int d_name(CELL dt) {
  return dt + D_OFFSET_NAME;
}


/*---------------------------------------------------------------------
  Next, a more complext word. This will walk through the entries to
  find one with a name that matches the specified name. This is *slow*,
  but works ok unless you have a really large dictionary. (I've not
  run into issues with this in practice).
  ---------------------------------------------------------------------*/

int d_lookup(CELL Dictionary, char *name) {
  CELL dt = 0;
  CELL i = Dictionary;
  char *dname;
  while (memory[i] != 0 && i != 0) {
    dname = string_extract(d_name(i));
    if (strcmp(dname, name) == 0) {
      dt = i;
      i = 0;
    } else {
      i = memory[i];
    }
  }
  return dt;
}


/*---------------------------------------------------------------------
  My last dictionary related word returns the `xt` pointer for a word.
  This is used to help keep various important bits up to date.
  ---------------------------------------------------------------------*/

CELL d_xt_for(char *Name, CELL Dictionary) {
  return memory[d_xt(d_lookup(Dictionary, Name))];
}


/*---------------------------------------------------------------------
  This interface tracks a few words and variables in the image. These
  are:

  Dictionary - the latest dictionary header
  NotFound   - called when a word is not found
  interpret  - the heart of the interpreter/compiler

  I have to call this periodically, as the Dictionary will change as
  new words are defined, and the user might write a new error handler
  or interpreter.
  ---------------------------------------------------------------------*/

void update_rx() {
  Dictionary = memory[2];
  interpret = d_xt_for("interpret", Dictionary);
  NotFound = d_xt_for("err:notfound", Dictionary);
}


/*---------------------------------------------------------------------
  Now on to I/O and extensions!

  RRE provides a lot of additional functionality over the base RETRO
  system. First up is support for files.

  The RRE file model is intended to be similar to that of the standard
  C libraries and wraps fopen(), fclose(), etc.
  ---------------------------------------------------------------------*/

#ifdef ENABLE_FILES

/*---------------------------------------------------------------------
  I keep an array of file handles. RETRO will use the index number as
  its representation of the file.
  ---------------------------------------------------------------------*/

FILE *ioFileHandles[MAX_OPEN_FILES];


/*---------------------------------------------------------------------
  `ioGetFileHandle()` returns a file handle, or 0 if there are no
  available handle slots in the array.
  ---------------------------------------------------------------------*/

CELL ioGetFileHandle() {
  CELL i;
  for(i = 1; i < MAX_OPEN_FILES; i++)
    if (ioFileHandles[i] == 0)
      return i;
  return 0;
}


/*---------------------------------------------------------------------
  `ioOpenFile()` opens a file. This pulls from the RETRO data stack:

  - mode     (number, TOS)
  - filename (string, NOS)

  Modes are:

  | Mode | Corresponds To | Description          |
  | ---- | -------------- | -------------------- |
  |  0   | rb             | Open for reading     |
  |  1   | w              | Open for writing     |
  |  2   | a              | Open for append      |
  |  3   | rb+            | Open for read/update |

  The file name should be a NULL terminated string. This will attempt
  to open the requested file and will return a handle (index number
  into the `ioFileHandles` array).
  ---------------------------------------------------------------------*/

CELL ioOpenFile() {
  CELL slot, mode, name;
  slot = ioGetFileHandle();
  mode = data[sp]; sp--;
  name = data[sp]; sp--;
  char *request = string_extract(name);
  if (slot > 0) {
    if (mode == 0)  ioFileHandles[slot] = fopen(request, "rb");
    if (mode == 1)  ioFileHandles[slot] = fopen(request, "w");
    if (mode == 2)  ioFileHandles[slot] = fopen(request, "a");
    if (mode == 3)  ioFileHandles[slot] = fopen(request, "rb+");
  }
  if (ioFileHandles[slot] == NULL) {
    ioFileHandles[slot] = 0;
    slot = 0;
  }
  stack_push(slot);
  return slot;
}


/*---------------------------------------------------------------------
  `ioReadFile()` reads a byte from a file. This takes a file pointer
  from the stack and pushes the character that was read to the stack.
  ---------------------------------------------------------------------*/

CELL ioReadFile() {
  CELL slot = stack_pop();
  CELL c = fgetc(ioFileHandles[slot]);
  return feof(ioFileHandles[slot]) ? 0 : c;
}


/*---------------------------------------------------------------------
  `ioWriteFile()` writes a byte to a file. This takes a file pointer
  (TOS) and a byte (NOS) from the stack. It does not return any values
  on the stack.
  ---------------------------------------------------------------------*/

CELL ioWriteFile() {
  CELL slot, c, r;
  slot = data[sp]; sp--;
  c = data[sp]; sp--;
  r = fputc(c, ioFileHandles[slot]);
  return (r == EOF) ? 0 : 1;
}


/*---------------------------------------------------------------------
  `ioCloseFile()` closes a file. This takes a file handle from the
  stack and does not return anything on the stack.
  ---------------------------------------------------------------------*/

CELL ioCloseFile() {
  fclose(ioFileHandles[data[sp]]);
  ioFileHandles[data[sp]] = 0;
  sp--;
  return 0;
}


/*---------------------------------------------------------------------
  `ioGetFilePosition()` provides the current index into a file. This
  takes the file handle from the stack and returns the offset.
  ---------------------------------------------------------------------*/

CELL ioGetFilePosition() {
  CELL slot = data[sp]; sp--;
  return (CELL) ftell(ioFileHandles[slot]);
}


/*---------------------------------------------------------------------
  `ioSetFilePosition()` changes the current index into a file to the
  specified one. This takes a file handle (TOS) and new offset (NOS)
  from the stack.
  ---------------------------------------------------------------------*/

CELL ioSetFilePosition() {
  CELL slot, pos, r;
  slot = data[sp]; sp--;
  pos  = data[sp]; sp--;
  r = fseek(ioFileHandles[slot], pos, SEEK_SET);
  return r;
}


/*---------------------------------------------------------------------
  `ioGetFileSize()` returns the size of a file, or 0 if empty. If the
  file is a directory, it returns -1. It takes a file handle from the
  stack.
  ---------------------------------------------------------------------*/

CELL ioGetFileSize() {
  CELL slot, current, r, size;
  slot = data[sp]; sp--;
  struct stat buffer;
  int    status;
  status = fstat(fileno(ioFileHandles[slot]), &buffer);
  if (!S_ISDIR(buffer.st_mode)) {
    current = ftell(ioFileHandles[slot]);
    r = fseek(ioFileHandles[slot], 0, SEEK_END);
    size = ftell(ioFileHandles[slot]);
    fseek(ioFileHandles[slot], current, SEEK_SET);
  } else {
    r = -1;
  }
  return (r == 0) ? size : 0;
}


/*---------------------------------------------------------------------
  `ioDeleteFile()` removes a file. This takes a file name (as a string)
  from the stack.
  ---------------------------------------------------------------------*/

CELL ioDeleteFile() {
  CELL name = data[sp]; sp--;
  char *request = string_extract(name);
  return (unlink(request) == 0) ? -1 : 0;
}


/*---------------------------------------------------------------------
  `ioFlushFile()` flushes any pending writes to disk. This takes a
  file handle from the stack.
  ---------------------------------------------------------------------*/

void ioFlushFile() {
  CELL slot;
  slot = data[sp]; sp--;
  fflush(ioFileHandles[slot]);
}

#endif


/*---------------------------------------------------------------------
  UNIX. Or Linux. Or BSD. Or whatever. This section adds functions for
  interacting with the host OS. It's tested on FreeBSD and Linux, but
  likely won't work on Windows or any other host not supporting POSIX.
  ---------------------------------------------------------------------*/

#ifdef ENABLE_UNIX

/*---------------------------------------------------------------------
  First step is to define the instruction values for each of these.
  ---------------------------------------------------------------------*/

#define UNIX_SYSTEM -8000
#define UNIX_FORK   -8001
#define UNIX_EXIT   -8002
#define UNIX_GETPID -8003
#define UNIX_EXEC0  -8004	
#define UNIX_EXEC1  -8005
#define UNIX_EXEC2  -8006
#define UNIX_EXEC3  -8007
#define UNIX_WAIT   -8008
#define UNIX_KILL   -8009
#define UNIX_POPEN  -8010
#define UNIX_PCLOSE -8011
#define UNIX_WRITE  -8012
#define UNIX_CHDIR  -8013
#define UNIX_GETENV -8014
#define UNIX_PUTENV -8015
#define UNIX_SLEEP  -8016


/*---------------------------------------------------------------------
  `unixOpenPipe()` is like `ioOpenFile()`, but for pipes. This pulls
  from the data stack:

  - mode       (number, TOS)
  - executable (string, NOS)

  Modes are:

  | Mode | Corresponds To | Description          |
  | ---- | -------------- | -------------------- |
  |  0   | r              | Open for reading     |
  |  1   | w              | Open for writing     |
  |  3   | r+             | Open for read/update |

  The file name should be a NULL terminated string. This will attempt
  to open the requested file and will return a handle (index number
  into the `ioFileHandles` array).

  Once opened, you can use the standard file words to read/write to the
  process.
  ---------------------------------------------------------------------*/

CELL unixOpenPipe() {
  CELL slot, mode, name;
  slot = ioGetFileHandle();
  mode = data[sp]; sp--;
  name = data[sp]; sp--;
  char *request = string_extract(name);
  if (slot > 0) {
    if (mode == 0)  ioFileHandles[slot] = popen(request, "r");
    if (mode == 1)  ioFileHandles[slot] = popen(request, "w");
    if (mode == 3)  ioFileHandles[slot] = popen(request, "r+");
  }
  if (ioFileHandles[slot] == NULL) {
    ioFileHandles[slot] = 0;
    slot = 0;
  }
  stack_push(slot);
  return slot;
}


/*---------------------------------------------------------------------
  `unixClosePipe()` closes an open pipe. This takes a file handle from
  the stack.
  ---------------------------------------------------------------------*/

CELL unixClosePipe() {
  pclose(ioFileHandles[data[sp]]);
  ioFileHandles[data[sp]] = 0;
  sp--;
  return 0;
}

void unix_system() {
  system(string_extract(stack_pop()));
}

void unix_fork() {
  stack_push(fork());
}

void unix_exec0() {
  char path[1024];
  strcpy(path, string_extract(stack_pop()));
  execl(path, path, (char *)0);
  stack_push(errno);
}

void unix_exec1() {
  char path[1024];
  char arg0[1024];
  strcpy(arg0, string_extract(stack_pop()));
  strcpy(path, string_extract(stack_pop()));
  execl(path, path, arg0, (char *)0);
  stack_push(errno);
}

void unix_exec2() {
  char path[1024];
  char arg0[1024], arg1[1024];
  strcpy(arg1, string_extract(stack_pop()));
  strcpy(arg0, string_extract(stack_pop()));
  strcpy(path, string_extract(stack_pop()));
  execl(path, path, arg0, arg1, (char *)0);
  stack_push(errno);
}

void unix_exec3() {
  char path[1024];
  char arg0[1024], arg1[1024], arg2[1024];
  strcpy(arg2, string_extract(stack_pop()));
  strcpy(arg1, string_extract(stack_pop()));
  strcpy(arg0, string_extract(stack_pop()));
  strcpy(path, string_extract(stack_pop()));
  execl(path, path, arg0, arg1, arg2, (char *)0);
  stack_push(errno);
}

void unix_exit() {
  exit(stack_pop());
}

void unix_getpid() {
  stack_push(getpid());
}

void unix_wait() {
  CELL a;
  stack_push(wait(&a));
}

void unix_kill() {
  CELL a;
  a = stack_pop();
  kill(stack_pop(), a);
}

void unix_write() {
  CELL a, b, c;
  c = stack_pop();
  b = stack_pop();
  a = stack_pop();
  write(fileno(ioFileHandles[c]), string_extract(a), b);
}

void unix_chdir() {
  chdir(string_extract(stack_pop()));
}

void unix_getenv() {
  CELL a, b;
  a = stack_pop();
  b = stack_pop();
  string_inject(getenv(string_extract(b)), a);
}

void unix_putenv() {
  putenv(string_extract(stack_pop()));
}

void unix_sleep() {
  sleep(stack_pop());
}

void ngaUnixUnit() {
  switch (stack_pop()) {
      case UNIX_SYSTEM: unix_system();   break;
      case UNIX_FORK:   unix_fork();     break;
      case UNIX_EXEC0:  unix_exec0();    break;
      case UNIX_EXEC1:  unix_exec1();    break;
      case UNIX_EXEC2:  unix_exec2();    break;
      case UNIX_EXEC3:  unix_exec3();    break;
      case UNIX_EXIT:   unix_exit();     break;
      case UNIX_GETPID: unix_getpid();   break;
      case UNIX_WAIT:   unix_wait();     break;
      case UNIX_KILL:   unix_kill();     break;
      case UNIX_POPEN:  unixOpenPipe();  break;
      case UNIX_PCLOSE: unixClosePipe(); break;
      case UNIX_WRITE:  unix_write();    break;
      case UNIX_CHDIR:  unix_chdir();    break;
      case UNIX_GETENV: unix_getenv();   break;
      case UNIX_PUTENV: unix_putenv();   break;
      case UNIX_SLEEP:  unix_sleep();    break;
      default:                           break;
  }
}
#endif


/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/

#ifdef ENABLE_FLOATING_POINT

double Floats[8192];
CELL fsp;

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_push(double value) {
    fsp++;
    Floats[fsp] = value;
}

double float_pop() {
    fsp--;
    return Floats[fsp + 1];
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_from_number() {
    float_push((double)stack_pop());
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_from_string() {
    float_push(atof(string_extract(stack_pop())));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_to_string() {
    snprintf(string_data, 8192, "%f", float_pop());
    string_inject(string_data, stack_pop());
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_add() {
    double a = float_pop();
    double b = float_pop();
    float_push(a+b);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_sub() {
    double a = float_pop();
    double b = float_pop();
    float_push(b-a);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_mul() {
    double a = float_pop();
    double b = float_pop();
    float_push(a*b);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_div() {
    double a = float_pop();
    double b = float_pop();
    float_push(b/a);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_floor() {
    float_push(floor(float_pop()));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_ceil() {
    float_push(ceil(float_pop()));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_eq() {
    double a = float_pop();
    double b = float_pop();
    if (a == b)
        stack_push(-1);
    else
        stack_push(0);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_neq() {
    double a = float_pop();
    double b = float_pop();
    if (a != b)
        stack_push(-1);
    else
        stack_push(0);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_lt() {
    double a = float_pop();
    double b = float_pop();
    if (b < a)
        stack_push(-1);
    else
        stack_push(0);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_gt() {
    double a = float_pop();
    double b = float_pop();
    if (b > a)
        stack_push(-1);
    else
        stack_push(0);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_depth() {
    stack_push(fsp);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_dup() {
    double a = float_pop();
    float_push(a);
    float_push(a);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_drop() {
    float_pop();
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_swap() {
    double a = float_pop();
    double b = float_pop();
    float_push(a);
    float_push(b);
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_log() {
    double a = float_pop();
    double b = float_pop();
    float_push(log(b) / log(a));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_pow() {
    double a = float_pop();
    double b = float_pop();
    float_push(pow(b, a));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_to_number() {
    double a = float_pop();
    if (a > 2147483647)
      a = 2147483647;
    if (a < -2147483648)
      a = -2147483648;
    stack_push((CELL)round(a));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_sin() {
  float_push(sin(float_pop()));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_cos() {
  float_push(cos(float_pop()));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_tan() {
  float_push(tan(float_pop()));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_asin() {
  float_push(asin(float_pop()));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_acos() {
  float_push(acos(float_pop()));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void float_atan() {
  float_push(atan(float_pop()));
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void ngaFloatingPointUnit() {
    switch (stack_pop()) {
        case 0:  float_from_number();  break;
        case 1:  float_from_string();  break;
        case 2:  float_to_string();    break;
        case 3:  float_add();          break;
        case 4:  float_sub();          break;
        case 5:  float_mul();          break;
        case 6:  float_div();          break;
        case 7:  float_floor();        break;
        case 8:  float_eq();           break;
        case 9:  float_neq();          break;
        case 10: float_lt();           break;
        case 11: float_gt();           break;
        case 12: float_depth();        break;
        case 13: float_dup();          break;
        case 14: float_drop();         break;
        case 15: float_swap();         break;
        case 16: float_log();          break;
        case 17: float_pow();          break;
        case 18: float_to_number();    break;
        case 19: float_sin();          break;
        case 20: float_cos();          break;
        case 21: float_tan();          break;
        case 22: float_asin();         break;
        case 23: float_acos();         break;
        case 24: float_atan();         break;
        case 25: float_ceil();         break;
        default:                       break;
    }
}

#endif

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/

#ifdef ENABLE_GOPHER

void error(const char *msg) {
  perror(msg);
  exit(0);
}

void gopher_fetch(char *host, CELL port, char *selector, CELL dest) {
  int sockfd, portno, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  char data[128 * 1024 + 1];
  char buffer[1025];
  portno = (int)port;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");
  server = gethostbyname(host);
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
  bzero(data, 128 * 1024 + 1);
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
     (char *)&serv_addr.sin_addr.s_addr,
     server->h_length);
  serv_addr.sin_port = htons(portno);
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
    error("ERROR connecting");
  n = write(sockfd,selector,strlen(selector));
  if (n < 0)
     error("ERROR writing to socket");
  n = write(sockfd,"\n",strlen("\n"));
  if (n < 0)
     error("ERROR writing to socket");
  n = 1;
  while (n > 0) {
    bzero(buffer,1025);
    n = read(sockfd,buffer,1024);
    strcat(data, buffer);
  }
  close(sockfd);
  string_inject(data, dest);
  stack_push(strlen(data));
}


/* <addr> <server> <port> <selector> */
void ngaGopherUnit() {
  char server[1025];
  char selector[4097];
  CELL port, dest;
  strcpy(selector, string_extract(stack_pop()));
  port = stack_pop();
  strcpy(server, string_extract(stack_pop()));
  dest = stack_pop();
  gopher_fetch(server, port, selector, dest);
}

#endif


void execute(int cell) {
  CELL a, b, c;
  CELL opcode;
  char path[1024];
  char arg0[1024], arg1[1024], arg2[1024];
  char arg3[1024], arg4[1024], arg5[1024];
  rp = 1;
  ip = cell;
  while (ip < IMAGE_SIZE) {
    if (ip == NotFound) {
      printf("%s ?\n", string_extract(TIB));
    }
    opcode = memory[ip];
    if (ngaValidatePackedOpcodes(opcode) != 0) {
      ngaProcessPackedOpcodes(opcode);
    } else if (opcode >= 0 && opcode < 27) {
      ngaProcessOpcode(opcode);
    } else {
      switch (opcode) {
        case IO_TTY_PUTC:  putc(stack_pop(), stdout); fflush(stdout); break;
        case IO_TTY_GETC:  stack_push(getc(stdin));                   break;
        case -9999:        include_file(string_extract(stack_pop())); break;
        case IO_FS_OPEN:   ioOpenFile();                              break;
        case IO_FS_CLOSE:  ioCloseFile();                             break;
        case IO_FS_READ:   stack_push(ioReadFile());                  break;
        case IO_FS_WRITE:  ioWriteFile();                             break;
        case IO_FS_TELL:   stack_push(ioGetFilePosition());           break;
        case IO_FS_SEEK:   ioSetFilePosition();                       break;
        case IO_FS_SIZE:   stack_push(ioGetFileSize());               break;
        case IO_FS_DELETE: ioDeleteFile();                            break;
        case IO_FS_FLUSH:  ioFlushFile();                             break;
        case -6000: ngaFloatingPointUnit(); break;
        case -6100: stack_push(sys_argc - 2); break;
        case -6101: a = stack_pop();
                    b = stack_pop();
                    stack_push(string_inject(sys_argv[a + 2], b));
                    break;
        case -6200: ngaGopherUnit(); break;
        case -6300: ngaUnixUnit(); break;
        default:   printf("Invalid instruction!\n");
                   printf("At %d, opcode %d\n", ip, opcode);
                   exit(1);
      }
    }
    ip++;
    if (rp == 0)
      ip = IMAGE_SIZE;
  }
}


/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
/* The `evaluate` function moves a token into the Retro
   token buffer, then calls the Retro `interpret` word
   to process it. */

void evaluate(char *s) {
  if (strlen(s) == 0)
    return;
  update_rx();
  string_inject(s, TIB);
  stack_push(TIB);
  execute(interpret);
}


/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
/* `read_token` reads a token from the specified file.
   It will stop on a whitespace or newline. It also
   tries to handle backspaces, though the success of this
   depends on how your terminal is configured. */

int not_eol(int ch) {
  return (ch != (char)10) && (ch != (char)13) && (ch != (char)32) && (ch != EOF) && (ch != 0);
}

void read_token(FILE *file, char *token_buffer, int echo) {
  int ch = getc(file);
  if (echo != 0)
    putchar(ch);
  int count = 0;
  while (not_eol(ch))
  {
    if ((ch == 8 || ch == 127) && count > 0) {
      count--;
      if (echo != 0) {
        putchar(8);
        putchar(32);
        putchar(8);
      }
    } else {
      token_buffer[count++] = ch;
    }
    ch = getc(file);
    if (echo != 0)
      putchar(ch);
  }
  token_buffer[count] = '\0';
}

/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
char *read_token_str(char *s, char *token_buffer, int echo) {
  int ch = (char)*s++;
  if (echo != 0)
    putchar(ch);
  int count = 0;
  while (not_eol(ch))
  {
    if ((ch == 8 || ch == 127) && count > 0) {
      count--;
      if (echo != 0) {
        putchar(8);
        putchar(32);
        putchar(8);
      }
    } else {
      token_buffer[count++] = ch;
    }
    ch = (char)*s++;
    if (echo != 0)
      putchar(ch);
  }
  token_buffer[count] = '\0';
  return s;
}


/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
/* Compile image.c and link against the image.o */
#include "image.c"


/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void dump_stack() {
  CELL i;
  if (sp == 0)
    return;
  printf("\nStack: ");
  for (i = 1; i <= sp; i++) {
    if (i == sp)
      printf("[ TOS: %d ]", data[i]);
    else
      printf("%d ", data[i]);
  }
  printf("\n");
}


/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
int fenced(char *s)
{
  int a = strcmp(s, "```");
  int b = strcmp(s, "~~~");
  if (a == 0) return 1;
  if (b == 0) return 1;
              return 0;
}


/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
void include_file(char *fname) {
  int inBlock = 0;
  char source[64 * 1024];
  char fence[4];
  FILE *fp;
  fp = fopen(fname, "r");
  if (fp == NULL)
    return;
  while (!feof(fp))
  {
    read_token(fp, source, 0);
    strncpy(fence, source, 3);
    fence[3] = '\0';
    if (fenced(fence)) {
      if (inBlock == 0)
        inBlock = 1;
      else
        inBlock = 0;
    } else {
      if (inBlock == 1)
        evaluate(source);
    }
  }
  fclose(fp);
}


/*---------------------------------------------------------------------
  ---------------------------------------------------------------------*/
int main(int argc, char **argv) {
  int i, interactive;
  ngaPrepare();
  for (i = 0; i < ngaImageCells; i++)
    memory[i] = ngaImage[i];
  update_rx();

  interactive = 0;

  sys_argc = argc;
  sys_argv = argv;

  if (argc > 1) {
    if (strcmp(argv[1], "-i") == 0) {
      interactive = 1;
      if (argc >= 4 && strcmp(argv[2], "-f") == 0) {
        include_file(argv[3]);
      }
    } else if (strcmp(argv[1], "-c") == 0) {
      interactive = 2;
      if (argc >= 4 && strcmp(argv[2], "-f") == 0) {
        include_file(argv[3]);
      }
    } else if (strcmp(argv[1], "-h") == 0) {
      printf("Scripting Usage: rre filename\n\n");
      printf("Interactive Usage: rre args\n\n");
      printf("Valid Arguments:\n\n");
      printf("  -h\n");
      printf("  Display this help text\n\n");
      printf("  -i\n");
      printf("  Launches in interactive mode (line buffered)\n\n");
      printf("  -c\n");
      printf("  Launches in interactive mode (character buffered)\n\n");
      printf("  -i -f filename\n");
      printf("  Launches in interactive mode (line buffered) and load the contents of the\n  specified file\n\n");
      printf("  -c -f filename\n");
      printf("  Launches in interactive mode (character buffered) and load the contents\n  of the specified file\n\n");
    } else {
      include_file(argv[1]);
    }
  }

  if (interactive == 1) {
    execute(d_xt_for("banner", Dictionary));
    while (1) execute(d_xt_for("listen", Dictionary));
  }
  if (interactive == 2) {
    execute(d_xt_for("banner", Dictionary));
    while (1) execute(d_xt_for("listen-cbreak", Dictionary));
  }

  if (sp >= 1)
    dump_stack();

  exit(0);
}


/* Nga ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Copyright (c) 2008 - 2017, Charles Childers
   Copyright (c) 2009 - 2010, Luke Parrish
   Copyright (c) 2010,        Marc Simpson
   Copyright (c) 2010,        Jay Skeer
   Copyright (c) 2011,        Kenneth Keating
   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

enum vm_opcode {
  VM_NOP,  VM_LIT,    VM_DUP,   VM_DROP,    VM_SWAP,   VM_PUSH,  VM_POP,
  VM_JUMP, VM_CALL,   VM_CCALL, VM_RETURN,  VM_EQ,     VM_NEQ,   VM_LT,
  VM_GT,   VM_FETCH,  VM_STORE, VM_ADD,     VM_SUB,    VM_MUL,   VM_DIVMOD,
  VM_AND,  VM_OR,     VM_XOR,   VM_SHIFT,   VM_ZRET,   VM_END
};
#define NUM_OPS VM_END + 1

CELL ngaLoadImage(char *imageFile) {
  FILE *fp;
  CELL imageSize;
  long fileLen;
  if ((fp = fopen(imageFile, "rb")) != NULL) {
    /* Determine length (in cells) */
    fseek(fp, 0, SEEK_END);
    fileLen = ftell(fp) / sizeof(CELL);
    rewind(fp);
    /* Read the file into memory */
    imageSize = fread(&memory, sizeof(CELL), fileLen, fp);
    fclose(fp);
  }
  else {
    printf("Unable to find the ngaImage!\n");
    exit(1);
  }
  return imageSize;
}

void ngaPrepare() {
  ip = sp = rp = 0;
  for (ip = 0; ip < IMAGE_SIZE; ip++)
    memory[ip] = VM_NOP;
  for (ip = 0; ip < STACK_DEPTH; ip++)
    data[ip] = 0;
  for (ip = 0; ip < ADDRESSES; ip++)
    address[ip] = 0;
}

void inst_nop() {
}

void inst_lit() {
  sp++;
  ip++;
  TOS = memory[ip];
}

void inst_dup() {
  sp++;
  data[sp] = NOS;
}

void inst_drop() {
  data[sp] = 0;
   if (--sp < 0)
     ip = IMAGE_SIZE;
}

void inst_swap() {
  int a;
  a = TOS;
  TOS = NOS;
  NOS = a;
}

void inst_push() {
  rp++;
  TORS = TOS;
  inst_drop();
}

void inst_pop() {
  sp++;
  TOS = TORS;
  rp--;
}

void inst_jump() {
  ip = TOS - 1;
  inst_drop();
}

void inst_call() {
  rp++;
  TORS = ip;
  ip = TOS - 1;
  inst_drop();
}

void inst_ccall() {
  int a, b;
  a = TOS; inst_drop();  /* False */
  b = TOS; inst_drop();  /* Flag  */
  if (b != 0) {
    rp++;
    TORS = ip;
    ip = a - 1;
  }
}

void inst_return() {
  ip = TORS;
  rp--;
}

void inst_eq() {
  NOS = (NOS == TOS) ? -1 : 0;
  inst_drop();
}

void inst_neq() {
  NOS = (NOS != TOS) ? -1 : 0;
  inst_drop();
}

void inst_lt() {
  NOS = (NOS < TOS) ? -1 : 0;
  inst_drop();
}

void inst_gt() {
  NOS = (NOS > TOS) ? -1 : 0;
  inst_drop();
}

void inst_fetch() {
  switch (TOS) {
    case -1: TOS = sp - 1; break;
    case -2: TOS = rp; break;
    case -3: TOS = IMAGE_SIZE; break;
    default: TOS = memory[TOS]; break;
  }
}

void inst_store() {
  if (TOS <= IMAGE_SIZE && TOS >= 0) {
    memory[TOS] = NOS;
    inst_drop();
    inst_drop();
  } else {
     ip = IMAGE_SIZE;
  }
}

void inst_add() {
  NOS += TOS;
  inst_drop();
}

void inst_sub() {
  NOS -= TOS;
  inst_drop();
}

void inst_mul() {
  NOS *= TOS;
  inst_drop();
}

void inst_divmod() {
  int a, b;
  a = TOS;
  b = NOS;
  TOS = b / a;
  NOS = b % a;
}

void inst_and() {
  NOS = TOS & NOS;
  inst_drop();
}

void inst_or() {
  NOS = TOS | NOS;
  inst_drop();
}

void inst_xor() {
  NOS = TOS ^ NOS;
  inst_drop();
}

void inst_shift() {
  CELL y = TOS;
  CELL x = NOS;
  if (TOS < 0)
    NOS = NOS << (TOS * -1);
  else {
    if (x < 0 && y > 0)
      NOS = x >> y | ~(~0U >> y);
    else
      NOS = x >> y;
  }
  inst_drop();
}

void inst_zret() {
  if (TOS == 0) {
    inst_drop();
    ip = TORS;
    rp--;
  }
}

void inst_end() {
  ip = IMAGE_SIZE;
}

typedef void (*Handler)(void);
Handler instructions[NUM_OPS] = {
  inst_nop, inst_lit, inst_dup, inst_drop, inst_swap, inst_push, inst_pop,
  inst_jump, inst_call, inst_ccall, inst_return, inst_eq, inst_neq, inst_lt,
  inst_gt, inst_fetch, inst_store, inst_add, inst_sub, inst_mul, inst_divmod,
  inst_and, inst_or, inst_xor, inst_shift, inst_zret, inst_end
};

void ngaProcessOpcode(CELL opcode) {
  instructions[opcode]();
}

int ngaValidatePackedOpcodes(CELL opcode) {
  CELL raw = opcode;
  CELL current;
  int valid = -1;
  int i;
  for (i = 0; i < 4; i++) {
    current = raw & 0xFF;
    if (!(current >= 0 && current <= 26))
      valid = 0;
    raw = raw >> 8;
  }
  return valid;
}

void ngaProcessPackedOpcodes(int opcode) {
  CELL raw = opcode;
  int i;
  for (i = 0; i < 4; i++) {
    ngaProcessOpcode(raw & 0xFF);
    raw = raw >> 8;
  }
}
