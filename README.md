# Slow: Live life in the slow lane

`slow` limits the output rate of a command to a specified bits/second. It can
be used as a simple filter for stdout/stdin, but it can also be used to wrap
interactive tools.

## Why?

I acquired a VT220 terminal (max speed: 19200 bps) and wanted it to display
some dynamic information (scrolling banners, graphs, etc). I wasn't always near
the terminal, and I wanted a way to verify that the things I was trying to do
weren't going to fall over at slower character rates.

## As a simple filter

Slow can be used as a simple filter for stdout:

```
slow -b 1200 date
```

Or for stdout + stdin:

```
echo foo | slow -b 1200 -i sed s/foo/bar/
```

## Wrapping interactive programs

Slow is even more fun when you use it to wrap an interactive program like a shell:

```
slow -b 1200 -it bash
```

## Demo

![slow compile running at 300bps](img/slow-build.gif)

## License

Copyright (C) 2024 Lars Kellogg-Stedman <lars@oddbit.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
