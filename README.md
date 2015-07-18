[![Build Status](https://travis-ci.org/dafyddcrosby/sw3m.svg)](https://travis-ci.org/dafyddcrosby/sw3m)

# sw3m

Looking for a web browser that's secure, speedy, small, standards-driven, and
just plain super-duper?

## Rationale

"The nice thing about standards is that there are so many to choose from."

sw3m is a text-based browser that:
- Has a permissive license (ie MIT)
- Aims for a 'clean' codebase, with portability handled separately
- Will handle modern web standards to the extent a text-based browser can
- Focuses on hardening and security - recognizes that, yes, the web *is*
  hostile, even to a text-based browser

## Frequently Asked Questions

Q. Why was support removed for DOS + Windows?

A. Microsoft killed POSIX support in Windows XP, and Interix with Windows 8.
Yes, there's Cygwin and MinGW, but there's little benefit to supporting those
versus the upkeep. Generally speaking, sw3m is intended for systems where
POSIX isn't an afterthought/sales gimmick.

Q. Why was support removed for OS/2?

A. It's dead, Jim.

Q. explicit_bzero isn't POSIX, what gives?

A. memset can get optimized out of the code, which is not something you want
when you're trying to clear the memory. explicit_bzero is easily implemented,
and is currently built-in when not found by autotools.

Q. Will sw3m work on ``<blah>``

A. The reference platform is the latest OpenBSD stable release. Long term,
there are plans to create a sw3m-portable (as is done for OpenSSH and
LibreSSL). However, portability work is unlikely to start until the OpenBSD
releases are in good shape.

Q. Why is ``<feature>`` not enabled by default?!

A. While sw3m aims to support a great deal of functionality 'out of the box'
(eg not requiring recompilation to enable), where possible certain
functionality will not enabled by _default_ - *you* must turn it on in the
configuration. This removes attack surface if you only want to use sw3m for
dumping parsed HTML, etc.
