snap-carriageways
=================

Attemps to snap points from GPS logs onto the center of directional carriageways,
without reference to any external street map.

It is mostly just clustering nearby points together, on the assumption that
the middle of the road is probably near the mean of the points, but also
only clustering points that are headed approximately the same direction.

It probably should be a 3-dimensional kmeans with direction as one of the
dimensions, but I don't know how you would know how many clusters to expect.

In practice the GPS noise often means you end up with another phantom carriageway
just beyond the edge of the buffer, and lines bounce back and forth between
it and the real center. So it looks a lot better if you look at the output
as points instead of lines.
