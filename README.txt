Conspy
======

  This program is like VNC - but allows you to take over
  a text mode Linux virtual console rather than GUI.  It
  works on Linux and BSD systems.

  Documentation can be found in the man page.

  All documentation is readable online at the home page:
    http://conspy.sourceforge.net/


Dependencies
------------

  - C library.


Building and Installing
-----------------------

  Packages are available for Debian and RedHat style
  distributions at the home page.  If you install using
  one of them you can skip this section.

  The build dependencies are:
    - GNU build system (autoconf, automake, make, gcc),
      http://www.gnu.org/software/autoconf,
      http://www.gnu.org/software/automake,
      http://www.gnu.org/software/make,
      http://gcc.gnu.org/.
    - A POSIX system (unix shell, sed, etc).

  To build the program, in the directory containing this file run:
    make

  To install, in the directory containing this file run:
    make install

  To run the test suite, in the directory containing this file run:
    make test


License
-------

  Copyright (c) 2007-2014,2015,2016 Russell Stuart.

  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU Affero General Public License as published by
  the Free Software Foundation, either version 3 of the License, or (at your
  option) any later version.

  The copyright holders grant you an additional permission under Section 7
  of the GNU Affero General Public License, version 3, exempting you from
  the requirement in Section 6 of the GNU General Public License, version 3,
  to accompany Corresponding Source with Installation Information for the
  Program or any work based on the Program. You are still required to
  comply with all other Section 6 requirements to provide Corresponding
  Source.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.


--
Russell Stuart
2014-May-09
