# orez
Orez is a literate programming tool almost without denpendency on any document formatting language and programming language.

## Installation

You might install it into your system via the following command:

```console
$ git clone https://github.com/liyanrui/orez.git
$ cd orez
$ make
$ sudo make install
```

## Basic Usage

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

The above is just example and it does not mean that we should write code like that. 

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

You might write some scripts to convert the YAML docuemnt into a particular document format, such as LaTeX, HTML. I have provide two scripts for ConTeXt and Markdown respectivily. Their usages are as follows:

```console
$ orez -w hello-world.orz | orez-ctx-weave > hello-world.tex
$ orez -w hello-world.orz | orez-md-weave > hello-world.md
```
