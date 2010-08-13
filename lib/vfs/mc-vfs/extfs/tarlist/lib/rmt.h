/* Definitions for communicating with a remote tape drive.

   Copyright (C) 1988, 1992, 1996, 1997, 2001, 2003, 2004, 2007 Free
   Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* A filename is remote if it contains a colon not preceded by a slash,
   to take care of `/:/' which is a shorthand for `/.../<CELL-NAME>/fs'
   on machines running OSF's Distributing Computing Environment (DCE) and
   Distributed File System (DFS).  However, when --force-local, a
   filename is never remote.  */

#define rmtopen(dev_name, oflag, mode, command) \
   open (dev_name, oflag, mode)

#define rmtread(fd, buffer, length) \
   safe_read (fd, buffer, length)

#define rmtlseek(fd, offset, where) \
   lseek (fd, offset, where)

#define rmtclose(fd) \
   close (fd)
