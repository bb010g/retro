# RETRO

This is a set of extensions for RRE.

# Console Input

~~~
:c:get (-c) as{ 'liii.... i #1 d }as ;
~~~

---------------------------------------------------------------

# Floating Point

~~~
:n:to-float  (n-_f:-n)   #0 `-6000 ;
:s:to-float  (s-_f:-n)   #1 `-6000 ;
:f:to-string (f:n-__-s) s:empty dup #2 `-6000 ;
:f:+     (f:ab-c)    #3 `-6000 ;
:f:-     (f:ab-c)    #4 `-6000 ;
:f:*     (f:ab-c)    #5 `-6000 ;
:f:/     (f:ab-c)    #6 `-6000 ;
:f:floor (f:ab-c)    #7 `-6000 ;
:f:eq?   (f:ab-c)    #8 `-6000 ;
:f:-eq?  (f:ab-c)    #9 `-6000 ;
:f:lt?   (f:ab-c)   #10 `-6000 ;
:f:gt?   (f:ab-c)   #11 `-6000 ;
:f:depth (-n)       #12 `-6000 ;
:f:dup   (f:a-aa)   #13 `-6000 ;
:f:drop  (f:a-)     #14 `-6000 ;
:f:swap  (f:ab-ba)  #15 `-6000 ;
:f:log   (f:ab-c)   #16 `-6000 ;
:f:power (f:ab-c)   #17 `-6000 ;
:f:to-number (f:a-__-n)  #18 `-6000 ;
:f:sin   (f:f-f)    #19 `-6000 ;
:f:cos   (f:f-f)    #20 `-6000 ;
:f:tan   (f:f-f)    #21 `-6000 ;
:f:asin  (f:f-f)    #22 `-6000 ;
:f:acos  (f:f-f)    #23 `-6000 ;
:f:atan  (f:f-f)    #24 `-6000 ;
:f:ceiling (f:f-f)  #25 `-6000 ;
:f:sqrt  (f:f-f)    #26 `-6000 ;
:f:square (f:n-m)   f:dup f:* ;
:f:over  (f:ab-aba) f:to-string f:dup s:to-float f:swap ;
:f:tuck  (f:ab-bab) f:swap f:over ;
:f:positive? (-f__f:a-) #0 n:to-float f:gt? ;
:f:negative? (-f__f:a-) #0 n:to-float f:lt? ;
:f:negate (f:a-b)  #-1 n:to-float f:* ;
:f:abs    (f:a-b)  f:dup f:negative? [ f:negate ] if ;
:prefix:. (s-__f:-a)
  compiling? [ s:keep ] [ s:temp ] choose &s:to-float class:word ; immediate
:f:put (f:a-) f:to-string s:put ;
:f:PI (f:-F) .3.141592 ;
:f:E  (f:-F) .2.718281 ;
:f:NAN (f:-n) .0 .0 f:/ ;
:f:INF (f:-n) .1.0 .0 f:/ ;
:f:-INF (f:-n) .-1.0 .0 f:/ ;
:f:nan? (f:n-,-f) f:dup f:-eq? ;
:f:inf? (f:n-,-f) f:INF f:eq? ;
:f:-inf? (f:n-,-f) f:-INF f:eq? ;
:f:round (-|f:a-b)
  f:dup f:negative?
  [ .0.5 f:- f:ceiling ]
  [ .0.5 f:+ f:floor   ] choose ;
~~~

---------------------------------------------------------------

# Float

## Description

This implements a means of encoding floating point values into signed integer cells. The technique is described in the paper titled "Encoding floating point numbers to shorter integers" by Kiyoshi Yoneda and Charles Childers.

This will extend the `f:` vocabulary and adds a new `u:` vocabulary.

## Code & Commentary

Define some constants. The range is slightly reduced from the standard integer range as the smallest value is used for NaN.

~~~
n:MAX n:dec          'u:MAX const
n:MAX n:dec n:negate 'u:MIN const
n:MIN                'u:NAN const
n:MAX                'u:INF const
n:MAX n:negate       'u:-INF const
~~~

~~~
:u:n?    (u-f)
  u:MIN n:inc u:MAX n:dec n:between? ;
:u:max?  (u-f) u:MAX eq? ;
:u:min?  (u-f) u:MIN eq? ;
:u:zero? (u-f) n:zero? ;
:u:nan?  (u-f) u:NAN eq? ;
:u:inf?  (u-f) u:INF eq? ;
:u:-inf? (u-f) u:-INF eq? ;
:u:clip  (u-u) u:MIN u:MAX n:limit ;
~~~

Define the scaling factors. Adjust these as needed for your application.

~~~
:f:U1      (-|f:-b)  .1.e9 ;
:f:BALANCE (-|f:-b)  .1. ; 
~~~

~~~
{{
  :f:scale   (-|f:a-b) f:U1 f:* ;
  :f:descale (-|f:a-b) f:U1 f:/ ;

  :f:encode (-|f:a-b)__n/(s_+_n_)
    f:BALANCE f:over f:+ f:/ ;

  :f:decode (-|f:a-b)_su/(1_-_u_)
    .1. f:over f:- f:/ f:BALANCE f:* ;

---reveal---

  :f:to-u  (-u|f:a-)
    f:dup f:encode f:scale f:round f:to-number u:clip
    f:dup f:nan? [ drop u:NAN ] if
    f:dup f:inf? [ drop u:INF ] if
    f:dup f:-inf? [ drop u:-INF ] if
    f:drop ;

  :u:to-f  (u-|f:-b)
    dup n:to-float f:descale f:decode
    dup u:nan?  [ f:drop f:NAN ] if
    dup u:inf?  [ f:drop f:INF ] if
    dup u:-inf? [ f:drop f:-INF ] if
    drop ;
}}
~~~

~~~
:f:store (a-|f:n-) [ f:to-u ] dip store ;
:f:fetch (a-|f:-n) fetch u:to-f ;
~~~

---------------------------------------------------------------

# Scripting: Command Line Arguments

~~~
:sys:argc (-n) `-6100 ;
:sys:argv (n-s) s:empty swap `-6101 ;
~~~

# System Interaction

The `unix:` namespace contains words for interacting with the
host operating system.

`unix:system` runs another application using the system shell
and returns after execution is completed.

~~~
:unix:system (s-)    #-8000 `-6300 ;
~~~

`unix:fork` forks the current process and returns a process
identifier.

~~~
:unix:fork   (-n)    #-8001 `-6300 ;
~~~

`unix:exit` takes a return code and exits RRE, returning the
specified code.

~~~
:unix:exit   (n-)    #-8002 `-6300 ;
~~~

`unix:getpid` returns the current process identifier.

~~~
:unix:getpid (-n)    #-8003 `-6300 ;
~~~

This group is used to execute a new process in place of the
current one. These take a program and optionally 1-3 arguments.
They map to the execl() system call.

Example:

    '/usr/bin/cal '2 '2019 unix:exec2

~~~
:unix:exec0  (s-)    #-8004 `-6300 ;
:unix:exec1  (ss-)   #-8005 `-6300 ;
:unix:exec2  (sss-)  #-8006 `-6300 ;
:unix:exec3  (ssss-) #-8007 `-6300 ;
~~~

`unix:wait` waits for a child process to complete. This maps to
the wait() system call.

~~~
:unix:wait   (-n)    #-8008 `-6300 ;
~~~

`unix:kill` terminates a process. Takes a process and a signal
to send.

~~~
:unix:kill (nn-)  #-8009 `-6300 ;
~~~

The next two words allow opening and closing pipes. The first,
`unix:popen` takes the name of a program and a file mode and
returns a file handle usable with words in the `file:` namespace.
The second, `unix:pclose` closes the pipe.

~~~
:unix:popen (sn-n) #-8010 `-6300 ;
:unix:pclose (n-) #-8011 `-6300 ;
~~~

~~~
:unix:write (sh-) [ dup s:length ] dip #-8012 `-6300 ;
~~~

`unix:chdir` changes the current working directory to the
specified one.

~~~
:unix:chdir (s-) #-8013 `-6300 ;
~~~

~~~
:unix:getenv (sa-) #-8014 `-6300 ;
:unix:putenv (s-)  #-8015 `-6300 ;
~~~

`unix:sleep` pauses execution for the specified number of
seconds.

~~~
:unix:sleep (n-) #-8016 `-6300 ;
~~~

---------------------------------------------------------------

~~~
:unix:io:n:put (n-) #-8100 `-6300 ;
:unix:io:s:put (s-) #-8101 `-6300 ;
~~~

# Interactive Listener

~~~
'NoEcho var

{{
  :version (-)    @Version #100 /mod n:put $. c:put n:put ;
  :eol?    (c-f)  [ ASCII:CR eq? ] [ ASCII:LF eq? ] [ ASCII:SPACE eq? ] tri or or ;
  :valid?  (s-sf) dup s:length n:-zero? ;
  :ok      (-)    @NoEcho not 0; drop compiling? [ nl 'Ok_ s:put ] -if ;
  :check-eof (c-c) dup [ #-1 eq? ] [ #4 eq? ] bi or [ 'bye d:lookup d:xt fetch call ] if ;
  :check-bs  (c-c) dup [ #8 eq? ] [ #127 eq? ] bi or [ buffer:get buffer:get drop-pair ] if ;
  :s:get      (-s) [ #1025 buffer:set
                     [ c:get dup buffer:add check-eof check-bs eol? ] until
                      buffer:start s:chop ] buffer:preserve ;
---reveal---
  :banner  (-)    @NoEcho not 0; drop
                  'RETRO_12_(rx- s:put version $) c:put nl
                  EOM n:put '_MAX,_TIB_@_1025,_Heap_@_ s:put here n:put nl ;
  :bye     (-)    #0 unix:exit ;
  :listen  (-)
    ok repeat s:get valid? [ interpret ok ] [ drop ] choose again ;
}}
~~~

~~~
:include (s-) `-9999 ;
~~~

~~~
{{
  :gather (c-)
    dup [ #8 eq? ] [ #127 eq? ] bi or [ drop ] [ buffer:add ] choose ;
  :cycle (q-qc)  repeat c:get dup-pair swap call not 0; drop gather again ;
---reveal---
  :parse-until (q-s)
    [ s:empty buffer:set cycle drop-pair buffer:start ] buffer:preserve ;
}}

:s:get (-s) [ [ ASCII:LF eq? ] [ ASCII:CR eq? ] bi or ] parse-until ;
~~~

