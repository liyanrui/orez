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
        return 0;
}
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
```

Using the tangle mode of orez could abstract the source code of C "Hello, World" program:

```console
$ orez --tangle --entrance="hello world" hello-world.orz --output=hello-world.c
```

The more shorter form for the above command is:

```console
$ orez -t -e "hello world" hello-world.orz -o hello-world.c
```

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
