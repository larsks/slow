# Slow: Experience life at a slower pace

`slow` limits the output rate of a command to a specified bits/second.

As an output filter:

```
slow -b 1200 date
```

As an input/output filter:

```
echo foo | slow -b 1200 -i sed s/foo/bar/
```

For running interactive programs:

```
slow -b 1200 -it bash
```

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
