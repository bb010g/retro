#!/usr/bin/env retro

This will scan a source file and create output for a `tags`
file in the minimal *ctags* format. In this, the tags file
will contain one line per tag, with a tab separated structure:

    tag-name<tab>tag-file<tab>tag-address

I am using the line number as the tag address.

To generate a tags file:

    retro-tags sourcefile > tags

To start, I bring in the `retro-unu` tool to locate the code
blocks and call a combinator for each line in the code block.
This one is extended to keep track of the tag address and
file name as well.

~~~
'TagAddress var
'TagFile var

{{
  'Fenced var
  :toggle-fence @Fenced not !Fenced ;
  :fenced? (-f) @Fenced ;
  :handle-line (s-)
    fenced? [ over call ] [ drop ] choose ;
  :prepare
    #0 !TagAddress   over s:keep !TagFile ;
---reveal---
  :unu (sq-)
    prepare
    swap [ &TagAddress v:inc
           dup '~~~ s:eq?
           [ drop toggle-fence ]
           [ handle-line       ] choose
         ] file:for-each-line drop ;
}}
~~~

Next, identification of things to tag begins. This will use
multiple passes of the source file.

First, colon definitions.

~~~
:colon-definition?
  dup ': s:begins-with? ;

:output-location
  tab @TagFile s:put tab @TagAddress n:put nl ;

:output-name
  ASCII:SPACE s:tokenize #0 a:fetch n:inc s:put ;

#0 sys:argv [ s:trim colon-definition? &drop -if; output-name output-location ] unu
~~~

Then variables.

~~~
:variable?
  dup 'var s:ends-with? over 'var<n> s:ends-with? or ;

:output-name
  ASCII:SPACE s:tokenize [ s:trim dup '' s:begins-with? [ n:inc s:put ] &drop choose ] a:for-each ;

#0 sys:argv [ s:trim variable? &drop -if; output-name output-location ] unu
~~~

Constants.

~~~
:constant?
  dup 'const s:ends-with? ;

:output-name
  ASCII:SPACE s:tokenize [ s:trim dup '' s:begins-with? [ n:inc s:put ] &drop choose ] a:for-each ;

#0 sys:argv [ s:trim constant? &drop -if; output-name output-location ] unu
~~~

And finally, words made with `d:create`:

~~~
:created?
  dup 'd:create s:ends-with? ;

:output-name
  ASCII:SPACE s:tokenize [ s:trim dup '' s:begins-with? [ n:inc s:put ] &drop choose ] a:for-each ;

#0 sys:argv [ s:trim created? &drop -if; output-name output-location ] unu
~~~
