# RETRO 2019.12

This is the changelog for the development builds of Retro.
The version number is likely to change; I'm targetting a
July window for this release.

## Bug Fixes

- (all) strl* functions now renamed, included on all builds
- (all) `d:add-header` is extended by retro.forth to remap spaces back to underscores
- (doc) fixed stack comments in glossary
- (ex ) fixed issue in mail.forth
- (rre) `clock:year` corrected
- (rre) `clock:month` corrected

## Build

- Merged Linux & BSD Makefiles
- Now builds on Solaris

## Core Language

- rename: `a:nth` to `a:th`
- rename: `v:update-using` to `v:update`
- add: `a:fetch`
- add: `a:store`
- faster: `times`
- faster: `times<with-index>`
- faster: `while`
- faster: `until`

## Documentation

- merged BSD, Linux, macOS build instructions
- updated Starting instructions
- added implementation notes on arrays

## Examples

- add bury.forth
- add compat.forth
- add gopher.forth
- add magic-8th-ball.forth
- add mandelbrot.forth
- add shell.forth
- add sqlite3 wrapper
- add unix-does-user-exist.forth
- improved 99bottles.forth
- improved edit.forth
- corrected an issue in mail.forth
- cleanup publish-examples.forth
- publish-examples.forth now uses `retro-document` to generate glossaries

## General

- reorganized directory tree

## I/O

- (rre) added `clock:utc:` namespace
- (rre) remove gopher downloader

## Interfaces

- retro-compiler: runtime now supports scripting arguments
- retro-unix: replaces earlier rre.c
- retro-windows: rre, adapted for windows
- retro-unix: remove FullScreenListener
- retro-unix: ok prompt now a hook
- retro-unix: rewrite the listener
