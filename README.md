DIRT - the stuff you don't think about
=========

This is a collection of random useful C++ libraries, with a focus on performance
and usability.  There isn't much documentation because I only just split it off
into its own repo.  So far there is:

 - uni: Some random universal stuff that doesn't depend on anything else,
   including array and string classes that are faster than the STL's, a tiny
   callback reference type, modern assertions, and UTF-8/UTF-16 conversion
   routines.
 - tap: A testing library for the Test Anything Protocol.
 - iri: A lightweight International Resource Identifier (Unicode URI) class.
 - ayu: A data language, a C++ reflection and serialization system, and a
   resource management system.
 - geo: A math and geometry library including vectors, ranges, matrixes, and
   scalar utilities.
 - gl\_api: A version of the OpenGL API written with macros, so you can
   customize it to mean whatever you want.
 - glow: Image loading and some thin wrappers around OpenGL objects for use with
   ayu.
 - snd: Some audio codecs and a simple stereo mixer.
 - control: A text command system made to work with ayu.
 - whereami: Get the directory of the executable.
   From https://github.com/gpakosz/whereami
 - wind: A simple SDL\_Window wrapper, some types to represent keyboard and
   mouse input, and some step-draw loops, made to work with ayu and SDL2.

### BUILDING AND USAGE

These are things I made to scratch my own itches.  You can use them if you want
but the interface and organization are subject to change without notice.

Most of the code here requires C++20.

This repo has no build workflow of its own yet.  I've been using it with gcc,
but it might compile with clang and msvc with a little work.
