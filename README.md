DIRT - the stuff you don't think about
=========

This is a collection of random useful C++ libraries, with a focus on performance
and usability.  There isn't much documentation because I only just split it off
into its own repo.  So far there is:

 - tap: A testing library for the Test Anything Protocol.  Does not depend on
   anything else in this repo.
 - uni: Some random universal stuff that doesn't depend on anything else,
   including array and string classes that are faster than the STL's, a tiny
   callback reference type, modern assertions, and UTF-8/UTF-16 conversion
   routines.
 - iri: A lightweight International Resource Identifier class.
 - ayu: A data language, a C++ reflection and serialization system, and a
   resource management system.
 - geo: A math and geometry library including vectors, ranges, matrixes, and
   scalar utilities.
 - gl\_api: A version of the OpenGL API written with macros, so you can
   customize it to mean whatever you want.
 - glow: Some thin wrappers around OpenGL objects for use with ayu.
 - control: Types representing keyboard and mouse inputs, and a command system,
   all made to work with ayu.
 - wind: A thin wrapper around SDL\_Window for use with ayu.

### BUILDING AND USAGE

These are things I made to scratch my own itches.  You can use them if you want
but the interface and organization are subject to change without notice.

Most of the code here requires C++20.

This repo has no build workflow of its own yet.  I've been testing it out with
gcc, but theoretically it should compile with clang and msvc with a little work,
