# RETRO 2019.12

This is the changelog for the development builds of Retro.

## Bug Fixes

- (all) strl* functions now only compiled if using GLIBC.
- (rre) `clock:year` corrected
- (rre) `clock:month` corrected

## Build

- Merged Linux & BSD Makefiles

## Core Language

- rename `a:nth` to `a:th`

## Documentation

- updated Linux build instructions
- updated Starting instructions

## Examples

- add sqlite3 wrapper
- add mandelbrot.forth

## General

- reorganized directory tree

## I/O

- added `clock:utc:` namespace

## Interfaces

- retro-compiler: runtime now supports scripting arguments
