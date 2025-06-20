.. index:: ! pscoast
.. include:: module_core_purpose.rst_

*******
pscoast
*******

|pscoast_purpose|

Synopsis
--------

.. include:: common_SYN_OPTs.rst_

**gmt pscoast** |-J|\ *parameters*
|SYN_OPT-R|
[ |SYN_OPT-Area| ]
[ |SYN_OPT-B| ]
[ |-C|\ *fill*\ [**+l**\|\ **r**] ]
[ |-D|\ *resolution*\ [**+f**] ]
[ |-E|\ *dcw* ]
[ |-F|\ *box* ]
[ |-G|\ [*fill*] ]
[ |-I|\ *river*\ [/\ *pen*] ]
[ |-K| ]
[ |-L|\ *scalebar* ]
[ |-M| ]
[ |-N|\ *border*\ [/*pen*] ]
[ |-O| ]
[ |-P| ]
[ |-Q| ]
[ |-S|\ [*fill*] ]
[ |-T|\ *rose* ]
[ |SYN_OPT-U| ]
[ |SYN_OPT-V| ]
[ |-W|\ [*level*/]\ *pen* ]
[ |SYN_OPT-X| ]
[ |SYN_OPT-Y| ]
[ |SYN_OPT-bo| ]
[ |SYN_OPT-g| ]
[ |SYN_OPT-p| ]
[ |SYN_OPT-t| ]
[ |SYN_OPT--| ]

.. include:: coast.rst
    :start-after: .. module_common_begins
    :end-before: .. module_common_ends

.. include:: common_classic.rst_

Examples
--------

To plot a green Africa with white outline on blue background, with
permanent major rivers in thick blue pen, additional major rivers in
thin blue pen, and national borders as dashed lines on a Mercator map at
scale 0.1 inch/degree, use::

    gmt pscoast -R-30/30/-40/40 -Jm0.1i -B5 -I1/1p,blue -N1/0.25p,- \
        -I2/0.25p,blue -W0.25p,white -Ggreen -Sblue -P > africa.ps

To plot Iceland using the lava pattern (# 28) at 100 dots per inch, on a
Mercator map at scale 1 cm/degree, run::

    gmt pscoast -RIS+r1 -Jm1c -Baf -Wthin -Gp28+r100 -P > iceland.ps

To initiate a clip path for Africa so that the subsequent colorimage of
gridded topography is only seen over land, using a Mercator map at scale
0.1 inch/degree, use::

    gmt pscoast -R-30/30/-40/40 -Jm0.1i -B5 -G -P -K > africa.ps
    gmt grdimage -J -R @earth_relief_05m -Cgeo -O -K >> africa.ps
    gmt pscoast -Q -O >> africa.ps

To plot Great Britain, Italy, and France in blue with a red outline and
Spain, Portugal and Greece in yellow (no outline), and pick up the plot
domain from the extents of these countries, use::

    gmt pscoast -JM6i -Baf -EGB,IT,FR+gblue+p0.25p,red -EES,PT,GR+gyellow -P > map.ps

To extract a high-resolution coastline data table for Iceland to be used
in your analysis, try::

    gmt pscoast -RIS -Dh -W -M > iceland.txt

To lay down a clip path around France that will clip your later plotting
until you end the clipping with psclip -C, try::

    gmt pscoast -R-10/10/40/52 -JM15c -E+c -P -K > clipped_FR.ps

**pscoast** will first look for coastline files in directory
**$GMT_SHAREDIR**/coast If the desired file is not found, it will look
for the file **$GMT_SHAREDIR**/coastline.conf. This file may contain
any number of records that each holds the full pathname of an
alternative directory. Comment lines (#) and blank lines are allowed.
The desired file is then sought for in the alternate directories.

.. include:: explain_gshhg.rst_

.. include:: coast.rst
    :start-after: .. module_note_begins
    :end-before: .. module_note_ends

See Also
--------

:doc:`gmt`, :doc:`gmt.conf`,
:doc:`gmtcolors`,
:doc:`grdlandmask`,
:doc:`psbasemap`
