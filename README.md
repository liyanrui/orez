# Orez

Orez is a literate programming tool almost without denpendency on any document formatting language and programming language.

## Installation

You might install it into your system via the following command:

```console
$ git clone https://github.com/liyanrui/orez.git
$ cd orez
$ make
$ sudo make install
```

## Basic Source Document Format

For example, there is a file named hello-world.orz and its content is as follows:

```
This is a Hello Wrold program written in C language.

@ hello world # [C]
<main-function>
int main(void)
{
        # display "Hello, World!" on screen @
@

We can use the <puts> function provided by C standard
library to display some information on screen:

@ display "Hello, World!" \
  on screen #
puts("Hello, World!");
@

However, in order to convince C compiler the <puts> function
has been defined, we need to include stdio.h:

@ hello world # <main-functio n> ^+
#include <stdio.h>
@

If there is not any error occurs, this program should end up returning 0: 

@ hello world # +
        return 0;
}
@
```

The above is just an example of the input file format for orez. It does not mean that we should write code like that. 

## Tangle

Using the tangle mode of orez could abstract the source code of C "Hello, World" program:

```console
$ orez --tangle --entrance="hello world" hello-world.orz --output=hello-world.c
```

The more shorter form for the above command is:

```console
$ orez -t -e "hello world" hello-world.orz -o hello-world.c
```

If using the `--line_num` or `-l` option could, the output of tangle mode contains the line number of code fragments:

```console
$ orez -t -l -e "hello world" hello-world.orz -o hello-world.c
```

The result is as follows:

```c
#line 22 "hello-world.orz"
#include <stdio.h>
#line 5 "hello-world.orz"
int main(void)
{
#line 15 "hello-world.orz"
        puts("Hello, World!");
#line 28 "hello-world.orz"
        return 0;
}
```

These line numbers could help us to know the position of a code fragments in the source file, e.g. hello-world.orz.

## Weave

Using the weave mode of orez could convert the hello-world.orz into a YAML document:

```console
$ orez --weave hello-world.orz --output=hello-world.yml
```

or

```console
$ orez -w hello-world.orz -o hello-world.yml
```

or 

```console
$ orez -w hello-world.orz > hello-world.yml
```

The content of hello-world.yml looks like this:

```YAML
- DOC_FRAG: |-
    'This is a Hello Wrold program written in C language.'
- CODE_FRAG:
    NAME: |-
        'hello world'
    ID: 1
    LANG: C
    SOURCE_LABEL: |-
        'main-function'
    CONTENT:
        - SNIPPET: |-
            'int main(void)
            {
            '
        - INDENT: '        '
        - REF:
            NAME: |-
                'display "Hello, World!" on screen'
- DOC_FRAG: |-
    'We can use the <puts> function provided by C standard
    library to display some information on screen:'
- CODE_FRAG:
    NAME: |-
        'display "Hello, World!" \
          on screen'
    LANG: C
    CONTENT:
        - SNIPPET: |-
            'puts("Hello, World!");'
    EMISSIONS:
        - EMISSION:
            NAME: |-
                'hello world'
            ID: 1
- DOC_FRAG: |-
    'However, in order to convince C compiler the <puts> function
    has been defined, we need to include stdio.h:'
- CODE_FRAG:
    NAME: |-
        'hello world'
    ID: 2
    LANG: C
    TARGET_LABEL: |-
        'main-functio n'
    OPERATOR: ^+
    CONTENT:
        - SNIPPET: |-
            '#include <stdio.h>'
- DOC_FRAG: |-
    'If there is not any error occurs, this program should end up returning 0:'
- CODE_FRAG:
    NAME: |-
        'hello world'
    ID: 3
    LANG: C
    OPERATOR: +
    CONTENT:
        - SNIPPET: |-
            '        return 0;
            }'
- DOC_FRAG: |-
    ''
```

You might write some scripts to convert the YAML docuemnt into a particular document format, such as LaTeX, HTML. I have provided two python scripts for ConTeXt MkIV and Markdown respectivily. 

The usages of orez-ctx is as follows:

```console
$ orez -w hello-world.orz | orez-ctx > hello-world.tex
```

The reulst is as follows:

```TeX
This is a Hello Wrold program written in C language.

\startC
/BTEX\pagereference[helloworld1]/ETEX/BTEX\CodeFragmentName{@ hello\ world \#}/ETEX
/BTEX\reference[helloworld-mainfunction]{<main-function>}\OrezReference{<main-function>}/ETEX
int main(void)
{
        /BTEX\Callee{\# display\ "Hello,\ World!"\ on\ screen @}\ <\at[displayHelloWorldonscreen]>/ETEX
\stopC

We can use the <puts> function provided by C standard
library to display some information on screen:

\startC
/BTEX\pagereference[displayHelloWorldcrlfonscreen]/ETEX/BTEX\CodeFragmentName{@ display\ "Hello,\ World!"\ \crlf
\ \ on\ screen \#}/ETEX
puts("Hello, World!");
/BTEX\OrezSymbol{=>}/ETEX /BTEX\Emission{hello world}\ <\at[helloworld1]>/ETEX
\stopC

However, in order to convince C compiler the <puts> function
has been defined, we need to include stdio.h:

\startC
/BTEX\pagereference[helloworld2]/ETEX/BTEX\CodeFragmentName{@ hello\ world \#}/ETEX /BTEX\in[helloworld-mainfunction]/ETEX /BTEX\OrezSymbol{$\wedge+$}/ETEX
#include <stdio.h>
\stopC

If there is not any error occurs, this program should end up returning 0:

\startC
/BTEX\pagereference[helloworld3]/ETEX/BTEX\CodeFragmentName{@ hello\ world \#}/ETEX /BTEX\OrezSymbol{$+$}/ETEX
        return 0;
}
\stopC
```

The orez-md depends on the yaml and pygments modules. Its usages is as follows:

```console
$ orez -w hello-world.orz | orez-md > hello-world.md
```

The result is:

```markdown
This is a Hello Wrold program written in C language.

<pre id="helloworld1" class="orez-code-fragment">
<span class="orez-code-fragment-name">@ hello world #</span>
<span id="helloworld-mainfunction" class="orez-source-label">&lt;main-function&gt;</span>
<span class="kt">int</span> <span class="nf">main</span><span class="p">(</span><span class="kt">void</span><span class="p">)</span>
<span class="p">{</span>
        <a href="#displayHelloWorldonscreen" class="orez-callee-link"># display "Hello, World!" on screen @</a>
</pre>

We can use the <puts> function provided by C standard
library to display some information on screen:

<pre id="displayHelloWorldonscreen" class="orez-code-fragment">
<span class="orez-code-fragment-name">@ display "Hello, World!" \
  on screen #</span>
<span class="n">puts</span><span class="p">(</span><span class="s">&quot;Hello, World!&quot;</span><span class="p">);</span>
<span class="orez-symbol">=&gt;</span> <a href="#helloworld1" class="proc-emissions-name">hello world</a>
</pre>

However, in order to convince C compiler the <puts> function
has been defined, we need to include stdio.h:

<pre id="helloworld2" class="orez-code-fragment">
<span class="orez-code-fragment-name">@ hello world #</span>  <a class="orez-target-label" href="#helloworld-mainfunction">&lt;main-functio n&gt;</a>  <span class="orez-symbol">^+</span>
<span class="cp">#include</span> <span class="cpf">&lt;stdio.h&gt;</span><span class="cp"></span>
</pre>

If there is not any error occurs, this program should end up returning 0:

<pre id="helloworld3" class="orez-code-fragment">
<span class="orez-code-fragment-name">@ hello world #</span>  <span class="orez-symbol">+</span>
        <span class="k">return</span> <span class="mi">0</span><span class="p">;</span>
<span class="p">}</span>
</pre>
```

## More Details

Please see https://github.com/liyanrui/orez/tree/master/example
