# input srctop myfulldir srcdir buildtop libgccfilename
# something like ../../../../asrc/lib/krb5/asn.1/../../../ lib/krb5/asn.1
# 
# output a sequence of sed commands for recognizing and replacing srctop,
# something like:
# s; ../../../../asrc/lib/krb5/asn.1/../../../; $(SRCTOP)/;g
# s; ../../../../asrc/lib/krb5/../../; $(SRCTOP)/;g
# s; ../../../../asrc/lib/../; $(SRCTOP)/;g
# s; ../../../../asrc/; $(SRCTOP)/;g
# s; $(SRCTOP)/lib/krb5/asn.1/; $(srcdir)/;g
# s; $(SRCTOP)/lib/krb5/; $(srcdir)/../;g
# ...

# Output some mostly-fixed patterns first
h
s|^\([^ ]*\) \([^ ]*\) \([^ ]*\) \([^ ]*\) \([^ ]*\)$|# This file is automatically generated by depgen.sed, do not edit it.\
#\
# Parameters used to generate this instance:\
#\
# SRCTOP = \1\
# thisdir = \2\
# srcdir = \3\
# BUILDTOP = \4\
# libgcc file name = \5\
#\
\
# First, remove redundant leading "//" and "./" ...\
s;///*;/;g\
s; \\./; ;g|p
x

h
s|^[^ ]* [^ ]* [^ ]* [^ ]* ||
s|libgcc\.[^ ]*$|include|
s|\.|\\.|g
s|\([^ ][^ ]*\)|\
# Remove gcc's include files resulting from "fixincludes";\
# they're essentially system include files.\
s;\1/[^ ]* ;;g\
s;\1/[^ ]*$;;g\
\
# Recognize $(SRCTOP) and make it a variable reference.\
# (Is this step needed, given the substitutions below?)|p
x

# Drop the last (possibly empty?) word, gcc's prefix.  Then save four words.
s, [^ ]*$,,

h
s/ .*$//
s,\.,\\.,g
s,^,s; ,
s,$,/; $(SRCTOP)/;g,
p
x

# now recognize $(srcdir) and make it a variable reference
# too, unless followed by "/../"
h
s/^[^ ]* [^ ]* //
s/[^ ]*$//
s/\./\\./g
s|^\(.*\)$|\
# Now make $(srcdir) variable references, unless followed by "/../".\
s; \1/; $(srcdir);g\
s; $(srcdir)/\.\./; \1/../;\
\
# Recognize variants of $(SRCTOP).|p
x

# just process first "word"
h
s/ .*$//

# replace multiple slashes with one single one
s,///*,/,g
# replace /./ with /
s,/\./,/,g
# strip trailing slashes, but not if it'd leave the string empty
s,\(..*\)///*,\1/,
# quote dots
s,\.,\\.,g
# turn string into sed pattern
s,^,s; ,
s,$,/; $(SRCTOP)/;g,
# emit potentially multiple patterns
:loop
/\/[a-z][a-zA-Z0-9_.\-]*\/\\\.\\\.\// {
p
s;/[a-z][a-zA-Z0-9_.\-]*/\\\.\\\./;/;
bloop
}
p

x
h
s|^.*$|\
# Now try to produce pathnames relative to $(srcdir).|
p

# now process second "word"
x
h
s/^[^ ]* //
s/ [^ ]* [^ ]*$//

# treat "." specially
/^\.$/{
d
q
}
# make sed pattern
s,^,s; $(SRCTOP)/,
s,$,/; $(srcdir)/;g,
# emit potentially multiple patterns
:loop2
\,[^/)]/; , {
p
# strip trailing dirname off first part; append "../" to second part
s,/[a-z][a-zA-Z0-9_.\-]*/; ,/; ,
s,/;g,/\.\./;g,
bloop2
}

x
s/^[^ ]* [^ ]* [^ ]* //
s/\./\\./g
s|\(^.*$\)|\
# Now substitute for BUILDTOP:\
s; \1/; $(BUILDTOP)/;g|
p

# kill implicit print at end; don't change $(SRCTOP) into .. sequence
s/^.*$/\
# end of sed code generated by depgen.sed/
