/* RETRO -------------------------------------------------------------
  A personal, minimalistic forth
  Copyright (c) 2016 - 2018 Charles Childers

  This is `ri`, an inteActiveInstance, full screen, console based interface
  for RETRO, modeled after the original interface from RETRO4. The
  screen is laid out as follows:

    +-----------------------------------------------------------+
    | output area                                               |
    |                                                           |
    |                                                           |
    |                                                           |
    +-----------------------------------------------------------+
    | input area                 | stack area                   |
    +-----------------------------------------------------------+

  Input is processed as it is entered. Output is shown above the input
  area, and the stack is show to the right of the input area. The
  stack display is formatted like this:

    D: 3 [ 100 ] 98 97

  The number after `D:` is the stack depth. The number in brackets is
  the top item on the stack, and othe items are shown following this.
  As with input, the stack display is updated as code is processed.

  To clear the output area, use TAB.

  To exit, press CTRL+D.

  I'll include commentary throughout the source, so read on.
  ---------------------------------------------------------------------*/


/*---------------------------------------------------------------------
  Begin by including the various C headers needed.
  ---------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ncurses.h>


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
#define IMAGE_SIZE   524288       /* Amount of RAM. 512k cells         */
#define ADDRESSES    1024         /* Depth of address stack            */
#define STACK_DEPTH  128          /* Depth of data stack               */

CELL sp, rp, ip;                  /* Data, address, instruction pointers */
CELL data[STACK_DEPTH];           /* The data stack                    */
CELL address[ADDRESSES];          /* The address stack                 */
CELL memory[IMAGE_SIZE + 1];      /* The memory for the image          */

typedef struct {
  CELL sp, rp, ip;                /* Data, address, instruction pointers */
  CELL data[STACK_DEPTH];         /* The data stack                  */
  CELL address[ADDRESSES];        /* The address stack               */
  CELL memory[IMAGE_SIZE + 1];    /* The memory for the image        */
} RetroInstance;

RetroInstance Pristine;
RetroInstance Instances[5];

int ActiveInstance;

#define TOS  data[sp]             /* Shortcut for top item on stack    */
#define NOS  data[sp-1]           /* Shortcut for second item on stack */
#define TORS address[rp]          /* Shortcut for top item on address stack */


void generic_output();
void generic_output_query();
void io_keyboard_handler();
void io_keyboard_query();
void io_filesystem_query();
void io_filesystem_handler();
void io_unix_query();
void io_unix_handler();
void io_floatingpoint_query();
void io_floatingpoint_handler();
void io_gopher_query();
void io_gopher_handler();
void io_scripting_handler();
void io_scripting_query();

#define NUM_DEVICES  3

typedef void (*Handler)(void);

Handler IO_deviceHandlers[NUM_DEVICES + 1] = {
  generic_output,
  io_filesystem_handler,
  io_floatingpoint_handler,
};

Handler IO_queryHandlers[NUM_DEVICES + 1] = {
  generic_output_query,
  io_filesystem_query,
  io_floatingpoint_query,
};

/*---------------------------------------------------------------------
  `ri` embeds the image into the binary. This includes the image data
  (converted to a .c file by an external tool).
  ---------------------------------------------------------------------*/

#include "ri_image.c"


/*---------------------------------------------------------------------
  Moving forward, a few variables. These are updated to point to the
  latest values in the image.
  ---------------------------------------------------------------------*/

CELL Dictionary;
CELL NotFound;
CELL Interpret;
CELL Compiler;


/*---------------------------------------------------------------------
  Now define WINDOW structures for the various interface regions.
  ---------------------------------------------------------------------*/

WINDOW *output, *input, *stack, *back;


/*---------------------------------------------------------------------
  Function prototypes.
  ---------------------------------------------------------------------*/

CELL stack_pop();
void stack_push(CELL value);
CELL string_inject(char *str, CELL buffer);
char *string_extract(CELL at);
CELL d_link(CELL dt);
CELL d_xt(CELL dt);
CELL d_class(CELL dt);
CELL d_name(CELL dt);
CELL d_lookup(CELL Dictionary, char *name);
CELL d_xt_for(char *Name, CELL Dictionary);
void update_rx();
void execute(CELL cell);
void evaluate(char *s);
CELL ngaLoadImage(char *imageFile);
void ngaPrepare();
void ngaProcessOpcode(CELL opcode);
void ngaProcessPackedOpcodes(CELL opcode);
int ngaValidatePackedOpcodes(CELL opcode);


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

char *output_cache(int n) {
  switch(n) {
    case 0: return "/tmp/ri.output1"; break;
    case 1: return "/tmp/ri.output2"; break;
    case 2: return "/tmp/ri.output3"; break;
    case 3: return "/tmp/ri.output4"; break;
    case 4: return "/tmp/ri.output5"; break;
  }
  return "";
}


void swap_image(int to) {
  int i;

  char *target = output_cache(ActiveInstance);

  for (i = 0; i < IMAGE_SIZE; i++)
    Instances[ActiveInstance].memory[i] = memory[i];
  for (i = 0; i < STACK_DEPTH; i++)
    Instances[ActiveInstance].data[i] = data[i];
  for (i = 0; i < ADDRESSES; i++)
    Instances[ActiveInstance].address[i] = address[i];
  Instances[ActiveInstance].sp = sp;
  Instances[ActiveInstance].rp = rp;
  Instances[ActiveInstance].ip = ip;
  FILE *f = fopen(target, "w");
  putwin(output, f);
  fclose(f);

  target = output_cache(to);

  for (i = 0; i < IMAGE_SIZE; i++)
    memory[i] = Instances[to].memory[i];
  for (i = 0; i < STACK_DEPTH; i++)
    data[i] = Instances[to].data[i];
  for (i = 0; i < ADDRESSES; i++)
    address[i] = Instances[to].address[i];
  sp = Instances[to].sp;
  rp = Instances[to].rp;
  ip = Instances[to].ip;
  f = fopen(target, "r");
  output = getwin(f);
  fclose(f);

  ActiveInstance = to;
}


/*---------------------------------------------------------------------
  Strings are next. RETRO uses C-style NULL terminated strings. So I
  can easily inject or extract a string. Injection iterates over the
  string, copying it into the image. This also takes care to ensure
  that the NULL terminator is added.
  ---------------------------------------------------------------------*/

CELL string_inject(char *str, CELL buffer) {
  CELL i = 0;
  while (*str) {
    memory[buffer + i] = (CELL)*str++;
    memory[buffer + i + 1] = 0;
    i++;
  }
  return buffer;
}


/*---------------------------------------------------------------------
  Extracting a string is similar, but I have to iterate over the VM
  memory instead of a C string and copy the charaters into a buffer.
  This uses a static buffer (`string_data`) as I prefer to avoid using
  `malloc()`.
  ---------------------------------------------------------------------*/

char string_data[1025];
char *string_extract(CELL at) {
  CELL starting = at;
  CELL i = 0;
  while(memory[starting] && i < 1024)
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

CELL d_link(CELL dt) {
  return dt + D_OFFSET_LINK;
}

CELL d_xt(CELL dt) {
  return dt + D_OFFSET_XT;
}

CELL d_class(CELL dt) {
  return dt + D_OFFSET_CLASS;
}

CELL d_name(CELL dt) {
  return dt + D_OFFSET_NAME;
}


/*---------------------------------------------------------------------
  Next, a more complext word. This will walk through the entries to
  find one with a name that matches the specified name. This is *slow*,
  but works ok unless you have a really large dictionary. (I've not
  run into issues with this in practice).
  ---------------------------------------------------------------------*/

CELL d_lookup(CELL Dictionary, char *name) {
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
  Interpret  - the heart of the interpreter/compiler
  Compiler   - the compiler state

  I have to call this periodically, as the Dictionary will change as
  new words are defined, and the user might write a new error handler
  or interpreter.
  ---------------------------------------------------------------------*/

void update_rx() {
  Dictionary = memory[2];
  Compiler = d_xt_for("Compiler", Dictionary);
  NotFound = d_xt_for("err:notfound", Dictionary);
  Interpret = d_xt_for("interpret", Dictionary);
}


/*---------------------------------------------------------------------
  With these out of the way, I implement `execute`, which takes an
  address and runs the code at it. This has a couple of interesting
  bits.

  Nga uses packed instruction bundles, with up to four instructions per
  bundle. Since RETRO requires an additional instruction to handle
  displaying a character, I define the handler for that here.

  This will also exit if the address stack depth is zero (meaning that
  the word being run, and it's dependencies) are finished.
  ---------------------------------------------------------------------*/

void generic_output() {
  wprintw(output, "%c", stack_pop());
}

void generic_output_query() {
  stack_push(0);
  stack_push(0);
}

void execute(CELL cell) {
  CELL opcode;
  rp = 1;
  ip = cell;
  while (ip < IMAGE_SIZE) {
    if (ip == NotFound) {
      wprintw(output, "%s ?\n", string_extract(TIB));
    }
    opcode = memory[ip];
    if (ngaValidatePackedOpcodes(opcode) != 0) {
      ngaProcessPackedOpcodes(opcode);
    } else if (opcode >= 0 && opcode < 27) {
      ngaProcessOpcode(opcode);
    } else {
      wprintw(output, "Invalid instruction!\n");
      wprintw(output, "At %d, opcode %d\n", ip, opcode);
      exit(1);
    }
    ip++;
    if (rp == 0)
      ip = IMAGE_SIZE;
  }
}


/*---------------------------------------------------------------------
  RETRO's `interpret` word expects a token on the stack. This next
  function copies a token to the `TIB` (text input buffer) and then
  calls `interpret` to process it.
  ---------------------------------------------------------------------*/

void evaluate(char *s) {
  if (strlen(s) == 0) return;
  update_rx();
  string_inject(s, TIB);
  stack_push(TIB);
  execute(Interpret);
}


/*---------------------------------------------------------------------
  I now turn to the actual user interface.

  First, a function to display the stack.
  ---------------------------------------------------------------------*/

void dump_stack() {
  CELL i;
  wclear(stack);
  wprintw(stack, "I:%d | H:%d | ", ActiveInstance + 1, memory[3]);
  wprintw(stack, "D:%d | ", sp);
  if (sp == 0)
    return;
  for (i = sp; i > 0 && i != sp - 12; i--)
    if (i == sp)
      wprintw(stack, "[ %d ]", data[i]);
    else
      wprintw(stack, " %d", data[i]);
}


/*---------------------------------------------------------------------
  Then a function to lay out the interface (and adjust color scheme)
  ---------------------------------------------------------------------*/

void setup_interface() {
  initscr();
  start_color();
  init_pair(1, COLOR_WHITE, COLOR_BLUE);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_GREEN, COLOR_BLACK);
  printw("first");
  refresh();
  cbreak();

  back = newwin(LINES - 1, COLS, 0, 0);
//  output = newwin(LINES - 1, COLS, 0, 0);
//  input = newwin(1, COLS / 2, LINES - 1, 0);
//  stack = newwin(1, COLS / 2, LINES - 1, COLS / 2);
  output = newwin(LINES - 2, COLS, 0, 0);
  input = newwin(1, COLS, LINES - 1, 0);
  stack = newwin(1, COLS, LINES - 2, 0);
  scrollok(output, TRUE);
  scrollok(input, TRUE);
  keypad(input, TRUE);

  wbkgd(input, COLOR_PAIR(2) | A_BOLD);
  wbkgd(stack, COLOR_PAIR(1));
  wbkgd(output, COLOR_PAIR(3));

  wrefresh(back);
  wrefresh(output);
  wrefresh(input);
  wrefresh(stack);
  doupdate();
}


/*---------------------------------------------------------------------
  Display some help on startup
  ---------------------------------------------------------------------*/

void start_screen() {
  wprintw(output, "Welcome to RETRO!\n");
  wprintw(output, "\n");
  wprintw(output, "Some tips:\n\n - TAB to clear the display\n");
  wprintw(output, " - CTRL+d to exit\n");
  wprintw(output, " - Input is run when you hit SPACE\n");
  wprintw(output, "\n");
  wprintw(output, "Use /1 to /5 to switch instances and /reload to restart the current instance\n");
  wprintw(output, "\n");

  wrefresh(output);
  dump_stack();
  wrefresh(stack);
  doupdate();
}

#ifndef CTRL
#define CTRL(c) ((c) & 037)
#endif


/*---------------------------------------------------------------------
  The `main()` routine. This sets up the Nga VM, loads the image, and
  enters an evaluation loop.
  ---------------------------------------------------------------------*/

int main() {
  int ch;                              /* Character being read       */
  char c[1024];                        /* Input buffer               */
  int n = 0;                           /* Index to input buffer      */

  int target;

  ngaPrepare();

  for (n = 0; n < ngaImageCells; n++)  /* Copy the embedded image to */
    memory[n] = ngaImage[n];           /* the Nga VM memory          */
  n = 0;


  int i;
  for (i = 0; i < IMAGE_SIZE; i++) {
    Instances[0].memory[i] = memory[i];
    Instances[1].memory[i] = memory[i];
    Instances[2].memory[i] = memory[i];
    Instances[3].memory[i] = memory[i];
    Instances[4].memory[i] = memory[i];
    Pristine.memory[i] = memory[i];
  }

  update_rx();

  setup_interface();
  start_screen();

  FILE *f;
  f = fopen("/tmp/ri.output1", "w");  putwin(output, f);  fclose(f);
  f = fopen("/tmp/ri.output2", "w");  putwin(output, f);  fclose(f);
  f = fopen("/tmp/ri.output3", "w");  putwin(output, f);  fclose(f);
  f = fopen("/tmp/ri.output4", "w");  putwin(output, f);  fclose(f);
  f = fopen("/tmp/ri.output5", "w");  putwin(output, f);  fclose(f);

  while ((ch = wgetch(input)) != CTRL('d')) {
    switch (ch) {
      case KEY_BACKSPACE:              /* Handle backspaces          */
      case 127:
        n--;
        c[n] = '\0';
        break;
      case 9:                          /* TAB = clear output         */
        wclear(input);
        wclear(output);
        wrefresh(output);
        doupdate();
        break;
      case 10:                         /* ENTER or SPACE = evaluate  */
      case 13:
      case 32:
        if (strlen(c) == 2 && c[0] == '/') {
          switch(c[1]) {
            case '1': target = 0; break;
            case '2': target = 1; break;
            case '3': target = 2; break;
            case '4': target = 3; break;
            case '5': target = 4; break;
          }
          swap_image(target);
          update_rx();
          n = 0;
          c[0] = '\0';
          wrefresh(output);
          wclear(input);
          wrefresh(input);
          dump_stack();
          wrefresh(stack);
          break;
        }
        if (strcmp(c, "/reload") == 0) {
          for (i = 0; i < IMAGE_SIZE; i++)
            memory[i] = Pristine.memory[i];
          sp = 0;
          n = 0;
          c[0] = '\0';
          update_rx();
          wrefresh(output);
          wclear(input);
          wrefresh(input);
          dump_stack();
          wrefresh(stack);
        }
        evaluate(c);
        n = 0;
        c[0] = '\0';
        update_rx();
        dump_stack();
        wrefresh(output);
        wrefresh(stack);
        if (memory[Compiler] == 0)     /* Clear the input if not     */
          wclear(input);               /* compiling                  */
        wrefresh(input);
        break;
      default:                         /* Default is to add char to  */
        c[n++] = ch;                   /* the input string           */
        c[n] = '\0';
        break;
    }
  }

  endwin();
  unlink("/tmp/ri.output1");
  unlink("/tmp/ri.output2");
  unlink("/tmp/ri.output3");
  unlink("/tmp/ri.output4");
  unlink("/tmp/ri.output5");
  exit(0);
}


/* Nga ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Copyright (c) 2008 - 2018, Charles Childers
   Copyright (c) 2009 - 2010, Luke Parrish
   Copyright (c) 2010,        Marc Simpson
   Copyright (c) 2010,        Jay Skeer
   Copyright (c) 2011,        Kenneth Keating
   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

enum vm_opcode {
  VM_NOP,  VM_LIT,    VM_DUP,   VM_DROP,    VM_SWAP,   VM_PUSH,  VM_POP,
  VM_JUMP, VM_CALL,   VM_CCALL, VM_RETURN,  VM_EQ,     VM_NEQ,   VM_LT,
  VM_GT,   VM_FETCH,  VM_STORE, VM_ADD,     VM_SUB,    VM_MUL,   VM_DIVMOD,
  VM_AND,  VM_OR,     VM_XOR,   VM_SHIFT,   VM_ZRET,   VM_END,   VM_IE,
  VM_IQ,   VM_II
};
#define NUM_OPS VM_II + 1

#ifndef NUM_DEVICES
#define NUM_DEVICES 0
#endif

CELL ngaLoadImage(char *imageFile) {
  FILE *fp;
  CELL imageSize;
  long fileLen;
  CELL i;
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
    for (i = 0; i < ngaImageCells; i++)
      memory[i] = ngaImage[i];
    imageSize = i;
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
  CELL a;
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
  CELL a, b;
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
  CELL a, b;
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

void inst_ie() {
  sp++;
  TOS = NUM_DEVICES;
}

void inst_iq() {
  CELL Device = TOS;
  inst_drop();
  IO_queryHandlers[Device]();
}

void inst_ii() {
  CELL Device = TOS;
  inst_drop();
  IO_deviceHandlers[Device]();
}

Handler instructions[NUM_OPS] = {
  inst_nop, inst_lit, inst_dup, inst_drop, inst_swap, inst_push, inst_pop,
  inst_jump, inst_call, inst_ccall, inst_return, inst_eq, inst_neq, inst_lt,
  inst_gt, inst_fetch, inst_store, inst_add, inst_sub, inst_mul, inst_divmod,
  inst_and, inst_or, inst_xor, inst_shift, inst_zret, inst_end, inst_ie,
  inst_iq, inst_ii
};

void ngaProcessOpcode(CELL opcode) {
  if (opcode != 0)
    instructions[opcode]();
}

int ngaValidatePackedOpcodes(CELL opcode) {
  CELL raw = opcode;
  CELL current;
  int valid = -1;
  int i;
  for (i = 0; i < 4; i++) {
    current = raw & 0xFF;
    if (!(current >= 0 && current <= 29))
      valid = 0;
    raw = raw >> 8;
  }
  return valid;
}

void ngaProcessPackedOpcodes(CELL opcode) {
  CELL raw = opcode;
  int i;
  for (i = 0; i < 4; i++) {
    ngaProcessOpcode(raw & 0xFF);
    raw = raw >> 8;
  }
}
