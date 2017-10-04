# Roo: A Block Editor UI for RETRO

This is an interface layer for RETRO built around a block editor. This
has some interesting features:

- gopher backed block storage (with server written in RETRO)
- modal editing (ala *vi*)
- editor commands are just words in the dictionary

So getting started, some configuration settings for the server side:

~~~
:SERVER (-sn)  'forthworks.com #8008 ;
~~~

`SERVER` returns the server url and port.

Next, create a buffer to store the currently loaded block. With the
server-side storage I don't need to keep more than the current block
in memory.

A block is 1024 bytes; this includes one additional to use a terminator.
Doing this allows the block to be passed to `s:evaluate` as a string.

~~~
'Block d:create
  #1025 allot
~~~

I also define a variable, `Current-Block`, which holds the number of
the currently loaded block.

~~~
'Current-Block var
~~~

With that done, it's now time for a word to load a block from the server.

~~~
:selector<get>  (-s)  @Current-Block '/r/%n s:with-format ;
:load-block     (-)   &Block SERVER selector<get> gopher:get drop ;
~~~

........................................................................

~~~
:display-block (-)
  ASCII:ESC putc '[2J puts
  ASCII:ESC putc '[H puts
  &Block #16 [ #64 [ fetch-next putc ] times $| putc nl ] times drop
  #64 [ $- putc ] times $+ putc sp @Current-Block putn nl ;
~~~

~~~
#0 !Current-Block load-block

:keys
    $n [ &Current-Block v:inc load-block ] case
    $p [ &Current-Block v:dec load-block ] case
    $q [ `26 ] case
    drop ;

:go
  repeat
    display-block
    getc keys
  again ;

go
~~~
